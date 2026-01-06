#ifndef UTILS_H
#define UTILS_H
#include "FlyFish.h"

float GetSign(float value);
float GetEuclideanSign(BiVector const& biVector);
// @param direction must be normalized
TriVector OffsetPointAlongLine(TriVector const& point, BiVector const& direction, float distance);

#endif
