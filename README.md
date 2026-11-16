# FlyTracer

A real-time Vulkan compute shader raytracer built for educational purposes at DAE. Uses Projective Geometric Algebra (PGA) via the [FlyFish](https://github.com/fdlombae/FlyFish) library for 3D transformations.

*Disclaimer: Most of this code is generated with Claude Code*

## Features

- **Vulkan 1.4 Compute Shader Raytracing** - Whitted-style ray tracing with shadows and mirror/glossy reflections, accumulated over frames for anti-aliasing
- **PGA throughout** - Instances are motors (and mirror planes), applied as sandwich products on the CPU and in the shader
- **Raytracing in PGA** - Camera motor, watertight Plücker edge tests and plane reflections, written with `flyfish.glsl`; `--benchmark` times every scene
- **Multiple Shading Models** - Flat, Lambert, Phong, and PBR materials
- **Two-level BVH** - A top-level BVH over mesh instances, a bottom-level BVH per mesh
- **OBJ Mesh Loading** - Load 3D models with materials via tinyobjloader
- **Texture Support** - Mipmapped sRGB textures, filtered with ray cones
- **Linear Lighting** - sRGB inputs converted to linear, ACES tone mapping
- **Scene Primitives** - Spheres, planes, and triangle meshes
- **ImGui Integration** - Debug UI and depth-tested debug lines
- **Cross-Platform** - macOS (KosmicKrisp) and Windows

## Requirements

- CMake 3.20+
- C++23 compiler (Clang 14+, GCC 13+, or Visual Studio 2022 17.14+)
- A GPU and driver with Vulkan 1.4, and the Vulkan SDK 1.4 with the glslc shader compiler (macOS: SDK 1.4.363+ with the KosmicKrisp driver, Apple Silicon and macOS 26 or later)

## Building

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run (from any directory)
./build/bin/FlyTracer                      # The test box scene
./build/bin/FlyTracer --scene debug        # The debug drawing demo
./build/bin/FlyTracer --render-scale 0.5   # Raytrace at half the window's resolution: faster on slow GPUs
./build/bin/FlyTracer --help
```

The **Renderer** window in the bottom left corner shows the frame time. Under *Settings* it changes the number of
bounces, the exposure and the render scale while the program runs. **F5** compiles `engine/shaders/raytracer.comp`
again and reloads it, so shader changes show without restarting; compile errors are printed to the console and the
running shader stays.

### Visual Studio

Open the folder with CMake support or generate a solution:

```bash
cmake -B build -G "Visual Studio 17 2022"
```

## Writing a Scene

A scene derives from `GameScene` and overrides the hooks it needs. The engine calls `OnInit` once, and then
`OnInput`, `OnUpdate` and `OnGui` every frame. A scene only describes what is in the world; the renderer does the rest.

```cpp
// game/include/MyScene.h
#pragma once
#include "GameScene.h"
#include "OrbitCamera.h"

class MyScene final : public GameScene {
public:
    explicit MyScene(const std::string& resourceDir) : GameScene(resourceDir) {}

protected:
    void OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnGui() override;

private:
    OrbitCamera m_orbit{TriVector::Point(0.0f, 2.0f, 0.0f), 0.0f, 0.3f, 20.0f, 5.0f, 60.0f};
    uint32_t m_sphereId{0};
    float m_time{0.0f};
};
```

```cpp
// game/src/MyScene.cpp
#include "MyScene.h"
#include <cmath>
#include <imgui.h>

void MyScene::OnInit() {
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));
    m_sphereId = AddSphere(TriVector::Point(5.0f, 1.0f, 0.0f), 1.0f, Scene::Material::Metal(Scene::Color::Red()));

    const uint32_t meshId = LoadMesh("pheasant.obj", "pheasant.png");  // From game/resources
    const uint32_t pheasantId = AddMeshInstance(meshId, TriVector::Point(0.0f, 0.0f, 0.0f), "pheasant");
    GetInstance(pheasantId)->scale = 0.25f;

    AddPointLight(TriVector::Point(5.0f, 10.0f, 5.0f), Scene::Color::White(), 1.5f, 50.0f);
}

void MyScene::OnInput(const Input& input) {
    m_orbit.HandleInput(input);  // Right-drag orbits, scrolling zooms
    SetCamera(m_orbit.Eye(), m_orbit.target);
    if (input.WasPressed(Key::R)) m_time = 0.0f;
}

