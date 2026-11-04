#pragma once

#include "GameScene.h"
#include "MirrorSolver.h"
#include "OrbitCamera.h"
#include <array>
#include <optional>

// HornetTracer: a laser at the origin always fires along +x. Two mirrors next to it, turned by the
// PGA mirror solver, steer the beam to a pheasant flying around. Space fires; a hit pheasant tumbles
// down and flies on.
class HornetScene final : public GameScene {
public:
    explicit HornetScene(const std::string& resourceDir);

protected:
    void OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnGui() override;

private:
    static constexpr float kFlightSpeed = 0.35f;   // Radians per second around the laser
    static constexpr float kFallDuration = 1.5f;   // Seconds
    static constexpr float kShotDuration = 0.25f;  // Seconds the fired beam is shown
    static constexpr float kPheasantScale = 0.4f;
    static constexpr float kMirrorScale = 1.4f;

    [[nodiscard]] static TriVector FlightPosition(float time);
    [[nodiscard]] Motor PheasantTransform() const;
    [[nodiscard]] TriVector PheasantCenter() const;
    void UpdateMirrors();
    void DrawBeam();

    OrbitCamera m_orbit{TriVector(0.0f, 5.0f, 0.0f), 0.5f, 0.45f, 44.0f, 10.0f, 90.0f};
    bool m_guiVisible{true};

    uint32_t m_pheasantInstanceId{0};
    TriVector m_pheasantCenter{0.0f, 0.0f, 0.0f};  // The middle of the scaled model, before its motor
    std::array<uint32_t, 2> m_mirrorInstanceIds{};
    float m_flightTime{0.0f};
    bool m_paused{false};

    std::optional<MirrorSolution> m_solution;
    TriVector m_target{0.0f, 0.0f, 0.0f};

    int m_hits{0};
    float m_shotTimer{0.0f};
    std::optional<float> m_fallTime;    // Seconds since the hit while falling
    Motor m_hitTransform{1, 0, 0, 0, 0, 0, 0, 0};
};
