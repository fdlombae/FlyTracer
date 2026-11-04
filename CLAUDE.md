# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.
- All generated code should be clean and readable.
- Don't add unnecessary comments.
- Write in the style of Josuttis
- Add simple unit tests for new code
- Don't add unnecessary code, dependencies, build steps or requirements
- Use the same coding style as the rest of the project
- Use the same naming conventions as the rest of the project
- Use the same directory structure as the rest of the project
- Use FlyFish Library as much as possible for calculations
- During development, always end your task by compiling and running the project. If applicable, take screenshots of the output.
- Ask clarification on prompts if you are unsure.
- Only add debug information to the project when requested by the instructor.

## Project Overview

FlyTracer is a real-time Vulkan 1.4 compute shader raytracer written in C++23 for educational purposes at DAE. It uses Projective Geometric Algebra (PGA) via the header-only FlyFish library for 3D transformations.
It needs to compile cross platform through cmake on both Windows and MacOs.

## Build Commands

```bash
# Configure (defaults to Release with LTO)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Debug build
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build all targets (executable, shaders, resources, tests)
cmake --build build

# Run (from any directory: shaders and resources are resolved next to the executable)
./build/bin/FlyTracer
./build/bin/FlyTracer --scene debug
./build/bin/FlyTracer --scene testbox
./build/bin/FlyTracer --scene bireflection   # also birot, bitrans, quaternion, plucker, hornet
./build/bin/FlyTracer --no-vsync --fullscreen

# Benchmark: every scene headless, fixed time step, default 200 frames at 1920 x 1080 pixels
# (compare at 1080p or more; smaller sizes are dominated by fixed overhead)
./build/bin/FlyTracer --benchmark 200 --width 1920 --height 1080 --benchmark-csv results.csv

# Run all tests via CTest
ctest --test-dir build

# Run individual test executables
./build/bin/FlyTracer_Test        # BVH, PGA, camera, input, GPU encoding, GPU timing, mesh, mesh measure and mesh line tests
./build/bin/FlyTracer_ConfigTest  # Config tests
./build/bin/FlyTracer_ShaderTest  # flyfish.glsl against FlyFish, and the GLSL structs, bindings and push constants against GpuScene.h, on the GPU (tests/shaders/)
./build/bin/FlyTracer_SceneTest   # Golden images of every game scene (needs a GPU); references in tests/reference/scenes, delete one to re-record
./build/bin/FlyTracer_RenderTest  # Golden image and benchmark tests (need a GPU); delete tests/reference/render_scene.ppm to re-record

# Validation (Debug builds): synchronization and GPU-assisted validation
VK_LAYER_VALIDATE_SYNC=1 ./build-debug/bin/FlyTracer
FLYTRACER_GPU_VALIDATION=1 ./build-debug/bin/FlyTracer

# Visual Studio (Windows)
cmake -B build -G "Visual Studio 17 2022"
```

Requires: CMake 3.20+, C++23 compiler, Vulkan SDK 1.4 with `glslc` shader compiler. On macOS the KosmicKrisp driver (SDK 1.4.363+, Apple Silicon, macOS 26+) is required; MoltenVK is not supported. Dependencies (SDL3, tinyobjloader, Dear ImGui, Google Test, FlyFish) are fetched automatically via CMake FetchContent. Only `FlyFish.h` is downloaded, pinned to a commit in `CMakeLists.txt`; use `-DFETCHCONTENT_SOURCE_DIR_FLYFISH=<FlyFish>/src` to build against a local FlyFish checkout.

## Architecture

### Engine/Game Split

- **`engine/`** — Two static libraries: `FlyTracerCore` (CPU only, no Vulkan: scenes, meshes, BVHs, the encoding of the scene for the GPU; the unit tests link only this) and `FlyTracerEngine` (window, Vulkan renderer, ImGui, benchmark, on top of the core), plus the shaders.
- **`game/`** — Executable that links against the engine. Contains scene implementations and the `main()` entry point with CLI arg parsing.
- **FlyFish** — Header-only PGA math library (fetched by CMake) providing Motor, TriVector, BiVector, Vector types.