void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;
    const BiVector yAxis = BiVector::LineFromPoints(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    const Motor rotation = Motor::Rotation(Angle::FromRadians(m_time), yAxis);

    // Every transformation is a sandwich with a motor: M X ~M
    if (Scene::Sphere* sphere = GetSphere(m_sphereId)) {
        sphere->center = ApplyMotor(rotation, TriVector::Point(5.0f, 1.0f, 0.0f));
    }
    if (MeshInstance* pheasant = FindInstance("pheasant")) {
        const Motor hop = Motor::Translation(0.0f, std::abs(std::sin(3.0f * m_time)), 0.0f);
        pheasant->transform = hop * rotation;  // The rightmost motor acts first: turn, then hop
    }
    DrawDebugLine(yAxis, 5.0f, Scene::Color::Yellow());  // Debug lines are cleared before every OnUpdate
}

void MyScene::OnGui() {
    ImGui::Begin("My Scene");
    ImGui::Text("FPS: %.1f", GetFPS());
    ImGui::End();
}
```

To run it, add `game/src/MyScene.cpp` to `GAME_SCENE_SOURCES` in `CMakeLists.txt` and one line to the table in
`game/src/SceneRegistry.cpp`:

```cpp
{"myscene", create<MyScene>},
```

Then `./build/bin/FlyTracer --scene myscene` starts it. `TestBoxScene` and `DebugDemoScene` are complete examples.

### The Scene API

| What | Functions (all in `engine/include/GameScene.h`) |
|------|-----------|
| Camera | `SetCamera(eye, target, up)`, `SetCameraFov`, `SetExposure`; `OrbitCamera` for mouse control |
| Spheres and planes | `AddSphere`, `AddPlane`, `AddGroundPlane`, `GetSphere`, `GetPlane` |
| Meshes | `LoadMesh`, `AddMesh`, `AddMeshInstance`, `GetInstance`, `FindInstance`, `SetInstanceTransform`, `RemoveMeshInstance` |
| Lights | `AddPointLight`, `AddDirectionalLight`, `AddSpotLight`, `GetLight`, `RemoveLight` |
| Materials | `Scene::Material::Flat`, `Lambert`, `Phong`, `PBR`, `Metal`, `Matte`, `Glossy` (in `Scene.h`); colors are sRGB |
| Debug drawing | `DrawDebugLine`, `DrawDebugPoint`, `DrawDebugAxes` |
| Input | `input.IsDown(Key::W)`, `input.WasPressed(Key::Space)`, `input.MouseDeltaX()`, ... (in `Input.h`) |

The `Get...` and `FindInstance` functions return a pointer to change the object through, or `nullptr` for an unknown
id or name. Don't keep these pointers: adding or removing an object of the same kind invalidates them.

### PGA in a Nutshell

The math types come from [FlyFish](https://github.com/fdlombae/FlyFish) (namespace `flyfish`; `PGA.h` brings them into scope):

| Type | Is a | Made with |
|------|------|-----------|
| `TriVector` | Point or direction | `TriVector::Point(x, y, z)`, `TriVector::Direction(x, y, z)` |
| `Vector` | Plane `a*x + b*y + c*z + d = 0` | `Vector::Plane(a, b, c, d)` |
| `BiVector` | Line | `p & q` (the join of two points), `BiVector::LineFromPoints(...)` |
| `Motor` | Rotation and translation | `Motor::Rotation(Angle::FromDegrees(90.0f), line)`, `Motor::Translation(x, y, z)` |

`a & b` is the join (the line through two points, the plane through a line and a point), `a ^ b` the meet (where two
elements intersect) and `a * b` the geometric product. Motors compose with `*`, and the rightmost one acts first.
`PGA.h` adds small helpers: `ApplyMotor` (points, lines and planes), `ReflectInPlane`, `ReflectionMotor` (two
reflections make a motor), `MotorPower` (a fraction of a motion, for animations), `ClosestPointOnLine` and
`ClosestPointOnPlane`. A mirrored copy of a mesh instance is `AddMeshInstance(instance->Reflected(plane))`.

## Project Structure

```
FlyTracer/
├── engine/
│   ├── include/       # Engine headers
│   ├── src/           # Engine implementation
│   └── shaders/       # GLSL compute shaders
├── game/
│   ├── include/       # Game-specific headers
│   ├── src/           # Game implementation
│   └── resources/     # Models, textures, config
└── tests/             # Unit tests
```

## Dependencies

Fetched automatically via CMake FetchContent:
- [SDL3](https://github.com/libsdl-org/SDL) - Windowing and input
- [Dear ImGui](https://github.com/ocornut/imgui) - Debug UI
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) - OBJ loading
- [Google Test](https://github.com/google/googletest) - Unit testing
- [FlyFish](https://github.com/fdlombae/FlyFish) - PGA math (single header)
