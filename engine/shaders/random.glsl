// Small random number generator for sample jitter

#ifndef RANDOM_GLSL
#define RANDOM_GLSL

// PCG hash, from "Hash Functions for GPU Rendering" (Jarzynski and Olano, JCGT 2020)
uint pcgHash(uint value) {
    uint state = value * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint seedRandom(ivec2 pixel, uint frameIndex) {
    return pcgHash(uint(pixel.x) + pcgHash(uint(pixel.y) + pcgHash(frameIndex)));
}

// Uniform in [0, 1): the top 24 bits convert to float exactly, all 32 could round up to 1
float random(inout uint state) {
    state = pcgHash(state);
    return float(state >> 8u) / 16777216.0;
}

#endif // RANDOM_GLSL
