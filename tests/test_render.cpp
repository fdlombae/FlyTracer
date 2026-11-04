#include <gtest/gtest.h>
#include "Benchmark.h"
#include "PGA.h"
#include "render_helpers.h"
#include "test_meshes.h"
#include "test_scene.h"
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Golden image test: renders a small scene headless and compares it with a reference image.
// Delete tests/reference/render_scene.ppm to record a new reference after an intended change.

namespace {

constexpr VkExtent2D kImageSize{192, 128};
constexpr uint32_t kSamples = 16;

// A sphere, a plane, a mirrored and rotated tetrahedron mesh and a debug line: exercises the
// analytic primitives, both BVH levels, the PGA instance transform and the overlay
class TestScene final : public GameScene {
public:
    TestScene() : GameScene("") {}

protected:
    void OnInit() override {
        AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color(0.6f, 0.6f, 0.6f)));
        AddSphere(TriVector(-1.5f, 1.0f, 0.0f), 1.0f, Scene::Material::Metal(Scene::Color(0.9f, 0.4f, 0.3f), 0.1f));
        AddPointLight(TriVector(2.0f, 6.0f, 4.0f), Scene::Color::White(), 3.0f, 20.0f);

        auto mesh = std::make_unique<Mesh>(MakeMesh({TriVector(0, 0, 0), TriVector(1, 0, 0), TriVector(0, 1, 0), TriVector(0, 0, 1)},
                                                    {{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}}));
        mesh->ComputeNormals();
        mesh->AddMaterial(Scene::Material::PBR(Scene::Color(0.2f, 0.5f, 0.9f), 0.0f, 0.4f));
        const uint32_t meshId = AddMesh(std::move(mesh));

        const Motor transform = TranslationMotor(1.5f, 0.0f, 0.0f) *
                                Motor::Rotation(30.0f, BiVector(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
        const uint32_t instanceId = AddMeshInstance(meshId, transform);
        GetInstance(instanceId)->scale = 1.5f;
        *GetInstance(instanceId) = GetInstance(instanceId)->Reflected(Vector(0.0f, 0.0f, 0.0f, 1.0f));

        SetCamera(TriVector(0.0f, 2.5f, 6.0f), TriVector(0.0f, 0.8f, 0.0f));
        DrawDebugLine(TriVector(-3.0f, 0.01f, 1.5f), TriVector(3.0f, 0.01f, 1.5f), Scene::Color::Yellow());
    }
};

RgbImage Render(const VulkanContext& context) {
    TestScene scene;
    scene.Init();
    return RenderImage(context, scene, kImageSize, kSamples);
}

} // anonymous namespace

TEST(RenderTest, MatchesReferenceImage) {
    const auto context = TryCreateContext();
    if (!context) GTEST_SKIP() << "No usable Vulkan device";
    ExpectMatchesReference(Render(*context), std::string(FLYTRACER_TEST_REFERENCE_DIR) + "/render_scene");
}

TEST(BenchmarkTest, TimesEveryMeasuredFrame) {
    if (!TryCreateContext()) GTEST_SKIP() << "No usable Vulkan device";
    const BenchmarkSettings settings{.extent = kImageSize, .warmupFrames = 2, .frames = 5,
                                     .shaderDir = ExecutableRelativePath("shaders").string()};
    const BenchmarkReport report = RunBenchmark(settings, {{"test", [] { return std::make_unique<TestScene>(); }}});

    ASSERT_EQ(report.results.size(), 1u);
    const BenchmarkResult& result = report.results[0];
    EXPECT_EQ(result.scene, "test");
    EXPECT_EQ(result.frames, 5u);
    EXPECT_GT(result.submitMilliseconds.median, 0.0);
    EXPECT_GE(result.submitMilliseconds.percentile95, result.submitMilliseconds.median);
    if (result.gpuMilliseconds) {
        EXPECT_GT(result.gpuMilliseconds->median, 0.0);
    }
}

// The renderer uploads meshes added after Init, reads material changes every frame, and leaves out
// instances with scale 0
TEST(RenderTest, SceneChangesAfterInitAreDrawn) {
    const auto context = TryCreateContext();
    if (!context) GTEST_SKIP() << "No usable Vulkan device";
    OpenScene scene;
    scene.SetCamera(TriVector(0.0f, 0.0f, 5.0f), TriVector(0.0f, 0.0f, 0.0f));
    RaytracingPass raytracing(*context, VkExtent2D{16, 16}, ExecutableRelativePath("shaders").string(), 1);
    raytracing.UploadMeshes(scene.GetMeshes());
    VulkanHelpers::CommandSubmitter submitter = MakeSubmitter(*context);
    uint32_t frame = 0;
    const auto centerPixel = [&] {
        submitter.Submit([&](VkCommandBuffer cmd) {
            raytracing.Record(cmd, frame++ % RaytracingPass::kFramesInFlight, scene);
        });
        const RgbImage image = ReadOutput(*context, raytracing, submitter);
        const size_t center = (size_t{image.height / 2} * image.width + image.width / 2) * 3;
        return std::array{image.pixels[center], image.pixels[center + 1], image.pixels[center + 2]};
    };
    const auto sky = centerPixel();

    auto mesh = std::make_unique<Mesh>(MakeBox(2.0f, 2.0f, 2.0f));
    mesh->AddMaterial(Scene::Material::Flat(Scene::Color(1.0f, 0.0f, 0.0f)));
    const uint32_t meshId = scene.AddMesh(std::move(mesh));
    const uint32_t instanceId = scene.AddMeshInstance(meshId, TriVector(0.0f, 0.0f, 0.0f));
    const auto red = centerPixel();
    EXPECT_GT(red[0], 150);
    EXPECT_LT(red[1], 50);

    scene.GetMesh(meshId)->GetMaterial(0) = Scene::Material::Flat(Scene::Color(0.0f, 1.0f, 0.0f));
    const auto green = centerPixel();
    EXPECT_LT(green[0], 50);
    EXPECT_GT(green[1], 150);

    scene.GetInstance(instanceId)->scale = 0.0f;
    EXPECT_EQ(centerPixel(), sky);
}
