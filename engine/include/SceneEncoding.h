#pragma once

#include "BVH.h"
#include "GameScene.h"
#include "GpuScene.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The scene as the raytracer's buffers hold it, encoded on the CPU: the meshes once, the rest every
// frame. RaytracingPass uploads the results.

// What a mesh was like when it was encoded. When this changes the meshes are encoded again: material
// colors and the like are read every frame, but geometry and textures only with the meshes.
struct MeshLayout {
    const Mesh* mesh{nullptr};
    size_t vertexCount{0};
    size_t triangleCount{0};
    std::vector<std::string> texturePaths;  // One per material, empty for an untextured one

    bool operator==(const MeshLayout&) const = default;
};

// What the frames need to know about the encoded meshes
struct MeshTable {
    std::vector<MeshLayout> layouts;            // Per mesh
    std::vector<AABB> bounds;                   // Per mesh, in object space
    std::vector<int32_t> materialTextureSlots;  // Per material of all meshes, in order; -1 for none
    std::vector<std::string> texturePaths;      // Of texture slot i + 1; slot 0 is white
};

// All meshes concatenated, with their indices offset into the shared buffers
struct MeshData {
    std::vector<Gpu::Vertex> vertices;
    std::vector<Triangle> triangles;
    std::vector<Gpu::Edge> edges;
    std::vector<Gpu::EdgeTriangle> edgeTriangles;  // In each mesh's BVH leaf order
    std::vector<Gpu::NodePair> pairs;
    std::vector<uint32_t> rootPairs;               // Per mesh
    MeshTable table;
};

// Textures beyond maxTextures - 1 get no slot
[[nodiscard]] MeshData EncodeMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes, uint32_t maxTextures);

// Whether meshes were added or replaced, or changed their vertex, triangle or material counts or textures
[[nodiscard]] bool MeshesChanged(const MeshTable& table, const std::vector<std::unique_ptr<Mesh>>& meshes);

// The scene data of one frame
struct FrameData {
    std::vector<Gpu::Material> materials;      // Those of the meshes first, then of the spheres and planes
    std::vector<Gpu::Sphere> spheres;
    std::vector<Gpu::Plane> planes;
    std::vector<Gpu::Light> lights;
    std::vector<Gpu::MeshInstance> instances;  // The drawn instances, in the top-level BVH's leaf order
    std::vector<Gpu::NodePair> scenePairs;     // The top-level BVH, over the instances in world space
    std::vector<Gpu::DebugLine> debugLines;
};

// The scene's meshes must still match the table (see MeshesChanged). An instance is drawn when it is
// visible, its mesh was encoded and has triangles, and its scale is finite and not zero.
[[nodiscard]] FrameData EncodeFrame(const GameScene& scene, const MeshTable& table);
