# Primitives

FlyTracer supports two analytic primitive types: **spheres** and **planes**. Both are intersected exactly, without triangles.

## Spheres

Spheres are defined by a center point (a TriVector) and a radius.

### Adding Spheres

```cpp
uint32_t id = AddSphere(TriVector(x, y, z), radius, material);
```

### Example

```cpp
void MyScene::OnInit() {
    // Red metallic sphere
    m_redSphere = AddSphere(TriVector(-3.0f, 2.0f, 0.0f), 2.0f,
                            Scene::Material::Metal(Scene::Color::Red(), 0.2f));

    // Blue glossy sphere
    m_blueSphere = AddSphere(TriVector(3.0f, 1.5f, 0.0f), 1.5f,
                             Scene::Material::Glossy(Scene::Color::Blue(), 0.1f));

    // White matte sphere
    AddSphere(TriVector(0.0f, 4.0f, -2.0f), 1.0f,
              Scene::Material::Matte(Scene::Color::White()));
}
```

### Modifying Spheres

`GetSphere` returns a `Scene::Sphere*` (or `nullptr` for an unknown id). Change it directly; the next frame shows the change.

```cpp
if (Scene::Sphere* sphere = GetSphere(sphereId)) {
    // Move it with a motor: M p ~M
    const Motor up = TranslationMotor(0.0f, 1.0f, 0.0f);
    sphere->center = ApplyMotor(up, sphere->center);

    sphere->radius = 3.0f;
    sphere->material = Scene::Material::PBR(Scene::Color::Green(), 0.5f, 0.3f);
}
```

## Planes

Planes are infinite surfaces given as PGA Vectors.

### Plane Representation

`Vector(e0, e1, e2, e3)` is the plane `e1*x + e2*y + e3*z + e0 = 0`:

- `(e1, e2, e3)` is the normal direction
- `e0` is minus the distance from the origin along that normal

So `Vector(-3, 0, 1, 0)` is the plane `y = 3`. Planes are lit from both sides.

### Adding Planes

```cpp
uint32_t id = AddPlane(Vector(e0, nx, ny, nz), material);

// Convenience: horizontal ground plane at height y
uint32_t id = AddGroundPlane(height, material);
```

### Example: Cornell Box

```cpp
void MyScene::OnInit() {
    // Floor at y = 0
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));

    // Ceiling at y = 10: -y + 10 = 0
    AddPlane(Vector(10.0f, 0.0f, -1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.9f, 0.9f, 0.9f)));

    // Left wall at x = -5 (red): x + 5 = 0
    AddPlane(Vector(5.0f, 1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.9f, 0.2f, 0.2f)));

    // Right wall at x = 5 (green): -x + 5 = 0
    AddPlane(Vector(5.0f, -1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.2f, 0.9f, 0.2f)));

    // Back wall at z = -5: z + 5 = 0
    AddPlane(Vector(5.0f, 0.0f, 0.0f, 1.0f),
             Scene::Material::Lambert(Scene::Color(0.8f, 0.8f, 0.8f)));
}
```

### Modifying Planes

```cpp
if (Scene::Plane* plane = GetPlane(planeId)) {
    plane->plane = Vector(-5.0f, 0.0f, 1.0f, 0.0f);  // Move to y = 5
    plane->material = Scene::Material::Phong(Scene::Color::Blue(), 64.0f);
}
```

## Common Patterns

### Animated Sphere

```cpp
class MyScene : public GameScene {
    uint32_t m_sphereId{0};
    float m_time{0.0f};

    void OnInit() override {
        m_sphereId = AddSphere(TriVector(0.0f, 2.0f, 0.0f), 1.0f,
                               Scene::Material::Metal(Scene::Color(1.0f, 0.84f, 0.0f)));
    }

    void OnUpdate(float deltaTime) override {
        m_time += deltaTime;
        if (auto* sphere = GetSphere(m_sphereId)) {
            sphere->center = TriVector(0.0f, 2.0f + std::sin(m_time * 2.0f), 0.0f);
        }
    }
};
```
