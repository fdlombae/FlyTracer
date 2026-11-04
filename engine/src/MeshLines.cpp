#include "MeshLines.h"
#include <algorithm>
#include <map>
#include <utility>

MeshEdges BuildEdges(const Mesh& mesh) {
    const std::vector<uint32_t> welded = mesh.WeldedVertexIds();
    // A welded id stands for all vertices at its position; any of them gives the position
    std::vector<TriVector> positions(welded.size());
    for (size_t i = 0; i < welded.size(); ++i) {
        positions[welded[i]] = mesh.Vertices()[i].position;
    }

    MeshEdges result;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> edgeIds;
    // An edge is stored once, anchored at its corner with the lower id
    const auto use = [&](uint32_t from, uint32_t to) {
        const auto [low, high] = std::minmax(from, to);
        const auto [it, isNew] = edgeIds.try_emplace({low, high}, static_cast<uint32_t>(result.edges.size()));
        if (isNew) {
            result.edges.push_back({positions[low], positions[high] - positions[low]});
        }
        return EdgeUse{it->second, from > to};
    };

    result.triangles.reserve(mesh.TriangleCount());
    for (const Triangle& triangle : mesh.Triangles()) {
        const std::array corners{welded[triangle.indices[0]], welded[triangle.indices[1]], welded[triangle.indices[2]]};
        TriangleEdges edges;
        for (size_t i = 0; i < 3; ++i) {
            edges.opposite[i] = use(corners[(i + 1) % 3], corners[(i + 2) % 3]);
        }
        result.triangles.push_back(edges);
    }
    return result;
}
