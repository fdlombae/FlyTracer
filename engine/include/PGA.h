#pragma once

#include "FlyFish.h"
#include <cmath>
#include <string>

// Small helpers on top of FlyFish. Every transformation is a sandwich product X' = V X ~V; the
// shaders use FlyFish's operations through engine/shaders/flyfish.glsl.

// Motor translating by (x, y, z): Motor::Translation along the direction (x, y, z)
[[nodiscard]] inline Motor TranslationMotor(float x, float y, float z) noexcept {
    return Motor{1.0f, -0.5f * x, -0.5f * y, -0.5f * z, 0.0f, 0.0f, 0.0f, 0.0f};
}

// Motor translating the origin to the given point
[[nodiscard]] inline Motor TranslationMotor(const TriVector& position) noexcept {
    return TranslationMotor(position.e032(), position.e013(), position.e021());
}

// Motor that reflects in plane `first` and then in plane `second`: the geometric product second * first
[[nodiscard]] inline Motor ReflectionMotor(const Vector& first, const Vector& second) noexcept {
    return second * first;
}

// Raises a motor that is a pure rotation or a pure translation, like any product of two
// reflections, to the power t: t = 0 is the identity, t = 1 the motor itself. Rotations take the
// shortest way, by at most half a turn.
[[nodiscard]] inline Motor MotorPower(const Motor& motor, float t) {
    const BiVector rotation = motor.Grade2();
    const float sinHalfAngle = rotation.Norm();
    if (sinHalfAngle < 1e-6f) {
        // M = s + d with d the translation part: (M / s)^t = 1 + t d / s
        const float factor = t / motor.s();
        return Motor{1.0f, factor * motor.e01(), factor * motor.e02(), factor * motor.e03(), 0.0f, 0.0f, 0.0f, 0.0f};
    }
    // M = cos(a) - sin(a) L for a normalized line L rotates by 2a around L; M and -M are the same motion
    const float sign = motor.s() < 0.0f ? -1.0f : 1.0f;
    const float halfAngle = std::atan2(sinHalfAngle, sign * motor.s());
    return Motor::Rotation(2.0f * halfAngle * t * RAD_TO_DEG, rotation * -sign);
}

// M / sqrt(M ~M): a unit rotor part and no e0123 in M ~M, so M is a rigid motion again after the
// drift that repeated products accumulate. M ~M = 1 + b e0123 has the inverse square root 1 - b/2 e0123.
[[nodiscard]] inline Motor NormalizedMotor(const Motor& motor) {
    const Motor unitRotor = motor.Normalized();
    const float b = (unitRotor * ~unitRotor).e0123();
    return unitRotor * Motor{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f * b};
}

// Divides out the weight (e123), so points end up with w = 1; directions (w = 0) are unchanged
[[nodiscard]] inline TriVector Euclidean(const TriVector& point) noexcept {
    const float w = point.e123();
    if (std::abs(w) < 1e-12f) return point;
    return TriVector(point.e032() / w, point.e013() / w, point.e021() / w, 1.0f);
}

// The direction of a line, its (e23, e31, e12) part, as an ideal point
[[nodiscard]] inline TriVector LineDirection(const BiVector& line) noexcept {
    return TriVector(line.e23(), line.e31(), line.e12(), 0.0f);
}

// The normal of a plane, its (e1, e2, e3) part, as an ideal point
[[nodiscard]] inline TriVector PlaneNormal(const Vector& plane) noexcept {
    return TriVector(plane.e1(), plane.e2(), plane.e3(), 0.0f);
}

// The point of a line closest to a point: where the plane through the point perpendicular to the line
// meets the line
[[nodiscard]] inline TriVector ClosestPointOnLine(const BiVector& line, const TriVector& point) {
    return Euclidean((point | line) ^ line);
}

// The point of a plane closest to a point: where the line through the point perpendicular to the plane
// meets the plane
[[nodiscard]] inline TriVector ClosestPointOnPlane(const Vector& plane, const TriVector& point) {
    return Euclidean((plane | point) ^ plane);
}

// M p ~M
[[nodiscard]] inline TriVector ApplyMotor(const Motor& motor, const TriVector& point) {
    return Euclidean((motor * point * ~motor).Grade3());
}

// M L ~M for a line; a motor keeps the line's orientation
[[nodiscard]] inline BiVector ApplyMotor(const Motor& motor, const BiVector& line) {
    return (motor * line * ~motor).Grade2();
}

// M a ~M for a plane
[[nodiscard]] inline Vector ApplyMotor(const Motor& motor, const Vector& plane) {
    return (motor * plane * ~motor).Grade1();
}

// The rotation around the origin that turns direction `from` into direction `to`, as two reflections:
// in the plane perpendicular to `from`, which turns it into -from, then in the plane perpendicular to
// the direction halfway between them. Opposite directions take a half turn around a perpendicular axis.
[[nodiscard]] inline Motor RotationBetween(const TriVector& from, const TriVector& to) {
    const TriVector origin(0.0f, 0.0f, 0.0f);
    const TriVector a = from * (1.0f / from.VNorm());
    const TriVector halfway = a + to * (1.0f / to.VNorm());
    const Vector first = (origin & a) | origin;
    if (halfway.VNorm() > 1e-6f) {
        return ReflectionMotor(first, ((origin & halfway) | origin).Normalized());
    }
    const TriVector side = std::abs(a.e013()) < 0.9f ? TriVector(0.0f, 1.0f, 0.0f, 0.0f) : TriVector(1.0f, 0.0f, 0.0f, 0.0f);
    return ReflectionMotor(first, ((origin & a) & side).Normalized());
}

// FlyFish's notation of an element, like "-2*e0 + 0.88*e1", with the coefficients rounded
template<typename Element>
[[nodiscard]] std::string ToRoundedString(const Element& element, int decimals = 2) {
    const float scale = std::pow(10.0f, static_cast<float>(decimals));
    Element rounded = element;
    for (float& coefficient : rounded) {
        coefficient = std::round(coefficient * scale) / scale;
    }
    return rounded.ToString();
}

// Reflection of a point or direction in a plane: P p ~P
[[nodiscard]] inline TriVector ReflectInPlane(const Vector& plane, const TriVector& point) {
    return Euclidean((plane * point * ~plane).Grade3());
}
