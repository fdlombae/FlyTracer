#include <gtest/gtest.h>
#include "BVH.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

// Unit boxes with their lower corner at the given x positions
std::vector<AABB> BoxesAt(const std::vector<float>& xPositions) {
    std::vector<AABB> boxes;
    for (float x : xPositions) {
        AABB box;
        box.Grow(x, 0.0f, 0.0f);
        box.Grow(x + 0.5f, 0.5f, 0.0f);
        boxes.push_back(box);
    }
    return boxes;
}

// Walks the BVH, checks every leaf's primitives lie in its bounds, counts how often each primitive
// is referenced and returns the maximum depth
uint32_t CheckNode(const BVH& bvh, const std::vector<AABB>& boxes, uint32_t nodeIdx, uint32_t depth,
                   std::vector<int>& uses) {
    const BVHNode& node = bvh.nodes[nodeIdx];
    if (node.count == 0) {
        return std::max(CheckNode(bvh, boxes, node.leftFirst, depth + 1, uses),
                        CheckNode(bvh, boxes, node.leftFirst + 1, depth + 1, uses));
    }
    for (int32_t i = 0; i < node.count; ++i) {
        const uint32_t primitive = bvh.primitiveOrder[node.leftFirst + i];
        ++uses[primitive];
        for (int axis = 0; axis < 3; ++axis) {
            EXPECT_GE(boxes[primitive].min[axis], node.bounds.min[axis]);
            EXPECT_LE(boxes[primitive].max[axis], node.bounds.max[axis]);
        }
    }
    return depth;
}

} // anonymous namespace

TEST(BVHTest, EmptyInput) {
    const BVH bvh = BuildBVH({}, 4);
    EXPECT_TRUE(bvh.nodes.empty());
    EXPECT_TRUE(bvh.primitiveOrder.empty());
}

// Every primitive is referenced exactly once, and large leaves are split
TEST(BVHTest, CoversAllPrimitives) {
    std::vector<float> xPositions;
    for (int i = 0; i < 1000; ++i) {
        xPositions.push_back(static_cast<float>(i));
    }
    const std::vector<AABB> boxes = BoxesAt(xPositions);

    const BVH bvh = BuildBVH(boxes, 4);

    std::vector<int> uses(boxes.size(), 0);
    CheckNode(bvh, boxes, 0, 0, uses);
    EXPECT_TRUE(std::all_of(uses.begin(), uses.end(), [](int count) { return count == 1; }));
    for (const BVHNode& node : bvh.nodes) {
        EXPECT_LE(node.count, 4);
    }
}

// Exponentially spaced boxes make the SAH peel off a few per level; without the limit the depth would be 37
TEST(BVHTest, RespectsMaxDepth) {
    std::vector<float> xPositions;
    for (int i = 0; i < 180; ++i) {
        xPositions.push_back(std::pow(1.6f, static_cast<float>(i)));
    }
    const std::vector<AABB> boxes = BoxesAt(xPositions);

    const BVH bvh = BuildBVH(boxes, 1);

    std::vector<int> uses(boxes.size(), 0);
    EXPECT_LE(CheckNode(bvh, boxes, 0, 0, uses), kMaxBVHDepth);
    EXPECT_TRUE(std::all_of(uses.begin(), uses.end(), [](int count) { return count == 1; }));
}

// Identical boxes cannot be split and stay in one leaf
TEST(BVHTest, IdenticalPrimitivesStayInOneLeaf) {
    const BVH bvh = BuildBVH(BoxesAt(std::vector<float>(10, 3.0f)), 2);
    ASSERT_EQ(bvh.nodes.size(), 1u);
    EXPECT_EQ(bvh.nodes[0].count, 10);
}

namespace {

bool Contains(const AABB& box, const std::array<float, 3>& p) {
    for (int axis = 0; axis < 3; ++axis) {
        if (p[axis] < box.min[axis] || p[axis] > box.max[axis]) return false;
    }
    return true;
}

// Checks that a pair's children bound all corners below them, and counts the uses of every triangle
void CheckPair(const std::vector<NodePair>& pairs, int32_t pairIndex, const BVH& bvh,
               const std::vector<std::array<float, 3>>& corners, std::vector<int32_t>& below, std::vector<int>& uses) {
    for (const PairChild& child : pairs[pairIndex]) {
        std::vector<int32_t> triangles;
        if (child.count > 0) {
            for (int32_t i = child.index; i < child.index + child.count; ++i) {
                triangles.push_back(static_cast<int32_t>(bvh.primitiveOrder[i]));
                ++uses[bvh.primitiveOrder[i]];
            }
        } else if (child.count == 0) {
            EXPECT_GT(child.index, pairIndex);
            CheckPair(pairs, child.index, bvh, corners, triangles, uses);
        }
        for (const int32_t triangle : triangles) {
            for (size_t corner = 0; corner < 3; ++corner) {
                EXPECT_TRUE(Contains(child.box, corners[3 * triangle + corner])) << "triangle " << triangle;
            }
        }
        below.insert(below.end(), triangles.begin(), triangles.end());
    }
}

} // anonymous namespace

TEST(NodePairTest, ChildrenBoundEverythingBelowThem) {
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> r(-10.0f, 10.0f);
    std::uniform_real_distribution<float> small(-0.5f, 0.5f);
    std::vector<std::array<float, 3>> corners;
    std::vector<AABB> bounds;
    for (int triangle = 0; triangle < 500; ++triangle) {
        const std::array<float, 3> center{r(rng), r(rng), r(rng)};
        AABB box;
        for (int corner = 0; corner < 3; ++corner) {
            const std::array<float, 3> p{center[0] + small(rng), center[1] + small(rng), center[2] + small(rng)};
            corners.push_back(p);
            box.Grow(p[0], p[1], p[2]);
        }
        bounds.push_back(box);
    }
    const BVH bvh = BuildBVH(bounds, 4);
    const std::vector<NodePair> pairs = PairChildren(bvh);

    std::vector<int32_t> below;
    std::vector<int> uses(bounds.size(), 0);
    CheckPair(pairs, 0, bvh, corners, below, uses);
    EXPECT_TRUE(std::ranges::all_of(uses, [](int count) { return count == 1; }));
}

TEST(NodePairTest, SingleLeafBecomesOnePair) {
    AABB box;
    box.Grow(0.0f, 0.0f, 0.0f);
    box.Grow(1.0f, 1.0f, 0.0f);
    const std::vector<NodePair> pairs = PairChildren(BuildBVH({box}, 4));
    ASSERT_EQ(pairs.size(), 1u);
    EXPECT_EQ(pairs[0][0].index, 0);
    EXPECT_EQ(pairs[0][0].count, 1);
    EXPECT_EQ(pairs[0][1].count, -1);
}

TEST(NodePairTest, EmptyBVHHasNoPairs) {
    EXPECT_TRUE(PairChildren(BuildBVH({}, 4)).empty());
}
