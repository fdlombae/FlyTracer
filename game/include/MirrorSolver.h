#pragma once

#include "FlyFish.h"
#include "PGA.h"
#include <optional>

// Two mirrors that steer a laser beam to a target. The beam leaves the laser through the first pivot,
// where the first mirror reflects it to the second pivot, where the second mirror reflects it to the
// target. Each mirror is the plane through its pivot that reflects the incoming direction into the
// outgoing one; together the two reflections are one motor, which moves the laser line onto the beam.
struct MirrorSolution {
    Vector firstMirror;
    Vector secondMirror;
    Motor tandem;        // secondMirror * firstMirror
    BiVector beam;       // The laser line moved by the tandem motor: the beam leaving the second mirror
    TriVector firstHit;  // Where the laser line meets the first mirror
    TriVector secondHit; // Where the beam reflected by the first mirror meets the second one
};

// The mirror through a pivot that reflects a beam coming from `from` towards `to`: its normal is the
// outgoing minus the incoming unit direction. None when the beam would have to go straight on.
[[nodiscard]] inline std::optional<Vector> MirrorThrough(const TriVector& from, const TriVector& pivot,
                                                         const TriVector& to) {
    const TriVector incoming = pivot - from;
    const TriVector outgoing = to - pivot;
    if (incoming.VNorm() < 1e-6f || outgoing.VNorm() < 1e-6f) return std::nullopt;

    const TriVector normal = outgoing * (1.0f / outgoing.VNorm()) - incoming * (1.0f / incoming.VNorm());
    if (normal.VNorm() < 1e-4f) return std::nullopt;
    return ((pivot & normal) | pivot).Normalized();
}

[[nodiscard]] inline std::optional<MirrorSolution> SolveMirrors(const TriVector& laser, const TriVector& firstPivot,
                                                                const TriVector& secondPivot, const TriVector& target) {
    const auto firstMirror = MirrorThrough(laser, firstPivot, secondPivot);
    const auto secondMirror = MirrorThrough(firstPivot, secondPivot, target);
    if (!firstMirror || !secondMirror) return std::nullopt;

    MirrorSolution solution;
    solution.firstMirror = *firstMirror;
    solution.secondMirror = *secondMirror;
    solution.tandem = ReflectionMotor(*firstMirror, *secondMirror);

    // Follow the beam: the meet of the laser line and the first mirror, then the reflected beam, which
    // seems to come from the laser's mirror image, meets the second mirror
    const BiVector laserLine = laser & firstPivot;
    solution.beam = ApplyMotor(solution.tandem, laserLine);
    solution.firstHit = Euclidean(laserLine ^ *firstMirror);
    const BiVector reflected = ReflectInPlane(*firstMirror, laser) & solution.firstHit;
    solution.secondHit = Euclidean(reflected ^ *secondMirror);
    return solution;
}
