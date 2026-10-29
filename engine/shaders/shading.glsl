// Surfaces, lights and the shading models

#ifndef SHADING_GLSL
#define SHADING_GLSL

struct Surface {
    TriVector position;
    TriVector shadowOrigin;     // Where shadow rays start, see surfaceAt
    TriVector normal;           // Shading normal, on the side of the incoming ray
    TriVector geometricNormal;  // True surface normal, on the side of the incoming ray
    Material material;
    vec3 albedo;                // Material color times texture, linear
};

struct LightSample {
    TriVector direction;  // Towards the light
    float distance;
    vec3 radiance;
};

// The plane through a point perpendicular to a unit direction, oriented along it: its join with a
// point is that point's height above the plane. It is the inner product of the point with the line
// through it along the direction, which points the other way.
Vector tangentPlane(TriVector p, TriVector normal) {
    Vector plane = inner(p, join(p, normal));
    return Vector(-plane.normal, -plane.e0);
}

// The plane whose join with a point of a triangle (p, a, b) is the barycentric weight of corner p:
// it contains the opposite edge a b and the triangle's normal, scaled to give 1 at p
Vector barycentricPlane(TriVector p, TriVector a, TriVector b, TriVector normal) {
    Vector plane = join(join(a, b), normal);
    float atCorner = join(p, plane);
    return Vector(plane.normal / atCorner, plane.e0 / atCorner);
}

// Mip level from the ray cone's footprint ("Texture Level of Detail Strategies for Real-Time Ray
// Tracing", Akenine-Möller et al., Ray Tracing Gems ch. 20): texture area per world area at the hit
float textureLevel(int textureIndex, float coneWidth, float cosine, float uvPerWorldArea) {
    vec2 size = vec2(textureSize(textures[nonuniformEXT(textureIndex)], 0));
    return 0.5 * log2(uvPerWorldArea * size.x * size.y) + log2(coneWidth / max(cosine, 1e-4));
}

