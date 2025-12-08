#include <gtest/gtest.h>
#include "Mesh.h"
#include <cmath>
#include <stdexcept>

class MeshTest : public ::testing::Test {
protected:
    Mesh mesh;
};

// Test default construction
TEST_F(MeshTest, DefaultConstruction) {
    EXPECT_TRUE(mesh.vertices().empty());
    EXPECT_TRUE(mesh.triangles().empty());
    EXPECT_EQ(mesh.vertexCount(), 0);
    EXPECT_EQ(mesh.triangleCount(), 0);
}

// Test adding vertices
TEST_F(MeshTest, AddVertex) {
    Vertex v;
    // Position: TriVector uses e032=x, e013=y, e021=z, e123=w
    v.position = TriVector(1.0f, 2.0f, 3.0f, 1.0f);
    // Normal: Vector uses e0=d, e1=nx, e2=ny, e3=nz
    v.normal = Vector(0.0f, 0.0f, 1.0f, 0.0f);
    v.texCoord[0] = 0.5f;
    v.texCoord[1] = 0.5f;

    mesh.addVertex(v);

    EXPECT_EQ(mesh.vertexCount(), 1);
    EXPECT_FLOAT_EQ(mesh.vertices()[0].position.e032(), 1.0f);
    EXPECT_FLOAT_EQ(mesh.vertices()[0].position.e013(), 2.0f);
    EXPECT_FLOAT_EQ(mesh.vertices()[0].position.e021(), 3.0f);
}

// Test adding triangles
TEST_F(MeshTest, AddTriangle) {
    // Add 3 vertices first
    for (int i = 0; i < 3; ++i) {
        Vertex v;
        v.position = TriVector(static_cast<float>(i), 0.0f, 0.0f, 1.0f);
        mesh.addVertex(v);
    }

    Triangle tri;
    tri.indices[0] = 0;
    tri.indices[1] = 1;
    tri.indices[2] = 2;
    tri.materialIndex = 0;

    mesh.addTriangle(tri);

    EXPECT_EQ(mesh.triangleCount(), 1);
    EXPECT_EQ(mesh.triangles()[0].indices[0], 0);
    EXPECT_EQ(mesh.triangles()[0].indices[1], 1);
    EXPECT_EQ(mesh.triangles()[0].indices[2], 2);
}

// Test clear functionality
TEST_F(MeshTest, Clear) {
    Vertex v;
    mesh.addVertex(v);
    Triangle tri;
    tri.indices[0] = 0;
    tri.indices[1] = 0;
    tri.indices[2] = 0;
    mesh.addTriangle(tri);

    mesh.clear();

    EXPECT_TRUE(mesh.vertices().empty());
    EXPECT_TRUE(mesh.triangles().empty());
}

// Test material management
TEST_F(MeshTest, AddMaterial) {
    Material mat;
    mat.name = "TestMaterial";
    mat.diffuse[0] = 1.0f;
    mat.diffuse[1] = 0.0f;
    mat.diffuse[2] = 0.0f;
    mat.shininess = 64.0f;

    mesh.addMaterial(mat);

    EXPECT_EQ(mesh.materialCount(), 1);
    EXPECT_EQ(mesh.getMaterial(0).name, "TestMaterial");
    EXPECT_FLOAT_EQ(mesh.getMaterial(0).shininess, 64.0f);
}

// Test getting material out of bounds throws exception
TEST_F(MeshTest, GetMaterialOutOfBoundsThrows) {
    EXPECT_THROW((void)mesh.getMaterial(999), std::out_of_range);
}

// Test hasMaterial method
TEST_F(MeshTest, HasMaterial) {
    EXPECT_FALSE(mesh.hasMaterial(0));

    Material mat;
    mat.name = "TestMaterial";
    mesh.addMaterial(mat);

    EXPECT_TRUE(mesh.hasMaterial(0));
    EXPECT_FALSE(mesh.hasMaterial(1));
}

