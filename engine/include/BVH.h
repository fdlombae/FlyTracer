#pragma once

#include "FlyFish.h"
#include "ShaderInterface.h"
#include <array>
#include <cstdint>
#include <vector>

// Axis-aligned bounding box
struct AABB {
    std::array<float, 3> min{1e30f, 1e30f, 1e30f};
    std::array<float, 3> max{-1e30f, -1e30f, -1e30f};

    void Grow(float x, float y, float z) noexcept;
    void Grow(const TriVector& point) noexcept { Grow(point.e032(), point.e013(), point.e021()); }
    void Grow(const AABB& other) noexcept;
    [[nodiscard]] float Centroid(int axis) const noexcept { return 0.5f * (min[axis] + max[axis]); }
    [[nodiscard]] float Extent(int axis) const noexcept { return max[axis] - min[axis]; }
    // Half the surface area, which is all the SAH needs for comparisons
    [[nodiscard]] float HalfArea() const noexcept;
};

// Leaves (count > 0) reference count primitives starting at leftFirst in the primitive order;
// internal nodes have their children at leftFirst and leftFirst + 1.
struct BVHNode {
    AABB bounds;
    int32_t leftFirst{0};
    int32_t count{0};
};

struct BVH {
    std::vector<BVHNode> nodes;
    std::vector<uint32_t> primitiveOrder;  // Leaf ranges index into this list of primitive indices
};

// The shader's traversal stack holds one entry more than this depth
inline constexpr uint32_t kMaxBVHDepth = BVH_STACK_SIZE - 1;

// Builds a BVH over primitives given by their bounds, splitting with the binned surface area heuristic.
// Leaves hold at most maxLeafSize primitives, except where kMaxBVHDepth stops subdivision.
[[nodiscard]] BVH BuildBVH(const std::vector<AABB>& primitiveBounds, int32_t maxLeafSize);

// A child as its parent stores it, with the box around everything below it
struct PairChild {
    int32_t index{-1};  // A leaf's first primitive in the BVH's primitive order, or an internal node's pair
    int32_t count{-1};  // Primitives of a leaf, 0 for an internal node, -1 when there is no child
    AABB box;
};

using NodePair = std::array<PairChild, 2>;

// The BVH's internal nodes as pairs of their children, so a traversal reads the boxes of both with
// one load. Depth first from the root, which is pair 0; a BVH that is a single leaf becomes one pair
// without a second child.
[[nodiscard]] std::vector<NodePair> PairChildren(const BVH& bvh);
