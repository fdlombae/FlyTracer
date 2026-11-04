#include "BVH.h"
#include <algorithm>
#include <limits>
#include <numeric>

void AABB::Grow(float x, float y, float z) noexcept {
    const std::array<float, 3> p{x, y, z};
    for (int axis = 0; axis < 3; ++axis) {
        min[axis] = std::min(min[axis], p[axis]);
        max[axis] = std::max(max[axis], p[axis]);
    }
}

void AABB::Grow(const AABB& other) noexcept {
    for (int axis = 0; axis < 3; ++axis) {
        min[axis] = std::min(min[axis], other.min[axis]);
        max[axis] = std::max(max[axis], other.max[axis]);
    }
}

float AABB::HalfArea() const noexcept {
    const float x = Extent(0);
    const float y = Extent(1);
    const float z = Extent(2);
    return x * y + y * z + z * x;
}

namespace {

class Builder {
public:
    Builder(const std::vector<AABB>& primitiveBounds, int32_t maxLeafSize)
        : m_bounds(primitiveBounds), m_maxLeafSize(maxLeafSize)
    {
        m_bvh.primitiveOrder.resize(primitiveBounds.size());
        std::ranges::iota(m_bvh.primitiveOrder, 0u);
        m_bvh.nodes.reserve(2 * primitiveBounds.size());
    }

    BVH Build() && {
        if (m_bounds.empty()) return std::move(m_bvh);

        m_bvh.nodes.emplace_back();
        m_bvh.nodes[0].count = static_cast<int32_t>(m_bounds.size());
        updateBounds(0);
        subdivide(0, 0);
        return std::move(m_bvh);
    }

private:
    static constexpr int kBinCount = 8;

    void updateBounds(uint32_t nodeIdx) {
        BVHNode& node = m_bvh.nodes[nodeIdx];
        for (int32_t i = 0; i < node.count; ++i) {
            node.bounds.Grow(m_bounds[m_bvh.primitiveOrder[node.leftFirst + i]]);
        }
    }

    // Primitives are binned by the centroid of their bounds along each axis; every boundary
    // between bins is a candidate split, scored by the SAH from the bin bounds.
    void subdivide(uint32_t nodeIdx, uint32_t depth) {
        // Copy: m_bvh.nodes grows below
        const BVHNode node = m_bvh.nodes[nodeIdx];
        if (node.count <= m_maxLeafSize || depth >= kMaxBVHDepth) return;

        AABB centroidBounds;
        for (int32_t i = 0; i < node.count; ++i) {
            const AABB& bounds = m_bounds[m_bvh.primitiveOrder[node.leftFirst + i]];
            centroidBounds.Grow(bounds.Centroid(0), bounds.Centroid(1), bounds.Centroid(2));
        }
        const auto binIndex = [&](uint32_t primitive, int axis) {
            const float offset = m_bounds[primitive].Centroid(axis) - centroidBounds.min[axis];
            return std::min(kBinCount - 1, static_cast<int>(offset * kBinCount / centroidBounds.Extent(axis)));
        };

        int bestAxis = -1;
        int bestSplit = 0;
        float bestCost = std::numeric_limits<float>::infinity();

        for (int axis = 0; axis < 3; ++axis) {
            if (centroidBounds.Extent(axis) < 1e-6f) continue;

            std::array<AABB, kBinCount> binBounds{};
            std::array<int, kBinCount> binCounts{};
            for (int32_t i = 0; i < node.count; ++i) {
                const uint32_t primitive = m_bvh.primitiveOrder[node.leftFirst + i];
                const int bin = binIndex(primitive, axis);
                ++binCounts[bin];
                binBounds[bin].Grow(m_bounds[primitive]);
            }

            // Split s puts bins [0, s) on the left and [s, kBinCount) on the right
            std::array<float, kBinCount> leftCost{};
            std::array<int, kBinCount> leftCount{};
            AABB left;
            int count = 0;
            for (int split = 1; split < kBinCount; ++split) {
                left.Grow(binBounds[split - 1]);
                count += binCounts[split - 1];
                leftCount[split] = count;
                leftCost[split] = static_cast<float>(count) * left.HalfArea();
            }

            AABB right;
            int rightCount = 0;
            for (int split = kBinCount - 1; split >= 1; --split) {
                right.Grow(binBounds[split]);
                rightCount += binCounts[split];
                if (leftCount[split] == 0 || rightCount == 0) continue;

                const float cost = leftCost[split] + static_cast<float>(rightCount) * right.HalfArea();
                if (cost < bestCost) {
                    bestCost = cost;
                    bestAxis = axis;
                    bestSplit = split;
                }
            }
        }

        if (bestAxis < 0 || bestCost >= static_cast<float>(node.count) * node.bounds.HalfArea()) return;

        const auto first = m_bvh.primitiveOrder.begin() + node.leftFirst;
        const auto middle = std::partition(first, first + node.count,
            [&](uint32_t primitive) { return binIndex(primitive, bestAxis) < bestSplit; });
        const auto leftCount = static_cast<int32_t>(middle - first);

        const auto leftChildIdx = static_cast<uint32_t>(m_bvh.nodes.size());
        m_bvh.nodes.push_back({.leftFirst = node.leftFirst, .count = leftCount});
        m_bvh.nodes.push_back({.leftFirst = node.leftFirst + leftCount, .count = node.count - leftCount});

        m_bvh.nodes[nodeIdx].leftFirst = static_cast<int32_t>(leftChildIdx);
        m_bvh.nodes[nodeIdx].count = 0;

        updateBounds(leftChildIdx);
        updateBounds(leftChildIdx + 1);
        subdivide(leftChildIdx, depth + 1);
        subdivide(leftChildIdx + 1, depth + 1);
    }

    const std::vector<AABB>& m_bounds;
    const int32_t m_maxLeafSize;
    BVH m_bvh;
};

} // anonymous namespace

BVH BuildBVH(const std::vector<AABB>& primitiveBounds, int32_t maxLeafSize) {
    return Builder(primitiveBounds, maxLeafSize).Build();
}

std::vector<NodePair> PairChildren(const BVH& bvh) {
    if (bvh.nodes.empty()) return {};

    const auto childOf = [&](int32_t nodeIndex) {
        const BVHNode& node = bvh.nodes[nodeIndex];
        return PairChild{node.count > 0 ? node.leftFirst : -1, node.count, node.bounds};
    };

    std::vector<NodePair> pairs;
    if (bvh.nodes[0].count > 0) {
        pairs.push_back({childOf(0), PairChild{}});
        return pairs;
    }
    // Depth first: an internal child's pair follows its parent's, before those of the parent's right child
    const auto addPair = [&](const auto& self, int32_t nodeIndex) -> int32_t {
        const auto pairIndex = static_cast<int32_t>(pairs.size());
        pairs.emplace_back();
        const int32_t left = bvh.nodes[nodeIndex].leftFirst;
        NodePair pair{childOf(left), childOf(left + 1)};
        for (int32_t i = 0; i < 2; ++i) {
            if (pair[i].count == 0) {
                pair[i].index = self(self, left + i);
            }
        }
        pairs[pairIndex] = pair;
        return pairIndex;
    };
    addPair(addPair, 0);
    return pairs;
}
