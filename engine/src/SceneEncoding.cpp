#include "SceneEncoding.h"
#include "MeshLines.h"
#include <algorithm>
#include <cmath>
#include <ranges>
#include <unordered_map>

namespace {

// The GPU encoding of every element
auto encoded(const auto& elements) {
    return elements | std::views::transform([](const auto& element) { return Gpu::Encode(element); });
}

MeshLayout layoutOf(const Mesh& mesh) {
    return {&mesh, mesh.VertexCount(), mesh.TriangleCount(),
            mesh.Materials() | std::views::transform(&Scene::Material::texturePath) | std::ranges::to<std::vector>()};
}

// World-space bounds of an instance: the transformed corners of its mesh bounds
AABB worldBounds(const MeshInstance& instance, const AABB& meshBounds) {
    AABB bounds;
    for (int corner = 0; corner < 8; ++corner) {
        const TriVector local((corner & 1) ? meshBounds.max[0] : meshBounds.min[0],
                              (corner & 2) ? meshBounds.max[1] : meshBounds.min[1],
                              (corner & 4) ? meshBounds.max[2] : meshBounds.min[2]);
        bounds.Grow(instance.ToWorld(local));
    }
    return bounds;
}

bool isDrawn(const MeshInstance& instance, const MeshTable& table) {
    return instance.visible && instance.meshId < table.layouts.size() &&
           table.layouts[instance.meshId].triangleCount > 0 &&
           std::isfinite(instance.scale) && instance.scale != 0.0f;
}

} // anonymous namespace

MeshData EncodeMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes, uint32_t maxTextures) {
    MeshData data;
    std::unordered_map<std::string, int32_t> textureSlots;

    for (const auto& mesh : meshes) {
        const auto vertexOffset = static_cast<uint32_t>(data.vertices.size());
        const auto triangleOffset = static_cast<uint32_t>(data.triangles.size());
        const auto edgeOffset = static_cast<uint32_t>(data.edges.size());
        const auto recordOffset = static_cast<int32_t>(data.edgeTriangles.size());
        const auto pairOffset = static_cast<int32_t>(data.pairs.size());
        const auto materialOffset = static_cast<uint32_t>(data.table.materialTextureSlots.size());

        data.vertices.append_range(encoded(mesh->Vertices()));
        for (const Scene::Material& material : mesh->Materials()) {
            int32_t slot = -1;
            if (!material.texturePath.empty()) {
                const auto [it, inserted] = textureSlots.try_emplace(
                    material.texturePath, static_cast<int32_t>(data.table.texturePaths.size() + 1));
                if (inserted) {
                    data.table.texturePaths.push_back(material.texturePath);
                }
                slot = it->second < static_cast<int32_t>(maxTextures) ? it->second : -1;
            }
            data.table.materialTextureSlots.push_back(slot);
        }
        for (Triangle triangle : mesh->Triangles()) {
            for (uint32_t& index : triangle.indices) {
                index += vertexOffset;
            }
            triangle.materialIndex += materialOffset;
            data.triangles.push_back(triangle);
        }

        // The edges, and the triangle records in the BVH's leaf order: a leaf's records follow each other
        const MeshEdges meshEdges = BuildEdges(*mesh);
        data.edges.append_range(encoded(meshEdges.edges));
        const BVH& bvh = mesh->GetBVH();
        for (uint32_t triangleIndex : bvh.primitiveOrder) {
            data.edgeTriangles.push_back(
                Gpu::Encode(meshEdges.triangles[triangleIndex], edgeOffset, triangleIndex + triangleOffset));
        }
        data.rootPairs.push_back(static_cast<uint32_t>(pairOffset));
        for (const NodePair& pair : PairChildren(bvh)) {
            data.pairs.push_back(Gpu::Encode(pair, pairOffset, recordOffset));
        }

        data.table.layouts.push_back(layoutOf(*mesh));
        data.table.bounds.push_back(mesh->Bounds());
    }
    return data;
}

bool MeshesChanged(const MeshTable& table, const std::vector<std::unique_ptr<Mesh>>& meshes) {
    return !std::ranges::equal(meshes | std::views::transform([](const auto& mesh) { return layoutOf(*mesh); }),
                               table.layouts);
}

FrameData EncodeFrame(const GameScene& scene, const MeshTable& table) {
    FrameData frame;
    const Scene::SceneData& data = scene.GetSceneData();

    // Materials: the meshes' first, so triangles can index them directly, then spheres and planes
    size_t materialIndex = 0;
    for (size_t meshId = 0; meshId < table.layouts.size(); ++meshId) {
        for (const Scene::Material& material : scene.GetMeshes()[meshId]->Materials()) {
            frame.materials.push_back(Gpu::Encode(material, table.materialTextureSlots[materialIndex++]));
        }
    }
    for (const Scene::Sphere& sphere : data.spheres) {
        frame.spheres.push_back(Gpu::Encode(sphere, static_cast<uint32_t>(frame.materials.size())));
        frame.materials.push_back(Gpu::Encode(sphere.material, -1));
    }
    for (const Scene::Plane& plane : data.planes) {
        frame.planes.push_back(Gpu::Encode(plane, static_cast<uint32_t>(frame.materials.size())));
        frame.materials.push_back(Gpu::Encode(plane.material, -1));
    }
    frame.lights.append_range(encoded(data.lights));

    // The top-level BVH over the world bounds of the drawn instances, which are stored in its leaf
    // order so a leaf references a range of them
    std::vector<const MeshInstance*> drawnInstances;
    std::vector<AABB> instanceBounds;
    for (const MeshInstance& instance : scene.GetMeshInstances()) {
        if (isDrawn(instance, table)) {
            drawnInstances.push_back(&instance);
            instanceBounds.push_back(worldBounds(instance, table.bounds[instance.meshId]));
        }
    }
    const BVH sceneBvh = BuildBVH(instanceBounds, 1);
    for (const NodePair& pair : PairChildren(sceneBvh)) {
        frame.scenePairs.push_back(Gpu::Encode(pair, 0, 0));
    }
    for (uint32_t index : sceneBvh.primitiveOrder) {
        frame.instances.push_back(Gpu::Encode(*drawnInstances[index]));
    }

    frame.debugLines.append_range(encoded(scene.GetDebugLines()));
    return frame;
}
