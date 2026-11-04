// A ray as a point and a direction, with the helpers that intersect it in PGA. Needs flyfish.glsl.

#ifndef RAY_GLSL
#define RAY_GLSL

// A ray starts at a point and runs along a direction: the line join(origin, direction)
struct Ray {
    TriVector origin;
    TriVector direction;
};

// The same ray in coordinates with the anchor as the origin (an origin split). Joins with lines and
// planes near the anchor then work with small numbers and stay precise.
Ray relativeTo(Ray ray, vec3 anchor) {
    return Ray(point(ray.origin.xyz - anchor), ray.direction);
}

// Where the ray meets a plane, in units of its direction. Its joins with the plane are linear along
// the ray: join(origin + t direction, plane) = join(origin, plane) + t join(direction, plane) = 0.
float rayParameter(Ray ray, Vector plane) {
    return -join(ray.origin, plane) / join(ray.direction, plane);
}

// The cosine of the angle between two unit directions: the inner product of the planes through the
// origin perpendicular to them
float cosine(TriVector a, TriVector b) {
    return inner(Vector(a.xyz, 0.0), Vector(b.xyz, 0.0));
}

TriVector normalized(TriVector direction) {
    return TriVector(normalize(direction.xyz), 0.0);
}

#endif // RAY_GLSL
