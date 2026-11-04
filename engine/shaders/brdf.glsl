// How surfaces respond to light, shared by both raytracers: the sky, the BRDFs and the sampling of
// glossy reflections. Directions are unit vectors; the cosines are those of the angles with the normal.

#ifndef BRDF_GLSL
#define BRDF_GLSL

// Linear radiance of the sky gradient, and of the ground as seen from below the horizon
const vec3 SKY_HORIZON = vec3(0.83, 0.87, 0.91);
const vec3 SKY_ZENITH = vec3(0.42, 0.64, 0.89);
const vec3 GROUND_BOUNCE = vec3(0.30, 0.29, 0.27);

vec3 skyColor(vec3 direction) {
    return mix(SKY_HORIZON, SKY_ZENITH, clamp(direction.y, 0.0, 1.0));
}

// Average radiance of the sky and ground around a surface with this normal, ignoring occlusion.
// A diffuse surface reflects albedo times this: a cheap stand-in for indirect light.
vec3 ambientLight(vec3 normal) {
    return mix(GROUND_BOUNCE, skyColor(normal), 0.5 + 0.5 * normal.y);
}

float distributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Smith's shadowing-masking with Schlick's approximation, with k = (roughness + 1)^2 / 8 for analytic
// lights (Karis, "Real Shading in Unreal Engine 4")
float geometrySmith(float NdotV, float NdotL, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return (NdotV / (NdotV * (1.0 - k) + k)) * (NdotL / (NdotL * (1.0 - k) + k));
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 specularColor(Material material, vec3 albedo) {
    return mix(vec3(0.04), albedo, material.metallic);
}

// Fraction of the light arriving from L that leaves towards V, per unit solid angle, with H the
// direction halfway between them. NdotV must be positive.
vec3 brdf(Material material, vec3 albedo, float NdotL, float NdotV, float NdotH, float VdotH) {
    if (material.shadingMode == SHADING_LAMBERT) {
        return albedo / PI;
    }
    if (material.shadingMode == SHADING_PHONG) {
        // Normalized Blinn-Phong: (n + 8) / 8π keeps the highlight's energy independent of n
        float n = material.shininess;
        float specular = material.specular;
        return (1.0 - specular) * albedo / PI + vec3(specular * (n + 8.0) / (8.0 * PI) * pow(NdotH, n));
    }

    // Cook-Torrance with GGX; a minimum roughness avoids an infinitely small highlight
    float roughness = max(material.roughness, 0.05);
    vec3 F = fresnelSchlick(VdotH, specularColor(material, albedo));
    vec3 specular = distributionGGX(NdotH, roughness) * geometrySmith(NdotV, NdotL, roughness) * F
                  / (4.0 * NdotV * NdotL + 1e-4);
    vec3 diffuse = (1.0 - F) * (1.0 - material.metallic) * albedo / PI;
    return diffuse + specular;
}

// Fraction of the light that the specular lobe reflects towards V, on average: Fresnel-Schlick,
// damped at grazing angles for rough surfaces (Lagarde). A dielectric diffusely reflects the rest.
vec3 specularAlbedo(Material material, vec3 albedo, float NdotV) {
    vec3 F0 = specularColor(material, albedo);
    return F0 + (max(vec3(1.0 - material.roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);
}

// How much of the ambient light a surface reflects diffusely: what its specular part leaves
vec3 diffuseWeight(Material material, vec3 albedo, float NdotV) {
    if (material.shadingMode == SHADING_PHONG) {
        return vec3(1.0 - material.specular);
    }
    if (material.shadingMode == SHADING_PBR) {
        return (1.0 - specularAlbedo(material, albedo, NdotV)) * (1.0 - material.metallic);
    }
    return vec3(1.0);
}

// Smith's masking for GGX: the fraction of the microfacets around a normal N that is visible from a
// direction at cosine cosTheta with N
float smithG1(float cosTheta, float alpha) {
    float alpha2 = alpha * alpha;
    return 2.0 * cosTheta / (cosTheta + sqrt(alpha2 + (1.0 - alpha2) * cosTheta * cosTheta));
}

// A microfacet normal distributed like the GGX normals visible from V, D(H) G1(V) max(V.H, 0) / N.V,
// with the spherical caps of Dupuy and Benyoub, "Sampling Visible GGX Normals with Spherical Caps"
// (2023). Visible microfacets always face V, so their reflections are rarely masked. Both directions
// are in the surface's tangent frame, where the normal is (0, 0, 1).
vec3 sampleVisibleGGX(vec3 localV, float alpha, inout uint rng) {
    // Stretch to a hemisphere configuration, sample the cap of directions visible from V there
    vec3 stretchedV = normalize(vec3(alpha * localV.xy, localV.z));
    float phi = 2.0 * PI * random(rng);
    float z = (1.0 - random(rng)) * (1.0 + stretchedV.z) - stretchedV.z;
    float sinTheta = sqrt(clamp(1.0 - z * z, 0.0, 1.0));
    vec3 stretchedH = vec3(sinTheta * cos(phi), sinTheta * sin(phi), z) + stretchedV;
    return normalize(vec3(alpha * stretchedH.xy, max(stretchedH.z, 0.0)));
}

#endif // BRDF_GLSL
