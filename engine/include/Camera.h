#pragma once

#include "FlyFish.h"
#include "PGA.h"
#include <cmath>
#include <optional>

// Pinhole camera looking from eye towards target; up is a direction (e123 = 0)
struct Camera {
    TriVector eye{0.0f, 3.5f, 8.0f};
    TriVector target{0.0f, 1.5f, 0.0f};
    TriVector up{0.0f, 1.0f, 0.0f, 0.0f};
    float fov{45.0f};       // Vertical field of view in degrees
    float exposure{1.0f};   // Scales the radiance before tone mapping
};

// The motor that moves the camera's own frame, at the origin looking down -z with +y up and +x to
// the right, into the world: the origin goes to the eye, -z to the viewing direction. It first turns
// -z towards the viewing direction, then rolls around that direction until +y points up.
// Returns nullopt when eye equals target or the view direction is parallel to the up hint.
[[nodiscard]] inline std::optional<Motor> CameraMotor(const Camera& camera) {
    // The view line, and the planes through it: along the up hint, with the camera's right as its
    // normal (the join of a line and a direction d has normal d x the line's direction), and along
    // that right, with the camera's up as its normal
    const BiVector view = Euclidean(camera.eye) & Euclidean(camera.target);
    const TriVector forward = LineDirection(view);
    const Vector side = view & camera.up;
    if (forward.VNorm() < 1e-6f || side.Norm() < 1e-6f * forward.VNorm() * camera.up.VNorm()) {
        return std::nullopt;
    }
    const TriVector right = PlaneNormal(side) * -1.0f;
    const Vector top = view & right;
    const TriVector up = PlaneNormal(top);

    const Motor turn = RotationBetween(TriVector(0.0f, 0.0f, -1.0f, 0.0f), forward);
    const TriVector turnedUp = ApplyMotor(turn, TriVector(0.0f, 1.0f, 0.0f, 0.0f));
    // The inner product of two lines through the origin is minus the product of their directions'
    // lengths and the cosine of their angle; turnedUp has length 1
    const TriVector origin(0.0f, 0.0f, 0.0f);
    const bool upsideDown = -((origin & turnedUp) | (origin & up)) < -0.9999f * up.VNorm();
    const Motor roll = upsideDown ? Motor::Rotation(180.0f, origin & forward) : RotationBetween(turnedUp, up);
    return TranslationMotor(Euclidean(camera.eye)) * roll * turn;
}

// The line from the eye through a point of a window of the given size, measured from its top left
// like the mouse: the ray the raytracer traces through that point
[[nodiscard]] inline std::optional<BiVector> ViewRay(const Camera& camera, float x, float y,
                                                     float width, float height) {
    const auto motor = CameraMotor(camera);
    if (!motor || width <= 0.0f || height <= 0.0f) return std::nullopt;

    const float tanHalfFov = std::tan(0.5f * camera.fov * DEG_TO_RAD);
    const float right = (2.0f * x / width - 1.0f) * (width / height) * tanHalfFov;
    const float up = (1.0f - 2.0f * y / height) * tanHalfFov;
    const TriVector direction = ApplyMotor(*motor, TriVector(right, up, -1.0f, 0.0f));
    return Euclidean(camera.eye) & direction;
}
