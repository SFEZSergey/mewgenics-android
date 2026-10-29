#include "GlaielEngine.hpp"
#include "libtess2/Include/tesselator.h"
#include <fstream>
#include <iostream>
#include <cstring>

namespace glaiel {

GlaielEngine::GlaielEngine() {}

GlaielEngine::~GlaielEngine() {}

bool GlaielEngine::Initialize(const std::string& base_path) {
    base_path_ = base_path;
    if (!base_path_.empty() && base_path_.back() != '/' && base_path_.back() != '\\') {
        base_path_ += "/";
    }

    std::cout << "[GlaielEngine] Инициализация нативного рантайма: " << base_path_ << std::endl;
    initialized_ = true;
    return true;
}

bool GlaielEngine::LoadSwfList() {
    std::string path = base_path_ + "swfs/swflist.gon";
    try {
        GonObject swflist = GonObject::Load(path);
        std::cout << "[GlaielEngine] Загружен swflist.gon!" << std::endl;

        if (swflist.Contains("game")) {
            const auto& game_array = swflist["game"];
            swf_files_.clear();
            for (size_t i = 0; i < game_array.size(); ++i) {
                swf_files_.push_back(game_array[i].String());
            }
            std::cout << "[GlaielEngine] Найдено SWF файлов в игре: " << swf_files_.size() << std::endl;
            return true;
        }
    } catch (const std::string& err) {
        std::cerr << "[GlaielEngine] Ошибка загрузки swflist.gon: " << err << std::endl;
    }
    return false;
}

bool GlaielEngine::LoadCatGen() {
    std::string path = base_path_ + "data/catgen.gon";
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        // Оборачиваем корневые поля файла в объект
        std::string wrapped = "{\n" + content + "\n}";
        catgen_data_ = GonObject::LoadFromBuffer(wrapped);
        std::cout << "[GlaielEngine] УСПЕШНО загружен catgen.gon! Число параметров кошек: " << catgen_data_.size() << std::endl;
        return true;
    } catch (const std::string& err) {
        std::cerr << "[GlaielEngine] Ошибка загрузки catgen.gon: " << err << std::endl;
    }
    return false;
}

bool GlaielEngine::ParseSwfHeader(const std::string& relative_swf, SwfHeader& out_header) {
    std::string full_path = base_path_ + "swfs/" + relative_swf;
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.read(out_header.signature, 3);
    file.read(reinterpret_cast<char*>(&out_header.version), 1);
    file.read(reinterpret_cast<char*>(&out_header.file_length), 4);

    out_header.frame_width = 1920;
    out_header.frame_height = 1080;
    out_header.frame_rate = 60.0f;
    out_header.frame_count = 1;
    return true;
}

void GlaielEngine::Update(float delta_time) {
    // Обновление логики и анимаций
}

void GlaielEngine::Render() {
    // Отрисовка геометрии через Vulkan SDL_GPU
}

} // namespace glaiel

static std::unique_ptr<glaiel::GlaielEngine> g_engine;

extern "C" {

int glaiel_native_init(const char* base_path) {
    g_engine = std::make_unique<glaiel::GlaielEngine>();
    return g_engine->Initialize(base_path ? base_path : "") ? 0 : -1;
}

int glaiel_native_load_data() {
    if (!g_engine) return -1;
    bool ok = g_engine->LoadSwfList() && g_engine->LoadCatGen();
    return ok ? 0 : -1;
}

void glaiel_native_step(float dt) {
    if (g_engine) {
        g_engine->Update(dt);
        g_engine->Render();
    }
}

}
