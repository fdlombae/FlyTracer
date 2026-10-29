#pragma once

#include "BVH.h"
#include "GameScene.h"
#include "MeshLines.h"
#include "Scene.h"
#include "ShaderInterface.h"
#include <array>
#include <cstddef>
#include <cstdint>

// GPU encodings of the scene, laid out to match the std430 structs in engine/shaders/common.glsl.
// Colors are converted to linear here, planes and motors are normalized.
namespace Gpu {

using Float3 = std::array<float, 3>;
using Float4 = std::array<float, 4>;

struct Vertex {
    Float3 position;
    float u;
    Float3 normal;
    float v;
};

struct Material {
    Float3 color;
    int32_t shadingMode;
    float metallic;
    float roughness;
    float shininess;
    float reflectivity;
    int32_t textureIndex;  // -1: untextured
    float specular;
    float _pad[2];
};

struct Sphere {
    Float3 center;
    float radius;
    uint32_t materialIndex;
    uint32_t _pad[3];
};

// n . x + e0 = 0 with |n| = 1
struct Plane {
    Float3 normal;
    float e0;
    uint32_t materialIndex;
    uint32_t _pad[3];
};

struct Light {
    Float3 position;
    int32_t type;
    Float3 direction;  // Normalized direction the light travels
    float range;
    Float3 color;      // Linear color times intensity
    float spotCosOuter;  // Cosine of the cone angle
    float spotCosInner;  // Cosine where the fade to the edge starts
    float _pad[3];
};

// A normalized motor as its rotor part (s, e23, e31, e12) and dual part (e01, e02, e03, e0123)
struct EncodedMotor {
    Float4 rotor;
    Float4 dual;
};

// A motor, plus the optional mirror plane (nx, ny, nz, e0)
struct MeshInstance {
    Float4 rotor;
    Float4 dual;
    Float4 mirror;
    float scale;
    uint32_t meshId;
    uint32_t hasMirror;
    uint32_t _pad;
};

struct DebugLine {
    Float4 from;
    Float4 to;
    Float4 color;  // Linear
};

// The mesh records and BVH nodes are tightly packed (scalar block layout); the records are in the
// BVH's leaf order

struct Edge {
    Float3 anchor;
    Float3 direction;
};

inline constexpr uint32_t kReversedEdge = REVERSED_EDGE;

// The edges opposite the corners, and the triangle (vertices and material) for shading
struct EdgeTriangle {
    std::array<uint32_t, 3> edges;
    uint32_t triangle;
};

struct Box {
    Float3 minimum;
    Float3 maximum;
};

// The children of an internal BVH node together, see ::NodePair
struct NodePair {
    std::array<Box, 2> boxes;
    std::array<int32_t, 2> index;  // First record of a leaf, or the pair of an internal node
    std::array<int32_t, 2> count;  // Records of a leaf, 0 for an internal node, -1 for no child
};

// The camera is a motor (see CameraMotor in Camera.h)
struct PushConstants {
    Float4 cameraRotor;
    Float4 cameraDual;
    float tanHalfFov;
    uint32_t sphereCount;
    uint32_t planeCount;
    uint32_t lightCount;
    uint32_t instanceCount;
    uint32_t debugLineCount;
    uint32_t sampleIndex;  // Samples already accumulated for this view
    uint32_t maxBounces;
    float exposure;
    uint32_t frameIndex;  // Counts every frame, so random numbers differ even while accumulation restarts
};

static_assert(sizeof(Vertex) == 32);
static_assert(sizeof(Material) == 48 && offsetof(Material, specular) == 36);
static_assert(sizeof(Sphere) == 32 && offsetof(Sphere, materialIndex) == 16);
static_assert(sizeof(Plane) == 32 && offsetof(Plane, materialIndex) == 16);
static_assert(sizeof(Light) == 64 && offsetof(Light, color) == 32 && offsetof(Light, spotCosOuter) == 44);
static_assert(sizeof(MeshInstance) == 64 && offsetof(MeshInstance, scale) == 48);
static_assert(sizeof(DebugLine) == 48);
static_assert(sizeof(Edge) == 24);
static_assert(sizeof(EdgeTriangle) == 16);
static_assert(sizeof(NodePair) == 64 && offsetof(NodePair, index) == 48);
static_assert(sizeof(PushConstants) == 72 && offsetof(PushConstants, frameIndex) == 68);
static_assert(sizeof(PushConstants) <= 128, "Vulkan only guarantees 128 bytes of push constants");

// sRGB transfer function, per channel
[[nodiscard]] float SrgbToLinear(float value) noexcept;

[[nodiscard]] Vertex Encode(const ::Vertex& vertex);
[[nodiscard]] Material Encode(const Scene::Material& material, int32_t textureIndex);
[[nodiscard]] Sphere Encode(const Scene::Sphere& sphere, uint32_t materialIndex);
[[nodiscard]] Plane Encode(const Scene::Plane& plane, uint32_t materialIndex);
[[nodiscard]] Light Encode(const Scene::Light& light);
[[nodiscard]] EncodedMotor Encode(const Motor& motor);
[[nodiscard]] MeshInstance Encode(const ::MeshInstance& instance);
[[nodiscard]] DebugLine Encode(const ::DebugLine& line);
[[nodiscard]] Edge Encode(const AnchoredEdge& edge);
// edgeOffset: where the mesh's edges start in the buffer of all meshes' edges
[[nodiscard]] EdgeTriangle Encode(const TriangleEdges& edges, uint32_t edgeOffset, uint32_t triangle);
// The offsets of the mesh's first pair and first record in the buffers of all meshes
[[nodiscard]] NodePair Encode(const ::NodePair& pair, int32_t pairOffset, int32_t recordOffset);

} // namespace Gpu
