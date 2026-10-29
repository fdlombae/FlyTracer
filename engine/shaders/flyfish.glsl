// FlyFish's PGA types and operations in GLSL, with the same coefficients and signs as FlyFish.h:
//   C++ a & b (join)  -> join(a, b)       C++ a ^ b (meet)   -> meet(a, b)
//   C++ a | b (inner) -> inner(a, b)      C++ a * b          -> geometricProduct(a, b)
//   C++ M * X * ~M    -> sandwich(M, X)   C++ ~M (unit M)    -> reverse(M)
// tests/test_flyfish_glsl.cpp runs every function on the GPU and compares it with FlyFish.

#ifndef FLYFISH_GLSL
#define FLYFISH_GLSL

// Plane e1 x + e2 y + e3 z + e0 = 0
struct Vector {
    vec3 normal;  // e1, e2, e3
    float e0;
};

// Line with Plücker coordinates: the join of points a and b has direction b - a and moment a x b
struct BiVector {
    vec3 direction;  // e23, e31, e12
    vec3 moment;     // e01, e02, e03
};

// Point (w = 1) or direction (w = 0)
struct TriVector {
    vec3 xyz;  // e032, e013, e021
    float w;   // e123
};

// Rotation and translation. The bivector part is split as in "Clean up your Mesh!": its Euclidean
// part is the rotation axis direction, its ideal part (with e0) holds the translation.
struct Motor {
    float s;
    vec3 euclidean;  // e23, e31, e12
    vec3 ideal;      // e01, e02, e03
    float e0123;
};

TriVector point(vec3 position) {
    return TriVector(position, 1.0);
}

TriVector direction(vec3 xyz) {
    return TriVector(xyz, 0.0);
}

// The line through two points, or through a point along a direction
BiVector join(TriVector a, TriVector b) {
    return BiVector(a.w * b.xyz - b.w * a.xyz, cross(a.xyz, b.xyz));
}

// The plane through a point and a line, or through a line along a direction
Vector join(TriVector p, BiVector line) {
    return Vector(cross(p.xyz, line.direction) - p.w * line.moment, dot(p.xyz, line.moment));
}

Vector join(BiVector line, TriVector p) {
    return join(p, line);
}

// The signed distance of a point to a unit plane, times the point's weight
float join(TriVector p, Vector plane) {
    return dot(p.xyz, plane.normal) + p.w * plane.e0;
}

float join(Vector plane, TriVector p) {
    return -join(p, plane);
}

// Plücker product of two lines: zero when they intersect, its sign tells on which side they pass
float join(BiVector a, BiVector b) {
    return dot(a.direction, b.moment) + dot(a.moment, b.direction);
}

// The line where two planes intersect
BiVector meet(Vector a, Vector b) {
    return BiVector(cross(a.normal, b.normal), a.e0 * b.normal - b.e0 * a.normal);
}

// The point where a line passes through a plane; an ideal point (w = 0) when they are parallel
TriVector meet(BiVector line, Vector plane) {
    return TriVector(-plane.e0 * line.direction - cross(line.moment, plane.normal), dot(line.direction, plane.normal));
}

TriVector meet(Vector plane, BiVector line) {
    return meet(line, plane);
}

// The plane through a point, perpendicular to a line
Vector inner(TriVector p, BiVector line) {
    return Vector(-p.w * line.direction, dot(p.xyz, line.direction));
}

// The cosine of the angle between two unit planes
float inner(Vector a, Vector b) {
    return dot(a.normal, b.normal);
}

// Reflecting in plane a and then in plane b is the motor geometricProduct(b, a)
Motor geometricProduct(Vector a, Vector b) {
    return Motor(dot(a.normal, b.normal), cross(a.normal, b.normal), a.e0 * b.normal - b.e0 * a.normal, 0.0);
}

// The inverse of a unit motor
Motor reverse(Motor m) {
    return Motor(m.s, -m.euclidean, -m.ideal, m.e0123);
}

// M X ~M for a unit motor: the rotation acts on the coordinates, the translation only on points
TriVector sandwich(Motor m, TriVector x) {
    vec3 rotated = x.xyz + 2.0 * cross(m.euclidean, cross(m.euclidean, x.xyz) - m.s * x.xyz);
    vec3 translation = -2.0 * (m.s * m.ideal + cross(m.ideal, m.euclidean) + m.e0123 * m.euclidean);
    return TriVector(rotated + x.w * translation, x.w);
}

// P X ~P for a unit plane P: the mirror image of a point or direction
TriVector reflectInPlane(Vector plane, TriVector x) {
    return TriVector(x.xyz - 2.0 * join(x, plane) * plane.normal, x.w);
}

// The Euclidean norm: |normal| for a plane, the length of the direction for a line
float norm(Vector plane) {
    return length(plane.normal);
}

float norm(BiVector line) {
    return length(line.direction);
}

#endif // FLYFISH_GLSL
