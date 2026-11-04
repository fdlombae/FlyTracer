#include <gtest/gtest.h>
#include "test_meshes.h"
#include "test_scene.h"
#include <cmath>
#include <memory>
#include <stdexcept>
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

TEST_F(GameSceneTest, FindInstanceGivesTheLastWithTheName) {
    scene.AddMeshInstance(meshId, TriVector(1.0f, 0.0f, 0.0f), "bird");
    const uint32_t second = scene.AddMeshInstance(meshId, TriVector(2.0f, 0.0f, 0.0f), "bird");
    EXPECT_EQ(scene.FindInstance("bird"), scene.GetInstance(second));
    EXPECT_EQ(scene.FindInstance(""), nullptr);

    // Renaming an instance is seen by FindInstance
    scene.GetInstance(second)->name = "eagle";
    EXPECT_EQ(scene.FindInstance("eagle"), scene.GetInstance(second));
    EXPECT_NE(scene.FindInstance("bird"), scene.GetInstance(second));
}

TEST_F(GameSceneTest, MeshWithoutMaterialsGetsTheDefault) {
    ASSERT_EQ(scene.GetMesh(meshId)->MaterialCount(), 1u);
    EXPECT_EQ(scene.GetMeshMaterial(meshId, 0).shadingMode, Scene::Material::Default().shadingMode);
    EXPECT_THROW((void)scene.GetMeshMaterial(meshId, 1), std::out_of_range);
    EXPECT_THROW((void)scene.GetMeshMaterial(meshId + 1, 0), std::out_of_range);
}

TEST_F(GameSceneTest, InvalidMeshesAreRejected) {
    EXPECT_THROW(scene.AddMesh(nullptr), std::invalid_argument);

    auto missingVertex = makeTriangle();
    missingVertex->AddTriangle({{0, 1, 3}});
    EXPECT_THROW(scene.AddMesh(std::move(missingVertex)), std::invalid_argument);

    auto missingMaterial = makeTriangle();
    missingMaterial->AddMaterial(Scene::Material::Default());
    missingMaterial->AddTriangle({{0, 1, 2}, 1});
    EXPECT_THROW(scene.AddMesh(std::move(missingMaterial)), std::invalid_argument);

    auto idealVertex = makeTriangle();
    idealVertex->AddVertex({.position = TriVector(0.0f, 0.0f, 1.0f, 0.0f)});
    EXPECT_THROW(scene.AddMesh(std::move(idealVertex)), std::invalid_argument);

    auto nanVertex = makeTriangle();
    nanVertex->AddVertex({.position = TriVector(std::nanf(""), 0.0f, 0.0f)});
    EXPECT_THROW(scene.AddMesh(std::move(nanVertex)), std::invalid_argument);

    EXPECT_EQ(scene.GetMeshes().size(), 1u);
}

namespace {

class CountingScene final : public GameScene {
public:
    CountingScene(bool failInit = false) : GameScene(""), m_failInit(failInit) {}
    int shutdowns{0};

protected:
    void OnInit() override {
        if (m_failInit) throw std::runtime_error("Init failed");
    }
    void OnShutdown() override { ++shutdowns; }

private:
    bool m_failInit;
};

}  // namespace

TEST(GameSceneLifetimeTest, ShutdownOnceAfterInit) {
    CountingScene scene;
    scene.Shutdown();
    EXPECT_EQ(scene.shutdowns, 0);

    scene.Init();
    scene.Shutdown();
    scene.Shutdown();
    EXPECT_EQ(scene.shutdowns, 1);
}

TEST(GameSceneLifetimeTest, NoShutdownAfterFailedInit) {
    CountingScene scene(true);
    EXPECT_THROW(scene.Init(), std::runtime_error);
    scene.Shutdown();
    EXPECT_EQ(scene.shutdowns, 0);
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
