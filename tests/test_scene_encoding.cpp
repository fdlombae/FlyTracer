#include <gtest/gtest.h>
#include "SceneEncoding.h"
#include "test_meshes.h"
#include "test_scene.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

namespace {

std::unique_ptr<Mesh> makeBox(const Scene::Color& color) {
    auto mesh = std::make_unique<Mesh>(MakeBox(1.0f, 2.0f, 3.0f));
    mesh->AddMaterial(Scene::Material::Lambert(color));
    return mesh;
}

// The instances below a pair, in the order the leaves reference them
void collectLeaves(const std::vector<Gpu::NodePair>& pairs, int32_t pair, std::vector<int32_t>& instances) {
    for (int child = 0; child < 2; ++child) {
        if (pairs[pair].count[child] > 0) {
            for (int32_t i = 0; i < pairs[pair].count[child]; ++i) instances.push_back(pairs[pair].index[child] + i);
        } else if (pairs[pair].count[child] == 0) {
            collectLeaves(pairs, pairs[pair].index[child], instances);
        }
    }
}

} // anonymous namespace

TEST(SceneEncodingTest, MeshesShareBuffersWithOffsets) {
    std::vector<std::unique_ptr<Mesh>> meshes;
    meshes.push_back(makeBox(Scene::Color::White()));
    meshes.push_back(makeBox(Scene::Color::White()));
    for (auto& mesh : meshes) mesh->BuildBVH();
    const MeshData data = EncodeMeshes(meshes, 64);

    ASSERT_EQ(data.rootPairs.size(), 2u);
    EXPECT_EQ(data.rootPairs[0], 0u);
    EXPECT_GT(data.rootPairs[1], 0u);
    ASSERT_EQ(data.triangles.size(), 24u);
    ASSERT_EQ(data.edgeTriangles.size(), 24u);
    EXPECT_EQ(data.triangles[12].indices[0], meshes[0]->Triangles()[0].indices[0] + 8);  // Offset by mesh 0's vertices
    EXPECT_EQ(data.triangles[12].materialIndex, 1u);                                      // and by its material
    // Every record of the second mesh refers to one of its own triangles and edges
    for (size_t record = 12; record < 24; ++record) {
        EXPECT_GE(data.edgeTriangles[record].triangle, 12u);
        EXPECT_GE(data.edgeTriangles[record].edges[0] & ~Gpu::kReversedEdge, 18u);  // A box has 18 edges
    }
    // The second mesh's leaves refer to its own records
    std::vector<int32_t> records;
    collectLeaves(data.pairs, static_cast<int32_t>(data.rootPairs[1]), records);
    EXPECT_EQ(records.size(), 12u);
    EXPECT_TRUE(std::ranges::all_of(records, [](int32_t record) { return record >= 12 && record < 24; }));
}

TEST(SceneEncodingTest, ChangedMeshesAreNoticed) {
    OpenScene scene;
    const uint32_t meshId = scene.AddMesh(makeBox(Scene::Color::White()));
    const MeshTable table = EncodeMeshes(scene.GetMeshes(), 64).table;
    EXPECT_FALSE(MeshesChanged(table, scene.GetMeshes()));

    scene.GetMesh(meshId)->GetMaterial(0).color = Scene::Color(1.0f, 0.0f, 0.0f);
    EXPECT_FALSE(MeshesChanged(table, scene.GetMeshes()));  // Colors are read every frame

    scene.GetMesh(meshId)->GetMaterial(0).texturePath = "wood.png";
    EXPECT_TRUE(MeshesChanged(table, scene.GetMeshes()));
    scene.GetMesh(meshId)->GetMaterial(0).texturePath.clear();

    scene.AddMesh(makeBox(Scene::Color::White()));
    EXPECT_TRUE(MeshesChanged(table, scene.GetMeshes()));
}

TEST(SceneEncodingTest, MaterialsAreReadEveryFrame) {
    OpenScene scene;
    const uint32_t meshId = scene.AddMesh(makeBox(Scene::Color::White()));
    scene.AddSphere(TriVector(0.0f, 0.0f, 0.0f), 1.0f, Scene::Material::Lambert(Scene::Color::White()));
    scene.AddPlane(Vector(0.0f, 0.0f, 1.0f, 0.0f), Scene::Material::Lambert(Scene::Color::White()));
    const MeshTable table = EncodeMeshes(scene.GetMeshes(), 64).table;

    scene.GetMesh(meshId)->GetMaterial(0).color = Scene::Color(0.0f, 0.0f, 1.0f);
    const FrameData frame = EncodeFrame(scene, table);
    ASSERT_EQ(frame.materials.size(), 3u);
    EXPECT_FLOAT_EQ(frame.materials[0].color[0], 0.0f);
    EXPECT_FLOAT_EQ(frame.materials[0].color[2], 1.0f);
    EXPECT_EQ(frame.spheres[0].materialIndex, 1u);
    EXPECT_EQ(frame.planes[0].materialIndex, 2u);
}

TEST(SceneEncodingTest, OnlyDrawableInstancesAreEncoded) {
    OpenScene scene;
    const uint32_t meshId = scene.AddMesh(makeBox(Scene::Color::White()));
    scene.AddMeshInstance(meshId, TriVector(0.0f, 0.0f, 0.0f));
    scene.GetInstance(scene.AddMeshInstance(meshId, TriVector(5.0f, 0.0f, 0.0f)))->scale = 0.0f;
    scene.GetInstance(scene.AddMeshInstance(meshId, TriVector(9.0f, 0.0f, 0.0f)))->scale =
        std::numeric_limits<float>::quiet_NaN();
    scene.GetInstance(scene.AddMeshInstance(meshId, TriVector(7.0f, 0.0f, 0.0f)))->visible = false;
    const MeshTable table = EncodeMeshes(scene.GetMeshes(), 64).table;
    // A mesh added after the meshes were encoded has no buffers yet
    scene.AddMeshInstance(scene.AddMesh(makeBox(Scene::Color::White())), TriVector(3.0f, 0.0f, 0.0f));

    EXPECT_EQ(EncodeFrame(scene, table).instances.size(), 1u);
}

// Instances with the same bounds cannot be split, so a top-level leaf may hold several
TEST(SceneEncodingTest, TopLevelLeavesCoverEveryInstanceOnce) {
    OpenScene scene;
    const uint32_t meshId = scene.AddMesh(makeBox(Scene::Color::White()));
    for (const float x : {0.0f, 0.0f, 0.0f, 4.0f, 8.0f}) {
        scene.AddMeshInstance(meshId, TriVector(x, 0.0f, 0.0f));
    }
    const FrameData frame = EncodeFrame(scene, EncodeMeshes(scene.GetMeshes(), 64).table);

    ASSERT_EQ(frame.instances.size(), 5u);
    std::vector<int32_t> instances;
    collectLeaves(frame.scenePairs, 0, instances);
    std::ranges::sort(instances);
    EXPECT_EQ(instances, (std::vector<int32_t>{0, 1, 2, 3, 4}));
}
