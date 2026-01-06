#include "Utils.h"
#include "SDL3/SDL_stdinc.h"

float GetSign(float const value)
{
    float const denominator{ std::abs(value) < SDL_FLT_EPSILON ? 1.f : std::abs(value) };
    return value / denominator;
}

float GetEuclideanSign(BiVector const& biVector)
{
    float const a{ std::abs(biVector.e23()) < SDL_FLT_EPSILON ? 1.f : biVector.e23() },
        b{ std::abs(biVector.e31()) < SDL_FLT_EPSILON ? 1.f : biVector.e31() },
        c{ std::abs(biVector.e12()) < SDL_FLT_EPSILON ? 1.f : biVector.e12()  };
    return GetSign(a*b*c);
}

TriVector OffsetPointAlongLine(TriVector const& point, BiVector const& direction, float const distance)
{
    BiVector const translator{ direction.Normalized() * distance };
    Motor const T{ 1.f, translator.e23(), translator.e31(), translator.e12(), 0.f, 0.f, 0.f, 0.f };
    return (T * point * ~T).Grade3();
}
