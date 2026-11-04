#include "Application.h"
#include "Benchmark.h"
#include "Config.h"
#include "SceneRegistry.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
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
        "  --scene <name>   Scene:";
    for (const std::string_view name : kSceneNames) {
        std::cout << " " << name;
    }
    std::cout << "\n"
        "  --fullscreen     Run in fullscreen mode\n"
        "  --no-vsync       Disable vsync\n"
        "  --benchmark [N]  Render every scene headless for N frames (default: 200) at\n"
        "                   --width x --height pixels (default: 1920 x 1080) and print the\n"
        "                   raytracing times\n"
        "  --benchmark-csv <file>  Also write the benchmark results as CSV\n"
        "  --help, -h       Show this help message\n";
}

int runBenchmark(const FlyTracer::AppConfig& config, VkExtent2D extent, uint32_t frames, const std::string& csvPath) {
    const std::string resourceDir = ExecutableRelativePath(config.resourceDirectory).string();
    std::vector<BenchmarkScene> scenes;
    for (const std::string_view name : kSceneNames) {
        scenes.push_back({std::string(name), [name, resourceDir] { return CreateScene(name, resourceDir); }});
    }

    const BenchmarkReport report = RunBenchmark({
        .extent = extent,
        .frames = frames,
        .maxBounces = static_cast<uint32_t>(std::max(config.maxRayBounces, 1)),
        .shaderDir = ExecutableRelativePath(config.shaderDirectory).string(),
    }, scenes);

    std::cout << "\n";
    PrintBenchmark(std::cout, report);
    if (!csvPath.empty()) {
        std::ofstream csv(csvPath);
        if (!csv) {
            std::cerr << "Could not write " << csvPath << "\n";
            return EXIT_FAILURE;
        }
        WriteBenchmarkCsv(csv, report);
    }
    return EXIT_SUCCESS;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    try {
        FlyTracer::AppConfig config;
        (void)config.loadFromFile(ExecutableRelativePath("resources/config.cfg").string());
        std::string sceneName = "bireflection";
        bool titleGiven = false;
        bool benchmark = false;
        int benchmarkFrames = 200;
        std::string benchmarkCsv;
        VkExtent2D benchmarkExtent{1920, 1080};

        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];

            if (arg == "--width" && i + 1 < argc) {
                if (FlyTracer::parseNumber(argv[++i], config.windowWidth, 1, FlyTracer::AppConfig::kMaxWindowSize)) {
                    benchmarkExtent.width = static_cast<uint32_t>(config.windowWidth);
                } else {
                    std::cerr << "Invalid width value, using default\n";
                }
            } else if (arg == "--height" && i + 1 < argc) {
                if (FlyTracer::parseNumber(argv[++i], config.windowHeight, 1, FlyTracer::AppConfig::kMaxWindowSize)) {
                    benchmarkExtent.height = static_cast<uint32_t>(config.windowHeight);
                } else {
                    std::cerr << "Invalid height value, using default\n";
                }
            } else if (arg == "--title" && i + 1 < argc) {
                config.windowTitle = argv[++i];
                titleGiven = true;
            } else if (arg == "--scene" && i + 1 < argc) {
                sceneName = argv[++i];
            } else if (arg == "--fullscreen") {
                config.fullscreen = true;
            } else if (arg == "--no-vsync") {
                config.vsync = false;
            } else if (arg == "--benchmark") {
                benchmark = true;
                if (i + 1 < argc && FlyTracer::parseNumber(argv[i + 1], benchmarkFrames)) {
                    ++i;
                }
            } else if (arg == "--benchmark-csv" && i + 1 < argc) {
                benchmarkCsv = argv[++i];
            } else if (arg == "--help" || arg == "-h") {
                printHelp();
                return EXIT_SUCCESS;
            }
        }

        if (benchmark) {
            return runBenchmark(config, benchmarkExtent, static_cast<uint32_t>(std::max(benchmarkFrames, 1)),
                                benchmarkCsv);
        }
        if (sceneName == "hornet" && !titleGiven) {
            config.windowTitle = "HornetTracer";
        }
        Application(config, CreateScene(sceneName, ExecutableRelativePath(config.resourceDirectory).string())).run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