// Test compute normals
TEST_F(MeshTest, ComputeNormals) {
    // Create a simple triangle in the XY plane (normal should be +Z)
    Vertex v0, v1, v2;
    v0.position = TriVector(0.0f, 0.0f, 0.0f, 1.0f);
    v1.position = TriVector(1.0f, 0.0f, 0.0f, 1.0f);
    v2.position = TriVector(0.0f, 1.0f, 0.0f, 1.0f);

    mesh.addVertex(v0);
    mesh.addVertex(v1);
    mesh.addVertex(v2);

    Triangle tri;
    tri.indices[0] = 0;
    tri.indices[1] = 1;
    tri.indices[2] = 2;
    mesh.addTriangle(tri);

    mesh.computeNormals();

    // All vertices should have normal pointing in +Z direction
    // Normal uses Vector: e0=d, e1=nx, e2=ny, e3=nz
    for (const auto& v : mesh.vertices()) {
        EXPECT_NEAR(v.normal.e1(), 0.0f, 0.001f);  // nx
        EXPECT_NEAR(v.normal.e2(), 0.0f, 0.001f);  // ny
        EXPECT_NEAR(v.normal.e3(), 1.0f, 0.001f);  // nz
    }
}

// Test setVertices with move semantics
TEST_F(MeshTest, SetVerticesMove) {
    std::vector<Vertex> vertices(10);
    for (int i = 0; i < 10; ++i) {
        vertices[i].position = TriVector(static_cast<float>(i), 0.0f, 0.0f, 1.0f);
    }

    mesh.setVertices(std::move(vertices));

    EXPECT_EQ(mesh.vertexCount(), 10);
    EXPECT_FLOAT_EQ(mesh.vertices()[5].position.e032(), 5.0f);
}

// Test setTriangles with move semantics
TEST_F(MeshTest, SetTrianglesMove) {
    std::vector<Triangle> triangles(5);
    for (int i = 0; i < 5; ++i) {
        triangles[i].materialIndex = static_cast<uint32_t>(i);
    }

    mesh.setTriangles(std::move(triangles));

    EXPECT_EQ(mesh.triangleCount(), 5);
    EXPECT_EQ(mesh.triangles()[3].materialIndex, 3);
}

// Test Material default values
TEST(MaterialTest, DefaultValues) {
    Material mat;

    // Check default ambient
    EXPECT_FLOAT_EQ(mat.ambient[0], 0.1f);
    EXPECT_FLOAT_EQ(mat.ambient[1], 0.1f);
    EXPECT_FLOAT_EQ(mat.ambient[2], 0.1f);

    // Check default diffuse
    EXPECT_FLOAT_EQ(mat.diffuse[0], 0.8f);
    EXPECT_FLOAT_EQ(mat.diffuse[1], 0.8f);
    EXPECT_FLOAT_EQ(mat.diffuse[2], 0.8f);

    // Check default specular
    EXPECT_FLOAT_EQ(mat.specular[0], 1.0f);
    EXPECT_FLOAT_EQ(mat.specular[1], 1.0f);
    EXPECT_FLOAT_EQ(mat.specular[2], 1.0f);

    // Check other defaults
    EXPECT_FLOAT_EQ(mat.shininess, 32.0f);
    EXPECT_FLOAT_EQ(mat.opacity, 1.0f);
    EXPECT_EQ(mat.diffuseTextureIndex, -1);
    EXPECT_EQ(mat.specularTextureIndex, -1);
}

// Test Vertex value-initialized to zero
TEST(VertexTest, ValueInitializedToZero) {
    Vertex v{};  // Value initialization zeros POD types

    // Position should be zero (TriVector)
    EXPECT_FLOAT_EQ(v.position.e032(), 0.0f);
    EXPECT_FLOAT_EQ(v.position.e013(), 0.0f);
    EXPECT_FLOAT_EQ(v.position.e021(), 0.0f);

    // Normal should be zero (Vector)
    EXPECT_FLOAT_EQ(v.normal.e1(), 0.0f);
    EXPECT_FLOAT_EQ(v.normal.e2(), 0.0f);
    EXPECT_FLOAT_EQ(v.normal.e3(), 0.0f);

    // TexCoord should be zero
    EXPECT_FLOAT_EQ(v.texCoord[0], 0.0f);
    EXPECT_FLOAT_EQ(v.texCoord[1], 0.0f);
}

// Test Triangle default values
TEST(TriangleTest, DefaultValues) {
    Triangle tri;

    EXPECT_EQ(tri.materialIndex, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
