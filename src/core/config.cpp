#include "config.h"

#include <fstream>
#include <iostream>

namespace luu {

Config::Config(std::filesystem::path path) : path_(std::move(path)), data_(defaultData()) {}

nlohmann::json Config::defaultData() const {
    return {
        {"hotkey", "<alt>+u"},
        {"upscale_mode", "fsr"},
        {"target_resolution", {1920, 1080}},
        {"frame_gen_enabled", true},
        {"framegen_method", "lsfg"},
        {"quality", "high"},
        {"capture_backend", "auto"},
        {"capture_output", ""},
    };
}

bool Config::load() {
    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) {
        std::cerr << "[Config] No config at " << path_ << ", writing defaults\n";
        return save();
    }

    std::ifstream in(path_);
    if (!in) {
        std::cerr << "[Config] Error: could not open " << path_ << " for reading\n";
        return false;
    }

    nlohmann::json loaded;
    try {
        in >> loaded;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[Config] Error: malformed JSON in " << path_ << ": " << e.what() << "\n";

        auto backup = path_;
        backup += ".bak";
        std::error_code copy_ec;
        std::filesystem::copy_file(path_, backup,
                                    std::filesystem::copy_options::overwrite_existing, copy_ec);
        if (!copy_ec) {
            std::cerr << "[Config] Backed up corrupt file to " << backup << "\n";
        }

        std::cerr << "[Config] Falling back to defaults for this session\n";
        return false;
    }

    // Merge onto defaults so keys added in newer versions are still present.
    nlohmann::json merged = defaultData();
    merged.merge_patch(loaded);
    data_ = std::move(merged);
    return true;
}

bool Config::save() const {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) {
        std::cerr << "[Config] Error: could not create " << path_.parent_path() << ": "
                   << ec.message() << "\n";
        return false;
    }

    std::ofstream out(path_);
    if (!out) {
        std::cerr << "[Config] Error: could not open " << path_ << " for writing\n";
        return false;
    }

    out << data_.dump(4);
    if (!out) {
        std::cerr << "[Config] Error: write to " << path_ << " failed\n";
        return false;
    }
    return true;
}

}  // namespace luu
