#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iterator>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <utility>

namespace AgcDriver::DriverDetail {

namespace {

// Draw entries a newer copy displaced or the cache evicted die on this thread: freeing their
// variants (recompile results, artifacts) and recipe records took a sixth of the queue worker,
// under the cache mutex. APS5_NO_ENTRY_REAPER=1 frees them in place again.
class EntryReaper {
public:
    static EntryReaper& Get() {
        static auto* reaper = new EntryReaper();
        return *reaper;
    }
    static bool Enabled() {
        static const bool enabled = std::getenv("APS5_NO_ENTRY_REAPER") == nullptr;
        return enabled;
    }
    void Release(std::vector<std::shared_ptr<DrawEntry>>& entries) {
        if (entries.empty()) return;
        bool full = false;
        {
            std::lock_guard lock(mutex);
            if (queued.empty()) queued.swap(entries);
            else std::move(entries.begin(), entries.end(), std::back_inserter(queued));
            full = queued.size() >= WakeEntries;
        }
        entries.clear();
        // A futex wake per released draw was ~1% of the queue worker: the reaper is woken once a
        // batch has gathered and otherwise collects the rest on its own every 50 ms.
        if (full) wake.notify_one();
    }

private:
    EntryReaper() {
        std::thread([this] { run(); }).detach();
    }
    void run() {
        std::vector<std::shared_ptr<DrawEntry>> batch;
        for (;;) {
            {
                std::unique_lock lock(mutex);
                wake.wait_for(lock, std::chrono::milliseconds(50), [&] { return queued.size() >= WakeEntries; });
                batch.swap(queued);
            }
            batch.clear();
        }
    }
    static constexpr std::size_t WakeEntries = 64;
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<std::shared_ptr<DrawEntry>> queued;
};

}

bool DrawRecipeRecord::Matches(const std::vector<std::shared_ptr<DispatchVariant>>& variants) const {
    if (stages.size() != variants.size()) return false;
    for (std::size_t i = 0; i < stages.size(); ++i) {
        if (stages[i].owner_before(variants[i]) || variants[i].owner_before(stages[i])) return false;
    }
    return true;
}

bool DrawRecipeRecord::Expired() const {
    return std::any_of(stages.begin(), stages.end(), [](const std::weak_ptr<const DispatchVariant>& stage) { return stage.expired(); });
}

bool Driver::drawEntries() {
    static const bool entries = std::getenv("APS5_NO_DRAW_SRT_ENTRIES") == nullptr && !stampValidate();
    return entries;
}

bool Driver::verifyDrawEntries() {
    static const bool verify = std::getenv("APS5_VERIFY_DRAW_ENTRIES") != nullptr;
    return verify;
}

bool Driver::registerKeyEnabled() {
    static const bool registerKey = std::getenv("APS5_NO_DRAW_KEY") == nullptr;
    return registerKey;
}

bool Driver::verifyDrawRecipe() {
    static const bool verify = std::getenv("APS5_VERIFY_DRAW_RECIPE") != nullptr;
    return verify;
}

std::size_t Driver::drawCacheEntries() {
    static const std::size_t entries = [] {
        const char* text = std::getenv("APS5_DRAW_CACHE_ENTRIES");
        const auto parsed = text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
        return parsed != 0 ? static_cast<std::size_t>(parsed) : std::size_t{4096};
    }();
    return entries;
}

void Driver::accountDrawVariant(const DispatchVariant& variant, bool added) {
    if (added) {
        ++drawCacheVariants;
        drawCacheVariantBytes += variantBytes(variant);
    } else {
        --drawCacheVariants;
        drawCacheVariantBytes -= variantBytes(variant);
    }
}

void Driver::insertDrawEntry(std::uint64_t key, std::vector<std::shared_ptr<DispatchVariant>>& fresh, std::shared_ptr<const DrawDecode> decode) {
    static thread_local std::vector<std::shared_ptr<DrawEntry>> doomed;
    insertDrawEntryLocked(key, fresh, std::move(decode), doomed);
    if (EntryReaper::Enabled()) EntryReaper::Get().Release(doomed);
    else doomed.clear();
}

void Driver::insertDrawEntryLocked(std::uint64_t key, std::vector<std::shared_ptr<DispatchVariant>>& fresh, std::shared_ptr<const DrawDecode> decode, std::vector<std::shared_ptr<DrawEntry>>& doomed) {
    std::lock_guard cacheLock(drawCacheMutex);
    auto& counters = drawEntryCounters;
    ++counters.inserts;
    const auto found = drawCache.find(key);
    auto replacement = std::make_shared<DrawEntry>();
    replacement->stages.resize(fresh.size());
    replacement->decode = std::move(decode);
    if (found != drawCache.end()) {
        if (found->second->stages.size() == fresh.size()) replacement->stages = found->second->stages;
        if (found->second->decode != nullptr) replacement->decode = found->second->decode;
        replacement->recipes.store(found->second->recipes.load());
    }
    for (std::size_t i = 0; i < fresh.size(); ++i) {
        if (fresh[i] == nullptr) continue;
        auto& variants = replacement->stages[i];
        const auto present = std::find_if(variants.begin(), variants.end(), [&](const std::shared_ptr<DispatchVariant>& kept) { return kept->pushOffset == fresh[i]->pushOffset && kept->runs == fresh[i]->runs && kept->words == fresh[i]->words; });
        if (present != variants.end()) {
            ++counters.present;
            fresh[i] = *present;
            continue;
        }
        accountDrawVariant(*fresh[i], true);
        variants.insert(variants.begin(), fresh[i]);
        ++counters.variantsInserted;
        while (variants.size() > dispatchVariants()) {
            accountDrawVariant(*variants.back(), false);
            variants.pop_back();
            ++counters.variantsEvicted;
        }
    }
    replacement->touched = drawCacheHits;
    if (found == drawCache.end()) {
        drawOrder.push_front(key);
        replacement->order = drawOrder.begin();
        drawCache.emplace(key, std::move(replacement));
    } else {
        replacement->order = found->second->order;
        drawOrder.splice(drawOrder.begin(), drawOrder, replacement->order);
        doomed.push_back(std::exchange(found->second, std::move(replacement)));
    }
    while (drawCache.size() > drawCacheEntries()) {
        const auto last = drawCache.find(drawOrder.back());
        for (const auto& variants : last->second->stages) {
            for (const auto& variant : variants) accountDrawVariant(*variant, false);
        }
        drawOrder.erase(last->second->order);
        doomed.push_back(std::move(last->second));
        drawCache.erase(last);
        ++drawCacheEvictions;
    }
}

void Driver::eraseDrawEntry(std::uint64_t key, const std::shared_ptr<DrawEntry>& expected) {
    const auto found = drawCache.find(key);
    if (found == drawCache.end() || found->second != expected) return;
    for (const auto& variants : found->second->stages) {
        for (const auto& variant : variants) accountDrawVariant(*variant, false);
    }
    drawOrder.erase(found->second->order);
    drawCache.erase(found);
}

bool DecodeProgramsCurrent(const DrawDecode& decode, const ShaderRegistry&) {
    return std::all_of(decode.programs.begin(), decode.programs.end(), [&](const DrawProgram& program) {
        return !program.snapshot->header.empty() || ReadRawShader(program.snapshot->codeAddress + program.codeOffset * sizeof(std::uint32_t)) == program.snapshot;
    });
}

std::shared_ptr<const DrawRecipe> Driver::findDrawRecipe(std::uint64_t key, const std::vector<std::shared_ptr<DispatchVariant>>& stages, const std::vector<std::uint64_t>& shape) {
    std::shared_ptr<const std::vector<DrawRecipeRecord>> records;
    {
        std::lock_guard cacheLock(drawCacheMutex);
        const auto found = drawCache.find(key);
        if (found == drawCache.end()) return nullptr;
        records = found->second->recipes.load();
    }
    if (records == nullptr) return nullptr;
    for (const auto& record : *records) {
        if (record.Matches(stages)) return record.recipe;
    }
    if (!shape.empty()) {
        for (const auto& record : *records) {
            if (record.shape == shape) return record.recipe;
        }
    }
    return nullptr;
}

void Driver::attachDrawRecipe(std::uint64_t key, const std::vector<std::shared_ptr<DispatchVariant>>& stages, const std::vector<std::uint64_t>& shape, std::shared_ptr<const DrawRecipe> recipe) {
    std::shared_ptr<DrawEntry> entry;
    {
        std::lock_guard cacheLock(drawCacheMutex);
        const auto found = drawCache.find(key);
        if (found == drawCache.end()) return;
        entry = found->second;
    }
    const auto old = entry->recipes.load();
    auto records = std::make_shared<std::vector<DrawRecipeRecord>>();
    records->push_back({std::vector<std::weak_ptr<const DispatchVariant>>(stages.begin(), stages.end()), std::move(recipe), shape});
    if (old != nullptr) {
        for (const auto& record : *old) {
            // A record of the same shape is replaced (its recipe is the older one of the two).
            if (record.Matches(stages) || (!shape.empty() && record.shape == shape) || (record.Expired() && record.shape.empty()) || records->size() >= dispatchVariants()) continue;
            records->push_back(record);
        }
    }
    entry->recipes.store(std::move(records));
    VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Attach, VulkanDevice::RecipeKind::Draw);
}

}
