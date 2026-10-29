#pragma once

#include <bitset>
#include <cstddef>

enum class Key {
    W, A, S, D, Q, E,
    Up, Down, Left, Right,
    Space, Shift, Ctrl, Alt, Escape,
    H, R, P, G, V, F, T, L,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Count
};

enum class MouseButton { Left, Middle, Right, Count };

// Keyboard and mouse state for one frame, filled in by the engine
class Input {
public:
    [[nodiscard]] bool IsDown(Key key) const noexcept { return m_keys[index(key)]; }
    // True only in the frame the key went down
    [[nodiscard]] bool WasPressed(Key key) const noexcept {
        return m_keys[index(key)] && !m_previousKeys[index(key)];
    }
    [[nodiscard]] bool IsDown(MouseButton button) const noexcept { return m_buttons[index(button)]; }
    [[nodiscard]] bool WasPressed(MouseButton button) const noexcept {
        return m_buttons[index(button)] && !m_previousButtons[index(button)];
    }

    [[nodiscard]] float MouseX() const noexcept { return m_mouseX; }
    [[nodiscard]] float MouseY() const noexcept { return m_mouseY; }
    [[nodiscard]] float MouseDeltaX() const noexcept { return m_mouseDeltaX; }
    [[nodiscard]] float MouseDeltaY() const noexcept { return m_mouseDeltaY; }
    [[nodiscard]] float ScrollDelta() const noexcept { return m_scrollDelta; }
    [[nodiscard]] float DeltaTime() const noexcept { return m_deltaTime; }
    // In the units of the mouse position (points)
    [[nodiscard]] float WindowWidth() const noexcept { return m_windowWidth; }
    [[nodiscard]] float WindowHeight() const noexcept { return m_windowHeight; }

    // Engine side
    void SetKey(Key key, bool down) noexcept { m_keys[index(key)] = down; }
    void SetMouseButton(MouseButton button, bool down) noexcept { m_buttons[index(button)] = down; }
    void MoveMouse(float x, float y) noexcept {
        m_mouseDeltaX += x - m_mouseX;
        m_mouseDeltaY += y - m_mouseY;
        m_mouseX = x;
        m_mouseY = y;
    }
    void SetMousePosition(float x, float y) noexcept {
        m_mouseX = x;
        m_mouseY = y;
    }
    void Scroll(float delta) noexcept { m_scrollDelta += delta; }
    void SetDeltaTime(float deltaTime) noexcept { m_deltaTime = deltaTime; }
    void SetWindowSize(float width, float height) noexcept {
        m_windowWidth = width;
        m_windowHeight = height;
    }

    // Called after the scene has seen this frame's input
    void EndFrame() noexcept {
        m_previousKeys = m_keys;
        m_previousButtons = m_buttons;
        m_mouseDeltaX = 0.0f;
        m_mouseDeltaY = 0.0f;
        m_scrollDelta = 0.0f;
    }

    // Releases all keys and buttons, e.g. when the window loses focus and misses the release events
    void ReleaseAll() noexcept {
        m_keys.reset();
        m_buttons.reset();
    }

private:
    template<typename Enum>
    static constexpr size_t index(Enum value) noexcept { return static_cast<size_t>(value); }

    std::bitset<static_cast<size_t>(Key::Count)> m_keys;
    std::bitset<static_cast<size_t>(Key::Count)> m_previousKeys;
    std::bitset<static_cast<size_t>(MouseButton::Count)> m_buttons;
    std::bitset<static_cast<size_t>(MouseButton::Count)> m_previousButtons;
    float m_mouseX{0.0f};
    float m_mouseY{0.0f};
    float m_mouseDeltaX{0.0f};
    float m_mouseDeltaY{0.0f};
    float m_scrollDelta{0.0f};
    float m_deltaTime{0.0f};
    float m_windowWidth{0.0f};
    float m_windowHeight{0.0f};
};
