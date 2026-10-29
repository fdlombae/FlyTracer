#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <iostream>
#include <charconv>
#include <system_error>

namespace FlyTracer {

namespace detail {

[[nodiscard]] inline std::string_view trim(std::string_view str) noexcept {
    const auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    const auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace detail

// Parses the whole string as a number; out is unchanged when that fails
template<typename T>
bool parseNumber(std::string_view str, T& out) noexcept {
    const char* end = str.data() + str.size();
    T value{};
    const auto [ptr, ec] = std::from_chars(str.data(), end, value);
    if (ec != std::errc{} || ptr != end) return false;
    out = value;
    return true;
}

// The same, for a number in [min, max]
template<typename T>
bool parseNumber(std::string_view str, T& out, T min, T max) noexcept {
    T value{};
    if (!parseNumber(str, value) || value < min || value > max) return false;
    out = value;
    return true;
}

struct AppConfig {
    static constexpr int kMaxWindowSize{16384};
    static constexpr int kMaxRayBounces{32};

    // Window settings
    int windowWidth{1280};
    int windowHeight{720};
    std::string windowTitle{"FlyTracer"};
    bool fullscreen{false};
    bool vsync{true};

    // Raytracing settings
    int maxRayBounces{3};

    // Resource paths
    std::string resourceDirectory{"resources"};
    std::string shaderDirectory{"shaders"};

    [[nodiscard]] bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            std::cerr << "Config: Could not open " << filename << ", using defaults\n";
            return false;
        }

        // key=value lines; empty lines, comments (# or ;) and lines without = are skipped
        std::string line;
        while (std::getline(file, line)) {
            const auto trimmedLine = detail::trim(line);
            const auto pos = trimmedLine.find('=');
            if (trimmedLine.starts_with('#') || trimmedLine.starts_with(';') || pos == std::string_view::npos) {
                continue;
            }
            parseConfigLine(detail::trim(trimmedLine.substr(0, pos)), detail::trim(trimmedLine.substr(pos + 1)));
        }

        std::cout << "Config: Loaded settings from " << filename << "\n";
        return true;
    }

    [[nodiscard]] bool saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file) {
            std::cerr << "Config: Could not write to " << filename << "\n";
            return false;
        }

        file << "# FlyTracer Configuration File\n"
             << "# Generated automatically\n\n"
             << "# Window Settings\n"
             << "window_width=" << windowWidth << "\n"
             << "window_height=" << windowHeight << "\n"
             << "window_title=" << windowTitle << "\n"
             << "fullscreen=" << (fullscreen ? "true" : "false") << "\n"
             << "vsync=" << (vsync ? "true" : "false") << "\n\n"
             << "# Raytracing Settings\n"
             << "max_ray_bounces=" << maxRayBounces << "\n\n"
             << "# Resource Paths\n"
             << "resource_directory=" << resourceDirectory << "\n"
             << "shader_directory=" << shaderDirectory << "\n";

        std::cout << "Config: Saved settings to " << filename << "\n";
        return true;
    }

private:
    void parseConfigLine(std::string_view key, std::string_view value) {
        const bool flag = value == "true" || value == "1";
        if (key == "window_width") parseNumber(value, windowWidth, 1, kMaxWindowSize);
        else if (key == "window_height") parseNumber(value, windowHeight, 1, kMaxWindowSize);
        else if (key == "window_title") windowTitle = value;
        else if (key == "fullscreen") fullscreen = flag;
        else if (key == "vsync") vsync = flag;
        else if (key == "max_ray_bounces") parseNumber(value, maxRayBounces, 1, kMaxRayBounces);
        else if (key == "resource_directory") resourceDirectory = value;
        else if (key == "shader_directory") shaderDirectory = value;
    }
};

} // namespace FlyTracer
