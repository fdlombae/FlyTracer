#include <gtest/gtest.h>
#include "GpuScene.h"
#include "PGA.h"
#include <stdexcept>

TEST(GpuSceneTest, SrgbToLinear) {
    EXPECT_FLOAT_EQ(Gpu::SrgbToLinear(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Gpu::SrgbToLinear(1.0f), 1.0f);
    EXPECT_NEAR(Gpu::SrgbToLinear(0.5f), 0.2140f, 1e-4f);
}

TEST(GpuSceneTest, MaterialIsLinear) {
    const Gpu::Material material = Gpu::Encode(Scene::Material::Metal(Scene::Color(0.5f, 1.0f, 0.0f), 0.2f), 3);
    EXPECT_NEAR(material.color[0], 0.2140f, 1e-4f);
    EXPECT_FLOAT_EQ(material.color[1], 1.0f);
    EXPECT_EQ(material.shadingMode, static_cast<int32_t>(Scene::ShadingMode::PBR));
    EXPECT_FLOAT_EQ(material.metallic, 1.0f);
    EXPECT_EQ(material.textureIndex, 3);
}

TEST(GpuSceneTest, PlaneIsNormalized) {
    const Gpu::Plane plane = Gpu::Encode(Scene::Plane{Vector(-4.0f, 0.0f, 2.0f, 0.0f), {}}, 7);
    EXPECT_FLOAT_EQ(plane.normal[1], 1.0f);
    EXPECT_FLOAT_EQ(plane.e0, -2.0f);
    EXPECT_EQ(plane.materialIndex, 7u);
}

TEST(GpuSceneTest, LightIsLinearTimesIntensity) {
    const Scene::Light light = Scene::Light::Directional(TriVector(0.0f, -2.0f, 0.0f, 0.0f), Scene::Color::White(), 3.0f);
    const Gpu::Light gpu = Gpu::Encode(light);
    EXPECT_FLOAT_EQ(gpu.direction[1], -1.0f);
    EXPECT_FLOAT_EQ(gpu.color[0], 3.0f);
}

TEST(GpuSceneTest, PhongSpecular) {
    const Gpu::Material material = Gpu::Encode(Scene::Material::Phong(Scene::Color::White(), 32.0f, 0.1f, 0.3f), -1);
    EXPECT_FLOAT_EQ(material.reflectivity, 0.1f);
    EXPECT_FLOAT_EQ(material.specular, 0.3f);
}

// The plane at infinity and lights without a direction cannot be drawn
TEST(GpuSceneTest, RejectsDegenerateInput) {
    EXPECT_THROW((void)Gpu::Encode(Scene::Plane{Vector(1.0f, 0.0f, 0.0f, 0.0f), {}}, 0), std::invalid_argument);
    const TriVector none(0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_THROW((void)Gpu::Encode(Scene::Light::Directional(none, Scene::Color::White(), 1.0f)), std::invalid_argument);
    EXPECT_NO_THROW((void)Gpu::Encode(Scene::Light::Point(TriVector(0.0f, 1.0f, 0.0f), Scene::Color::White(), 1.0f, 5.0f)));
}

// Without softness the spot cone has a hard edge: both cosines are the cone angle's
TEST(GpuSceneTest, SpotSoftnessIsClamped) {
    const TriVector down(0.0f, -1.0f, 0.0f, 0.0f);
    const Scene::Light hard = Scene::Light::Spot(TriVector(0.0f, 5.0f, 0.0f), down, Scene::Color::White(), 1.0f, 10.0f, 30.0f, 0.0f);
    const Scene::Light tooSoft = Scene::Light::Spot(TriVector(0.0f, 5.0f, 0.0f), down, Scene::Color::White(), 1.0f, 10.0f, 30.0f, 2.0f);
    EXPECT_FLOAT_EQ(Gpu::Encode(hard).spotCosInner, Gpu::Encode(hard).spotCosOuter);
    EXPECT_FLOAT_EQ(Gpu::Encode(tooSoft).spotCosInner, 1.0f);
}

TEST(GpuSceneTest, VertexInterleavesUVs) {
    Vertex vertex{};
    vertex.position = TriVector(1.0f, 2.0f, 3.0f);
    vertex.normal = TriVector(0.0f, 1.0f, 0.0f, 0.0f);
    vertex.texCoord[0] = 0.25f;
    vertex.texCoord[1] = 0.75f;

    const Gpu::Vertex gpu = Gpu::Encode(vertex);
    EXPECT_FLOAT_EQ(gpu.position[2], 3.0f);
    EXPECT_FLOAT_EQ(gpu.u, 0.25f);
    EXPECT_FLOAT_EQ(gpu.normal[1], 1.0f);
    EXPECT_FLOAT_EQ(gpu.v, 0.75f);
}

TEST(GpuSceneTest, EdgeTriangleOffsetsAndMarksReversedEdges) {
    const TriangleEdges edges{{EdgeUse{0, false}, EdgeUse{1, true}, EdgeUse{2, false}}};
    const Gpu::EdgeTriangle gpu = Gpu::Encode(edges, 10, 7);
    EXPECT_EQ(gpu.edges[0], 10u);
    EXPECT_EQ(gpu.edges[1], 11u | Gpu::kReversedEdge);
    EXPECT_EQ(gpu.edges[2], 12u);
    EXPECT_EQ(gpu.triangle, 7u);
}

TEST(GpuSceneTest, NodePairLinksAreOffset) {
    NodePair pair;
    pair[0] = PairChild{3, 2, AABB{}};
    pair[1] = PairChild{4, 0, AABB{}};
    pair[1].box.Grow(-1.0f, -2.0f, -3.0f);
    pair[1].box.Grow(1.0f, 2.0f, 3.0f);
    const Gpu::NodePair gpu = Gpu::Encode(pair, 100, 1000);
    EXPECT_EQ(gpu.index[0], 1003);  // A leaf: records
    EXPECT_EQ(gpu.index[1], 104);   // An internal node: pairs
    EXPECT_FLOAT_EQ(gpu.boxes[1].minimum[0], -1.0f);
    EXPECT_FLOAT_EQ(gpu.boxes[1].maximum[2], 3.0f);
    EXPECT_EQ(gpu.count[0], 2);
    EXPECT_EQ(gpu.count[1], 0);
}

TEST(GpuSceneTest, MissingChildStaysMissing) {
    const NodePair pair{PairChild{0, 1, {}}, PairChild{}};
    EXPECT_EQ(Gpu::Encode(pair, 100, 1000).index[1], -1);
    EXPECT_EQ(Gpu::Encode(pair, 100, 1000).count[1], -1);
}