### Core Classes

- **`Application`** — Owns the SDL3 window (resizable, high pixel density), main loop and input mapping. Creates the `VulkanRenderer` and drives the `GameScene` each frame: `HandleInput`, `Update`, `DrawGui`, render.
- **`GameScene`** (abstract base) — Non-virtual engine interface (`Init`, `HandleInput`, `Update`, `DrawGui`, `Shutdown`) wrapping protected virtual hooks (`OnInit`, `OnInput`, `OnUpdate`, `OnGui`, `OnShutdown`). Scene data is private; derived scenes use the protected API: `SetCamera`, `LoadMesh`/`AddMesh`, `AddMeshInstance`, `AddSphere`, `AddPlane`, lights, debug drawing.
- **`Scene` types** (`Scene.h`) — `Material`, `Sphere`, `Plane`, `Light` in PGA types; colors are sRGB. `MeshInstance` (in `GameScene.h`) is a motor, uniform scale and optional mirror plane.
- **`Input`** — Key and mouse state with `IsDown` / `WasPressed` edge detection. `OrbitCamera` implements the usual right-drag orbit and scroll zoom with motors.
- **`Mesh`** — OBJ loading via tinyobjloader, normals, UVs, and its bottom-level `BVH` (`BuildBVH` in `BVH.h`, binned SAH, depth limited to the shader's stack). Measures after "Clean up your Mesh!" (De Keninck et al. 2025): `Volume`, `CenterOfMass`, `Inertia` (principal moments and a rotor, Jacobi with rotors) and `IsClosed` (welded edges run through equally often both ways).
- **`BVH.h`** — besides the SAH builder: `PairChildren`, which turns a BVH into node pairs holding both children's boxes, the GPU's node layout.
- **`MeshLines`** — `BuildEdges` welds vertices by position and stores every edge once as an anchored line (anchor point + direction), so the Plücker edge test is watertight and stays precise far from the origin.
- **`Camera.h`** — Eye-target-up camera; `CameraMotor` is the motor from the camera's own frame (looking down -z, +y up) to the world, built from joins, which the shader gets; `ViewRay` uses it for picking.
- **`SceneEncoding`** — The scene encoded for the GPU on the CPU, unit-testable: `EncodeMeshes` (all meshes concatenated, plus a `MeshTable`), `MeshesChanged` (meshes added or changed in counts or textures: `RaytracingPass::Record` uploads them again) and `EncodeFrame` (materials, primitives, lights, the drawn instances and the top-level BVH, debug lines; mesh materials are read every frame).
- **`ShaderInterface.h`** — `#define`s that C++ and GLSL both include: binding numbers, texture limit, BVH stack size, workgroup size, the reversed-edge bit. `FlyTracer_ShaderTest` checks the GLSL structs against `GpuScene.h`.
- **Vulkan side** — `VulkanContext` (instance, device, single queue; requires scalar block layout), `Swapchain` (recreated on resize), `ImGuiLayer`, `RaytracingPass` (compute pipeline, images, buffers and textures, uploads of the encoded scene, accumulation, `GpuTimer` timestamps around the dispatch) and `VulkanRenderer` (frames in flight, blit, submit, present). `VulkanHelpers` holds the small Vulkan building blocks (owning handles, buffers, images, descriptor and pipeline helpers, `enumerate` for the count-then-fill queries); its `CommandSubmitter` records, submits and waits for one-time command buffers (uploads, benchmark, tests). `Benchmark` renders scenes headless and reports GPU timestamp times (n/a on KosmicKrisp, which gives all timestamps in a command buffer the same value) and submit-to-completion times. `GpuScene` encodes scene types into the GPU structs (sRGB to linear, normalized motors and planes; mesh records and node pairs tightly packed).

### Rendering Pipeline

The raytracer runs as one Vulkan compute shader (`engine/shaders/raytracer.comp`), written in PGA with `flyfish.glsl` (see `docs/pga-tracer.md`). Whitted-style: direct light with shadow rays at every hit, then mirror or glossy reflections. The camera is a motor, rays are points with directions, instances move them into object space with sandwiches, triangles are tested with the Plücker products of their anchored edges (watertight), mirror bounces reflect in the tangent plane, the GGX tangent frame is a rotor. Each frame adds one jittered sample to a running average in an `rgba32f` accumulation image (reset when the camera or scene data changes), tone maps it (ACES) and writes sRGB to an `rgba8` output image, which is blitted to the (UNORM) swapchain before ImGui draws on top (dynamic rendering). Supporting GLSL files:
- `flyfish.glsl` — FlyFish's `Vector`, `BiVector`, `TriVector`, `Motor` with `join` (&), `meet` (^), `inner` (|), `geometricProduct`, `sandwich`, `reverse`, `reflectInPlane`, same coefficients and signs as FlyFish.h
- `common.glsl` — Constants and GPU structs (includes `engine/include/ShaderInterface.h`)
- `ray.glsl` — The ray, `relativeTo` (origin split) and `rayParameter` (where a ray meets a plane)
- `bindings.glsl` — Descriptor bindings and push constants
- `random.glsl` — PCG hash random numbers
- `intersections.glsl` — Ray-sphere/plane/box tests and the anchored Plücker edge test
- `traversal.glsl` — Two-level BVH traversal over node pairs (top level over instances, bottom level per mesh)
- `brdf.glsl` — Sky, BRDFs (Flat, Lambert, normalized Blinn-Phong, Cook-Torrance), GGX sampling in the tangent frame
- `shading.glsl` — Surfaces (tangent planes, shadow terminator, texture level of detail from planes), lights, shadows, reflections
- `output.glsl`, `debug_lines.glsl` — Tone mapping and sRGB encoding, the debug line overlay

### PGA Math (FlyFish)

Transforms use PGA Motors (dual quaternions encoding rotation + translation), always applied as sandwich products `M X ~M`, also on the GPU (no matrices):
- **TriVector** — 3D point (`e032`=x, `e013`=y, `e021`=z, `e123`=w; w = 0 for directions)
- **Vector** — Plane `e1*x + e2*y + e3*z + e0 = 0`
- **BiVector** — Line in Plucker coordinates, e.g. the join `p & q` of two points
- **Motor** — Rotation + translation; `T * R` applies `R` first. Helpers in `PGA.h`: `TranslationMotor`, `ApplyMotor` (points and planes), `ReflectInPlane`, `ReflectionMotor`, `MotorPower`, `NormalizedMotor`.
- Two reflections compose with FlyFish's geometric product: reflecting in `a` and then `b` is `b * a` (`ReflectionMotor(a, b)`).

### GPU Buffer Strategy

- Static (uploaded again when the meshes change, see `MeshesChanged`): all meshes' vertices and triangles (for shading), anchored edges, triangle edge records in BVH leaf order and bottom-level node pairs concatenated, with per-mesh root pair offsets; textures as sRGB images with mipmaps in a sampler array.
- Per frame in flight (host-visible, grown on demand): materials (meshes, then spheres and planes), spheres, planes, lights, instances in top-level leaf order, top-level node pairs, debug lines.
- Instances are sent as motors (rotor + dual part), scale and mirror plane.

### Adding a New Scene

1. Create header in `game/include/` and source in `game/src/` inheriting from `GameScene`, overriding the protected hooks (`OnInit()`, `OnInput(const Input&)`, `OnUpdate(float)`, `OnGui()`)
2. Add the `.cpp` to the executable sources in `CMakeLists.txt`
3. Register the scene name in `game/src/SceneRegistry.cpp` (`kSceneNames` and `CreateScene`), which `--scene`, the benchmark and `FlyTracer_SceneTest` use (record its reference image by running the test once)
