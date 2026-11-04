#include <gtest/gtest.h>
#include "Mesh.h"
#include "temp_file.h"
#include "test_meshes.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

class MeshTest : public ::testing::Test {
protected:
    Mesh mesh;

    // Loads OBJ text from a temporary file
    bool LoadObj(const std::string& obj) {
        const TempFile file(".obj");
        std::ofstream(file.Path()) << obj;
        return mesh.LoadFromFile(file.String());
    }
};

TEST_F(MeshTest, DefaultConstruction) {
    EXPECT_TRUE(mesh.Vertices().empty());
    EXPECT_TRUE(mesh.Triangles().empty());
    EXPECT_EQ(mesh.VertexCount(), 0);
    EXPECT_EQ(mesh.TriangleCount(), 0);
}

TEST_F(MeshTest, AddVertex) {
    Vertex v;
    // Position: TriVector uses e032=x, e013=y, e021=z, e123=w
    v.position = TriVector(1.0f, 2.0f, 3.0f, 1.0f);
    v.normal = TriVector(0.0f, 0.0f, 1.0f, 0.0f);
    v.texCoord[0] = 0.5f;
    v.texCoord[1] = 0.5f;

    mesh.AddVertex(v);

    EXPECT_EQ(mesh.VertexCount(), 1);
    EXPECT_FLOAT_EQ(mesh.Vertices()[0].position.e032(), 1.0f);
    EXPECT_FLOAT_EQ(mesh.Vertices()[0].position.e013(), 2.0f);
    EXPECT_FLOAT_EQ(mesh.Vertices()[0].position.e021(), 3.0f);
}

TEST_F(MeshTest, AddTriangle) {
    for (int i = 0; i < 3; ++i) {
        mesh.AddVertex({.position = TriVector(static_cast<float>(i), 0.0f, 0.0f, 1.0f)});
    }

    mesh.AddTriangle({{0, 1, 2}, 0});

    EXPECT_EQ(mesh.TriangleCount(), 1);
    EXPECT_EQ(mesh.Triangles()[0].indices[0], 0);
    EXPECT_EQ(mesh.Triangles()[0].indices[1], 1);
    EXPECT_EQ(mesh.Triangles()[0].indices[2], 2);
}

TEST_F(MeshTest, Clear) {
    mesh.AddVertex({});
    mesh.AddTriangle({{0, 0, 0}});

    mesh.BuildBVH();

    mesh.Clear();

    EXPECT_TRUE(mesh.Vertices().empty());
    EXPECT_TRUE(mesh.Triangles().empty());
    EXPECT_TRUE(mesh.GetBVH().nodes.empty());
    EXPECT_TRUE(mesh.GetBVH().primitiveOrder.empty());
}

TEST_F(MeshTest, AddMaterial) {
    Scene::Material mat = Scene::Material::Phong(Scene::Color::Red(), 64.0f);
    mat.texturePath = "texture.png";

    mesh.AddMaterial(mat);

    EXPECT_EQ(mesh.MaterialCount(), 1);
    EXPECT_EQ(mesh.GetMaterial(0).texturePath, "texture.png");
    EXPECT_FLOAT_EQ(mesh.GetMaterial(0).shininess, 64.0f);
}

TEST_F(MeshTest, GetMaterialOutOfBoundsThrows) {
    EXPECT_THROW((void)mesh.GetMaterial(999), std::out_of_range);
}

TEST_F(MeshTest, HasMaterial) {
    EXPECT_FALSE(mesh.HasMaterial(0));

    mesh.AddMaterial(Scene::Material{});

    EXPECT_TRUE(mesh.HasMaterial(0));
    EXPECT_FALSE(mesh.HasMaterial(1));
}

// Triangles wound counter-clockwise seen from outside, as in OBJ files, get outward normals
TEST_F(MeshTest, ComputeNormals) {
    for (const TriVector& p : {TriVector(0.0f, 0.0f, 0.0f), TriVector(1.0f, 0.0f, 0.0f), TriVector(0.0f, 1.0f, 0.0f)}) {
        mesh.AddVertex({.position = p});
    }
    mesh.AddTriangle(Triangle{{0, 1, 2}, 0});

    mesh.ComputeNormals();

    for (const auto& v : mesh.Vertices()) {
        EXPECT_NEAR(v.normal.e032(), 0.0f, 1e-6f);
        EXPECT_NEAR(v.normal.e013(), 0.0f, 1e-6f);
        EXPECT_NEAR(v.normal.e021(), 1.0f, 1e-6f);
        EXPECT_FLOAT_EQ(v.normal.e123(), 0.0f);
    }
}

