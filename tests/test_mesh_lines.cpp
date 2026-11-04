#include <gtest/gtest.h>
#include "MeshLines.h"
#include "test_meshes.h"
#include <array>
#include <cmath>
#include <optional>
#include <random>

namespace {

const TriVector kOrigin(0.0f, 0.0f, 0.0f);

struct Barycentric {
    float u;  // Weight of corner 1
    float v;  // Weight of corner 2
    float t;  // Distance along the ray, in units of its direction
};

// The ray's Plücker product with an edge, evaluated with the edge's anchor as the origin
float anchoredProduct(const TriVector& origin, const TriVector& direction, const AnchoredEdge& edge) {
    const TriVector localOrigin = kOrigin + (origin - edge.anchor);
    return (localOrigin & direction) & (kOrigin & edge.direction);
}

// The PGA tracer's edge test: the ray passes through the triangle when its products with the three
// edges have the same sign, and they are proportional to the barycentric weights
std::optional<Barycentric> intersectEdges(const MeshEdges& mesh, size_t triangle,
                                          const TriVector& origin, const TriVector& direction) {
    std::array<float, 3> products{};
    for (size_t i = 0; i < 3; ++i) {
        const EdgeUse& use = mesh.triangles[triangle].opposite[i];
        products[i] = anchoredProduct(origin, direction, mesh.edges[use.edge]) * (use.reversed ? -1.0f : 1.0f);
    }
    const bool sameSign = (products[0] >= 0 && products[1] >= 0 && products[2] >= 0) ||
                          (products[0] <= 0 && products[1] <= 0 && products[2] <= 0);
    const float sum = products[0] + products[1] + products[2];
    if (!sameSign || sum == 0.0f) return std::nullopt;

    // The plane through the anchor of one edge, along both its direction and that of the next
    const AnchoredEdge& a = mesh.edges[mesh.triangles[triangle].opposite[0].edge];
    const AnchoredEdge& b = mesh.edges[mesh.triangles[triangle].opposite[1].edge];
    const Vector plane = (kOrigin & a.direction) & b.direction;
    const TriVector localOrigin = kOrigin + (origin - a.anchor);
    const float t = -(localOrigin & plane) / (direction & plane);
    return Barycentric{products[1] / sum, products[2] / sum, t};
}

using Double3 = std::array<double, 3>;

Double3 toDouble(const TriVector& x) {
    return {x.e032(), x.e013(), x.e021()};
}

Double3 subtract(const Double3& a, const Double3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Double3 cross(const Double3& a, const Double3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double dot(const Double3& a, const Double3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// Möller-Trumbore in double precision as the reference
std::array<double, 3> referenceBarycentric(const Mesh& mesh, const TriVector& origin, const TriVector& direction) {
    const Triangle& triangle = mesh.Triangles()[0];
    const Double3 p0 = toDouble(mesh.Vertices()[triangle.indices[0]].position);
    const Double3 edge1 = subtract(toDouble(mesh.Vertices()[triangle.indices[1]].position), p0);
    const Double3 edge2 = subtract(toDouble(mesh.Vertices()[triangle.indices[2]].position), p0);
    const Double3 d = toDouble(direction);
    const Double3 h = cross(d, edge2);
    const double f = 1.0 / dot(edge1, h);
    const Double3 s = subtract(toDouble(origin), p0);
    const Double3 q = cross(s, edge1);
    return {f * dot(s, h), f * dot(d, q), f * dot(edge2, q)};
}

// Random triangles at the given distance from the origin, with rays from nearby aimed at points in
// and around them, not grazing and not too close to an edge
template<typename Check>
void forRandomRays(float offset, Check check) {
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> r(-1.0f, 1.0f);
    std::uniform_real_distribution<float> weight(-0.3f, 1.3f);
    for (int i = 0; i < 2000; ++i) {
        const auto randomPoint = [&](float spread) {
            return TriVector(offset + spread * r(rng), spread * r(rng), spread * r(rng));
        };
        const Mesh mesh = MakeMesh({randomPoint(1.0f), randomPoint(1.0f), randomPoint(1.0f)}, {{0, 1, 2}});
        const TriVector origin = randomPoint(4.0f);
        const float u = weight(rng);
        const float v = weight(rng);
        const std::array<TriVector, 3> p{mesh.Vertices()[0].position, mesh.Vertices()[1].position,
                                         mesh.Vertices()[2].position};
        const TriVector target = p[0] + (p[1] - p[0]) * u + (p[2] - p[0]) * v;
        const TriVector direction = target - origin;

        const std::array<double, 3> reference = referenceBarycentric(mesh, origin, direction);
        const Vector plane = p[0] & p[1] & p[2];
        const double cosine = std::abs(dot(toDouble(TriVector(plane.e1(), plane.e2(), plane.e3(), 0.0f)),
                                           toDouble(direction))) / (plane.Norm() * direction.VNorm());
        const double nearestEdge = std::min({std::abs(reference[0]), std::abs(reference[1]),
                                             std::abs(1.0 - reference[0] - reference[1])});
        if (cosine < 0.1 || nearestEdge < 1e-3) continue;
        check(mesh, origin, direction, reference);
    }
}

void expectMatchesReference(const std::optional<Barycentric>& hit, const std::array<double, 3>& reference,
                            double tolerance) {
    const bool inside = reference[0] >= 0 && reference[1] >= 0 && reference[0] + reference[1] <= 1;
    ASSERT_EQ(hit.has_value(), inside);
    if (!hit) return;
    EXPECT_NEAR(hit->u, reference[0], tolerance);
    EXPECT_NEAR(hit->v, reference[1], tolerance);
    EXPECT_NEAR(hit->t, reference[2], tolerance * std::max(1.0, std::abs(reference[2])));
}

} // anonymous namespace

// Two triangles whose shared corners are separate vertices (e.g. with different normals)
TEST(MeshLinesTest, SharedEdgeIsStoredOnce) {
    const Mesh mesh = MakeMesh({TriVector(0, 0, 0), TriVector(1, 0, 0), TriVector(0, 1, 0),
                                TriVector(0, 1, 0), TriVector(1, 0, 0), TriVector(1, 1, 0)},
                               {{0, 1, 2}, {3, 4, 5}});
    const MeshEdges edges = BuildEdges(mesh);
    ASSERT_EQ(edges.edges.size(), 5u);
    // The first triangle runs from (1, 0, 0) to (0, 1, 0) opposite corner 0, the second the other way opposite corner 2
    const EdgeUse first = edges.triangles[0].opposite[0];
    const EdgeUse second = edges.triangles[1].opposite[2];
    EXPECT_EQ(first.edge, second.edge);
    EXPECT_NE(first.reversed, second.reversed);
}

TEST(MeshLinesTest, OppositeEdgeRunsBetweenTheOtherCorners) {
    const std::array corners{TriVector(0, 0, 0), TriVector(2, 0, 0), TriVector(0, 3, 0)};
    const MeshEdges edges = BuildEdges(MakeMesh({corners[0], corners[1], corners[2]}, {{0, 1, 2}}));
    for (size_t i = 0; i < 3; ++i) {
        const EdgeUse use = edges.triangles[0].opposite[i];
        const AnchoredEdge& edge = edges.edges[use.edge];
        const TriVector from = use.reversed ? edge.anchor + edge.direction : edge.anchor;
        const TriVector to = use.reversed ? edge.anchor : edge.anchor + edge.direction;
        EXPECT_EQ(from, corners[(i + 1) % 3]);
        EXPECT_EQ(to, corners[(i + 2) % 3]);
    }
}

// Four triangles around one edge, like two crossing quads
TEST(MeshLinesTest, EdgeSharedByFourTriangles) {
    const Mesh mesh = MakeMesh({TriVector(0, 0, 0), TriVector(0, 1, 0), TriVector(1, 0, 0), TriVector(-1, 0, 0),
                                TriVector(0, 0, 1), TriVector(0, 0, -1)},
                               {{0, 1, 2}, {1, 0, 3}, {0, 1, 4}, {1, 0, 5}});
    const MeshEdges edges = BuildEdges(mesh);
    EXPECT_EQ(edges.edges.size(), 9u);
    for (const TriangleEdges& triangle : edges.triangles) {
        EXPECT_EQ(triangle.opposite[2].edge, edges.triangles[0].opposite[2].edge);
    }
}

TEST(MeshLinesTest, EdgeTestMatchesMollerTrumbore) {
    forRandomRays(0.0f, [](const Mesh& mesh, const TriVector& origin, const TriVector& direction,
                           const std::array<double, 3>& reference) {
        expectMatchesReference(intersectEdges(BuildEdges(mesh), 0, origin, direction), reference, 1e-4);
    });
}

// Plücker coordinates relative to the origin lose precision far from it; anchored edges do not
TEST(MeshLinesTest, EdgeTestIsPreciseFarFromTheOrigin) {
    forRandomRays(1000.0f, [](const Mesh& mesh, const TriVector& origin, const TriVector& direction,
                              const std::array<double, 3>& reference) {
        expectMatchesReference(intersectEdges(BuildEdges(mesh), 0, origin, direction), reference, 1e-3);
    });
}

// A flat fan of triangles in a tilted plane, seen from its front, so that no edge is a silhouette.
// Rays aimed exactly at a shared edge or the shared vertex hit one of the triangles: they evaluate the
// same stored lines. (Möller-Trumbore in floats misses about a fifth of these rays.)
TEST(MeshLinesTest, EdgeTestIsWatertight) {
    constexpr uint32_t kSides = 7;
    const TriVector center(0.31f, 0.17f, -0.23f);
    const TriVector u(0.83f, 0.31f, 0.11f, 0.0f);
    const TriVector v(-0.17f, 0.29f, 0.79f, 0.0f);
    std::vector<TriVector> positions{center};
    std::vector<std::array<uint32_t, 3>> triangles;
    for (uint32_t i = 0; i < kSides; ++i) {
        const float angle = 2.0f * 3.14159265f * (static_cast<float>(i) + 0.3f * std::sin(5.0f * i)) / kSides;
        positions.push_back(center + u * std::cos(angle) + v * std::sin(angle));
        triangles.push_back({0, i + 1, (i + 1) % kSides + 1});
    }
    const MeshEdges edges = BuildEdges(MakeMesh(positions, triangles));
    const Vector plane = positions[0] & positions[1] & positions[2];
    const TriVector front = TriVector(plane.e1(), plane.e2(), plane.e3(), 0.0f) * (-1.0f / plane.Norm());

    std::mt19937 rng(3);
    std::uniform_real_distribution<float> r(-1.0f, 1.0f);
    std::uniform_real_distribution<float> along(0.0f, 1.0f);
    int misses = 0;
    for (int i = 0; i < 20000; ++i) {
        const AnchoredEdge& spoke = edges.edges[edges.triangles[i % kSides].opposite[1].edge];
        const TriVector target = i % 10 == 0 ? center : spoke.anchor + spoke.direction * along(rng);
        const TriVector origin = center + front * (2.0f + r(rng)) + u * r(rng) + v * r(rng);
        bool hit = false;
        for (size_t triangle = 0; triangle < kSides && !hit; ++triangle) {
            hit = intersectEdges(edges, triangle, origin, target - origin).has_value();
        }
        misses += hit ? 0 : 1;
    }
    EXPECT_EQ(misses, 0);
}
