#include "Benchmark.h"
#include "Input.h"
#include "RaytracingPass.h"
#include "VulkanContext.h"
#include <format>

namespace {

constexpr float kTimeStep = 1.0f / 60.0f;

} // anonymous namespace

BenchmarkReport RunBenchmark(const BenchmarkSettings& settings, const std::vector<BenchmarkScene>& scenes) {
    const VulkanContext context;
    RaytracingPass raytracing(context, settings.extent, settings.shaderDir, settings.maxBounces);
    VulkanHelpers::CommandSubmitter submitter(context.Device(), context.CommandPool(), context.Queue());
    Input input;
    input.SetWindowSize(static_cast<float>(settings.extent.width), static_cast<float>(settings.extent.height));
    input.SetDeltaTime(kTimeStep);

    BenchmarkReport report{settings.extent, settings.maxBounces, 0.0, {}};
    std::vector<double> emptySubmits;
    for (uint32_t i = 0; i < settings.warmupFrames + settings.frames; ++i) {
        emptySubmits.push_back(submitter.Submit([](VkCommandBuffer) {}));
    }
    report.emptySubmitMilliseconds = Summarize(std::move(emptySubmits)).median;

    // A dispatch's timestamps are read kFramesInFlight frames later, so a few more frames are rendered
    const uint32_t firstMeasured = settings.warmupFrames + RaytracingPass::kFramesInFlight;
    for (const BenchmarkScene& benchmarkScene : scenes) {
        const std::unique_ptr<GameScene> scene = benchmarkScene.create();
        scene->Init();
        raytracing.UploadMeshes(scene->GetMeshes());

        std::vector<double> gpuMilliseconds;
        std::vector<double> submitMilliseconds;
        for (uint32_t frame = 0; frame < firstMeasured + settings.frames; ++frame) {
            scene->HandleInput(input);
            input.EndFrame();
            scene->Update(kTimeStep);
            const double submit = submitter.Submit([&](VkCommandBuffer cmdBuffer) {
                raytracing.Record(cmdBuffer, frame % RaytracingPass::kFramesInFlight, *scene);
            });
            if (frame >= settings.warmupFrames && submitMilliseconds.size() < settings.frames) {
                submitMilliseconds.push_back(submit);
            }
            if (frame >= firstMeasured) {
                if (const auto dispatch = raytracing.DispatchMilliseconds()) {
                    gpuMilliseconds.push_back(*dispatch);
                }
            }
        }
        scene->Shutdown();

        BenchmarkResult result{benchmarkScene.name, settings.frames, std::nullopt, Summarize(std::move(submitMilliseconds))};
        if (gpuMilliseconds.size() == settings.frames) {
            result.gpuMilliseconds = Summarize(std::move(gpuMilliseconds));
        }
        report.results.push_back(std::move(result));
    }
    return report;
}

namespace {

std::string formatMilliseconds(const std::optional<TimingSummary>& summary, double TimingSummary::* value) {
    return summary ? std::format("{:.3f}", (*summary).*value) : "n/a";
}

} // anonymous namespace

void PrintBenchmark(std::ostream& out, const BenchmarkReport& report) {
    out << std::format("Raytracing at {} x {} pixels, {} bounces. Times in ms; submit times include {:.3f} ms "
                       "for submitting and waiting on an empty command buffer.\n",
                       report.extent.width, report.extent.height, report.maxBounces, report.emptySubmitMilliseconds);
    out << std::format("{:<14} {:>7} {:>11} {:>11} {:>11} {:>11}\n",
                       "Scene", "Frames", "GPU median", "GPU 95%", "Sub. median", "Sub. 95%");
    for (const BenchmarkResult& result : report.results) {
        out << std::format("{:<14} {:>7} {:>11} {:>11} {:>11.3f} {:>11.3f}\n",
                           result.scene, result.frames,
                           formatMilliseconds(result.gpuMilliseconds, &TimingSummary::median),
                           formatMilliseconds(result.gpuMilliseconds, &TimingSummary::percentile95),
                           result.submitMilliseconds.median, result.submitMilliseconds.percentile95);
    }
}

void WriteBenchmarkCsv(std::ostream& out, const BenchmarkReport& report) {
    out << "scene,width,height,bounces,frames,gpu_median_ms,gpu_p95_ms,submit_median_ms,submit_p95_ms,"
           "empty_submit_ms\n";
    for (const BenchmarkResult& result : report.results) {
        out << std::format("{},{},{},{},{},{},{},{:.4f},{:.4f},{:.4f}\n",
                           result.scene, report.extent.width, report.extent.height,
                           report.maxBounces, result.frames,
                           formatMilliseconds(result.gpuMilliseconds, &TimingSummary::median),
                           formatMilliseconds(result.gpuMilliseconds, &TimingSummary::percentile95),
                           result.submitMilliseconds.median, result.submitMilliseconds.percentile95,
                           report.emptySubmitMilliseconds);
    }
}