// A vertex normal weighs its faces by their angle at the vertex: the corner of a right-angled
// and a 45 degree triangle meeting at a fold
TEST_F(MeshTest, ComputeNormalsWeighsByAngle) {
    for (const TriVector& p : {TriVector(0.0f, 0.0f, 0.0f), TriVector(1.0f, 0.0f, 0.0f), TriVector(0.0f, 1.0f, 0.0f),
                               TriVector(1.0f, 0.0f, -1.0f)}) {
        mesh.AddVertex({.position = p});
    }
    mesh.AddTriangle(Triangle{{0, 1, 2}, 0});  // Normal +z, 90 degrees at vertex 0
    mesh.AddTriangle(Triangle{{0, 1, 3}, 0});  // Normal +y, 45 degrees at vertex 0

    mesh.ComputeNormals();

    const TriVector& normal = mesh.Vertices()[0].normal;
    EXPECT_NEAR(normal.e013() / normal.e021(), 0.5f, 1e-5f);
    EXPECT_NEAR(normal.VNorm(), 1.0f, 1e-5f);
}

// Like the flipped OBJ v, generated v is 0 at the top, where image row 0 is
TEST_F(MeshTest, CylindricalUVsStartAtTheTop) {
    for (const TriVector& p : {TriVector(1.0f, 0.0f, 0.0f), TriVector(1.0f, 2.0f, 0.0f)}) {
        mesh.AddVertex({.position = p});
    }

    mesh.GenerateCylindricalUVs();

    EXPECT_FLOAT_EQ(mesh.Vertices()[0].texCoord[1], 1.0f);
    EXPECT_FLOAT_EQ(mesh.Vertices()[1].texCoord[1], 0.0f);
}

TEST_F(MeshTest, StandOnOrigin) {
    for (const TriVector& p : {TriVector(1.0f, 2.0f, 3.0f), TriVector(3.0f, 6.0f, 7.0f)}) {
        mesh.AddVertex({.position = p});
    }

    mesh.StandOnOrigin();

    EXPECT_FLOAT_EQ(mesh.Vertices()[0].position.e032(), -1.0f);
    EXPECT_FLOAT_EQ(mesh.Vertices()[0].position.e013(), 0.0f);
    EXPECT_FLOAT_EQ(mesh.Vertices()[0].position.e021(), -2.0f);
    EXPECT_FLOAT_EQ(mesh.Vertices()[1].position.e013(), 4.0f);
}

// Ns = 0 is a fully rough surface; the GGX alpha (roughness^2) matches sqrt(2 / (Ns + 2))
TEST(MeshMaterialTest, RoughnessFromShininess) {
    EXPECT_FLOAT_EQ(RoughnessFromShininess(0.0f), 1.0f);
    const float roughness = RoughnessFromShininess(98.0f);
    EXPECT_NEAR(roughness * roughness, std::sqrt(2.0f / 100.0f), 1e-6f);
}

// Corners that repeat the same position and texture coordinate share a vertex
TEST_F(MeshTest, LoadSharesRepeatedCorners) {
    ASSERT_TRUE(LoadObj(
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "f 1/1 2/2 3/3\nf 1/1 3/3 4/4\n"));

    EXPECT_EQ(mesh.VertexCount(), 4u);
    EXPECT_EQ(mesh.TriangleCount(), 2u);
}

