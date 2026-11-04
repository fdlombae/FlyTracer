#pragma once

#include "FlyFish.h"
#include "Mesh.h"
#include <array>
#include <cstdint>
#include <vector>

// A mesh's triangles as lines, for the raytracer's Plücker ray-triangle test

// The line anchor & direction, through the edge from anchor to anchor + direction. The raytracer
// evaluates its Plücker product with a ray relative to the anchor, which keeps it as precise far
// from the origin as near it.
struct AnchoredEdge {
    TriVector anchor;
    TriVector direction;  // e123 = 0
};

// An edge as a triangle runs through it: reversed when from anchor + direction to anchor
struct EdgeUse {
    uint32_t edge{0};
    bool reversed{false};
};

// opposite[i] is the edge between the two other corners, in winding order: from corner i + 1 to
// corner i + 2. A ray's Plücker product with it is proportional to the barycentric weight of corner i.
struct TriangleEdges {
    std::array<EdgeUse, 3> opposite;
};

// Vertices at the same position are welded, so neighboring triangles share their edge and both
// evaluate the same stored line: a ray through the edge cannot slip between them.
struct MeshEdges {
    std::vector<AnchoredEdge> edges;
    std::vector<TriangleEdges> triangles;  // In the order of the mesh's triangles
};

[[nodiscard]] MeshEdges BuildEdges(const Mesh& mesh);
