#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTEVALUATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTEVALUATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace ShaderRecompiler::Detail {

// Open-addressed IrValue* -> value table. The first 64 slots live in the object: an evaluator is
// built per draw stage (twice per walk, once more per ReadFirstLane), and the heap table it grew
// into on its first insert was an allocation, a 1 KiB clear and a free on the driver thread.
class EvaluatedValues {
public:
    EvaluatedValues() = default;
    EvaluatedValues(const EvaluatedValues&) = delete;
    EvaluatedValues& operator=(const EvaluatedValues&) = delete;

    bool Find(const IrValue* key, std::uint64_t& value) const {
        if (_count == 0) {
            return false;
        }
        const Slot* slots = Slots();
        const std::size_t mask = Capacity() - 1u;
        for (std::size_t index = Home(key, mask);; index = (index + 1u) & mask) {
            const auto& slot = slots[index];
            if (slot.key == key) {
                value = slot.value;
                return true;
            }
            if (slot.key == nullptr) {
                return false;
            }
        }
    }
    void Insert(const IrValue* key, std::uint64_t value) {
        if ((_count + 1u) * 2u > Capacity()) {
            Grow();
        }
        Slot* slots = Slots();
        const std::size_t mask = Capacity() - 1u;
        for (std::size_t index = Home(key, mask);; index = (index + 1u) & mask) {
            auto& slot = slots[index];
            if (slot.key == key) {
                return;
            }
            if (slot.key == nullptr) {
                slot = {key, value};
                ++_count;
                return;
            }
        }
    }

private:
    struct Slot {
        const IrValue* key = nullptr;
        std::uint64_t value = 0;
    };
    static constexpr std::size_t InlineSlots = 64;
    static std::size_t Home(const IrValue* key, std::size_t mask) {
        return static_cast<std::size_t>((reinterpret_cast<std::uintptr_t>(key) >> 4u) * 0x9e3779b97f4a7c15ull >> 32u) & mask;
    }
    std::size_t Capacity() const { return _heap.empty() ? InlineSlots : _heap.size(); }
    Slot* Slots() { return _heap.empty() ? _inline.data() : _heap.data(); }
    const Slot* Slots() const { return _heap.empty() ? _inline.data() : _heap.data(); }
    void Grow() {
        std::vector<Slot> next(Capacity() * 2u);
        const std::size_t mask = next.size() - 1u;
        const Slot* slots = Slots();
        for (std::size_t i = 0; i < Capacity(); ++i) {
            if (slots[i].key == nullptr) continue;
            for (std::size_t index = Home(slots[i].key, mask);; index = (index + 1u) & mask) {
                if (next[index].key == nullptr) {
                    next[index] = slots[i];
                    break;
                }
            }
        }
        _heap.swap(next);
    }
    std::array<Slot, InlineSlots> _inline{};
    std::vector<Slot> _heap;
    std::size_t _count = 0;
};

// The values being evaluated (a cycle guard): a walk nests a few levels deep, so the stack lives
// in the evaluator and only a deeper walk spills to the heap.
class VisitStack {
public:
    bool Contains(const IrValue* value) const {
        const auto inlined = std::min(_size, _inline.size());
        if (std::find(_inline.begin(), _inline.begin() + inlined, value) != _inline.begin() + inlined) return true;
        return std::find(_overflow.begin(), _overflow.end(), value) != _overflow.end();
    }
    void Push(IrValue* value) {
        if (_size < _inline.size()) _inline[_size] = value;
        else _overflow.push_back(value);
        ++_size;
    }
    void Pop() {
        --_size;
        if (_size >= _inline.size()) _overflow.pop_back();
    }

private:
    std::array<IrValue*, 32> _inline{};
    std::vector<IrValue*> _overflow;
    std::size_t _size = 0;
};

struct InaccessibleRead {
    const IrValue* read = nullptr;
    std::uint64_t address = 0;
};

class Evaluator {
public:
    Evaluator(const IrResourcePlan& program, const SrtRuntime& runtime, std::span<const std::uint8_t> cleanFlatSlots = {}, Evaluator* cleanEvaluator = nullptr, IrValue* activeMask = nullptr) : _program(program), _runtime(runtime), _cleanFlatSlots(cleanFlatSlots), _cleanEvaluator(cleanEvaluator), _activeMask(activeMask != nullptr ? activeMask->Resolve() : nullptr) {}

    bool Evaluate(IrValue* value, std::uint32_t& result);
    bool EvaluateWide(IrValue* raw, std::uint64_t& result);
    void ReportInaccessibleReads(InaccessibleRead* sink) { _inaccessible = sink; }
    void ReportNullRootReads(std::uint32_t* sink) { _nullRoots = sink; }

private:
    static float Float32(std::uint64_t bits);
    static std::uint64_t Float32Bits(float value);

    bool Arg(IrValue& inst, std::size_t index, std::uint64_t& result);
    bool EvaluatePhi(IrValue& inst, std::uint64_t& result);
    bool EvaluateExtract(IrValue& inst, std::uint64_t& result);
    bool EvaluateRawRead(IrValue& inst, std::uint64_t& result);
    bool EvaluateInst(IrValue& inst, std::uint64_t& result);

    const IrResourcePlan& _program;
    const SrtRuntime& _runtime;
    std::span<const std::uint8_t> _cleanFlatSlots;
    Evaluator* _cleanEvaluator = nullptr;
    IrValue* _activeMask = nullptr;
    InaccessibleRead* _inaccessible = nullptr;
    std::uint32_t* _nullRoots = nullptr;
    EvaluatedValues _cache;
    VisitStack _visiting;
};

}

#endif
