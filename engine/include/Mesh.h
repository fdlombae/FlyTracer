#pragma once

#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include "BVH.h"
#include "Scene.h"
#include "FlyFish.h"

struct Vertex {
    TriVector position;
    TriVector normal{0.0f, 0.0f, 0.0f, 0.0f};  // A direction (e123 = 0)
    float texCoord[2];
};

struct Triangle {
    uint32_t indices[3];
    uint32_t materialIndex{0};
};

// Principal moments of inertia about the center of mass, for unit density.
// ApplyMotor(rotor, axis i) is the principal axis with moments[i], for the coordinate axes x, y and z.
struct PrincipalInertia {
    Motor rotor{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 3> moments{};
};

// OBJ's Blinn-Phong exponent Ns as a roughness for the Cook-Torrance BRDF
[[nodiscard]] float RoughnessFromShininess(float shininess) noexcept;

class Mesh {
public:
    Mesh() = default;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;

    // OBJ with its MTL materials, which are looked up next to the OBJ file
    [[nodiscard]] bool LoadFromFile(const std::string& filename);

    [[nodiscard]] const std::vector<Vertex>& Vertices() const noexcept { return m_vertices; }
    [[nodiscard]] const std::vector<Triangle>& Triangles() const noexcept { return m_triangles; }
    [[nodiscard]] const std::vector<Scene::Material>& Materials() const noexcept { return m_materials; }
    [[nodiscard]] size_t VertexCount() const noexcept { return m_vertices.size(); }
    [[nodiscard]] size_t TriangleCount() const noexcept { return m_triangles.size(); }
    [[nodiscard]] size_t MaterialCount() const noexcept { return m_materials.size(); }

    void AddMaterial(const Scene::Material& material) { m_materials.push_back(material); }
    // Throws std::out_of_range for an invalid index
    [[nodiscard]] const Scene::Material& GetMaterial(size_t index) const { return m_materials.at(index); }
    [[nodiscard]] Scene::Material& GetMaterial(size_t index) { return m_materials.at(index); }
    [[nodiscard]] bool HasMaterial(size_t index) const noexcept { return index < m_materials.size(); }

    void Clear();
    void AddVertex(const Vertex& vertex) { m_vertices.push_back(vertex); }
    void AddTriangle(const Triangle& triangle) { m_triangles.push_back(triangle); }

    // Angle-weighted vertex normals, for triangles wound counter-clockwise seen from outside (as in OBJ)
    void ComputeNormals();
    void GenerateCylindricalUVs();
    [[nodiscard]] bool HasUVCoordinates() const noexcept;

    [[nodiscard]] AABB Bounds() const noexcept;
    // The same id for vertices at the same position, which may differ in normal or texture coordinates
    [[nodiscard]] std::vector<uint32_t> WeldedVertexIds() const;

    // Measures of the solid the triangles enclose, after "Clean up your Mesh! Part 1" (De Keninck,
    // Roelfs, Dorst and Eelbode, 2025). They are only meaningful when IsClosed() holds.
    [[nodiscard]] float Volume() const;
    [[nodiscard]] TriVector CenterOfMass() const;
    [[nodiscard]] PrincipalInertia Inertia() const;
    // Every edge (with vertices welded by position) is run through as often in one direction as in
    // the other: the surface has no holes and its triangles are wound consistently
    [[nodiscard]] bool IsClosed() const;
    // Centers x and z on the origin and moves the lowest point to y = 0
    void StandOnOrigin();

    // Bottom-level acceleration structure over the triangles, in object space
    static constexpr int32_t kMaxLeafTriangles = 4;
    void BuildBVH();
    [[nodiscard]] const BVH& GetBVH() const noexcept { return m_bvh; }

private:
    BVH m_bvh;
    std::vector<Vertex> m_vertices;
    std::vector<Triangle> m_triangles;
    std::vector<Scene::Material> m_materials;
};
