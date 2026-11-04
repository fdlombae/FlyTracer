# Getting Started

## Requirements

- **CMake** 3.20 or higher
- **C++23 compiler** - Clang, GCC 13+, or MSVC 2022
- **Vulkan SDK 1.4** - With `glslc` shader compiler. On macOS: SDK 1.4.363+ with the KosmicKrisp driver selected (Apple Silicon, macOS 26 or later)

## Building

### Command Line

```bash
# Clone the repository
git clone https://github.com/fdlombae/FlyTracer.git
cd FlyTracer

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run (from any directory: shaders and resources are found next to the executable)
./build/bin/FlyTracer
./build/bin/FlyTracer --scene testbox
```

### Visual Studio

Open the folder with CMake support, or generate a solution:

```bash
cmake -B build -G "Visual Studio 17 2022"
```

Then open `build/FlyTracer.sln`.

### CLion

Open the project folder directly. CLion will detect the CMakeLists.txt automatically.

## Project Structure

```
FlyTracer/
├── engine/
│   ├── include/       # Engine headers (GameScene.h, Scene.h, etc.)
│   ├── src/           # Engine implementation
│   └── shaders/       # GLSL compute shaders
├── game/
│   ├── include/       # Your scene headers
│   ├── src/           # Your scene implementation, the scene registry and main.cpp
│   └── resources/     # Models, textures, config files
└── tests/             # Unit tests
```

## Creating Your First Scene

1. Create a new header in `game/include/`:

```cpp
// game/include/MyScene.h
#pragma once
#include "GameScene.h"

class MyScene : public GameScene {
public:
    explicit MyScene(const std::string& resourceDir);

protected:
    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnInput(const Input& input) override;
    void OnGui() override;
};
```

2. Implement it in `game/src/`:

```cpp
// game/src/MyScene.cpp
#include "MyScene.h"
#include <imgui.h>

MyScene::MyScene(const std::string& resourceDir)
    : GameScene(resourceDir) {}

void MyScene::OnInit() {
    // Add scene objects here
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));
    AddSphere(TriVector(0.0f, 2.0f, 0.0f), 2.0f,
              Scene::Material::PBR(Scene::Color::Red()));
    AddPointLight(TriVector(5.0f, 10.0f, 5.0f),
                  Scene::Color::White(), 1.5f, 50.0f);

    SetCamera(TriVector(0.0f, 5.0f, 15.0f), TriVector(0.0f, 2.0f, 0.0f));
}

void MyScene::OnUpdate(float deltaTime) {
    // Animation logic here
}

void MyScene::OnInput(const Input& input) {
    // Input handling here
}

void MyScene::OnGui() {
    ImGui::Begin("My Scene");
    ImGui::Text("Hello, FlyTracer!");
    ImGui::End();
}
```

3. Register your scene in `game/src/SceneRegistry.cpp`: add its name to `kSceneNames` in `game/include/SceneRegistry.h`, and create it in `CreateScene`:

```cpp
} else if (name == "myscene") {
    return std::make_unique<MyScene>(resourceDir);
}
```

and run it with `./build/bin/FlyTracer --scene myscene`. The benchmark (`--benchmark`) and the scene image test then include it too.

4. Add your `.cpp` file to the scene sources in `CMakeLists.txt`:

```cmake
set(GAME_SCENE_SOURCES
    game/src/SceneRegistry.cpp
    ...
    game/src/MyScene.cpp
)
```

5. Build and run!


## Next Steps

- [Scene Overview](scenes/overview.md) - Understand the GameScene architecture
- [Primitives](scenes/primitives.md) - Add spheres and planes
- [Materials](scenes/materials.md) - Configure shading and colors
- [Meshes](scenes/meshes.md) - Load OBJ models with textures
