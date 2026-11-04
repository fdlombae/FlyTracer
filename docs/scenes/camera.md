# Camera

FlyTracer uses a simple **eye-target-up** pinhole camera with PGA TriVectors: the eye and target are points (e123 = 1), the up hint is a direction (an ideal point, e123 = 0).

## Setting the Camera

```cpp
void MyScene::OnInit() {
    SetCamera(TriVector(0.0f, 5.0f, 15.0f),        // Eye: behind and above
              TriVector(0.0f, 2.0f, 0.0f),         // Target: look at the center
              TriVector(0.0f, 1.0f, 0.0f, 0.0f));  // Up direction (optional, Y is the default)

    SetCameraFov(60.0f);  // Vertical field of view in degrees, default 45
    SetExposure(1.2f);    // Brightness before tone mapping, default 1
}
```

The camera can be changed at any time, for example every frame in `OnInput` or `OnUpdate`. When the camera stays the same, the raytracer keeps averaging new samples into the image (anti-aliasing and smooth glossy reflections); moving it restarts the average.

## Orbit Camera

`OrbitCamera` (in `OrbitCamera.h`) circles a target point: drag with the right mouse button to orbit, scroll to zoom. Its eye position is the point at `distance` behind the target, rotated by motors for pitch (around x) and yaw (around y) and translated to the target.

```cpp
#include "OrbitCamera.h"

class MyScene : public GameScene {
    // Target, yaw, pitch, distance, minimum and maximum distance
    OrbitCamera m_orbit{TriVector(0.0f, 2.0f, 0.0f), 0.0f, 0.3f, 20.0f, 5.0f, 60.0f};

    void OnInput(const Input& input) override {
        m_orbit.HandleInput(input);
        SetCamera(m_orbit.Eye(), m_orbit.target);
    }
};
```
