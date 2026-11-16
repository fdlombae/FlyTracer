#pragma once

#include "Input.h"
#include "PGA.h"
#include <algorithm>

// Camera circling a target: dragging with the right mouse button orbits, scrolling zooms
struct OrbitCamera {
    TriVector target{TriVector::Point(0.0f, 0.0f, 0.0f)};
    float yaw{0.0f};    // Radians around the vertical axis, 0 looks down -z
    float pitch{0.3f};  // Radians above the horizon
    float distance{50.0f};
    float minDistance{10.0f};
    float maxDistance{150.0f};
    float sensitivity{0.005f};  // Radians per pixel dragged

    void HandleInput(const Input& input) noexcept {
        if (input.IsDown(MouseButton::Right)) {
            yaw += input.MouseDeltaX() * sensitivity;
            pitch = std::clamp(pitch + input.MouseDeltaY() * sensitivity, -1.4f, 1.4f);
        }
        distance = std::clamp(distance - 2.0f * input.ScrollDelta(), minDistance, maxDistance);
    }

    // The point at the given distance behind the target, rotated by pitch around x and yaw around y
    [[nodiscard]] TriVector Eye() const {
        const BiVector xAxis = BiVector::LineFromPoints(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        const BiVector yAxis = BiVector::LineFromPoints(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        const Motor pitchRotation = Motor::Rotation(Angle::FromRadians(-pitch), xAxis);
        const Motor yawRotation = Motor::Rotation(Angle::FromRadians(yaw), yAxis);
        const Motor orbit = TranslationMotor(target) * yawRotation * pitchRotation;
        return ApplyMotor(orbit, TriVector::Point(0.0f, 0.0f, distance));
    }
};
