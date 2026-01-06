#include "Collisions.h"

#include <iostream>

#include "GlobalConstants.h"

TriVector Capsule::GetTopSphereOrigin() const
{
    TriVector topOrigin{ bottomPoint };
    topOrigin.e013() = height - radius;
    return topOrigin;
}

TriVector Capsule::GetBottomSphereOrigin() const
{
    TriVector bottomOrigin{ bottomPoint };
    bottomOrigin.e013() = radius;
    return bottomOrigin;
}

std::optional<Motor> ProcessWallCollision(Sphere const& sphere, Vector const& wall)
{
    // 2. For each plane, get the distance to it and check if distance is smaller than the collider's radius
    if (float const planeCameraSignedDistance{ (wall & sphere.center) };// NOTE: Both objects are already normalized
        planeCameraSignedDistance < sphere.radius)
    {
        // 3. Move the camera along plane's normal by distance - radius
        Vector const t{ wall * (planeCameraSignedDistance - sphere.radius) };// Translator
        // NOTE: Vector's Euclidean coefficients are the components of its Euclidean normal
        return Motor{ 1.f, t.e1(), t.e2(), t.e3(), 0.f, 0.f, 0.f, 0.f};
    }
    return std::nullopt;
}

std::optional<Motor> ProcessWallCollision(Capsule const& capsule, Vector const& wall)
{
    // 2.1. Getting the signed distances of each sphere to the plane
    float const topDistance{ wall & capsule.GetTopSphereOrigin() },
        bottomDistance{ wall & capsule.GetBottomSphereOrigin() };
    // 2.2. Finding the closest sphere
    float const smallestSignedDistance{
        std::abs(topDistance) < std::abs(bottomDistance) ? topDistance : bottomDistance
    };
    // 2.3. Seeing if the distance of the closest one is smaller than the radius
    if (std::abs(smallestSignedDistance) < capsule.radius)
    {
        // 4. Moving the character away by normal * (distance - radius)
        Vector translation{ wall * (smallestSignedDistance - capsule.radius) };
        // NOTE: Vector's Euclidean coefficients are the components of its Euclidean normal
        return Motor{ 1.f, translation.e1(), translation.e2(), translation.e3(), 0.f, 0.f, 0.f, 0.f};
    }
    return std::nullopt;
}

std::optional<std::pair<Motor, Motor>> ProcessCollision(Sphere const& s1, Sphere const& s2)
{
    TriVector const A{ s1.center.Normalized() }, B{ s2.center.Normalized() };
    // 1. Get the distance between the two and see if it is smaller than sum of radii
    if (float const distance{ (A & B).Norm() }, radii{ s1.radius + s2.radius };
        distance < radii)
    {
        // 2. Getting the translation motors
        Vector const directionB{ ((!A ^ !B) & e123).Normalized() };
        // 2.2. Getting the movement directions
        float const translationDistance{ distance - radii };
        Vector const translationA{ -directionB * translationDistance }, translationB{ directionB * translationDistance };
        Motor const TA{ 1.f, translationA.e1(), translationA.e2(), translationA.e3(), 0.f, 0.f, 0.f, 0.f };
        Motor const TB{ 1.f, translationB.e1(), translationB.e2(), translationB.e3(), 0.f, 0.f, 0.f, 0.f };
        return std::make_pair(TA, TB);
    }
    return std::nullopt;
}
