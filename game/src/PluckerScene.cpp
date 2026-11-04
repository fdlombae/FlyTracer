#include "PluckerScene.h"
#include "PGA.h"
#include <cmath>
#include <imgui.h>

namespace {

const TriVector kOrigin(0.0f, 0.0f, 0.0f);
const Scene::Color kLineColor(1.0f, 0.9f, 0.3f);
const Scene::Color kDirectionColor(1.0f, 0.35f, 0.25f);
const Scene::Color kMomentColor(0.3f, 0.9f, 0.35f);
const Scene::Color kClosestPointColor(0.3f, 0.55f, 1.0f);

ImVec4 ToImVec4(const Scene::Color& color) {
    return {color.r, color.g, color.b, 1.0f};
}

// The plane through the origin perpendicular to a direction: the moment and the closest point of
// every line with that direction lie in it
Vector PlaneThroughOriginPerpendicularTo(const TriVector& direction) {
    return (kOrigin & direction) | kOrigin;
}

} // anonymous namespace

PluckerScene::PluckerScene(const std::string& resourceDir)
    : GameScene(resourceDir) {}

void PluckerScene::OnInit() {
    const std::array colors{kDirectionColor, kMomentColor, kClosestPointColor};
    for (size_t i = 0; i < colors.size(); ++i) {
        m_handleSpheres[i] = AddSphere(kOrigin, kHandleRadius, Scene::Material::Flat(colors[i]));
    }
    Reset();
    SetCamera(m_orbit.Eye(), m_orbit.target);
}

// The line through (0.5, 1.5, 1.5) along (1.5, 0.5, -1): that point is perpendicular to the
// direction, so it is the closest point
void PluckerScene::Reset() {
    m_line = TriVector(0.5f, 1.5f, 1.5f) & TriVector(1.5f, 0.5f, -1.0f, 0.0f);
    m_dragged.reset();
}

TriVector PluckerScene::Direction() const {
    return LineDirection(m_line);
}

TriVector PluckerScene::Moment() const {
    return {m_line.e01(), m_line.e02(), m_line.e03(), 0.0f};
}

TriVector PluckerScene::ClosestPoint() const {
    return ClosestPointOnLine(m_line, kOrigin);
}

// v is drawn along the line from d; m and d start at the origin
TriVector PluckerScene::HandlePosition(Handle handle) const {
    switch (handle) {
        case Handle::Direction: return ClosestPoint() + Direction();
        case Handle::Moment: return kOrigin + Moment();
        default: return ClosestPoint();
    }
}

// The handle closest to the mouse ray, if the ray passes close enough
std::optional<PluckerScene::Handle> PluckerScene::PickHandle(const BiVector& ray) const {
    std::optional<Handle> picked;
    float closest = 1.5f * kHandleRadius;
    for (size_t i = 0; i < m_handleSpheres.size(); ++i) {
        const auto handle = static_cast<Handle>(i);
        const TriVector tip = HandlePosition(handle);
        const float distance = (tip - ClosestPointOnLine(ray, tip)).VNorm();
        if (distance < closest) {
            closest = distance;
            picked = handle;
        }
    }
    return picked;
}

void PluckerScene::OnInput(const Input& input) {
    m_orbit.HandleInput(input);
    SetCamera(m_orbit.Eye(), m_orbit.target);

    if (input.WasPressed(Key::H)) m_guiVisible = !m_guiVisible;
    if (input.WasPressed(Key::R)) Reset();

    const auto ray = ViewRay(GetCamera(), input.MouseX(), input.MouseY(), input.WindowWidth(), input.WindowHeight());
    if (!ray) return;

    if (!input.IsDown(MouseButton::Left)) {
        m_dragged.reset();
    }
    if (m_dragged) {
        // The tip follows the mouse in the plane it was grabbed in: the meet of the ray and that plane
        const TriVector target = *ray ^ m_dragPlane;
        if (std::abs(target.e123()) > 1e-6f) {
            Drag(Euclidean(target));
        }
        return;
    }

    m_hovered = PickHandle(*ray);
    if (m_hovered && input.WasPressed(MouseButton::Left)) {
        m_dragged = m_hovered;
        const TriVector tip = HandlePosition(*m_dragged);
        m_dragPlane = *ray | tip;
        m_dragPivot = ClosestPoint();
    }
}

