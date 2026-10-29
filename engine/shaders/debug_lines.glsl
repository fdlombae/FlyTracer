// The debug line overlay

#ifndef DEBUG_LINES_GLSL
#define DEBUG_LINES_GLSL

const float DEBUG_LINE_WIDTH = 2.0;    // Pixels
const float HIDDEN_LINE_OPACITY = 0.35;

// Draws the debug lines as fixed-width lines over the image. The ray is the pixel's camera ray: all
// of them start at the eye, which serves as the origin here, so the ray's line has no moment and
// its joins are cheap. A line point is the point of the segment closest to the ray; parts behind
// the scene (beyond sceneDistance) are dimmed.
vec3 drawDebugLines(Ray ray, float sceneDistance, float pixelAngle, vec3 color) {
    float halfWidth = 0.5 * DEBUG_LINE_WIDTH * pixelAngle;  // Seen from the eye: per unit of distance
    TriVector eye = point(vec3(0.0));
    BiVector rayLine = join(eye, ray.direction);

    for (uint i = 0u; i < pc.debugLineCount; ++i) {
        DebugLine line = debugLines[i];
        vec3 segment = line.to.xyz - line.from.xyz;
        Ray segmentRay = Ray(point(line.from.xyz - ray.origin.xyz), direction(segment));
        BiVector segmentLine = join(segmentRay.origin, segmentRay.direction);

        // The segment lies in the plane through the eye and its line. Every segment point is at
        // least as far from the ray, in angle, as the plane is: a ray at a sine of more than the
        // line's half width from the plane cannot come near the segment.
        Vector eyePlane = join(eye, segmentLine);
        float sineTimesNorm = cosine(ray.direction, direction(eyePlane.normal));  // The normal is not unit
        if (sineTimesNorm * sineTimesNorm > halfWidth * halfWidth * dot(eyePlane.normal, eyePlane.normal)) continue;

        // The segment point closest to the ray: where the segment's line meets the plane through the
        // ray along their common perpendicular. That perpendicular is the normal of the plane
        // through the ray parallel to the segment.
        TriVector perpendicular = direction(join(rayLine, segmentRay.direction).normal);
        float s;
        if (dot(perpendicular.xyz, perpendicular.xyz) > 1e-12 * dot(segment, segment)) {
            s = rayParameter(segmentRay, join(rayLine, perpendicular));
        } else {
            s = rayParameter(segmentRay, inner(eye, segmentLine));  // Parallel: the point nearest to the eye
        }
        TriVector closest = point(segmentRay.origin.xyz + clamp(s, 0.0, 1.0) * segment);

        // How far along the ray the point is, and how far from it: the plane through the point
        // perpendicular to the ray, and the plane through the point and the ray, whose norm is that
        // distance times the length of the ray's direction
        float t = rayParameter(Ray(eye, ray.direction), inner(closest, rayLine));
        Vector throughPoint = join(closest, rayLine);
        float reach = halfWidth * t;
        if (t > 0.0 && dot(throughPoint.normal, throughPoint.normal) < reach * reach * dot(rayLine.direction, rayLine.direction)) {
            float opacity = t < sceneDistance ? 1.0 : HIDDEN_LINE_OPACITY;
            color = mix(color, line.color.rgb, opacity);
        }
    }
    return color;
}

#endif // DEBUG_LINES_GLSL
