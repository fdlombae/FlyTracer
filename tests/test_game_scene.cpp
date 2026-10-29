#include <gtest/gtest.h>
#include "test_meshes.h"
#include "test_scene.h"
#include <memory>
#include <vector>

namespace {

std::unique_ptr<Mesh> makeTriangle() {
    return std::make_unique<Mesh>(
        MakeMesh({TriVector(0.0f, 0.0f, 0.0f), TriVector(1.0f, 0.0f, 0.0f), TriVector(0.0f, 1.0f, 0.0f)}, {{0, 1, 2}}));
}

class GameSceneTest : public ::testing::Test {
protected:
    OpenScene scene;
    uint32_t meshId{scene.AddMesh(makeTriangle())};
};

}  // namespace

TEST_F(GameSceneTest, RemovedInstanceKeepsOtherIdsValid) {
    const uint32_t a = scene.AddMeshInstance(meshId, TriVector(1.0f, 0.0f, 0.0f));
    const uint32_t b = scene.AddMeshInstance(meshId, TriVector(2.0f, 0.0f, 0.0f));
    const uint32_t c = scene.AddMeshInstance(meshId, TriVector(3.0f, 0.0f, 0.0f));

    EXPECT_TRUE(scene.RemoveMeshInstance(b));

    EXPECT_EQ(scene.GetInstance(b), nullptr);
    ASSERT_NE(scene.GetInstance(a), nullptr);
    ASSERT_NE(scene.GetInstance(c), nullptr);
    EXPECT_FLOAT_EQ(scene.GetInstance(a)->ToWorld(TriVector(0.0f, 0.0f, 0.0f)).e032(), 1.0f);
    EXPECT_FLOAT_EQ(scene.GetInstance(c)->ToWorld(TriVector(0.0f, 0.0f, 0.0f)).e032(), 3.0f);
    EXPECT_EQ(scene.GetMeshInstances().size(), 2u);
    EXPECT_EQ(scene.GetInstanceIds(), (std::vector<uint32_t>{a, c}));
}

TEST_F(GameSceneTest, RemoveUnknownInstanceFails) {
    const uint32_t id = scene.AddMeshInstance(meshId, TriVector(0.0f, 0.0f, 0.0f));
    EXPECT_FALSE(scene.RemoveMeshInstance(id + 1));
    EXPECT_TRUE(scene.RemoveMeshInstance(id));
    EXPECT_FALSE(scene.RemoveMeshInstance(id));
    EXPECT_TRUE(scene.GetMeshInstances().empty());
}

TEST_F(GameSceneTest, RemovedInstanceIdIsNotReused) {
    const uint32_t first = scene.AddMeshInstance(meshId, TriVector(0.0f, 0.0f, 0.0f));
    scene.RemoveMeshInstance(first);
    const uint32_t second = scene.AddMeshInstance(meshId, TriVector(0.0f, 0.0f, 0.0f));
    EXPECT_NE(first, second);
    EXPECT_EQ(scene.GetInstance(first), nullptr);
}

TEST_F(GameSceneTest, RemovedInstanceNameIsForgotten) {
    const uint32_t bird = scene.AddMeshInstance(meshId, TriVector(0.0f, 0.0f, 0.0f), "bird");
    scene.AddMeshInstance(meshId, TriVector(5.0f, 0.0f, 0.0f), "tree");

    scene.RemoveMeshInstance(bird);

    EXPECT_EQ(scene.FindInstance("bird"), nullptr);
    ASSERT_NE(scene.FindInstance("tree"), nullptr);
    EXPECT_EQ(scene.FindInstance("tree")->name, "tree");
}

TEST_F(GameSceneTest, SpawnedLightGetsNewId) {
    const uint32_t sun = scene.AddDirectionalLight(TriVector(0.0f, -1.0f, 0.0f, 0.0f), Scene::Color::White(), 1.0f);
    const uint32_t lamp = scene.AddPointLight(TriVector(0.0f, 5.0f, 0.0f), Scene::Color::White(), 1.0f, 10.0f);
    EXPECT_NE(sun, lamp);
    EXPECT_EQ(scene.GetSceneData().lights.size(), 2u);
    ASSERT_NE(scene.GetLight(lamp), nullptr);
    EXPECT_EQ(scene.GetLight(lamp)->type, Scene::LightType::Point);
}

TEST_F(GameSceneTest, RemovedLightKeepsOtherIdsValid) {
    const uint32_t a = scene.AddPointLight(TriVector(1.0f, 0.0f, 0.0f), Scene::Color::White(), 1.0f, 10.0f);
    const uint32_t b = scene.AddPointLight(TriVector(2.0f, 0.0f, 0.0f), Scene::Color::White(), 1.0f, 10.0f);
    const uint32_t c = scene.AddPointLight(TriVector(3.0f, 0.0f, 0.0f), Scene::Color::White(), 1.0f, 10.0f);

    EXPECT_TRUE(scene.RemoveLight(b));
    EXPECT_FALSE(scene.RemoveLight(b));

    EXPECT_EQ(scene.GetLight(b), nullptr);
    ASSERT_NE(scene.GetLight(c), nullptr);
    EXPECT_FLOAT_EQ(scene.GetLight(a)->position.e032(), 1.0f);
    EXPECT_FLOAT_EQ(scene.GetLight(c)->position.e032(), 3.0f);
    EXPECT_EQ(scene.GetSceneData().lights.size(), 2u);
    EXPECT_EQ(scene.GetLightIds(), (std::vector<uint32_t>{a, c}));

    const uint32_t d = scene.AddPointLight(TriVector(4.0f, 0.0f, 0.0f), Scene::Color::White(), 1.0f, 10.0f);
    EXPECT_NE(d, b);
    EXPECT_EQ(scene.GetLightIds(), (std::vector<uint32_t>{a, c, d}));
}
