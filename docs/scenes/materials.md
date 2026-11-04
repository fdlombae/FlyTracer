# Materials

Spheres, planes and meshes all use the same `Scene::Material`. FlyTracer supports four shading models, from flat colors to physically based rendering (PBR).

## Shading Modes

| Mode | Lighting | Reflections |
|------|----------|-------------|
| **Flat** | None: the color as is | None |
| **Lambert** | Diffuse | None |
| **Phong** | Diffuse + normalized Blinn-Phong highlight, split by `specular` | Whitted-style mirror, weighted by `reflectivity` |
| **PBR** | Cook-Torrance (GGX) with `metallic` and `roughness` | The GGX lobe of the same BRDF, weighted by Fresnel |

Rough PBR reflections are glossy: each frame reflects around a random GGX microfacet normal that is visible from the viewer, and the image averages them while the view stays the same. Objects that move restart the average every frame, so rough metals look grainy while they move.

## Creating Materials

### Flat

No lighting: just the color.

```cpp
auto mat = Scene::Material::Flat(Scene::Color::Red());
```

### Lambert

Diffuse shading based on the surface normal and the light direction.

```cpp
auto mat = Scene::Material::Lambert(Scene::Color(0.8f, 0.6f, 0.4f));
```

### Phong

Adds specular highlights to Lambert shading, and optionally mirror reflections. `specular` is the fraction of the light in the highlight (default 0.5); the diffuse part gets the rest.

```cpp
// Color and shininess (higher = sharper highlights)
auto mat = Scene::Material::Phong(Scene::Color::White(), 32.0f);

// With reflectivity (0.0 - 1.0): the fraction of light mirrored
auto mirror = Scene::Material::Phong(Scene::Color::White(), 64.0f, 0.5f);

// A weaker highlight: 20% specular, 80% diffuse
auto matte = Scene::Material::Phong(Scene::Color::White(), 16.0f, 0.0f, 0.2f);
```

### PBR (Physically Based Rendering)

```cpp
// Color, metallic, roughness
auto mat = Scene::Material::PBR(Scene::Color(1.0f, 0.84f, 0.0f), 1.0f, 0.3f);

// Metallic: 0.0 = dielectric (plastic, stone), 1.0 = metal
// Roughness: 0.0 = mirror smooth, 1.0 = completely rough
```

## Convenience Constructors

```cpp
// Metal: metallic, configurable roughness
auto chrome = Scene::Material::Metal(Scene::Color::White(), 0.1f);
auto brushed = Scene::Material::Metal(Scene::Color::Gray(), 0.4f);

// Matte: not metallic, fully rough
auto chalk = Scene::Material::Matte(Scene::Color::White());

// Glossy: not metallic, low roughness (shiny plastic)
auto plastic = Scene::Material::Glossy(Scene::Color::Red(), 0.2f);
```

## Colors

`Scene::Color` holds RGB values from 0 to 1 in **sRGB**, like colors picked in an image editor. The renderer converts them to linear values before lighting, and the final image is tone mapped and encoded back to sRGB.

```cpp
Scene::Color custom(0.8f, 0.4f, 0.2f);

Scene::Color::White()   Scene::Color::Black()   Scene::Color::Red()
Scene::Color::Green()   Scene::Color::Blue()    Scene::Color::Yellow()
Scene::Color::Cyan()    Scene::Color::Magenta() Scene::Color::Gray(0.5f)
```

## Material Properties

```cpp
struct Material {
    ShadingMode shadingMode;  // Flat, Lambert, Phong or PBR
    Color color;              // Base color (sRGB)
    float metallic;           // PBR: 0 = dielectric, 1 = metal
    float roughness;          // PBR: 0 = mirror, 1 = fully rough
    float shininess;          // Phong: highlight exponent (default 32)
    float reflectivity;       // Phong: fraction of light mirrored (0-1)
    float specular;           // Phong: fraction of light in the highlight (default 0.5)
    std::string texturePath;  // Meshes only: multiplies the color
};
```

## Examples

### Material Showcase

```cpp
void MyScene::OnInit() {
    AddGroundPlane(0.0f, Scene::Material::Lambert(Scene::Color::Gray(0.5f)));

    // Row 1: shading modes
    AddSphere(TriVector(-6.0f, 1.5f, 0.0f), 1.5f, Scene::Material::Flat(Scene::Color::Red()));
    AddSphere(TriVector(-2.0f, 1.5f, 0.0f), 1.5f, Scene::Material::Lambert(Scene::Color::Red()));
    AddSphere(TriVector(2.0f, 1.5f, 0.0f), 1.5f, Scene::Material::Phong(Scene::Color::Red(), 32.0f));
    AddSphere(TriVector(6.0f, 1.5f, 0.0f), 1.5f, Scene::Material::PBR(Scene::Color::Red(), 0.0f, 0.3f));

    // Row 2: PBR roughness from 0 to 1
    for (int i = 0; i < 5; ++i) {
        AddSphere(TriVector(-4.0f + i * 2.0f, 1.5f, 4.0f), 1.0f,
                  Scene::Material::PBR(Scene::Color::White(), 0.0f, i / 4.0f));
    }

    // Row 3: metallic from 0 to 1
    for (int i = 0; i < 5; ++i) {
        AddSphere(TriVector(-4.0f + i * 2.0f, 1.5f, 8.0f), 1.0f,
                  Scene::Material::PBR(Scene::Color(0.9f, 0.7f, 0.3f), i / 4.0f, 0.3f));
    }

    AddPointLight(TriVector(0.0f, 10.0f, 4.0f), Scene::Color::White(), 2.0f, 50.0f);
}
```

### Realistic Materials

```cpp
auto gold = Scene::Material::Metal(Scene::Color(1.0f, 0.84f, 0.0f), 0.3f);
auto copper = Scene::Material::Metal(Scene::Color(0.72f, 0.45f, 0.2f), 0.4f);
auto chrome = Scene::Material::Metal(Scene::Color(0.95f, 0.95f, 0.95f), 0.05f);
auto rubber = Scene::Material::Matte(Scene::Color(0.1f, 0.1f, 0.1f));
auto plastic = Scene::Material::Glossy(Scene::Color::Red(), 0.15f);

// A mirror (no refraction): Whitted-style reflection
auto mirror = Scene::Material::Phong(Scene::Color(0.9f, 0.9f, 1.0f), 128.0f, 0.9f);
```

### Changing Materials at Runtime

Every material can change every frame, also those of meshes:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;
    if (auto* sphere = GetSphere(m_sphereId)) {
        sphere->material.roughness = 0.5f * (std::sin(m_time) + 1.0f);  // 0 to 1
    }
    if (Mesh* mesh = GetMesh(m_meshId)) {
        mesh->GetMaterial(0).color = m_highlighted ? Scene::Color(1.0f, 0.8f, 0.2f) : Scene::Color::White();
    }
}
```

Changing a mesh material's texture makes the renderer upload the meshes again, which briefly waits for the GPU.