// Position, normals and material at a hit. coneWidth is the width of the pixel's ray cone there.
Surface surfaceAt(Hit hit, Ray ray, float coneWidth) {
    Surface surface;
    TriVector normal;
    TriVector geometricNormal;
    uint materialIndex;
    vec3 textureColor = vec3(1.0);
    TriVector hitPoint = point(ray.origin.xyz + hit.t * ray.direction.xyz);

    if (hit.type == HIT_SPHERE) {
        // The line from the center through the hit is perpendicular to the sphere
        Sphere sphere = spheres[hit.index];
        geometricNormal = normalized(direction(join(point(sphere.center), hitPoint).direction));
        normal = geometricNormal;
        surface.position = point(sphere.center + sphere.radius * geometricNormal.xyz);  // Exactly on the sphere
        surface.shadowOrigin = surface.position;
        materialIndex = sphere.materialIndex;
    } else if (hit.type == HIT_PLANE) {
        Plane plane = planes[hit.index];
        Vector unitPlane = Vector(plane.normal, plane.e0);
        surface.position = point(hitPoint.xyz - join(hitPoint, unitPlane) * plane.normal);  // Exactly on the plane
        surface.shadowOrigin = surface.position;
        geometricNormal = direction(plane.normal);
        normal = geometricNormal;
        materialIndex = plane.materialIndex;
    } else {
        // Interpolate in object space, relative to corner 0, then apply the instance's versor: scale, motor, mirror
        MeshInstance instance = instances[hit.instance];
        Triangle triangle = triangles[hit.index];
        Vertex v0 = vertices[triangle.indices[0]];
        Vertex v1 = vertices[triangle.indices[1]];
        Vertex v2 = vertices[triangle.indices[2]];
        vec3 weights = vec3(1.0 - hit.barycentrics.x - hit.barycentrics.y, hit.barycentrics);
        vec3 anchor = v0.positionU.xyz;
        TriVector p0 = point(vec3(0.0));
        TriVector p1 = point(v1.positionU.xyz - anchor);
        TriVector p2 = point(v2.positionU.xyz - anchor);
        TriVector n0 = normalized(direction(v0.normalV.xyz));
        TriVector n1 = normalized(direction(v1.normalV.xyz));
        TriVector n2 = normalized(direction(v2.normalV.xyz));

        TriVector local = point(weights.y * p1.xyz + weights.z * p2.xyz);
        TriVector localNormal = direction(weights.x * n0.xyz + weights.y * n1.xyz + weights.z * n2.xyz);
        Vector facePlane = join(join(p0, p1), p2);
        TriVector localFaceNormal = direction(facePlane.normal);

        // "Hacking the Shadow Terminator" (Hanika, Ray Tracing Gems II ch. 4): with interpolated normals
        // a flat triangle is shaded as if it were curved, but its shadow rays would still start on the
        // flat triangle and hit its neighbors. They start on the curved surface the normals describe
        // instead: lifted onto the tangent planes at the corners where the point lies below them,
        // blended with the barycentric weights.
        vec3 lift = weights.x * min(join(local, tangentPlane(p0, n0)), 0.0) * n0.xyz
                  + weights.y * min(join(local, tangentPlane(p1, n1)), 0.0) * n1.xyz
                  + weights.z * min(join(local, tangentPlane(p2, n2)), 0.0) * n2.xyz;
        TriVector localShadowOrigin = point(local.xyz - lift);

        Motor motor = instanceMotor(instance);
        surface.position = sandwich(motor, point((anchor + local.xyz) * instance.scale));
        surface.shadowOrigin = sandwich(motor, point((anchor + localShadowOrigin.xyz) * instance.scale));
        normal = sandwich(motor, localNormal);
        geometricNormal = sandwich(motor, localFaceNormal);
        if (instance.hasMirror != 0u) {
            Vector mirror = Vector(instance.mirror.xyz, instance.mirror.w);
            surface.position = reflectInPlane(mirror, surface.position);
            surface.shadowOrigin = reflectInPlane(mirror, surface.shadowOrigin);
            normal = reflectInPlane(mirror, normal);
            geometricNormal = reflectInPlane(mirror, geometricNormal);
        }
        normal = normalized(normal);
        geometricNormal = normalized(geometricNormal);
        materialIndex = triangle.materialIndex;

        Material material = materials[materialIndex];
        if (material.textureIndex >= 0) {
            vec2 uv0 = vec2(v0.positionU.w, v0.normalV.w);
            vec2 uv1 = vec2(v1.positionU.w, v1.normalV.w);
            vec2 uv2 = vec2(v2.positionU.w, v2.normalV.w);
            vec2 uv = weights.x * uv0 + weights.y * uv1 + weights.z * uv2;
            // Texture coordinates are linear over the triangle: u and v are the joins of a point with
            // planes U and V. Where those planes meet, their line's length is the texture area per
            // object area, which the scale squared turns into the area per world area.
            Vector barycentric1 = barycentricPlane(p1, p2, p0, localFaceNormal);
            Vector barycentric2 = barycentricPlane(p2, p0, p1, localFaceNormal);
            Vector planeU = Vector((uv1.x - uv0.x) * barycentric1.normal + (uv2.x - uv0.x) * barycentric2.normal, 0.0);
            Vector planeV = Vector((uv1.y - uv0.y) * barycentric1.normal + (uv2.y - uv0.y) * barycentric2.normal, 0.0);
            float uvPerWorldArea = norm(meet(planeU, planeV)) / (instance.scale * instance.scale);
            float facing = abs(cosine(geometricNormal, normalized(ray.direction)));
            float level = textureLevel(material.textureIndex, coneWidth, facing, uvPerWorldArea);
            textureColor = textureLod(textures[nonuniformEXT(material.textureIndex)], uv, level).rgb;
        }
    }

    surface.material = materials[materialIndex];
    surface.albedo = surface.material.color * textureColor;

    // Both sides of triangles and planes are lit: turn the normals towards the incoming ray
    if (cosine(geometricNormal, ray.direction) > 0.0) geometricNormal = direction(-geometricNormal.xyz);
    if (cosine(normal, geometricNormal) < 0.0) normal = direction(-normal.xyz);
    surface.normal = normal;
    surface.geometricNormal = geometricNormal;
    return surface;
}

LightSample sampleLight(Light light, TriVector position) {
    if (light.type == LIGHT_DIRECTIONAL) {
        return LightSample(direction(-light.direction), INFINITY, light.color);
    }

    // The line from the surface to the light: its direction is the difference of the two points,
    // so its length is their distance
    BiVector toLight = join(position, point(light.position));
    float distance = norm(toLight);
    TriVector towards = direction(toLight.direction / distance);
    // Inverse square beyond the range, finite at the light
    float attenuation = light.range * light.range / (light.range * light.range + distance * distance);
    if (light.type == LIGHT_SPOT) {
        float cosAngle = cosine(direction(-towards.xyz), direction(light.direction));
        attenuation *= light.spotCosInner > light.spotCosOuter
                     ? smoothstep(light.spotCosOuter, light.spotCosInner, cosAngle)
                     : step(light.spotCosOuter, cosAngle);  // A hard edge
    }
    return LightSample(towards, distance, light.color * attenuation);
}

