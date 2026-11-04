// Descriptor bindings (numbered in ShaderInterface.h) and push constants (laid out like GpuScene.h)

#ifndef BINDINGS_GLSL
#define BINDINGS_GLSL

layout(binding = BINDING_ACCUMULATION_IMAGE, rgba32f) uniform image2D accumulationImage;  // Linear radiance, averaged over samples
layout(binding = BINDING_OUTPUT_IMAGE, rgba8) uniform writeonly image2D outputImage;  // Tone-mapped, sRGB encoded

// Static mesh data: all meshes concatenated
layout(binding = BINDING_VERTICES) readonly buffer Vertices { Vertex vertices[]; };
layout(binding = BINDING_TRIANGLES) readonly buffer Triangles { Triangle triangles[]; };
layout(binding = BINDING_EDGES, scalar) readonly buffer Edges { Edge edges[]; };
layout(binding = BINDING_EDGE_TRIANGLES, scalar) readonly buffer EdgeTriangles { EdgeTriangle edgeTriangles[]; };  // Leaf order
layout(binding = BINDING_MESH_PAIRS, scalar) readonly buffer MeshPairs { NodePair meshPairs[]; };              // Object space
layout(binding = BINDING_MESH_ROOT_PAIRS) readonly buffer MeshRootPairs { uint meshRootPairs[]; };

// Scene data, updated every frame
layout(binding = BINDING_MATERIALS) readonly buffer Materials { Material materials[]; };
layout(binding = BINDING_SPHERES) readonly buffer Spheres { Sphere spheres[]; };
layout(binding = BINDING_PLANES) readonly buffer Planes { Plane planes[]; };
layout(binding = BINDING_LIGHTS) readonly buffer Lights { Light lights[]; };
layout(binding = BINDING_INSTANCES) readonly buffer Instances { MeshInstance instances[]; };  // In top-level leaf order
layout(binding = BINDING_SCENE_PAIRS, scalar) readonly buffer ScenePairs { NodePair scenePairs[]; };  // World space
layout(binding = BINDING_DEBUG_LINES) readonly buffer DebugLines { DebugLine debugLines[]; };

layout(binding = BINDING_TEXTURES) uniform sampler2D textures[MAX_TEXTURES];  // sRGB images, sampled as linear

layout(push_constant) uniform PushConstants {
    vec4 cameraRotor;  // The camera motor (see CameraMotor in Camera.h): s, e23, e31, e12
    vec4 cameraDual;   // e01, e02, e03, e0123
    float tanHalfFov;
    uint sphereCount;
    uint planeCount;
    uint lightCount;
    uint instanceCount;
    uint debugLineCount;
    uint sampleIndex;  // Samples already accumulated for this view
    uint maxBounces;
    float exposure;
    uint frameIndex;   // Counts every frame, so random numbers differ even while accumulation restarts
} pc;

#endif // BINDINGS_GLSL
