#include "Collisions.h"

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

std::optional<Motor> ProcessCollision(Sphere const& sphere, Vector const& wall)
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

std::optional<Motor> ProcessCollision(Capsule const& capsule, Vector const& wall)
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
        Vector vector{ wall * (smallestSignedDistance - capsule.radius) };
        // NOTE: Vector's Euclidean coefficients are the components of its Euclidean normal
        return Motor{ 1.f, vector.e1(), vector.e2(), vector.e3(), 0.f, 0.f, 0.f, 0.f};
    }
    return std::nullopt;
}
