#pragma once

#include "GameScene.h"
#include "GpuTiming.h"
#include <vulkan/vulkan.h>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

struct BenchmarkScene {
    std::string name;
    std::function<std::unique_ptr<GameScene>()> create;
};

struct BenchmarkSettings {
    VkExtent2D extent{1920, 1080};  // Pixels
    uint32_t warmupFrames{10};
    uint32_t frames{200};
    uint32_t maxBounces{3};
    std::string shaderDir{"shaders"};
};

struct BenchmarkResult {
    std::string scene;
    uint32_t frames{0};
    std::optional<TimingSummary> gpuMilliseconds;  // Timestamps around the dispatch, when the driver resolves them
    TimingSummary submitMilliseconds;              // Wall time from submitting a frame until it completed
};

struct BenchmarkReport {
    VkExtent2D extent{};
    uint32_t maxBounces{0};
    double emptySubmitMilliseconds{0.0};  // The part of the submit time that is not raytracing
    std::vector<BenchmarkResult> results;
};

// Renders every scene headless, advancing it by a fixed time step per frame so each run sees the
// same animation, and measures the raytracing of every frame
[[nodiscard]] BenchmarkReport RunBenchmark(const BenchmarkSettings& settings, const std::vector<BenchmarkScene>& scenes);

void PrintBenchmark(std::ostream& out, const BenchmarkReport& report);
void WriteBenchmarkCsv(std::ostream& out, const BenchmarkReport& report);
