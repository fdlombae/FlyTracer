#pragma once

#include "GameScene.h"
#include "OrbitCamera.h"
#include <array>
#include <numbers>
#include <optional>

class BiReflectionScene final : public GameScene {
public:
    enum class DemoMode { Interactive, Rotation, Translation };

    explicit BiReflectionScene(const std::string& resourceDir,
                               DemoMode mode = DemoMode::Interactive);

protected:
    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnInput(const Input& input) override;
    void OnGui() override;

private:
    static constexpr float kPi = std::numbers::pi_v<float>;
    static constexpr float kPlaneScale = 30.0f;
    static constexpr float kPheasantScale = 0.5f;
    static constexpr float kYawSpeed = 1.5f;
    static constexpr float kDistanceSpeed = 8.0f;
    static constexpr float kPitchSpeed = 1.0f;

    DemoMode m_demoMode{DemoMode::Interactive};
    bool m_guiVisible{true};
    OrbitCamera m_orbit{TriVector(0.0f, 5.0f, 0.0f)};

    // Meshes
    uint32_t m_pheasantMeshId{0};
    uint32_t m_pheasantInstanceId{0};
    std::array<uint32_t, 2> m_quadMeshIds{0, 0};

    // Reflection planes (up to 2)
    struct ReflectionPlane {
        Vector plane{0.0f, 1.0f, 0.0f, 0.0f};
        float yaw{0.0f};    // Radians around the y-axis, right-handed: the normal (cos, 0, -sin) at pitch 0
        float pitch{0.0f};
        float distance{0.0f};
        Scene::Color color{1.0f, 1.0f, 0.0f};
        std::optional<uint32_t> meshInstanceId;
    };
    std::array<ReflectionPlane, 2> m_planes;
    int m_planeCount{0};
    int m_selectedPlane{-1};

    // Reflected pheasants: in plane 1, and in plane 1 followed by plane 2
    std::array<std::optional<uint32_t>, 2> m_reflectedInstances;
    std::array<bool, 2> m_reflected{false, false};  // Whether each plane's reflection is active

    // Intersection
    bool m_planesIntersect{false};
    BiVector m_intersectionLine{0, 0, 0, 0, 0, 0};

    // Intersection lock
    bool m_intersectionLocked{false};
    BiVector m_lockedLine{0, 0, 0, 0, 0, 0};
    Vector m_lockedReference{0, 0, 1, 0};
    std::array<float, 2> m_lockedAngle{0.0f, 0.0f};  // Around the locked line, from the reference plane

    // Animation: the pheasant follows the bireflection motor from 0 to 1
    bool m_animating{false};
    float m_animProgress{0.0f};
    float m_animDuration{2.0f};
    Motor m_animMotor{1, 0, 0, 0, 0, 0, 0, 0};
    Motor m_originalTransform{1, 0, 0, 0, 0, 0, 0, 0};

    void SetupDemoMode();
    void UpdatePlane(int index);
    static void SetPlane(ReflectionPlane& rp, const Vector& plane);
    static Motor ComputePlaneMotor(const ReflectionPlane& rp);
    void UpdatePlaneInstance(int index);
    void DrawPlaneNormal(const ReflectionPlane& rp);
    void ReflectOverPlane(int planeIndex);
    void ComputeIntersection();
    void StartAnimation();
    void LockIntersection();
    void DrawLockFormulas() const;
    void UnlockIntersection();
    void AddPlane();
    void ResetPheasant();
};
