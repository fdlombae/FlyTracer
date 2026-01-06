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

template<typename T>
concept Collider = std::is_same_v<T, Sphere> || std::is_same_v<T, Capsule>;

// NOTE: Wall is an immutable plane, meaning that it cannot be moved, only the sphere can
// @return Motor by which to move the collider or std::nullopt if no collision
std::optional<Motor> ProcessWallCollision(Sphere const&, Vector const& wall);
std::optional<Motor> ProcessWallCollision(Capsule const&, Vector const& wall);
// @return Translation motors for spheres given in the same order as input
std::optional<std::pair<Motor, Motor>> ProcessCollision(Sphere const&, Sphere const&);

#endif
