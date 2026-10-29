#pragma once

#include "FlyFish.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Scene {

// ============================================================================
// Color - RGB in sRGB, like colors picked in an image editor.
// The renderer converts to linear before lighting.
// ============================================================================
struct Color {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};

    constexpr Color() noexcept = default;
    constexpr Color(float r_, float g_, float b_) noexcept : r(r_), g(g_), b(b_) {}

    static constexpr Color White() noexcept { return {1.0f, 1.0f, 1.0f}; }
    static constexpr Color Black() noexcept { return {0.0f, 0.0f, 0.0f}; }
    static constexpr Color Red() noexcept { return {1.0f, 0.0f, 0.0f}; }
    static constexpr Color Green() noexcept { return {0.0f, 1.0f, 0.0f}; }
    static constexpr Color Blue() noexcept { return {0.0f, 0.0f, 1.0f}; }
    static constexpr Color Yellow() noexcept { return {1.0f, 1.0f, 0.0f}; }
    static constexpr Color Cyan() noexcept { return {0.0f, 1.0f, 1.0f}; }
    static constexpr Color Magenta() noexcept { return {1.0f, 0.0f, 1.0f}; }
    static constexpr Color Gray(float v = 0.5f) noexcept { return {v, v, v}; }
};

// ============================================================================
// Shading models, from simplest to physically based
// ============================================================================
enum class ShadingMode : int32_t {
    Flat = 0,     // Unlit color
    Lambert = 1,  // Diffuse only
    Phong = 2,    // Diffuse + normalized Blinn-Phong highlight (weighted by specular) + Whitted mirror reflection
    PBR = 3       // Cook-Torrance GGX with metallic/roughness, reflections weighted by Fresnel
};

// ============================================================================
// Material - shared by spheres, planes and meshes
// ============================================================================
struct Material {
    ShadingMode shadingMode{ShadingMode::Lambert};
    Color color{1.0f, 1.0f, 1.0f};
    float metallic{0.0f};      // PBR: 0 = dielectric, 1 = metal
    float roughness{0.5f};     // PBR: 0 = mirror, 1 = fully rough
    float shininess{32.0f};    // Phong: highlight exponent
    float reflectivity{0.0f};  // Phong: fraction of light mirrored
    float specular{0.5f};      // Phong: fraction of light in the highlight, the rest is diffuse
    std::string texturePath;   // Meshes only: multiplies color, sampled with the mesh UVs

    static Material Flat(const Color& col) { return {.shadingMode = ShadingMode::Flat, .color = col}; }
    static Material Lambert(const Color& col) { return {.shadingMode = ShadingMode::Lambert, .color = col}; }

    static Material Phong(const Color& col, float shininess = 32.0f, float reflectivity = 0.0f,
                          float specular = 0.5f) {
        return {.shadingMode = ShadingMode::Phong, .color = col, .shininess = shininess,
                .reflectivity = reflectivity, .specular = specular};
    }

    static Material PBR(const Color& col, float metallic = 0.0f, float roughness = 0.5f) {
        return {.shadingMode = ShadingMode::PBR, .color = col, .metallic = metallic, .roughness = roughness};
    }

    static Material Metal(const Color& col, float roughness = 0.3f) { return PBR(col, 1.0f, roughness); }
    static Material Matte(const Color& col) { return PBR(col, 0.0f, 1.0f); }
    static Material Glossy(const Color& col, float roughness = 0.2f) { return PBR(col, 0.0f, roughness); }
};

// ============================================================================
// Primitives
// ============================================================================
struct Sphere {
    TriVector center{0.0f, 0.0f, 0.0f};
    float radius{1.0f};
    Material material;
};

// Plane e1*x + e2*y + e3*z + e0 = 0, the side its normal (e1, e2, e3) points to is the front
struct Plane {
    Vector plane{0.0f, 0.0f, 1.0f, 0.0f};
    Material material;
};

// ============================================================================
// Lights
// ============================================================================
enum class LightType : int32_t {
    Directional = 0,
    Point = 1,
    Spot = 2
};

struct Light {
    LightType type{LightType::Point};
    TriVector position{0.0f, 0.0f, 0.0f};          // Point and spot lights
    TriVector direction{0.0f, -1.0f, 0.0f, 0.0f};  // Direction the light travels (directional and spot)
    Color color;
    float intensity{1.0f};
    // Point and spot lights fall off with range^2 / (range^2 + distance^2): half intensity at the
    // range, inverse square beyond it and finite at the light
    float range{10.0f};
    float spotAngle{30.0f};     // Degrees from the spot axis to the edge of the cone
    float spotSoftness{0.2f};   // Fraction of the cone that fades out

    static Light Directional(const TriVector& direction, const Color& color, float intensity) {
        return {.type = LightType::Directional, .direction = direction, .color = color, .intensity = intensity};
    }

    static Light Point(const TriVector& position, const Color& color, float intensity, float range) {
        return {.type = LightType::Point, .position = position, .color = color, .intensity = intensity,
                .range = range};
    }

    static Light Spot(const TriVector& position, const TriVector& direction, const Color& color,
                      float intensity, float range, float angle, float softness = 0.2f) {
        return {.type = LightType::Spot, .position = position, .direction = direction, .color = color,
                .intensity = intensity, .range = range, .spotAngle = angle, .spotSoftness = softness};
    }
};

// ============================================================================
// Scene data container
// ============================================================================
struct SceneData {
    std::vector<Sphere> spheres;
    std::vector<Plane> planes;
    std::vector<Light> lights;
};

} // namespace Scene
