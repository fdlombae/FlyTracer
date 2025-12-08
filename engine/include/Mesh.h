#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include "Scene.h"
#include "FlyFish.h"

// Material properties for mesh rendering
struct Material {
    Scene::ShadingMode shadingMode{Scene::ShadingMode::PBR};

    float diffuse[3]{0.8f, 0.8f, 0.8f};
    float _pad0{0.0f};

    float ambient[3]{0.1f, 0.1f, 0.1f};
    float _pad1{0.0f};
    float specular[3]{1.0f, 1.0f, 1.0f};
    float shininess{32.0f};
    float emission[3]{0.0f, 0.0f, 0.0f};
    float opacity{1.0f};

    float metalness{0.0f};
    float roughness{0.5f};

    int32_t diffuseTextureIndex{-1};
    int32_t specularTextureIndex{-1};
    int32_t _pad2[2]{0, 0};

    std::string name;
    std::string diffuseTexturePath;
    std::string specularTexturePath;

    [[nodiscard]] Scene::GPUMaterial toGPU() const noexcept {
        Scene::GPUMaterial gpu{};
        gpu.diffuse[0] = diffuse[0];
        gpu.diffuse[1] = diffuse[1];
        gpu.diffuse[2] = diffuse[2];
        gpu.shininess = shininess;
        gpu.shadingMode = static_cast<int32_t>(shadingMode);
        gpu.diffuseTextureIndex = diffuseTextureIndex;
        gpu.metalness = metalness;
        gpu.roughness = roughness;
        return gpu;
    }

    [[nodiscard]] static Material Flat(float r, float g, float b) noexcept {
        Material m;
        m.shadingMode = Scene::ShadingMode::Flat;
        m.diffuse[0] = r; m.diffuse[1] = g; m.diffuse[2] = b;
        return m;
    }

    [[nodiscard]] static Material Lambert(float r, float g, float b) noexcept {
        Material m;
        m.shadingMode = Scene::ShadingMode::Lambert;
        m.diffuse[0] = r; m.diffuse[1] = g; m.diffuse[2] = b;
        return m;
    }

    [[nodiscard]] static Material Phong(float r, float g, float b, float shine = 32.0f) noexcept {
        Material m;
        m.shadingMode = Scene::ShadingMode::Phong;
        m.diffuse[0] = r; m.diffuse[1] = g; m.diffuse[2] = b;
        m.shininess = shine;
        return m;
    }

    [[nodiscard]] static Material PBR(float r, float g, float b, float metal = 0.0f, float rough = 0.5f) noexcept {
        Material m;
        m.shadingMode = Scene::ShadingMode::PBR;
        m.diffuse[0] = r; m.diffuse[1] = g; m.diffuse[2] = b;
        m.metalness = metal;
        m.roughness = rough;
        return m;
    }
};

// GPU-compatible vertex struct (POD, 32 bytes)
struct GPUVertex {
    float pos_x, pos_y, pos_z, tex_u;
    float norm_x, norm_y, norm_z, tex_v;
};
static_assert(sizeof(GPUVertex) == 32, "GPUVertex must be exactly 32 bytes");

// Vertex using PGA primitives - for CPU-side mesh processing
struct Vertex {
    TriVector position;
    Vector normal;
    float texCoord[2];
    float _pad[2];

    [[nodiscard]] GPUVertex toGPU() const noexcept {
        return GPUVertex{
            position.e032(), position.e013(), position.e021(), texCoord[0],
            normal.e1(), normal.e2(), normal.e3(), texCoord[1]
        };
    }
};

struct Triangle {
    uint32_t indices[3];
    uint32_t materialIndex{0};
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh() = default;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;

    [[nodiscard]] bool loadFromFile(const std::string& filename);
    [[nodiscard]] bool loadFromFile(const std::string& objFilename, const std::string& mtlBasePath);

    [[nodiscard]] const std::vector<Vertex>& vertices() const noexcept { return m_vertices; }
    [[nodiscard]] const std::vector<Triangle>& triangles() const noexcept { return m_triangles; }
    [[nodiscard]] const std::vector<Material>& materials() const noexcept { return m_materials; }
    [[nodiscard]] bool empty() const noexcept { return m_vertices.empty(); }
    [[nodiscard]] size_t vertexCount() const noexcept { return m_vertices.size(); }
    [[nodiscard]] size_t triangleCount() const noexcept { return m_triangles.size(); }
    [[nodiscard]] size_t materialCount() const noexcept { return m_materials.size(); }

    void addMaterial(const Material& material);
    [[nodiscard]] const Material& getMaterial(size_t index) const;
    [[nodiscard]] Material& getMaterial(size_t index);
    [[nodiscard]] bool hasMaterial(size_t index) const noexcept { return index < m_materials.size(); }

    void clear();
    void clearCPUData();
    void addVertex(const Vertex& vertex);
    void addTriangle(const Triangle& triangle);
    void setVertices(std::vector<Vertex>&& vertices);
    void setTriangles(std::vector<Triangle>&& triangles);

    void computeNormals();

    void scale(float factor);
    void translate(float x, float y, float z);
    void centerOnOrigin();

    void decimate(float targetRatio);

    void buildBVH();
    [[nodiscard]] const std::vector<Scene::BVHNode>& bvhNodes() const noexcept { return m_bvhNodes; }
    [[nodiscard]] const std::vector<uint32_t>& bvhTriIndices() const noexcept { return m_bvhTriIndices; }

    struct BoundingBox {
        float min[3]{0.0f, 0.0f, 0.0f};
        float max[3]{0.0f, 0.0f, 0.0f};

        [[nodiscard]] constexpr float width() const noexcept { return max[0] - min[0]; }
        [[nodiscard]] constexpr float height() const noexcept { return max[1] - min[1]; }
        [[nodiscard]] constexpr float depth() const noexcept { return max[2] - min[2]; }

        constexpr void center(float out[3]) const noexcept {
            out[0] = (min[0] + max[0]) * 0.5f;
            out[1] = (min[1] + max[1]) * 0.5f;
            out[2] = (min[2] + max[2]) * 0.5f;
        }

        [[nodiscard]] constexpr bool contains(float x, float y, float z) const noexcept {
            return x >= min[0] && x <= max[0] &&
                   y >= min[1] && y <= max[1] &&
                   z >= min[2] && z <= max[2];
        }
    };

    struct FaceNormal {
        float normal[3];
        uint32_t indices[3];
    };

    void computePhysicsData();
    [[nodiscard]] const BoundingBox& boundingBox() const noexcept { return m_boundingBox; }
    [[nodiscard]] const std::vector<FaceNormal>& faceNormals() const noexcept { return m_faceNormals; }
    [[nodiscard]] bool hasPhysicsData() const noexcept { return m_hasPhysicsData; }

private:
    void updateNodeBounds(uint32_t nodeIdx);
    void subdivideNode(uint32_t nodeIdx);
    [[nodiscard]] float evaluateSAH(uint32_t nodeIdx, int axis, float pos);

    std::vector<Scene::BVHNode> m_bvhNodes;
    std::vector<uint32_t> m_bvhTriIndices;
    std::vector<float> m_triCentroids;

    std::vector<Vertex> m_vertices;
    std::vector<Triangle> m_triangles;
    std::vector<Material> m_materials;

    BoundingBox m_boundingBox;
    std::vector<FaceNormal> m_faceNormals;
    bool m_hasPhysicsData{false};
};
