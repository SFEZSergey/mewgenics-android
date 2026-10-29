#include "GlaielEngine.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::string base_path = argc > 1 ? argv[1] : "/mnt/c/Mewgenics/PPSA37318";
    std::cout << "[TEST] Тестирование GlaielEngine с путем: " << base_path << std::endl;

    glaiel::GlaielEngine engine;
    if (!engine.Initialize(base_path)) {
        std::cerr << "[TEST] Ошибка инициализации!" << std::endl;
        return 1;
    }

    if (!engine.LoadSwfList()) {
        std::cerr << "[TEST] Ошибка загрузки swflist!" << std::endl;
        return 1;
    }

    if (!engine.LoadCatGen()) {
        std::cerr << "[TEST] Ошибка загрузки catgen!" << std::endl;
        return 1;
    }

    std::cout << "[TEST] УСПЕХ: Все нативные структуры движка Glaiel успешно распарсены!" << std::endl;
    return 0;
}
