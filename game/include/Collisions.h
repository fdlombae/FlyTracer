#ifndef SCENES_COLLISIONS_H
#define SCENES_COLLISIONS_H

#include <optional>

#include "FlyFish.h"

struct Sphere
{
    TriVector center;
    float radius;

};

struct Capsule
{
    TriVector bottomPoint;
    float radius, height;

    TriVector GetTopSphereOrigin() const;
    TriVector GetBottomSphereOrigin() const;
};

// NOTE: Wall is an immutable plane, meaning that it cannot be moved, only the sphere can.
// @return Whether there was a collision
std::optional<Motor> ProcessCollision(Sphere const&, Vector const& wall);
std::optional<Motor> ProcessCollision(Capsule const&, Vector const& wall);


#endif
