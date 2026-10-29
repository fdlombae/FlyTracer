# Input Handling

`OnInput` receives an `Input` object every frame with the state of the mouse and keyboard.

## Keys and Buttons

```cpp
input.IsDown(Key::W)                // Held down this frame
input.WasPressed(Key::Space)        // Went down this frame: use this for toggles and actions
input.IsDown(MouseButton::Right)    // Mouse button held
```

The available keys are `W A S D Q E`, the arrow keys (`Up Down Left Right`), `Space Shift Ctrl Alt Escape`, `H R P G V F T L` and the digits `Num0` to `Num9`. Escape also closes the application. Keys are released automatically when the window loses focus.

While an ImGui widget has keyboard focus (e.g. a text field), letter and digit keys go to ImGui instead of the scene; arrows and modifiers always reach the scene.

## Mouse and Time

| Function | Description |
|----------|-------------|
| `MouseX()`, `MouseY()` | Mouse position in window points |
| `MouseDeltaX()`, `MouseDeltaY()` | Movement since the last frame |
| `ScrollDelta()` | Scroll wheel movement since the last frame |
| `DeltaTime()` | Seconds since the last frame |

## Example

```cpp
void MyScene::OnInput(const Input& input) {
    if (input.WasPressed(Key::P)) {
        m_paused = !m_paused;
    }

    // Frame-rate independent movement
    const float step = 10.0f * input.DeltaTime();
    if (input.IsDown(Key::W)) m_position += step;
    if (input.IsDown(Key::S)) m_position -= step;

    if (input.IsDown(MouseButton::Left)) {
        m_angle += input.MouseDeltaX() * 0.01f;
    }
}
```
