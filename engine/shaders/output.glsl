// From linear radiance to display: tone mapping and sRGB encoding

#ifndef OUTPUT_GLSL
#define OUTPUT_GLSL

// ACES filmic curve, fitted by Krzysztof Narkowicz (2015): maps [0, inf) to [0, 1)
vec3 toneMapACES(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 linearToSrgb(vec3 color) {
    vec3 low = color * 12.92;
    vec3 high = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, lessThanEqual(color, vec3(0.0031308)));
}

#endif // OUTPUT_GLSL