// A face that refers to a missing position or texture coordinate fails the load and keeps the mesh
TEST_F(MeshTest, LoadRejectsMissingVertices) {
    ASSERT_TRUE(LoadObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

    EXPECT_FALSE(LoadObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n"));
    EXPECT_FALSE(LoadObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/2\n"));
    EXPECT_EQ(mesh.VertexCount(), 3u);
    EXPECT_EQ(mesh.TriangleCount(), 1u);
}

// A texture seam splits a vertex, but the computed normals of both halves are the same:
// the angle-weighted normal of the fold between a +z and a +y face
TEST_F(MeshTest, LoadSplitsTextureSeams) {
    ASSERT_TRUE(LoadObj(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 0 -1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\nvt 0.5 0.5\n"
        "f 1/1 2/2 3/3\nf 1/4 2/2 4/3\n"));

    ASSERT_EQ(mesh.VertexCount(), 5u);
    const Vertex& first = mesh.Vertices()[mesh.Triangles()[0].indices[0]];
    const Vertex& second = mesh.Vertices()[mesh.Triangles()[1].indices[0]];
    EXPECT_NE(first.texCoord[0], second.texCoord[0]);
    for (const Vertex* v : {&first, &second}) {
        EXPECT_NEAR(v->normal.e032(), 0.0f, 1e-5f);
        EXPECT_NEAR(v->normal.e013() / v->normal.e021(), 0.5f, 1e-5f);
    }
}

TEST(MaterialTest, DefaultValues) {
    Scene::Material mat;

    EXPECT_EQ(mat.shadingMode, Scene::ShadingMode::Lambert);
    EXPECT_FLOAT_EQ(mat.color.r, 1.0f);
    EXPECT_FLOAT_EQ(mat.metallic, 0.0f);
    EXPECT_FLOAT_EQ(mat.roughness, 0.5f);
    EXPECT_TRUE(mat.texturePath.empty());
}

// The factory functions set the shading model and its parameters
TEST(MaterialTest, Factories) {
    const auto metal = Scene::Material::Metal(Scene::Color(0.9f, 0.8f, 0.7f), 0.25f);
    EXPECT_EQ(metal.shadingMode, Scene::ShadingMode::PBR);
    EXPECT_FLOAT_EQ(metal.metallic, 1.0f);
    EXPECT_FLOAT_EQ(metal.roughness, 0.25f);

    const auto phong = Scene::Material::Phong(Scene::Color::Red(), 64.0f, 0.3f, 0.2f);
    EXPECT_EQ(phong.shadingMode, Scene::ShadingMode::Phong);
    EXPECT_FLOAT_EQ(phong.shininess, 64.0f);
    EXPECT_FLOAT_EQ(phong.reflectivity, 0.3f);
    EXPECT_FLOAT_EQ(phong.specular, 0.2f);
}

TEST(VertexTest, ValueInitializedToZero) {
    Vertex v{};
    EXPECT_FLOAT_EQ(v.position.e032(), 0.0f);
    EXPECT_FLOAT_EQ(v.position.e013(), 0.0f);
    EXPECT_FLOAT_EQ(v.position.e021(), 0.0f);

    EXPECT_FLOAT_EQ(v.normal.e032(), 0.0f);
    EXPECT_FLOAT_EQ(v.normal.e013(), 0.0f);
    EXPECT_FLOAT_EQ(v.normal.e021(), 0.0f);
    EXPECT_FLOAT_EQ(v.normal.e123(), 0.0f);

    EXPECT_FLOAT_EQ(v.texCoord[0], 0.0f);
    EXPECT_FLOAT_EQ(v.texCoord[1], 0.0f);
}

TEST(TriangleTest, DefaultValues) {
    Triangle tri;

    EXPECT_EQ(tri.materialIndex, 0);
}

// The mesh BVH covers all triangles
TEST_F(MeshTest, BuildBVH) {
    for (int i = 0; i < 100; ++i) {
        const auto first = static_cast<uint32_t>(mesh.VertexCount());
        for (const auto& [dx, dy] : {std::pair{0.0f, 0.0f}, std::pair{0.5f, 0.0f}, std::pair{0.0f, 0.5f}}) {
            mesh.AddVertex({.position = TriVector(static_cast<float>(i) + dx, dy, 0.0f)});
        }
        mesh.AddTriangle({{first, first + 1, first + 2}});
    }

    mesh.BuildBVH();

    const BVH& bvh = mesh.GetBVH();
    ASSERT_FALSE(bvh.nodes.empty());
    EXPECT_EQ(bvh.primitiveOrder.size(), 100u);
    EXPECT_FLOAT_EQ(bvh.nodes[0].bounds.min[0], 0.0f);
    EXPECT_FLOAT_EQ(bvh.nodes[0].bounds.max[0], 99.5f);
}

namespace {

// Corners wound counter-clockwise seen from outside, as in OBJ
Mesh MakeTetrahedron() {
    return MakeMesh({TriVector(0, 0, 0), TriVector(1, 0, 0), TriVector(0, 1, 0), TriVector(0, 0, 1)},
                    {{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}});
}

const Motor kScrew = TranslationMotor(3.0f, -2.0f, 5.0f) *
                     Motor::Rotation(40.0f, TriVector(1.0f, 0.0f, 0.0f) & TriVector(0.0f, 2.0f, 1.0f));

} // anonymous namespace

TEST(MeshMeasuresTest, TetrahedronVolume) {
    EXPECT_NEAR(MakeTetrahedron().Volume(), 1.0f / 6.0f, 1e-6f);
}

TEST(MeshMeasuresTest, TetrahedronCenterOfMass) {
    const TriVector center = MakeTetrahedron().CenterOfMass();
    EXPECT_NEAR(center.e032(), 0.25f, 1e-6f);
    EXPECT_NEAR(center.e013(), 0.25f, 1e-6f);
    EXPECT_NEAR(center.e021(), 0.25f, 1e-6f);
}

TEST(MeshMeasuresTest, ReversedWindingNegatesVolume) {
    const Mesh mesh = MakeMesh({TriVector(0, 0, 0), TriVector(1, 0, 0), TriVector(0, 1, 0), TriVector(0, 0, 1)},
                               {{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}});
    EXPECT_NEAR(mesh.Volume(), -1.0f / 6.0f, 1e-6f);
}

// Volume and center of mass move along with the mesh
TEST(MeshMeasuresTest, BoxMeasuresFollowItsMotor) {
    const Mesh box = MakeBox(2.0f, 3.0f, 4.0f, kScrew);
    EXPECT_NEAR(box.Volume(), 24.0f, 1e-3f);
    const TriVector center = box.CenterOfMass();
    const TriVector expected = ApplyMotor(kScrew, TriVector(0.0f, 0.0f, 0.0f));
    EXPECT_NEAR(center.e032(), expected.e032(), 1e-4f);
    EXPECT_NEAR(center.e013(), expected.e013(), 1e-4f);
    EXPECT_NEAR(center.e021(), expected.e021(), 1e-4f);
}

// A box with sizes a, b, c has the principal moments V (b^2 + c^2) / 12, ... around its own axes
TEST(MeshMeasuresTest, BoxPrincipalInertia) {
    const std::array<float, 3> sizes{2.0f, 3.0f, 4.0f};
    const PrincipalInertia inertia = MakeBox(sizes[0], sizes[1], sizes[2], kScrew).Inertia();
    const float volume = sizes[0] * sizes[1] * sizes[2];
    const std::array<TriVector, 3> axes{TriVector(1, 0, 0, 0), TriVector(0, 1, 0, 0), TriVector(0, 0, 1, 0)};

    for (size_t i = 0; i < 3; ++i) {
        const TriVector principalAxis = ApplyMotor(inertia.rotor, axes[i]);
        bool matched = false;
        for (size_t k = 0; k < 3; ++k) {
            const float other1 = sizes[(k + 1) % 3];
            const float other2 = sizes[(k + 2) % 3];
            const float moment = volume * (other1 * other1 + other2 * other2) / 12.0f;
            if (std::abs(inertia.moments[i] - moment) > 1e-3f * moment) continue;
            const TriVector boxAxis = ApplyMotor(kScrew, axes[k]);
            const float cosine = principalAxis.e032() * boxAxis.e032() + principalAxis.e013() * boxAxis.e013() +
                                 principalAxis.e021() * boxAxis.e021();
            EXPECT_NEAR(std::abs(cosine), 1.0f, 1e-4f) << "principal axis " << i;
            matched = true;
        }
        EXPECT_TRUE(matched) << "moment " << inertia.moments[i];
    }
}

TEST(MeshMeasuresTest, ClosedBox) {
    EXPECT_TRUE(MakeBox(1.0f, 2.0f, 3.0f, kScrew).IsClosed());
}

TEST(MeshMeasuresTest, BoxWithoutAFaceIsOpen) {
    Mesh box = MakeBox(1.0f, 1.0f, 1.0f);
    Mesh open;
    for (const Vertex& vertex : box.Vertices()) open.AddVertex(vertex);
    for (size_t i = 2; i < box.TriangleCount(); ++i) open.AddTriangle(box.Triangles()[i]);
    EXPECT_FALSE(open.IsClosed());
}

TEST(MeshMeasuresTest, FlippedTriangleIsNotClosed) {
    const Mesh box = MakeBox(1.0f, 1.0f, 1.0f);
    Mesh flipped;
    for (const Vertex& vertex : box.Vertices()) flipped.AddVertex(vertex);
    for (Triangle triangle : box.Triangles()) {
        if (flipped.TriangleCount() == 0) std::swap(triangle.indices[1], triangle.indices[2]);
        flipped.AddTriangle(triangle);
    }
    EXPECT_FALSE(flipped.IsClosed());
}

// Vertices that only differ in normal or texture coordinates are welded
TEST(MeshMeasuresTest, SplitVerticesAreWelded) {
    const Mesh box = MakeBox(1.0f, 1.0f, 1.0f);
    Mesh split;
    for (const Triangle& triangle : box.Triangles()) {
        Triangle copy = triangle;
        for (uint32_t& index : copy.indices) {
            split.AddVertex(box.Vertices()[index]);
            index = static_cast<uint32_t>(split.VertexCount() - 1);
        }
        split.AddTriangle(copy);
    }
    EXPECT_TRUE(split.IsClosed());
    EXPECT_NEAR(split.Volume(), 1.0f, 1e-5f);
}

TEST(MeshMeasuresTest, EmptyMeshIsNotClosed) {
    EXPECT_FALSE(Mesh().IsClosed());
}
