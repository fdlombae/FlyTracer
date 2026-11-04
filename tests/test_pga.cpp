#include <gtest/gtest.h>
#include "GameScene.h"
#include "OrbitCamera.h"
#include "PGA.h"
#include <cmath>

namespace {

void ExpectNear(const TriVector& actual, float x, float y, float z, float tolerance = 1e-4f) {
    EXPECT_NEAR(actual.e032(), x, tolerance);
    EXPECT_NEAR(actual.e013(), y, tolerance);
    EXPECT_NEAR(actual.e021(), z, tolerance);
}

const BiVector kYAxis(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

} // anonymous namespace

TEST(PGATest, TranslationMotorMatchesFlyFish) {
    const Motor ours = TranslationMotor(3.0f, 0.0f, 0.0f);
    const Motor flyFish = Motor::Translation(3.0f, BiVector(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    for (int i = 0; i < 8; ++i) {
        EXPECT_FLOAT_EQ(ours[i], flyFish[i]);
    }
    ExpectNear(ApplyMotor(TranslationMotor(1.0f, 2.0f, 3.0f), TriVector(1.0f, 1.0f, 1.0f)), 2.0f, 3.0f, 4.0f);
}

// T * R rotates first: the sandwich product applies the rightmost factor first
TEST(PGATest, MotorCompositionOrder) {
    const Motor rotation = Motor::Rotation(90.0f, kYAxis);
    const Motor translation = TranslationMotor(5.0f, 0.0f, 0.0f);
    ExpectNear(ApplyMotor(translation * rotation, TriVector(1.0f, 0.0f, 0.0f)), 5.0f, 0.0f, -1.0f);
    ExpectNear(ApplyMotor(rotation * translation, TriVector(1.0f, 0.0f, 0.0f)), 0.0f, 0.0f, -6.0f);
}

// The formula the shader uses gives the same points as FlyFish's sandwich product
TEST(PGATest, ReflectInPlane) {
    const Vector planeXIs2(-2.0f, 1.0f, 0.0f, 0.0f);
    ExpectNear(ReflectInPlane(planeXIs2, TriVector(5.0f, 1.0f, 0.0f)), -1.0f, 1.0f, 0.0f);
    // Directions only flip their component along the normal
    ExpectNear(ReflectInPlane(planeXIs2, TriVector(1.0f, 1.0f, 0.0f, 0.0f)), -1.0f, 1.0f, 0.0f);
}

// Reflecting in two planes equals the motor of the two planes
TEST(PGATest, ReflectionMotorEqualsTwoReflections) {
    const Vector first(0.5f, 1.0f, 0.0f, 0.0f);
    const Vector second = Vector(-1.0f, 1.0f, 1.0f, 0.0f) / std::sqrt(2.0f);
    const TriVector x(1.0f, 2.0f, 3.0f);

    const TriVector twice = ReflectInPlane(second, ReflectInPlane(first, x));
    const TriVector viaMotor = ApplyMotor(ReflectionMotor(first, second), x);
    ExpectNear(viaMotor, twice.e032(), twice.e013(), twice.e021());
}

// Parallel planes facing each other give a motor with s = -1, still a translation by twice their distance
TEST(PGATest, MotorPowerOfTranslation) {
    const Vector up(-1.0f, 0.0f, 1.0f, 0.0f);     // y = 1
    const Vector down(-1.0f, 0.0f, -1.0f, 0.0f);  // y = -1, normal pointing down
    const Motor motor = ReflectionMotor(up, down);
    const TriVector origin(0.0f, 0.0f, 0.0f);

    ExpectNear(ApplyMotor(MotorPower(motor, 1.0f), origin), 0.0f, -4.0f, 0.0f);
    ExpectNear(ApplyMotor(MotorPower(motor, 0.25f), origin), 0.0f, -1.0f, 0.0f);
}

TEST(PGATest, MotorPowerOfRotation) {
    const Vector first(0.5f, 1.0f, 0.0f, 0.0f);
    const Vector second = Vector(-1.0f, 1.0f, 1.0f, 0.0f) / std::sqrt(2.0f);
    const TriVector x(1.0f, 2.0f, 3.0f);

    for (const Motor& motor : {ReflectionMotor(first, second), ReflectionMotor(first, second * -1.0f)}) {
        const TriVector full = ApplyMotor(motor, x);
        ExpectNear(ApplyMotor(MotorPower(motor, 1.0f), x), full.e032(), full.e013(), full.e021());
        ExpectNear(ApplyMotor(MotorPower(motor, 0.0f), x), x.e032(), x.e013(), x.e021());

        const Motor half = MotorPower(motor, 0.5f);
        ExpectNear(ApplyMotor(half * half, x), full.e032(), full.e013(), full.e021());
    }
}

TEST(PGATest, ApplyMotorToPlane) {
    const Vector floor(0.0f, 0.0f, 1.0f, 0.0f);  // y = 0
    const BiVector xAxisAtHeight1 = TriVector(0.0f, 1.0f, 0.0f) & TriVector(1.0f, 1.0f, 0.0f);
    const Vector turned = ApplyMotor(Motor::Rotation(90.0f, xAxisAtHeight1), floor);

    // Right-handed around +x: the floor through (0, 0, 0) becomes the plane z = -1 through (0, 1, -1)
    EXPECT_NEAR(turned.e1(), 0.0f, 1e-6f);
    EXPECT_NEAR(turned.e2(), 0.0f, 1e-6f);
    EXPECT_NEAR(turned.e3(), 1.0f, 1e-6f);
    EXPECT_NEAR(turned.e0(), 1.0f, 1e-6f);
}

// Accumulated products drift away from a rigid motion; normalizing keeps the rotation and the
// position of the origin
TEST(PGATest, NormalizedMotorOfScaledMotor) {
    const Motor rigid = TranslationMotor(1.0f, 2.0f, 3.0f) *
                        Motor::Rotation(40.0f, BiVector(0.0f, 0.0f, 0.0f, 0.3f, 0.5f, 0.8f));

    const Motor normalized = NormalizedMotor(rigid * 1.7f);
    for (int i = 0; i < 8; ++i) {
        EXPECT_NEAR(normalized[i], rigid[i], 1e-5f);
    }
}

// A motor whose M ~M has an e0123 part is not a rigid motion; normalized it is, and it keeps the rotation
TEST(PGATest, NormalizedMotorIsRigid) {
    const Motor drifted{0.9f, 0.3f, -0.2f, 0.1f, 0.2f, 0.1f, 0.3f, 0.05f};

    const Motor normalized = NormalizedMotor(drifted);
    const Motor squared = normalized * ~normalized;
    EXPECT_NEAR(squared.s(), 1.0f, 1e-6f);
    EXPECT_NEAR(squared.e0123(), 0.0f, 1e-6f);
    EXPECT_NEAR(normalized.e23() / normalized.s(), drifted.e23() / drifted.s(), 1e-6f);
    EXPECT_NEAR(normalized.e12() / normalized.s(), drifted.e12() / drifted.s(), 1e-6f);
}

// FlyFish's notation with two decimals; components that round to zero disappear
TEST(PGATest, ToRoundedString) {
    EXPECT_EQ(ToRoundedString(Vector(-2.0f, 0.8798f, 0.001f, -0.3712f)), "-2*e0 + 0.88*e1 - 0.37*e3");
    EXPECT_EQ(ToRoundedString(BiVector(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f)), "e31");
    EXPECT_EQ(ToRoundedString(Vector(0.004f, 0.0f, 0.0f, 0.0f)), "0");
}

// The join of a point p and a direction v is the Plücker line: direction v, moment m = p x v
TEST(PGATest, JoinGivesPluckerCoordinates) {
    const BiVector line = TriVector(0.0f, 1.0f, 0.0f) & TriVector(2.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(line.e23(), 2.0f);
    EXPECT_FLOAT_EQ(line.e31(), 0.0f);
    EXPECT_FLOAT_EQ(line.e12(), 0.0f);
    // (0, 1, 0) x (2, 0, 0) = (0, 0, -2)
    EXPECT_FLOAT_EQ(line.e01(), 0.0f);
    EXPECT_FLOAT_EQ(line.e02(), 0.0f);
    EXPECT_FLOAT_EQ(line.e03(), -2.0f);
}

TEST(PGATest, ClosestPoints) {
    const BiVector line = TriVector(0.0f, 1.0f, 2.0f) & TriVector(1.0f, 0.0f, 0.0f, 0.0f);  // y = 1, z = 2
    ExpectNear(ClosestPointOnLine(line, TriVector(0.0f, 0.0f, 0.0f)), 0.0f, 1.0f, 2.0f);
    ExpectNear(ClosestPointOnLine(line, TriVector(5.0f, 3.0f, 2.0f)), 5.0f, 1.0f, 2.0f);

    const Vector planeYIs2(-2.0f, 0.0f, 1.0f, 0.0f);
    ExpectNear(ClosestPointOnPlane(planeYIs2, TriVector(3.0f, 7.0f, -1.0f)), 3.0f, 2.0f, -1.0f);
}

// A motor turns a line with its orientation: the x-axis turned 90 degrees around y runs along -z
TEST(PGATest, ApplyMotorToLine) {
    const BiVector xAxis = TriVector(0.0f, 0.0f, 0.0f) & TriVector(1.0f, 0.0f, 0.0f);
    const BiVector turned = ApplyMotor(Motor::Rotation(90.0f, kYAxis), xAxis);
    EXPECT_NEAR(turned.e23(), 0.0f, 1e-6f);
    EXPECT_NEAR(turned.e12(), -1.0f, 1e-6f);
    EXPECT_NEAR(turned.e01(), 0.0f, 1e-6f);
}

TEST(PGATest, RotationBetween) {
    const TriVector x(1.0f, 0.0f, 0.0f, 0.0f);
    const TriVector direction(0.0f, 2.0f, 2.0f, 0.0f);
    const TriVector turned = ApplyMotor(RotationBetween(x, direction), x);
    ExpectNear(turned, 0.0f, std::sqrt(0.5f), std::sqrt(0.5f));

    ExpectNear(ApplyMotor(RotationBetween(x, x * -3.0f), x), -1.0f, 0.0f, 0.0f);
}

TEST(PGATest, InstanceToWorld) {
    MeshInstance instance;
    instance.transform = TranslationMotor(0.0f, 10.0f, 0.0f) * Motor::Rotation(90.0f, kYAxis);
    instance.scale = 2.0f;
    ExpectNear(instance.ToWorld(TriVector(1.0f, 0.0f, 0.0f)), 0.0f, 10.0f, -2.0f);

    instance.mirror = Vector(0.0f, 0.0f, 1.0f, 0.0f);  // y = 0
    ExpectNear(instance.ToWorld(TriVector(1.0f, 0.0f, 0.0f)), 0.0f, -10.0f, -2.0f);
}

// A mirrored instance reflected again becomes a motor that does both reflections
TEST(PGATest, ReflectedTwiceIsAMotor) {
    MeshInstance instance;
    instance.transform = TranslationMotor(1.0f, 2.0f, 3.0f);
    const Vector first(0.0f, 1.0f, 0.0f, 0.0f);   // x = 0
    const Vector second(-4.0f, 1.0f, 0.0f, 0.0f); // x = 4

    const MeshInstance once = instance.Reflected(first);
    ASSERT_TRUE(once.mirror.has_value());
    ExpectNear(once.ToWorld(TriVector(0.0f, 0.0f, 0.0f)), -1.0f, 2.0f, 3.0f);

    const MeshInstance twice = once.Reflected(second);
    EXPECT_FALSE(twice.mirror.has_value());
    ExpectNear(twice.ToWorld(TriVector(0.0f, 0.0f, 0.0f)), 9.0f, 2.0f, 3.0f);
}

TEST(PGATest, OrbitCameraEye) {
    OrbitCamera orbit{TriVector(0.0f, 5.0f, 0.0f), 0.0f, 0.0f, 10.0f};
    ExpectNear(orbit.Eye(), 0.0f, 5.0f, 10.0f);

    orbit.pitch = 0.3f;
    orbit.yaw = 0.5f;
    const TriVector eye = orbit.Eye();
    ExpectNear(eye, 10.0f * std::sin(0.5f) * std::cos(0.3f), 5.0f + 10.0f * std::sin(0.3f),
               10.0f * std::cos(0.5f) * std::cos(0.3f));
}
