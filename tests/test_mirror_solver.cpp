#include <gtest/gtest.h>
#include "MirrorSolver.h"

namespace {

void ExpectNear(const TriVector& actual, const TriVector& expected, float tolerance = 1e-4f) {
    EXPECT_NEAR(actual.e032(), expected.e032(), tolerance);
    EXPECT_NEAR(actual.e013(), expected.e013(), tolerance);
    EXPECT_NEAR(actual.e021(), expected.e021(), tolerance);
}

const TriVector kLaser(0.0f, 0.0f, 0.0f);
const TriVector kFirstPivot(3.0f, 0.0f, 0.0f);
const TriVector kSecondPivot(3.0f, 2.0f, 0.0f);

} // anonymous namespace

// The beam hits both pivots and the line leaving the second mirror goes through the target, towards it
TEST(MirrorSolverTest, BeamReachesTarget) {
    const TriVector target(10.0f, 6.0f, 4.0f);
    const auto solution = SolveMirrors(kLaser, kFirstPivot, kSecondPivot, target);
    ASSERT_TRUE(solution.has_value());

    ExpectNear(solution->firstHit, kFirstPivot);
    ExpectNear(solution->secondHit, kSecondPivot);
    ExpectNear(ClosestPointOnLine(solution->beam, target), target);

    const TriVector toTarget = target - solution->secondHit;
    const float alongBeam = solution->beam.e23() * toTarget.e032() + solution->beam.e31() * toTarget.e013() +
                            solution->beam.e12() * toTarget.e021();
    EXPECT_GT(alongBeam, 0.0f);
}

// Turning the beam from +x to +y takes a mirror at 45 degrees
TEST(MirrorSolverTest, FirstMirrorIsAt45Degrees) {
    const auto solution = SolveMirrors(kLaser, kFirstPivot, kSecondPivot, TriVector(0.0f, 5.0f, 9.0f));
    ASSERT_TRUE(solution.has_value());
    EXPECT_NEAR(std::abs(solution->firstMirror.e1()), std::sqrt(0.5f), 1e-5f);
    EXPECT_NEAR(std::abs(solution->firstMirror.e2()), std::sqrt(0.5f), 1e-5f);
    EXPECT_NEAR(solution->firstMirror.e3(), 0.0f, 1e-5f);
}

// The tandem motor does what the two reflections do one after the other
TEST(MirrorSolverTest, TandemIsBothReflections) {
    const auto solution = SolveMirrors(kLaser, kFirstPivot, kSecondPivot, TriVector(-4.0f, 8.0f, 2.0f));
    ASSERT_TRUE(solution.has_value());
    const TriVector point(1.0f, -2.0f, 0.5f);
    const TriVector twice = ReflectInPlane(solution->secondMirror, ReflectInPlane(solution->firstMirror, point));
    ExpectNear(ApplyMotor(solution->tandem, point), twice);
}

// A target straight on behind the second pivot needs no reflection there: the mirror would be edge-on
TEST(MirrorSolverTest, StraightOnHasNoMirror) {
    EXPECT_FALSE(SolveMirrors(kLaser, kFirstPivot, kSecondPivot, TriVector(3.0f, 10.0f, 0.0f)).has_value());
}
