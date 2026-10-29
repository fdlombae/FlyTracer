# PGA Basics

FlyTracer uses **Projective Geometric Algebra (PGA)** for 3D transformations via the [FlyFish](https://github.com/fdlombae/FlyFish) library. This page covers the essentials you need for scene creation.

## Why PGA?

PGA provides one algebra for:

- **Points** as TriVectors
- **Planes** as Vectors
- **Lines** as BiVectors
- **Transformations** as Motors (rotation + translation) and planes (reflections)

All transformations use the same **sandwich product**: `X' = M X ~M`. FlyTracer uses it everywhere, including on the GPU: mesh instances are sent to the shader as motors, not matrices.

## Core Types

### TriVector (Points and Directions)

Points have homogeneous weight `e123 = 1`; directions (ideal points, "points at infinity") have `e123 = 0`.

```cpp
TriVector point(x, y, z);                // A point (e123 = 1)
TriVector direction(dx, dy, dz, 0.0f);   // A direction

float x = point.e032();  // x coordinate
float y = point.e013();  // y coordinate
float z = point.e021();  // z coordinate
```

!!! note "Coordinate Names"
    The names `e032`, `e013`, `e021` come from the PGA basis elements:

    - `e032()` → X
    - `e013()` → Y
    - `e021()` → Z

### Vector (Planes)

`Vector(e0, e1, e2, e3)` is the plane `e1*x + e2*y + e3*z + e0 = 0`, with normal `(e1, e2, e3)`.

```cpp
Vector floor(0.0f, 0.0f, 1.0f, 0.0f);     // y = 0
Vector wall(-5.0f, 1.0f, 0.0f, 0.0f);     // x = 5
Vector ceiling(10.0f, 0.0f, -1.0f, 0.0f); // y = 10, normal pointing down
```

### BiVector (Lines)

BiVectors are lines in Plücker coordinates: `BiVector(e01, e02, e03, e23, e31, e12)`, with direction `(e23, e31, e12)`. The easiest way to make one is the **join** (`&`) of two points, or of a point and a direction:

```cpp
BiVector xAxis(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);  // The x axis through the origin
BiVector yAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
BiVector zAxis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

BiVector line = TriVector(1.0f, 2.0f, 3.0f) & TriVector(4.0f, 2.0f, 3.0f);
BiVector throughPoint = TriVector(1.0f, 2.0f, 3.0f) & TriVector(0.0f, 1.0f, 0.0f, 0.0f);  // Vertical line
```

The **meet** (`^`) of two planes is their line of intersection.

### Motor (Transformations)

```cpp
// Identity motor
Motor identity{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

// Translation by (dx, dy, dz)
Motor T = TranslationMotor(dx, dy, dz);

// Rotation by an angle in degrees around a line
Motor R = Motor::Rotation(45.0f, yAxis);

// Combined: the rightmost motor acts first
Motor combined = T * R;  // Rotate around the y axis, then translate
```

`TranslationMotor`, `ApplyMotor` (for points and planes), `ReflectInPlane`, `ReflectionMotor`, `MotorPower`, `NormalizedMotor` and `Euclidean` are small helpers in `PGA.h`.

## Transforming Objects

### Points

```cpp
TriVector point(1.0f, 2.0f, 3.0f);
Motor R = Motor::Rotation(90.0f, yAxis);

TriVector rotated = ApplyMotor(R, point);    // (R * point * ~R).Grade3(), normalized
```

### Spheres

Sphere centers are TriVectors:

```cpp
void MyScene::OnUpdate(float deltaTime) {
    const Motor R = Motor::Rotation(deltaTime * 30.0f, yAxis);  // 30 degrees per second
    if (auto* sphere = GetSphere(m_sphereId)) {
        sphere->center = ApplyMotor(R, sphere->center);
    }
}
```

### Mesh Instances

Mesh instances store their transform as a motor (plus a uniform scale and an optional mirror plane):

```cpp
void MyScene::OnUpdate(float deltaTime) {
    const Motor R = Motor::Rotation(deltaTime * 45.0f, yAxis);
    if (auto* instance = FindInstance("myMesh")) {
        instance->transform = R * instance->transform;  // Rotate around the world's y axis
    }
}
```

## Reflections

A plane reflects points with the same sandwich product: `P X ~P`. Two reflections make a motor: reflecting in plane `a` and then in plane `b` is the motor `b * a` (a rotation around their line of intersection by twice the angle between them, or a translation by twice their distance when they are parallel).

```cpp
TriVector mirrored = ReflectInPlane(wall, point);
Motor twice = ReflectionMotor(first, second);  // second * first: reflect in first, then in second
```

## On the GPU

The raytracer uses the same algebra: `engine/shaders/flyfish.glsl` has FlyFish's types and operations in GLSL (`join` for `&`, `meet` for `^`, `inner` for `|`, `sandwich` for `M X ~M`, ...). Mesh instances reach the shader as motors, and rays enter an instance's object space with the reverse motor `~M` (and the mirror, which is its own inverse). See [The PGA Raytracer](pga-tracer.md) for how it traces rays with lines, planes and motors.

## Quick Reference

| Type | Represents | Create | Common Use |
|------|------------|--------|------------|
| `TriVector` | Point / direction | `TriVector(x, y, z)`, `TriVector(x, y, z, 0)` | Positions, sphere centers, light directions |
| `Vector` | Plane | `Vector(e0, nx, ny, nz)` | Floors, walls, mirrors |
| `BiVector` | Line | `point & point` | Rotation axes |
| `Motor` | Rigid motion | `TranslationMotor()`, `Motor::Rotation()` | Moving and rotating objects |

## Further Reading

- [The PGA Raytracer](pga-tracer.md) - How FlyTracer traces rays with lines, planes and motors
- [FlyFish Library](https://github.com/fdlombae/FlyFish) - The PGA library used by FlyTracer
- [Geometric Algebra Primer](https://bivector.net/PGA4CS.html) - PGA for Computer Scientists
- [ganja.js](https://enkimute.github.io/ganja.js/) - Interactive PGA visualizations
