// The interface between the engine and engine/shaders/raytracer.comp: descriptor bindings and limits.
// C++ and GLSL both include this file, so it only holds #defines.

#ifndef SHADER_INTERFACE_H
#define SHADER_INTERFACE_H

#define BINDING_ACCUMULATION_IMAGE 0
#define BINDING_OUTPUT_IMAGE 1
#define BINDING_VERTICES 2
#define BINDING_TRIANGLES 3
#define BINDING_EDGES 4
#define BINDING_EDGE_TRIANGLES 5
#define BINDING_MESH_PAIRS 6
#define BINDING_MESH_ROOT_PAIRS 7
#define BINDING_MATERIALS 8
#define BINDING_SPHERES 9
#define BINDING_PLANES 10
#define BINDING_LIGHTS 11
#define BINDING_INSTANCES 12
#define BINDING_SCENE_PAIRS 13
#define BINDING_DEBUG_LINES 14
#define BINDING_TEXTURES 15
#define BINDING_COUNT 16

#define MAX_TEXTURES 64            // Texture slots; slot 0 is white, for untextured materials
#define BVH_STACK_SIZE 32          // Entries of the traversal stack; BuildBVH keeps the depth below it
#define WORKGROUP_SIZE 8           // Pixels along each side of a compute workgroup
#define REVERSED_EDGE 0x80000000u  // Bit 31 of an edge index: the triangle runs through that edge reversed

#endif // SHADER_INTERFACE_H
