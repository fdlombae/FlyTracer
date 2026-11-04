# Debug Visualization

FlyTracer provides debug drawing functions to visualize points, lines, and coordinate axes on top of the raytraced scene. The raytracer draws them itself as lines of constant pixel width, compared against the scene's depth: parts hidden behind objects are drawn dimmed.

## Setup

Debug drawing is enabled by default (`SetDebugDrawEnabled(false)` turns it off). The lines are cleared before every `OnUpdate`, so draw them there:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    DrawDebugLine(TriVector(0, 0, 0), TriVector(5, 5, 0), Scene::Color::Red());
}
```

## Lines

Draw lines between two points in 3D space.

```cpp
DrawDebugLine(TriVector(x1, y1, z1), TriVector(x2, y2, z2), color);
```

### Example: Grid

```cpp
void MyScene::OnUpdate(float deltaTime) {
    const float gridSize = 10.0f;
    const float gridStep = 2.0f;
    const Scene::Color gridColor(0.5f, 0.5f, 0.5f);

    // Draw grid lines along X axis
    for (float x = -gridSize; x <= gridSize; x += gridStep) {
        DrawDebugLine(TriVector(x, 0.01f, -gridSize), TriVector(x, 0.01f, gridSize), gridColor);
    }
    // Draw grid lines along Z axis
    for (float z = -gridSize; z <= gridSize; z += gridStep) {
        DrawDebugLine(TriVector(-gridSize, 0.01f, z), TriVector(gridSize, 0.01f, z), gridColor);
    }
}
```

## Points

Draw points as small 3D crosses.

```cpp
DrawDebugPoint(TriVector(x, y, z, 1.0f), size, color);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `point` | `TriVector` | Position (w component should be 1.0) |
| `size` | `float` | Cross size (default: 0.1) |
| `color` | `Scene::Color` | Point color (default: white) |

### Example

```cpp
void MyScene::OnUpdate(float deltaTime) {
    // Mark sphere positions
    DrawDebugPoint(TriVector(5.0f, 1.0f, 0.0f, 1.0f), 0.3f, Scene::Color::Cyan());
    DrawDebugPoint(TriVector(-5.0f, 1.0f, 0.0f, 1.0f), 0.3f, Scene::Color::Magenta());
}
```

## Coordinate Axes

Draw RGB coordinate axes (X=red, Y=green, Z=blue) at a position, or the frame of a motor: the world axes transformed by it.

```cpp
// At a position
DrawDebugAxes(TriVector(x, y, z), size);

// The frame of a motor, e.g. a mesh instance's transform
DrawDebugAxes(instance->transform, size);
```

### Example

```cpp
void MyScene::OnUpdate(float deltaTime) {
    // World origin axes
    DrawDebugAxes(TriVector(0.0f, 0.0f, 0.0f), 5.0f);

    // Axes at an object's position
    if (auto* sphere = GetSphere(m_sphereId)) {
        DrawDebugAxes(sphere->center, 1.0f);
    }
}
```

## BiVector Lines (PGA)

Draw lines from a BiVector (Plücker line representation). Useful for visualizing PGA line elements. The segment is centered on the point of the line closest to the origin.

```cpp
DrawDebugLine(BiVector line, float halfLength, Scene::Color color);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `line` | `BiVector` | Plücker line (e.g., from regressive product of two points) |
| `halfLength` | `float` | How far to extend the line in each direction (default: 10.0) |
| `color` | `Scene::Color` | Line color (default: white) |

### Example: Lines from Points

```cpp
void MyScene::OnUpdate(float deltaTime) {
    // Create a line from two points using the regressive product
    TriVector p1(0.0f, 2.0f, 0.0f, 1.0f);
    TriVector p2(5.0f, 4.0f, 5.0f, 1.0f);
    BiVector line = p1 & p2;  // Join operation

    DrawDebugLine(line, 8.0f, Scene::Color::Yellow());
}
```

### Example: Rotating Line

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;

    // Create a line
    TriVector pA(0.0f, 5.0f, 0.0f, 1.0f);
    TriVector pB(3.0f, 5.0f, 3.0f, 1.0f);
    BiVector line = pA & pB;

    // Rotate it using a Motor
    Motor R(std::cos(m_time), 0, 0, 0, 0, std::sin(m_time), 0, 0);
    BiVector rotatedLine = (R * line * ~R).Grade2();

    DrawDebugLine(rotatedLine, 6.0f, Scene::Color::Cyan());
}
```

## Toggling Visibility

Use member variables to conditionally draw debug elements:

```cpp
class MyScene : public GameScene {
    bool m_showAxes{true};
    bool m_showGrid{true};

    void OnUpdate(float deltaTime) override {
        if (m_showAxes) {
            DrawDebugAxes(TriVector(0, 0, 0), 5.0f);
        }
        if (m_showGrid) {
            // Draw grid...
        }
    }

    void OnInput(const Input& input) override {
        // Toggle with number keys
        if (input.WasPressed(Key::Num1)) m_showAxes = !m_showAxes;
        if (input.WasPressed(Key::Num2)) m_showGrid = !m_showGrid;
    }
};
```
