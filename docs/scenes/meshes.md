# Meshes

FlyTracer loads OBJ meshes with optional textures. Every mesh gets a bottom-level BVH over its triangles (in object space); every frame the renderer builds a top-level BVH over the instances (in world space), so rays only visit the meshes and triangles they can hit.

## Loading Meshes

Place your `.obj` and texture files in the `game/resources/` directory; the build copies them next to the executable.

```cpp
// Load mesh with texture
uint32_t meshId = LoadMesh("model.obj", "texture.png");

// Load mesh without texture
uint32_t meshId = LoadMesh("model.obj");
```

Meshes are moved so they are centered on the origin with their lowest point at y = 0. Textures are sRGB images with mipmaps; the raytracer picks the mip level from the size of the pixel's ray cone where it hits.

!!! note "Supported Formats"
    - **Meshes**: Wavefront OBJ (`.obj`)
    - **Textures**: PNG (via SDL)

A mesh can also be built in code, with `Mesh::AddVertex`, `AddTriangle` and `AddMaterial`, and added with `AddMesh(std::move(mesh))`.

Meshes can be loaded or added at any time, also while the scene runs: the renderer uploads them before the next frame, which briefly waits for the GPU. Material properties of a mesh (`GetMesh(meshId)->GetMaterial(i)`) can change every frame. Changing a mesh's vertices or triangles after it was added is not supported: add a new mesh instead.

`GetMesh(meshId)` gives the mesh's measures, for closed meshes (`IsClosed()`): `Volume()`, `CenterOfMass()` and `Inertia()`, the principal moments of inertia with a rotor that turns the coordinate axes into the principal axes. See [Mesh Measures](../pga-tracer.md#mesh-measures) for how PGA computes them.

## Creating Instances

A mesh can have any number of instances, each with its own transform: a **motor** (rotation and translation), a uniform **scale** and optionally a **mirror** plane. A point `p` of the mesh ends up at `M (scale p) ~M`, reflected in the mirror plane when there is one. Instances with scale 0 are not drawn.

```cpp
// Place at a position
uint32_t instanceId = AddMeshInstance(meshId, TriVector(x, y, z), "optionalName");

// Place with a motor: rotate 45 degrees around the y axis, then move to (2, 0, 0)
const BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
const Motor transform = TranslationMotor(2.0f, 0.0f, 0.0f) * Motor::Rotation(45.0f, yAxis);
uint32_t rotatedId = AddMeshInstance(meshId, transform, "rotatedObject");
```

## Modifying Instances

```cpp
if (MeshInstance* instance = GetInstance(instanceId)) {
    instance->scale = 2.0f;           // Uniform scale
    instance->visible = false;        // Hide the instance
    instance->transform = newMotor;   // New rotation and translation
}

if (MeshInstance* bird = FindInstance("pheasant")) {
    bird->scale = 0.5f;
}

SetInstanceTransform(instanceId, newMotor);
SetInstancePosition(instanceId, TriVector(newX, newY, newZ));  // Resets the rotation
SetInstanceVisible(instanceId, false);
```

## Removing Instances

Instances can be added and removed at any time, also while the scene runs. `RemoveMeshInstance` returns `false` for an unknown id. The ids of the other instances stay valid, and a removed id is never given out again. `GetInstanceIds()` lists the ids of all instances.

```cpp
RemoveMeshInstance(instanceId);  // GetInstance(instanceId) now returns nullptr
```

Pointers returned by `GetInstance` and `FindInstance` are invalid after adding or removing an instance, so look them up again instead of keeping them. Don't remove instances while looping over `GetInstanceIds()`; remember the id and remove it after the loop.

## Reflections

`Reflected(plane)` returns the instance reflected in a plane: it gets that plane as its mirror. Reflecting a mirrored instance again turns the two reflections into a single motor, exactly as in PGA where the product of two planes is a motor.

```cpp
const Vector mirrorPlane(0.0f, 1.0f, 0.0f, 0.0f);  // x = 0
const MeshInstance mirrored = FindInstance("pheasant")->Reflected(mirrorPlane);
const uint32_t copyId = AddMeshInstance(mirrored.meshId, mirrored.transform, "mirrored");
GetInstance(copyId)->mirror = mirrored.mirror;
GetInstance(copyId)->scale = mirrored.scale;
```

## MeshInstance Structure

```cpp
struct MeshInstance {
    uint32_t meshId;              // The loaded mesh
    Motor transform;              // Rotation and translation
    float scale;                  // Uniform scale factor
    std::optional<Vector> mirror; // Reflection applied after the motor
    bool visible;                 // Hidden instances are not rendered
    std::string name;             // Optional identifier

    TriVector ToWorld(const TriVector& localPoint) const;
    MeshInstance Reflected(const Vector& plane) const;
};
```

## Example: Textured Model

```cpp
class ModelScene : public GameScene {
    uint32_t m_instanceId{0};
    float m_rotation{0.0f};

public:
    explicit ModelScene(const std::string& resourceDir)
        : GameScene(resourceDir) {}

protected:
    void OnInit() override {
        const uint32_t meshId = LoadMesh("pheasant.obj", "pheasant.png");
        m_instanceId = AddMeshInstance(meshId, TriVector(0.0f, 0.0f, 0.0f), "bird");
        GetInstance(m_instanceId)->scale = 0.5f;

        AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray()));
        AddPointLight(TriVector(5.0f, 10.0f, 5.0f), Scene::Color::White(), 2.0f, 50.0f);
        SetCamera(TriVector(0.0f, 3.0f, 8.0f), TriVector(0.0f, 1.0f, 0.0f));
    }

    void OnUpdate(float deltaTime) override {
        m_rotation += deltaTime * 30.0f;  // Degrees per second
        const BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        SetInstanceTransform(m_instanceId, Motor::Rotation(m_rotation, yAxis));
    }
};
```

## Example: Multiple Instances

```cpp
void MyScene::OnInit() {
    const uint32_t treeId = LoadMesh("tree.obj", "tree.png");

    // A forest: every instance shares the same mesh data and BVH
    for (int x = -5; x <= 5; x += 2) {
        for (int z = -5; z <= 5; z += 2) {
            const uint32_t id = AddMeshInstance(treeId, TriVector(x, 0.0f, z));
            GetInstance(id)->scale = 0.8f + 0.1f * ((x + z) % 3);
        }
    }
}
```

## OBJ File Requirements

For best results, make sure your OBJ files:

- Have **triangulated** faces (no quads or n-gons)
- Include **vertex normals** (`vn` lines); otherwise smooth normals are computed, from faces wound counter-clockwise seen from outside (as OBJ specifies)
- Include **texture coordinates** (`vt` lines) if using textures; otherwise cylindrical UVs are generated

!!! tip "Blender Export Settings"
    When exporting from Blender:

    - Enable "Triangulate Faces"
    - Enable "Write Normals"
    - Enable "Include UVs"
