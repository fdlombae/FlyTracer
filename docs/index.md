# FlyTracer

A real-time **Vulkan compute shader raytracer** built for educational purposes. Uses Projective Geometric Algebra (PGA) via the [FlyFish](https://github.com/fdlombae/FlyFish) library for 3D transformations: object transforms are motors (and mirror planes), applied as sandwich products on the CPU and on the GPU.

## Features

- **Vulkan 1.4 Compute Shader Raytracing** - Whitted-style ray tracing with shadows and (glossy) reflections, accumulated over frames for anti-aliasing
- **Multiple Shading Models** - Flat, Lambert, Phong, and PBR materials
- **Two-level BVH** - A top-level BVH over mesh instances and a bottom-level BVH per mesh
- **OBJ Mesh Loading** - Load 3D models with mipmapped textures
- **Scene Primitives** - Spheres and infinite planes
- **Multiple Light Types** - Point, directional, and spot lights
- **PGA Transformations** - Motors for rotations and translations, planes for reflections
- **Raytracing in PGA** - Rays, triangles, reflections and the camera as points, lines, planes and motors (see [The PGA Raytracer](pga-tracer.md))
- **Linear lighting** - sRGB colors and textures are converted to linear, then tone mapped

## Quick Example

```cpp
class MyScene : public GameScene {
public:
    explicit MyScene(const std::string& resourceDir)
        : GameScene(resourceDir) {}

protected:
    void OnInit() override {
        // Add a ground plane
        AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color(0.5f, 0.5f, 0.5f)));

        // Add a red metallic sphere
        AddSphere(TriVector(0.0f, 2.0f, 0.0f), 2.0f,
                  Scene::Material::Metal(Scene::Color(0.9f, 0.2f, 0.2f)));

        // Add a point light
        AddPointLight(TriVector(5.0f, 10.0f, 5.0f),
                      Scene::Color::White(), 1.5f, 50.0f);

        // Look from the eye at the target
        SetCamera(TriVector(0.0f, 5.0f, 15.0f), TriVector(0.0f, 2.0f, 0.0f));
    }
};
```

## Getting Started

Check out the [Getting Started](getting-started.md) guide to build and run FlyTracer, then explore the [Creating Scenes](scenes/overview.md) documentation to build your own raytraced scenes.
