#include "Application.h"
#include "Config.h"
#include "TestBoxScene.h"
#include <iostream>
#include <memory>
#include <string_view>
#include <cstdlib>

namespace {

void printHelp() {
    std::cout <<
        "FlyTracer - Vulkan Compute Shader Raytracer\n\n"
        "Usage: FlyTracer [options]\n\n"
        "Options:\n"
        "  --width <N>      Window width (default: 1280)\n"
        "  --height <N>     Window height (default: 720)\n"
        "  --title <str>    Window title\n"
        "  --fullscreen     Run in fullscreen mode\n"
        "  --no-vsync       Disable vsync\n"
        "  --help, -h       Show this help message\n";
}

[[nodiscard]] bool parseIntArg(const char* value, int& out) noexcept {
    try {
        out = std::stoi(value);
        return true;
    } catch (...) {
        return false;
    }
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    try {
        FlyTracer::AppConfig config;
        (void)config.loadFromFile("resources/config.cfg");

        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];

            if (arg == "--width" && i + 1 < argc) {
                if (!parseIntArg(argv[++i], config.windowWidth)) {
                    std::cerr << "Invalid width value, using default\n";
                }
            } else if (arg == "--height" && i + 1 < argc) {
                if (!parseIntArg(argv[++i], config.windowHeight)) {
                    std::cerr << "Invalid height value, using default\n";
                }
            } else if (arg == "--title" && i + 1 < argc) {
                config.windowTitle = argv[++i];
            } else if (arg == "--fullscreen") {
                config.fullscreen = true;
            } else if (arg == "--no-vsync") {
                config.vsync = false;
            } else if (arg == "--help" || arg == "-h") {
                printHelp();
                return EXIT_SUCCESS;
            }
        }

        auto scene = std::make_unique<TestBoxScene>(config.resourceDirectory);
        Application app(config.windowWidth, config.windowHeight, config.windowTitle,
                        std::move(scene), config.shaderDirectory);
        app.run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
