# Lighting

FlyTracer supports three light types: **point lights**, **directional lights**, and **spot lights**. Every light casts shadows: at each hit the raytracer sends a shadow ray to every light, and any object in between blocks it. Light colors are sRGB like all colors; the renderer converts them to linear and multiplies by the intensity.

Besides the lights, every lit surface receives a soft **sky light**: the average color of the sky gradient and the ground around its normal. It keeps areas without direct light from being black, but does not take occlusion into account.

## Point Lights

Emit light in all directions from a single point. The intensity falls off as `range² / (range² + distance²)`: it halves at `range`, falls off with the inverse square of the distance beyond it, and stays finite close to the light.

```cpp
uint32_t id = AddPointLight(
    TriVector(x, y, z),     // Position
    Scene::Color::White(),  // Color
    1.5f,                   // Intensity
    50.0f                   // Range: distance where the intensity has halved
);
```

### Example

```cpp
void MyScene::OnInit() {
    // Warm overhead light
    AddPointLight(TriVector(0.0f, 10.0f, 0.0f),
                  Scene::Color(1.0f, 0.95f, 0.8f), 2.0f, 100.0f);

    // Blue and red accent lights
    AddPointLight(TriVector(-5.0f, 3.0f, 5.0f),
                  Scene::Color(0.4f, 0.6f, 1.0f), 1.0f, 30.0f);
    AddPointLight(TriVector(5.0f, 3.0f, 5.0f),
                  Scene::Color(1.0f, 0.4f, 0.4f), 1.0f, 30.0f);
}
```

## Directional Lights

Parallel light from an infinitely distant source (like the sun). It has no position, only the **direction the light travels**, given as a direction (an ideal point, e123 = 0).

```cpp
uint32_t id = AddDirectionalLight(
    TriVector(dx, dy, dz, 0.0f),  // Direction the light travels (normalized for you)
    Scene::Color::White(),        // Color
    1.0f                          // Intensity
);
```

### Example

```cpp
void MyScene::OnInit() {
    // Sunlight shining down and slightly to the left
    AddDirectionalLight(TriVector(-0.5f, -1.0f, -0.3f, 0.0f),
                        Scene::Color(1.0f, 0.98f, 0.9f), 1.2f);
}
```

## Spot Lights

A cone of light from a point, pointing in a direction. The edge of the cone fades out softly.

```cpp
uint32_t id = AddSpotLight(
    TriVector(px, py, pz),         // Position
    TriVector(dx, dy, dz, 0.0f),   // Direction of the cone's axis
    Scene::Color::White(),         // Color
    2.0f,                          // Intensity
    30.0f,                         // Cone angle: degrees from the axis to the edge
    50.0f                          // Range
);
```

### Example

```cpp
void MyScene::OnInit() {
    // Spotlight pointing straight down at the center
    AddSpotLight(TriVector(0.0f, 15.0f, 0.0f), TriVector(0.0f, -1.0f, 0.0f, 0.0f),
                 Scene::Color::White(), 3.0f, 25.0f, 30.0f);
}
```

## Modifying Lights

`GetLight` returns a `Scene::Light*` (or `nullptr`). Change it directly; the next frame shows the change.

```cpp
if (Scene::Light* light = GetLight(lightId)) {
    light->position = TriVector(newX, newY, newZ);  // Point and spot lights
    light->color = Scene::Color(1.0f, 0.5f, 0.0f);
    light->intensity = 2.5f;
}
```

## Spawning and Removing Lights

Lights can be added and removed at any time, for example from `OnInput` or a button in `OnGui`; the renderer picks up the new set in the next frame. `RemoveLight` returns `false` for an unknown id. The ids of the other lights stay valid, and a removed id is never given out again. `GetLightIds()` lists the ids of all lights.

```cpp
void OnInput(const Input& input) override {
    if (input.WasPressed(Key::L)) {
        m_spawnedIds.push_back(AddPointLight(TriVector(0.0f, 5.0f, 0.0f), Scene::Color::Red(), 1.5f, 20.0f));
    }
    if (input.WasPressed(Key::R) && !m_spawnedIds.empty()) {
        RemoveLight(m_spawnedIds.back());
        m_spawnedIds.pop_back();
    }
}
```

As with instances, a pointer from `GetLight` is invalid after adding or removing a light. The test box scene (`--scene testbox`) spawns lights with **L** and lists all lights and instances with a remove button.

## The Light Struct

```cpp
struct Light {
    LightType type;        // Directional, Point or Spot
    TriVector position;    // Point and spot lights
    TriVector direction;   // Direction the light travels (directional and spot)
    Color color;
    float intensity;
    float range;           // Point and spot lights: half intensity here, inverse square beyond
    float spotAngle;       // Degrees from the spot axis to the edge of the cone
    float spotSoftness;    // Fraction of the cone that fades out (default 0.2)
};
```

## Animated Light

```cpp
class AnimatedLightScene : public GameScene {
    uint32_t m_lightId{0};
    float m_time{0.0f};

    void OnInit() override {
        m_lightId = AddPointLight(TriVector(0.0f, 5.0f, 0.0f),
                                  Scene::Color::White(), 1.5f, 30.0f);
    }

    void OnUpdate(float deltaTime) override {
        m_time += deltaTime;
        if (auto* light = GetLight(m_lightId)) {
            light->intensity = 1.5f + std::sin(m_time * 3.0f) * 0.5f;  // Pulsing
        }
    }
};
```
