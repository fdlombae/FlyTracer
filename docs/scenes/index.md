# Creating Scenes

This section covers everything you need to build raytraced scenes in FlyTracer.

## Topics

<div class="quick-links" markdown>

[**Overview** <br><span>Understand the GameScene lifecycle and architecture</span>](overview.md)

[**Primitives** <br><span>Add spheres and infinite planes to your scene</span>](primitives.md)

[**Materials** <br><span>Configure Flat, Lambert, Phong, and PBR shading</span>](materials.md)

[**Meshes** <br><span>Load OBJ models with textures</span>](meshes.md)

[**Lighting** <br><span>Point, directional, and spot lights</span>](lighting.md)

[**Camera** <br><span>Eye-target-up camera setup and controls</span>](camera.md)

[**Animation** <br><span>Transform objects with PGA Motors</span>](animation.md)

[**Input Handling** <br><span>Handle mouse and keyboard input</span>](input.md)

[**Debug UI** <br><span>Create ImGui debug interfaces</span>](imgui.md)

[**Debug Visualization** <br><span>Draw debug lines, points, and axes</span>](debug.md)

</div>

## Minimal Example

```cpp
class MyScene : public GameScene {
public:
    explicit MyScene(const std::string& resourceDir)
        : GameScene(resourceDir) {}

    void OnInit(VulkanRenderer* renderer) override {
        // Ground
        AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));

        // Sphere
        AddSphere(TriVector(0.0f, 2.0f, 0.0f), 2.0f,
                  Scene::Material::Metal(Scene::Color::Red(), 0.3f));

        // Light
        AddPointLight(TriVector(5.0f, 10.0f, 5.0f),
                      Scene::Color::White(), 1.5f, 50.0f);

        // Camera
        m_cameraEye = TriVector(0.0f, 5.0f, 12.0f);
        m_cameraTarget = TriVector(0.0f, 2.0f, 0.0f);
    }
};
```