// Shadow ray towards the light: any hit before the light blocks it
bool isOccluded(Surface surface, LightSample light) {
    Ray shadowRay = Ray(point(offsetRayOrigin(surface.shadowOrigin.xyz, surface.geometricNormal.xyz)), light.direction);
    return traceScene(shadowRay, light.distance, true).type != HIT_NONE;
}

// Fraction of the light arriving from L that leaves towards V, per unit solid angle
vec3 brdf(Surface surface, TriVector V, TriVector L) {
    TriVector N = surface.normal;
    TriVector H = normalized(direction(V.xyz + L.xyz));
    return brdf(surface.material, surface.albedo, max(cosine(N, L), 0.0), max(cosine(N, V), 1e-4),
                max(cosine(N, H), 0.0), max(cosine(H, V), 0.0));
}

// Light leaving the surface towards V: ambient plus every unoccluded light
vec3 shade(Surface surface, TriVector V) {
    if (surface.material.shadingMode == SHADING_FLAT) {
        return surface.albedo;
    }

    vec3 color = diffuseWeight(surface.material, surface.albedo, max(cosine(surface.normal, V), 0.0))
               * surface.albedo * ambientLight(surface.normal.xyz);

    for (uint i = 0u; i < pc.lightCount; ++i) {
        LightSample light = sampleLight(lights[i], surface.position);
        float NdotL = cosine(surface.normal, light.direction);
        if (NdotL <= 0.0 || isOccluded(surface, light)) continue;
        color += brdf(surface, V, light.direction) * light.radiance * NdotL;
    }
    return color;
}

// The tangent frame as a rotor, after "Look, Ma, No Matrices!" (De Keninck, 2024): it turns +z into
// the normal. As RotationBetween in PGA.h, it reflects in the plane perpendicular to +z and then in
// the plane perpendicular to the direction halfway between +z and the normal.
Motor tangentFrame(TriVector normal) {
    vec3 halfway = vec3(0.0, 0.0, 1.0) + normal.xyz;
    if (dot(halfway, halfway) < 1e-8) {
        return Motor(0.0, vec3(1.0, 0.0, 0.0), vec3(0.0), 0.0);  // The normal is -z: half a turn around x
    }
    return geometricProduct(Vector(normalize(halfway), 0.0), Vector(vec3(0.0, 0.0, 1.0), 0.0));
}

// The direction of the reflected ray, and how much of the light arriving along it reaches V.
// Phong mirrors: the reflection of the incoming direction in the tangent plane. PBR reflects in the
// plane of a visible GGX microfacet, so over the accumulated samples the reflection is as glossy as
// the highlights of the BRDF.
vec3 sampleReflection(Surface surface, TriVector V, inout uint rng, out TriVector reflected) {
    TriVector incoming = direction(-V.xyz);
    reflected = reflectInPlane(Vector(surface.normal.xyz, 0.0), incoming);
    if (surface.material.shadingMode == SHADING_PHONG) {
        return vec3(surface.material.reflectivity);  // Whitted's mirror coefficient
    }
    if (surface.material.shadingMode != SHADING_PBR) {
        return vec3(0.0);
    }

    Motor frame = tangentFrame(surface.normal);
    TriVector localV = sandwich(reverse(frame), V);
    float alpha = max(surface.material.roughness * surface.material.roughness, 1e-4);
    TriVector H = sandwich(frame, direction(sampleVisibleGGX(localV.xyz, alpha, rng)));
    reflected = reflectInPlane(Vector(H.xyz, 0.0), incoming);
    float NdotL = cosine(surface.normal, reflected);
    if (NdotL <= 0.0 || cosine(reflected, surface.geometricNormal) <= 0.0) {
        return vec3(0.0);  // Reflected into the surface: masked
    }

    // brdf * N.L / pdf, with pdf = D G1(V) / (4 N.V) for the reflected direction: F G1(L)
    return fresnelSchlick(max(cosine(V, H), 0.0), specularColor(surface.material, surface.albedo)) * smithG1(NdotL, alpha);
}

#endif // SHADING_GLSL
