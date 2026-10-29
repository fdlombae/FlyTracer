#include <gtest/gtest.h>
#include "Camera.h"
#include "PGA.h"
#include <array>
#include <cmath>

namespace {

using Float3 = std::array<float, 3>;

Float3 Coordinates(const TriVector& x) {
    return {x.e032(), x.e013(), x.e021()};
}

float Dot(const Float3& a, const Float3& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Float3 Cross(const Float3& a, const Float3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

void ExpectNear(const Float3& actual, const Float3& expected) {
    for (size_t i = 0; i < 3; ++i) EXPECT_NEAR(actual[i], expected[i], 1e-5f);
}

// The camera frame in the world: the motor applied to the camera's own axes
struct Frame {
    Float3 right;
    Float3 up;
    Float3 forward;
};

Frame FrameOf(const Motor& motor) {
    return {Coordinates(ApplyMotor(motor, TriVector(1.0f, 0.0f, 0.0f, 0.0f))),
            Coordinates(ApplyMotor(motor, TriVector(0.0f, 1.0f, 0.0f, 0.0f))),
            Coordinates(ApplyMotor(motor, TriVector(0.0f, 0.0f, -1.0f, 0.0f)))};
}

} // anonymous namespace

// Looking down -z with y up, the camera frame is the world frame
TEST(CameraTest, LookDownNegativeZ) {
    const auto motor = CameraMotor(Camera{TriVector(0.0f, 0.0f, 5.0f), TriVector(0.0f, 0.0f, 0.0f),
                                          TriVector(0.0f, 1.0f, 0.0f, 0.0f)});
    ASSERT_TRUE(motor.has_value());
    const Frame frame = FrameOf(*motor);
    ExpectNear(frame.right, {1.0f, 0.0f, 0.0f});
    ExpectNear(frame.up, {0.0f, 1.0f, 0.0f});
    ExpectNear(frame.forward, {0.0f, 0.0f, -1.0f});
    ExpectNear(Coordinates(ApplyMotor(*motor, TriVector(0.0f, 0.0f, 0.0f))), {0.0f, 0.0f, 5.0f});
}

// The camera looks at the target, upright as far as the up hint allows, from the eye
TEST(CameraTest, MotorLooksAtTheTarget) {
    const TriVector up(0.0f, 1.0f, 0.0f, 0.0f);
    const std::array cameras{
        Camera{TriVector(3.0f, 20.0f, 50.0f), TriVector(0.0f, 5.0f, 0.0f), TriVector(0.0f, 2.0f, 0.0f, 0.0f)},
        Camera{TriVector(0.0f, 1.0f, -8.0f), TriVector(0.0f, 1.0f, 0.0f), up},  // Looking along +z
        Camera{TriVector(0.0f, 1.0f, 8.0f), TriVector(0.0f, 1.0f, 0.0f), up},   // Looking along -z
        Camera{TriVector(1.0f, 2.0f, 3.0f), TriVector(2.0f, 2.0f, 3.0f), TriVector(0.0f, -1.0f, 0.0f, 0.0f)},
        Camera{TriVector(0.0f, 1.0f, 8.0f), TriVector(0.0f, 1.0f, 0.0f), TriVector(0.0f, -1.0f, 0.0f, 0.0f)},
    };
    for (const Camera& camera : cameras) {
        const auto motor = CameraMotor(camera);
        ASSERT_TRUE(motor.has_value());
        const Frame frame = FrameOf(*motor);
        const Float3 toTarget = Coordinates(camera.target - camera.eye);
        const float distance = std::sqrt(Dot(toTarget, toTarget));
        ExpectNear(frame.forward, {toTarget[0] / distance, toTarget[1] / distance, toTarget[2] / distance});
        EXPECT_NEAR(Dot(frame.up, frame.forward), 0.0f, 1e-5f);
        EXPECT_GT(Dot(frame.up, Coordinates(camera.up)), 0.0f);
        ExpectNear(frame.right, Cross(frame.forward, frame.up));  // Right-handed
        const TriVector eye = ApplyMotor(*motor, TriVector(0.0f, 0.0f, 0.0f));
        EXPECT_NEAR((eye - camera.eye).VNorm(), 0.0f, 1e-4f);
    }
}

// Test degenerate cameras are rejected
// The ray through the window center goes through the target; one through the right edge leans right
TEST(CameraTest, ViewRay) {
    const Camera camera{TriVector(0.0f, 0.0f, 10.0f), TriVector(0.0f, 0.0f, 0.0f), TriVector(0.0f, 1.0f, 0.0f, 0.0f), 90.0f};
    const auto center = ViewRay(camera, 400.0f, 300.0f, 800.0f, 600.0f);
    ASSERT_TRUE(center.has_value());
    const TriVector closest = ClosestPointOnLine(*center, camera.target);
    EXPECT_NEAR((closest - camera.target).VNorm(), 0.0f, 1e-5f);

    // fov 90: at the right edge of an 800 x 600 window the ray goes 4/3 to the right per unit forward
    const auto right = ViewRay(camera, 800.0f, 300.0f, 800.0f, 600.0f);
    ASSERT_TRUE(right.has_value());
    EXPECT_NEAR(right->e23() / -right->e12(), 4.0f / 3.0f, 1e-5f);
    EXPECT_FALSE(ViewRay(camera, 0.0f, 0.0f, 0.0f, 0.0f).has_value());
}

TEST(CameraTest, DegenerateCamera) {
    const TriVector up(0.0f, 1.0f, 0.0f, 0.0f);
    EXPECT_FALSE(CameraMotor(Camera{TriVector(1.0f, 2.0f, 3.0f), TriVector(1.0f, 2.0f, 3.0f), up}).has_value());
    EXPECT_FALSE(CameraMotor(Camera{TriVector(0.0f, 5.0f, 0.0f), TriVector(0.0f, 0.0f, 0.0f), up}).has_value());
}
