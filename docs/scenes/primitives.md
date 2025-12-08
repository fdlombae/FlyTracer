# Primitives

FlyTracer supports two primitive types: **spheres** and **planes**. Both are raytraced analytically for perfect precision.

## Spheres

Spheres are defined by a center point and radius.

### Adding Spheres

```cpp
// Using PGA TriVector for position
uint32_t id = addSphere(TriVector(x, y, z), radius, material);

// Using float coordinates
uint32_t id = addSphere(x, y, z, radius, material);
```

### Example

```cpp
void MyScene::onInit(VulkanRenderer* renderer) {
    // Red metallic sphere
    m_redSphere = addSphere(TriVector(-3.0f, 2.0f, 0.0f), 2.0f,
                            Scene::Material::Metal(Scene::Color::Red(), 0.2f));

    // Blue glossy sphere
    m_blueSphere = addSphere(TriVector(3.0f, 1.5f, 0.0f), 1.5f,
                             Scene::Material::Glossy(Scene::Color::Blue(), 0.1f));

    // White matte sphere
    addSphere(0.0f, 4.0f, -2.0f, 1.0f,
              Scene::Material::Matte(Scene::Color::White()));
}
```

### Modifying Spheres

After adding a sphere, you can modify it using the returned ID:

```cpp
// Get sphere pointer
Scene::GPUSphere* sphere = getSphere(sphereId);

if (sphere) {
    // Move the sphere
    sphere->setCenter(TriVector(newX, newY, newZ));

    // Change radius (requires re-upload, not recommended for animation)
    sphere->radius = 3.0f;
}

// Change material
setSphereMaterial(sphereId, Scene::Material::PBR(Scene::Color::Green(), 0.5f, 0.3f));
```

## Planes

Planes are infinite surfaces defined using PGA Vectors (homogeneous coordinates).

### Plane Representation

A plane in PGA is represented as a Vector with components `(d, nx, ny, nz)` where:

- `(nx, ny, nz)` is the normal direction
- `d` is the distance from origin along the normal

!!! note "Plane Equation"
    The plane equation is: `nx*x + ny*y + nz*z = d`

### Adding Planes

```cpp
// Using PGA Vector: Vector(d, nx, ny, nz)
uint32_t id = addPlane(Vector(distance, nx, ny, nz), material);

// Using components directly
uint32_t id = addPlane(nx, ny, nz, distance, material);

// Convenience: horizontal ground plane at height y
uint32_t id = addGroundPlane(height, material);
```

### Example: Cornell Box

```cpp
void MyScene::onInit(VulkanRenderer* renderer) {
    // Floor at y=0 (normal pointing up)
    addPlane(Vector(0.0f, 0.0f, 1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.7f, 0.7f, 0.7f)));

    // Or use the convenience function
    addGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));

    // Ceiling at y=10 (normal pointing down)
    addPlane(Vector(10.0f, 0.0f, -1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.9f, 0.9f, 0.9f)));

    // Left wall at x=-5 (normal pointing right, red)
    addPlane(Vector(5.0f, 1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.9f, 0.2f, 0.2f)));

    // Right wall at x=5 (normal pointing left, green)
    addPlane(Vector(5.0f, -1.0f, 0.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color(0.2f, 0.9f, 0.2f)));

    // Back wall at z=-5 (normal pointing forward)
    addPlane(Vector(5.0f, 0.0f, 0.0f, 1.0f),
             Scene::Material::Lambert(Scene::Color(0.8f, 0.8f, 0.8f)));
}
```

### Modifying Planes

```cpp
Scene::GPUPlane* plane = getPlane(planeId);

if (plane) {
    // Planes are typically static, but you can modify them
    plane->normal[0] = 0.0f;
    plane->normal[1] = 1.0f;
    plane->normal[2] = 0.0f;
    plane->distance = 5.0f;
}

// Change material
setPlaneMaterial(planeId, Scene::Material::Phong(Scene::Color::Blue(), 64.0f));
```

## Common Patterns

### Animated Sphere

```cpp
class MyScene : public GameScene {
    uint32_t m_sphereId{0};
    float m_time{0.0f};

    void onInit(VulkanRenderer* renderer) override {
        m_sphereId = addSphere(TriVector(0.0f, 2.0f, 0.0f), 1.0f,
                               Scene::Material::Metal(Scene::Color::Gold()));
    }

    void onUpdate(float deltaTime) override {
        m_time += deltaTime;

        if (auto* sphere = getSphere(m_sphereId)) {
            float y = 2.0f + std::sin(m_time * 2.0f);
            sphere->setCenter(TriVector(0.0f, y, 0.0f));
        }
    }
};
```

### Room Setup

```cpp
void setupRoom(float width, float height, float depth) {
    float hw = width / 2.0f;
    float hd = depth / 2.0f;

    // Floor and ceiling
    addGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray(0.7f)));
    addPlane(Vector(height, 0.0f, -1.0f, 0.0f),
             Scene::Material::Lambert(Scene::Color::Gray(0.9f)));

    // Walls
    addPlane(Vector(hw, 1.0f, 0.0f, 0.0f),   // Left
             Scene::Material::Lambert(Scene::Color(0.8f, 0.3f, 0.3f)));
    addPlane(Vector(hw, -1.0f, 0.0f, 0.0f),  // Right
             Scene::Material::Lambert(Scene::Color(0.3f, 0.8f, 0.3f)));
    addPlane(Vector(hd, 0.0f, 0.0f, 1.0f),   // Back
             Scene::Material::Lambert(Scene::Color::Gray(0.8f)));
}
```
