#include <gtest/gtest.h>
#include "SceneRegistry.h"
#include "render_helpers.h"
#include <optional>
#include <string>

// Golden images of every game scene. The references in tests/reference/scenes were rendered by the
// classic (vector math) raytracer this one replaced, so they also check that the PGA formulation
// renders the same images; small differences are sampling noise in glossy reflections. Delete a
// reference to record a new one after an intended change.

namespace {

constexpr VkExtent2D kImageSize{192, 108};
constexpr uint32_t kSamples = 64;

class SceneImageTest : public ::testing::TestWithParam<std::string_view> {};

} // anonymous namespace

TEST_P(SceneImageTest, MatchesReferenceImage) {
    const auto context = TryCreateContext();
    if (!context) GTEST_SKIP() << "No usable Vulkan device";

    const auto scene = CreateScene(GetParam(), ExecutableRelativePath("resources").string());
    scene->Init();
    scene->Update(0.0f);
    ExpectMatchesReference(RenderImage(*context, *scene, kImageSize, kSamples),
                           std::string(FLYTRACER_TEST_REFERENCE_DIR) + "/scenes/" + std::string(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(AllScenes, SceneImageTest, ::testing::ValuesIn(kSceneNames),
                         [](const auto& info) { return std::string(info.param); });
