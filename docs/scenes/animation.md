# Animation

FlyTracer uses the **Motor** type from PGA (Projective Geometric Algebra) for object transformations. A motor combines rotation and translation into one element, and transforms anything (points, lines, planes) with the sandwich product `M X ~M`. The renderer applies instance motors the same way on the GPU.

## The Update Loop

Animation happens in `OnUpdate`, called every frame with the elapsed time:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;
    // Animate objects here...
}
```

## Motors

### Creating Motors

```cpp
// Identity (no transformation)
Motor identity{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

// Translation by (x, y, z) (from PGA.h)
Motor T = TranslationMotor(x, y, z);

// Rotation: angle in degrees around a line (a BiVector)
BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);  // The y axis through the origin
Motor R = Motor::Rotation(45.0f, yAxis);

// Rotation around any line, e.g. the join of two points
BiVector line = TriVector(1.0f, 0.0f, 0.0f) & TriVector(1.0f, 1.0f, 0.0f);
Motor aroundLine = Motor::Rotation(90.0f, line);
```

### Composition Order

Motors compose by multiplication, and like matrices **the rightmost motor acts first**:

```cpp
Motor combined = T * R;  // First rotate (around the origin), then translate
```

### Applying a Motor

```cpp
TriVector moved = ApplyMotor(M, point);   // M point ~M, normalized to e123 = 1
```

### Common Rotation Axes

```cpp
BiVector xAxis(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);  // Pitch
BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);  // Yaw, most common for orbits
BiVector zAxis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);  // Roll
```

## Animating Spheres

Spheres store their center as a TriVector, so a motor moves it directly:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    const float degreesThisFrame = 30.0f * deltaTime;
    const Motor R = Motor::Rotation(degreesThisFrame, BiVector(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));

    if (auto* sphere = GetSphere(m_sphereId)) {
        sphere->center = ApplyMotor(R, sphere->center);  // Orbits the y axis
    }
}
```

## Animating Meshes

Mesh instances have a `transform` motor:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    const Motor R = Motor::Rotation(30.0f * deltaTime, BiVector(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));

    if (auto* instance = FindInstance("myMesh")) {
        // After the existing transform, rotate around the world's y axis
        instance->transform = R * instance->transform;
    }
}
```

Multiplying on the left applies the new motion in world space (after the existing transform); multiplying on the right (`instance->transform * R`) applies it in the object's own frame (before the existing transform).

### Rotation and Translation

```cpp
void MyScene::OnUpdate(float deltaTime) {
    m_time += deltaTime;

    const Motor R = Motor::Rotation(m_time * 45.0f, BiVector(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
    const Motor T = TranslationMotor(0.0f, std::sin(m_time * 2.0f) * 0.5f, 0.0f);

    if (auto* instance = FindInstance("floating")) {
        instance->transform = T * R;  // Spin in place, then bob up and down
    }
}
```

## Interpolating a Motor

A motor `M = cos(a) - sin(a) L` (for a normalized line `L`) rotates by `2a` around that line; `M` to the power `t` rotates by `2at`. A translation `M = 1 + d` to the power `t` is `1 + t d`. `MotorPower` (in `PGA.h`) does both, for any motor that is a pure rotation or translation, such as the product of two reflections. The bireflection scene animates the motor of two planes like this:

```cpp
const Motor M = ReflectionMotor(firstPlane, secondPlane);
pheasant->transform = MotorPower(M, t) * originalTransform;  // t from 0 to 1
```