void PluckerScene::Drag(const TriVector& target) {
    const TriVector direction = Direction();
    const Vector perpendicularPlane = PlaneThroughOriginPerpendicularTo(direction);

    switch (*m_dragged) {
        case Handle::Direction: {
            // The line turns around its closest point d: the tip stays in the plane through d
            // perpendicular to d, so d remains the closest point
            const TriVector tip = m_dragPivot.VNorm() > 1e-4f
                ? ClosestPointOnPlane((kOrigin & m_dragPivot) | m_dragPivot, target)
                : target;
            const TriVector newDirection = tip - m_dragPivot;
            if (newDirection.VNorm() >= kMinDirectionLength) {
                m_line = m_dragPivot & newDirection;
            }
            break;
        }
        case Handle::Moment: {
            // The direction stays, and the moment is kept perpendicular to it
            const TriVector moment = ClosestPointOnPlane(perpendicularPlane, target);
            m_line = BiVector(moment.e032(), moment.e013(), moment.e021(),
                              direction.e032(), direction.e013(), direction.e021());
            break;
        }
        case Handle::ClosestPoint:
            // The direction stays, and the line moves to pass through the tip, its new closest point
            m_line = ClosestPointOnPlane(perpendicularPlane, target) & direction;
            break;
        case Handle::Count:
            break;
    }
}

void PluckerScene::OnUpdate([[maybe_unused]] float deltaTime) {
    for (size_t i = 0; i < m_handleSpheres.size(); ++i) {
        const auto handle = static_cast<Handle>(i);
        if (auto* sphere = GetSphere(m_handleSpheres[i])) {
            sphere->center = HandlePosition(handle);
            sphere->radius = (m_dragged == handle || m_hovered == handle) ? 1.4f * kHandleRadius : kHandleRadius;
        }
    }

    const Scene::Color gridColor(0.55f, 0.6f, 0.65f);
    for (int i = -6; i <= 6; ++i) {
        const auto offset = static_cast<float>(i);
        DrawDebugLine(TriVector(offset, 0.0f, -6.0f), TriVector(offset, 0.0f, 6.0f), gridColor);
        DrawDebugLine(TriVector(-6.0f, 0.0f, offset), TriVector(6.0f, 0.0f, offset), gridColor);
    }
    DrawDebugPoint(kOrigin, 0.2f);

    const TriVector closest = ClosestPoint();
    DrawDebugLine(m_line, 12.0f, kLineColor);
    DrawDebugLine(closest, closest + Direction(), kDirectionColor);
    DrawDebugLine(kOrigin, kOrigin + Moment(), kMomentColor);
    DrawDebugLine(kOrigin, closest, kClosestPointColor);
}

void PluckerScene::OnGui() {
    if (!m_guiVisible) return;

    const TriVector v = Direction();
    const TriVector m = Moment();
    const TriVector d = ClosestPoint();

    ImGui::Begin("Plucker Line", nullptr, ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", GetFPS());
    ImGui::Separator();

    ImGui::TextColored(ToImVec4(kLineColor), "L = %s", ToRoundedString(m_line).c_str());
    ImGui::TextColored(ToImVec4(kDirectionColor), "v = (%.2f, %.2f, %.2f)", v.e032(), v.e013(), v.e021());
    ImGui::TextDisabled("    direction: the e23, e31, e12 part");
    ImGui::TextColored(ToImVec4(kMomentColor), "m = p x v = (%.2f, %.2f, %.2f)", m.e032(), m.e013(), m.e021());
    ImGui::TextDisabled("    moment: the e01, e02, e03 part");
    ImGui::TextColored(ToImVec4(kClosestPointColor), "d = v x m / |v|^2 = (%.2f, %.2f, %.2f)",
                       d.e032(), d.e013(), d.e021());
    ImGui::Separator();
    ImGui::Text("v . m = %.3f", v.e032() * m.e032() + v.e013() * m.e013() + v.e021() * m.e021());
    ImGui::Text("|m| / |v| = %.2f = |d|", m.VNorm() / v.VNorm());
    ImGui::Separator();

    if (ImGui::Button("Reset [R]")) Reset();
    ImGui::Text("Left-drag a tip:");
    ImGui::BulletText("v turns the line around d");
    ImGui::BulletText("m moves the line, v stays");
    ImGui::BulletText("d moves the line through it, v stays");
    ImGui::Text("Right-drag: Orbit  |  Scroll: Zoom  |  [H] GUI");
    ImGui::End();
}
