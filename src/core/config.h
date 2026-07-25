#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace luu {

// Loads/saves the global LUU settings.json. Missing keys fall back to
// built-in defaults; a corrupt file is backed up rather than overwritten
// silently, so a bad edit never destroys the user's config.
class Config {
public:
    explicit Config(std::filesystem::path path);

    // Reads the config file, creating it from defaults if it doesn't exist
    // yet. Returns false (and logs to stderr) if the file exists but could
    // not be parsed; defaults_ is used in that case so the app can still run.
    bool load();

    // Writes the current in-memory state back to disk. Returns false on
    // failure (e.g. no write permission).
    bool save() const;

    const std::filesystem::path& path() const { return path_; }
    const nlohmann::json& data() const { return data_; }

    template <typename T>
    T get(const std::string& key, T defaultValue) const {
        return data_.value(key, defaultValue);
    }

    template <typename T>
    void set(const std::string& key, T value) {
        data_[key] = std::move(value);
    }

private:
    nlohmann::json defaultData() const;

    std::filesystem::path path_;
    nlohmann::json data_;
};

}  // namespace luu
