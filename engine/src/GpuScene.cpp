#include "GpuScene.h"
#include "PGA.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Gpu {

namespace {

Float3 toLinear(const Scene::Color& color, float scale = 1.0f) noexcept {
    return {SrgbToLinear(color.r) * scale, SrgbToLinear(color.g) * scale, SrgbToLinear(color.b) * scale};
}

Float3 toArray(const TriVector& point) noexcept {
    return {point.e032(), point.e013(), point.e021()};
}

Float4 toArray4(const TriVector& point) noexcept {
    return {point.e032(), point.e013(), point.e021(), 0.0f};
}

} // anonymous namespace

float SrgbToLinear(float value) noexcept {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

Vertex Encode(const ::Vertex& vertex) {
    return {toArray(vertex.position), vertex.texCoord[0], toArray(vertex.normal), vertex.texCoord[1]};
}

Material Encode(const Scene::Material& material, int32_t textureIndex) {
    return {
        .color = toLinear(material.color),
        .shadingMode = static_cast<int32_t>(material.shadingMode),
        .metallic = material.metallic,
        .roughness = material.roughness,
        .shininess = material.shininess,
        .reflectivity = material.reflectivity,
        .textureIndex = textureIndex,
        .specular = material.specular,
    };
}

Sphere Encode(const Scene::Sphere& sphere, uint32_t materialIndex) {
    return {.center = toArray(Euclidean(sphere.center)), .radius = sphere.radius, .materialIndex = materialIndex};
}

Plane Encode(const Scene::Plane& plane, uint32_t materialIndex) {
    if (plane.plane.Norm() < 1e-12f) {
        throw std::invalid_argument("A plane needs a normal, the plane at infinity cannot be drawn");
    }
    const Vector p = plane.plane.Normalized();
    return {.normal = {p.e1(), p.e2(), p.e3()}, .e0 = p.e0(), .materialIndex = materialIndex};
}

Light Encode(const Scene::Light& light) {
    const float length = light.direction.VNorm();
    if (light.type != Scene::LightType::Point && length < 1e-12f) {
        throw std::invalid_argument("Directional and spot lights need a direction");
    }
    const float softness = std::clamp(light.spotSoftness, 0.0f, 1.0f);
    return {
        .position = toArray(Euclidean(light.position)),
        .type = static_cast<int32_t>(light.type),
        .direction = length >= 1e-12f ? toArray(light.direction * (1.0f / length)) : Float3{},
        .range = light.range,
        .color = toLinear(light.color, light.intensity),
        .spotCosOuter = std::cos(light.spotAngle * DEG_TO_RAD),
        .spotCosInner = std::cos(light.spotAngle * DEG_TO_RAD * (1.0f - softness)),
    };
}

EncodedMotor Encode(const Motor& motor) {
    const Motor m = NormalizedMotor(motor);
    return {{m.s(), m.e23(), m.e31(), m.e12()}, {m.e01(), m.e02(), m.e03(), m.e0123()}};
}

MeshInstance Encode(const ::MeshInstance& instance) {
    const auto [rotor, dual] = Encode(instance.transform);
    MeshInstance gpu{.rotor = rotor, .dual = dual, .scale = instance.scale, .meshId = instance.meshId};
    if (instance.mirror) {
        const Plane mirror = Encode(Scene::Plane{*instance.mirror, {}}, 0);
        gpu.mirror = {mirror.normal[0], mirror.normal[1], mirror.normal[2], mirror.e0};
        gpu.hasMirror = 1;
    }
    return gpu;
}

DebugLine Encode(const ::DebugLine& line) {
    const Float3 color = toLinear(line.color);
    return {toArray4(line.from), toArray4(line.to), {color[0], color[1], color[2], 1.0f}};
}

Edge Encode(const AnchoredEdge& edge) {
    return {toArray(edge.anchor), toArray(edge.direction)};
}

EdgeTriangle Encode(const TriangleEdges& edges, uint32_t edgeOffset, uint32_t triangle) {
    EdgeTriangle gpu{.triangle = triangle};
    for (size_t i = 0; i < 3; ++i) {
        const uint32_t index = edges.opposite[i].edge + edgeOffset;
        if (index >= kReversedEdge) {
            throw std::length_error("Too many mesh edges");
        }
        gpu.edges[i] = edges.opposite[i].reversed ? index | kReversedEdge : index;
    }
    return gpu;
}

// A leaf's index refers to records, an internal node's to pairs
NodePair Encode(const ::NodePair& pair, int32_t pairOffset, int32_t recordOffset) {
    NodePair gpu{};
    for (size_t i = 0; i < 2; ++i) {
        const PairChild& child = pair[i];
        gpu.boxes[i] = {child.box.min, child.box.max};
        gpu.index[i] = child.count > 0 ? child.index + recordOffset : child.count == 0 ? child.index + pairOffset : -1;
        gpu.count[i] = child.count;
    }
    return gpu;
}

} // namespace Gpu
