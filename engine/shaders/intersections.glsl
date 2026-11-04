// Ray-primitive intersections in PGA. They return true for a hit in (0, tMax) and write its distance,
// in units of the ray's direction, to t.

#ifndef INTERSECTIONS_GLSL
#define INTERSECTIONS_GLSL

// With the sphere's center as the origin, the ray's line passes it at the distance
// |moment| / |direction|. The point of the line closest to the center lies in the plane through the
// center perpendicular to the line; the ray enters and leaves half a chord before and after it.
bool chordOfSphere(Ray ray, vec3 center, float radius, out float closest, out float halfChord) {
    Ray local = relativeTo(ray, center);
    BiVector line = join(local.origin, local.direction);
    float directionSquared = dot(line.direction, line.direction);
    float distanceSquared = dot(line.moment, line.moment) / directionSquared;
    if (distanceSquared > radius * radius) return false;
    closest = rayParameter(local, inner(point(vec3(0.0)), line));
    halfChord = sqrt((radius * radius - distanceSquared) / directionSquared);
    return true;
}

bool intersectSphere(Ray ray, vec3 center, float radius, float tMax, out float t) {
    float closest;
    float halfChord;
    if (!chordOfSphere(ray, center, radius, closest, halfChord)) return false;
    t = closest - halfChord;
    if (t <= 0.0) t = closest + halfChord;  // Starting inside the sphere
    return t > 0.0 && t < tMax;
}

bool intersectPlane(Ray ray, Vector plane, float tMax, out float t) {
    t = rayParameter(ray, plane);  // Infinite or NaN when the ray runs parallel to the plane
    return t > 0.0 && t < tMax;
}

// Slab test. The ray meets the axis plane x = c, the plane e1 - c e0, at t = (c - origin.x) / direction.x,
// and likewise for y and z; it is inside the box where it is between all three pairs of planes.
float enterBox(Ray ray, vec3 inverseDirection, Box box, float tMax) {
    vec3 t0 = (box.minimum - ray.origin.xyz) * inverseDirection;
    vec3 t1 = (box.maximum - ray.origin.xyz) * inverseDirection;
    vec3 tNear = min(t0, t1);
    vec3 tFar = max(t0, t1);
    float enter = max(max(tNear.x, tNear.y), max(tNear.z, 0.0));
    float exit = min(min(tFar.x, tFar.y), tFar.z);
    return (enter <= exit && enter < tMax) ? enter : INFINITY;
}

// 1 / 0 is undefined in GLSL; a huge but finite inverse with the same sign gives the same slab test result
vec3 inverseOf(TriVector direction) {
    vec3 tiny = 1e-20 * (2.0 * step(0.0, direction.xyz) - 1.0);
    return 1.0 / mix(direction.xyz, tiny, lessThan(abs(direction.xyz), vec3(1e-20)));
}

// The Plücker product of the ray's line with an edge's line, with the edge's anchor as the origin:
//     join(join(point(origin - anchor), direction), BiVector(edge direction, no moment))
// The edge has no moment there, so this is the ray's moment around the anchor along the edge.
// Every triangle with this edge must get exactly the same number, which makes the test watertight,
// so it is written out with precise: the compiler may not fuse its operations differently where
// this function is inlined.
float edgeProduct(Ray ray, uint edgeIndex) {
    Edge edge = edges[edgeIndex & ~REVERSED_EDGE];
    precise vec3 localOrigin = ray.origin.xyz - edge.anchor;
    precise vec3 moment = cross(localOrigin, ray.direction.xyz);
    precise float product = dot(moment, edge.direction);
    return (edgeIndex & REVERSED_EDGE) != 0u ? -product : product;
}

// The ray passes the two edges on different sides
bool oppositeSigns(float a, float b) {
    return (a < 0.0 && b > 0.0) || (a > 0.0 && b < 0.0);
}

// The ray passes through the triangle when its Plücker products with the three edges, all run
// through in winding order, have the same sign. Each product is proportional to the barycentric
// weight of the corner opposite its edge.
bool intersectEdgeTriangle(Ray ray, EdgeTriangle triangle, float tMax, out float t, out vec2 barycentrics) {
    float product0 = edgeProduct(ray, triangle.edges[0]);
    float product1 = edgeProduct(ray, triangle.edges[1]);
    if (oppositeSigns(product0, product1)) return false;
    float product2 = edgeProduct(ray, triangle.edges[2]);
    if (oppositeSigns(product0, product2) || oppositeSigns(product1, product2)) return false;
    float sum = product0 + product1 + product2;
    if (sum == 0.0) return false;  // In the triangle's plane

    // The triangle's plane: through the anchor of edge 0, along its direction and that of edge 1
    Edge edge0 = edges[triangle.edges[0] & ~REVERSED_EDGE];
    Edge edge1 = edges[triangle.edges[1] & ~REVERSED_EDGE];
    Vector plane = join(BiVector(edge0.direction, vec3(0.0)), direction(edge1.direction));
    t = rayParameter(relativeTo(ray, edge0.anchor), plane);
    barycentrics = vec2(product1, product2) / sum;
    return t > 0.0 && t < tMax;
}

#endif // INTERSECTIONS_GLSL
