// Constants and the GPU structs, laid out like engine/include/GpuScene.h: std430, and scalar block
// layout for the mesh records and BVH nodes

#ifndef COMMON_GLSL
#define COMMON_GLSL

#include "ShaderInterface.h"

const float PI = 3.14159265359;
const float INFINITY = 1e30;

const int SHADING_FLAT = 0;
const int SHADING_LAMBERT = 1;
const int SHADING_PHONG = 2;
const int SHADING_PBR = 3;

const int LIGHT_DIRECTIONAL = 0;
const int LIGHT_POINT = 1;
const int LIGHT_SPOT = 2;

struct Vertex {
    vec4 positionU;  // xyz: object-space position, w: texture u
    vec4 normalV;    // xyz: object-space normal, w: texture v
};

struct Triangle {
    uint indices[3];
    uint materialIndex;
};

struct Material {
    vec3 color;  // Linear
    int shadingMode;
    float metallic;
    float roughness;
    float shininess;
    float reflectivity;
    int textureIndex;  // -1: untextured
    float specular;
};

struct Sphere {
    vec3 center;
    float radius;
    uint materialIndex;
};

// dot(normal, x) + e0 = 0
struct Plane {
    vec3 normal;
    float e0;
    uint materialIndex;
};

struct Light {
    vec3 position;
    int type;
    vec3 direction;  // Direction the light travels
    float range;     // Falloff range^2 / (range^2 + distance^2)
    vec3 color;      // Linear color times intensity
    float spotCosOuter;
    float spotCosInner;
};

// Motor (rotor: s, e23, e31, e12; dual: e01, e02, e03, e0123), uniform scale and an optional mirror plane
struct MeshInstance {
    vec4 rotor;
    vec4 dual;
    vec4 mirror;
    float scale;
    uint meshId;
    uint hasMirror;
};

struct DebugLine {
    vec4 from;
    vec4 to;
    vec4 color;
};

// "A Fast and Robust Method for Avoiding Self-Intersection" (Wächter and Binder, Ray Tracing Gems
// ch. 6): moves a point off the surface by a few float ULPs, so the offset scales with the coordinates
vec3 offsetRayOrigin(vec3 position, vec3 normal) {
    const float originThreshold = 1.0 / 32.0;
    const float floatScale = 1.0 / 65536.0;
    const float intScale = 256.0;

    ivec3 intOffset = ivec3(intScale * normal);
    vec3 offsetPosition = vec3(
        intBitsToFloat(floatBitsToInt(position.x) + (position.x < 0.0 ? -intOffset.x : intOffset.x)),
        intBitsToFloat(floatBitsToInt(position.y) + (position.y < 0.0 ? -intOffset.y : intOffset.y)),
        intBitsToFloat(floatBitsToInt(position.z) + (position.z < 0.0 ? -intOffset.z : intOffset.z)));
    // Near the origin a ULP is tiny, so use a small absolute offset there instead
    return mix(offsetPosition, position + floatScale * normal, lessThan(abs(position), vec3(originThreshold)));
}

// What the closest-hit search records; surface attributes are only computed for the final hit
const uint HIT_NONE = 0u;
const uint HIT_SPHERE = 1u;
const uint HIT_PLANE = 2u;
const uint HIT_TRIANGLE = 3u;

struct Hit {
    float t;
    uint type;
    uint index;      // Sphere, plane or triangle
    uint instance;   // Triangles only
    vec2 barycentrics;
};

// An edge from anchor to anchor + direction, stored once for all triangles that share it
struct Edge {
    vec3 anchor;
    vec3 direction;
};

// The edges opposite the three corners, and the triangle for shading
struct EdgeTriangle {
    uint edges[3];
    uint triangle;
};

// The bounding box of everything below a node
struct Box {
    vec3 minimum;
    vec3 maximum;
};

// A BVH node pair: both children of an internal node. A child with count > 0 is a leaf with count
// records from index on, with count 0 an internal node whose pair is index, with count -1 missing.
struct NodePair {
    Box boxes[2];
    ivec2 index;
    ivec2 count;
};

#endif // COMMON_GLSL
