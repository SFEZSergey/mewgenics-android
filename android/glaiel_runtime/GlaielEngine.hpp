#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "gon.h"

namespace glaiel {

struct SwfHeader {
    char signature[3]; // FWS, CWS, or ZWS
    uint8_t version;
    uint32_t file_length;
    int frame_width;
    int frame_height;
    float frame_rate;
    uint16_t frame_count;
};

class GlaielEngine {
public:
    GlaielEngine();
    ~GlaielEngine();

    bool Initialize(const std::string& base_path);
    bool LoadSwfList();
    bool LoadCatGen();
    bool ParseSwfHeader(const std::string& swf_path, SwfHeader& out_header);

    void Update(float delta_time);
    void Render();

    const std::string& GetBasePath() const { return base_path_; }
    const std::vector<std::string>& GetSwfFiles() const { return swf_files_; }

private:
    std::string base_path_;
    std::vector<std::string> swf_files_;
    GonObject catgen_data_;
    bool initialized_ = false;
};

} // namespace glaiel

extern "C" {
    int glaiel_native_init(const char* base_path);
    int glaiel_native_load_data();
    void glaiel_native_step(float dt);
}
