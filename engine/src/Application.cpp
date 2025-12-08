#include "Application.h"
#include "Mesh.h"
#include "FlyFish.h"  // PGA library for motor-based camera
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <vector>
#include <stdexcept>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Application::Application(int width, int height, const std::string& title,
                         std::unique_ptr<GameScene> scene,
                         const std::string& shaderDir)
    : m_width(width)
    , m_height(height)
    , m_title(title)
    , m_shaderDir(shaderDir)
    , m_gameScene(std::move(scene))
{
    // Initialize SDL and create window
    initSDL();

    // Create VulkanRenderer (handles all Vulkan initialization)
    VulkanRenderer::Config rendererConfig;
    rendererConfig.width = width;
    rendererConfig.height = height;
#ifdef NDEBUG
    rendererConfig.enableValidation = false;
#else
    rendererConfig.enableValidation = true;
#endif
    rendererConfig.shaderDir = m_shaderDir;
    m_renderer = std::make_unique<VulkanRenderer>(m_window, rendererConfig);

    // Initialize scene (scene loads its own meshes)
    m_gameScene->onInit(m_renderer.get());

    // Get scene data and mesh instances from the scene
    m_sceneData = m_gameScene->getSceneData();
    m_meshInstances = m_gameScene->getMeshInstances();

    // Update camera from scene
    m_cameraEye = m_gameScene->getCameraEye();
    m_cameraTarget = m_gameScene->getCameraTarget();
    m_cameraUp = m_gameScene->getCameraUp();

    // Upload meshes, scene data, instances to GPU
    uploadMeshes(m_gameScene->getMeshes());
    m_renderer->uploadSceneData(m_sceneData);
    m_renderer->uploadInstances(m_meshInstances);

    // Upload texture from scene (if specified)
    const std::string& texturePath = m_gameScene->getTextureFilename();
    if (!texturePath.empty()) {
        m_renderer->uploadTexture(texturePath);
    } else {
        // Upload dummy texture
        m_renderer->uploadTexture("");
    }

    // Create descriptor set and compute pipeline
    m_renderer->createDescriptorSet();
    m_renderer->createComputePipeline();
}

Application::~Application() noexcept {
    cleanup();
}

void Application::setScene(const Scene::SceneData& scene) {
    m_sceneData = scene;
    // In a production app, you'd want to update buffers here
    // For now, scene is set at initialization
}

void Application::setCamera(const TriVector& eye, const TriVector& target, const TriVector& up) {
    m_cameraEye = eye;
    m_cameraTarget = target;
    m_cameraUp = up;
}

void Application::uploadMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes) {
    m_renderer->uploadMeshes(meshes);
}

void Application::initSDL() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        throw std::runtime_error("Failed to initialize SDL: " + std::string(SDL_GetError()));
    }

    m_window = SDL_CreateWindow(m_title.c_str(), m_width, m_height, SDL_WINDOW_VULKAN);
    if (!m_window) {
        SDL_Quit();
        throw std::runtime_error("Failed to create window: " + std::string(SDL_GetError()));
    }
}


void Application::run() {
    m_running = true;
    Uint64 lastTime = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();

    while (m_running) {
        const Uint64 currentTime = SDL_GetPerformanceCounter();
        const float deltaTime = static_cast<float>(currentTime - lastTime) /
                               static_cast<float>(frequency);
        lastTime = currentTime;

        handleEvents();
        processInput(deltaTime);

        // Skip rendering when window is minimized to avoid Vulkan issues
        if (m_windowMinimized) {
            SDL_Delay(10);  // Don't spin CPU when minimized
            continue;
        }

        update(deltaTime);
        render();
    }

    // Wait for GPU to finish before cleanup
    m_renderer->waitIdle();
}

void Application::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        // Skip input when ImGui wants it
        bool imguiWantsMouse = ImGui::GetIO().WantCaptureMouse;
        bool imguiWantsKeyboard = ImGui::GetIO().WantCaptureKeyboard;

        if (event.type == SDL_EVENT_QUIT) {
            m_running = false;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) {
                m_running = false;
            }
            // Track key state for scene input
            // Arrow keys always go to the game (ImGui doesn't need them)
            // WASD/QE only when ImGui doesn't want keyboard (e.g., text input)
            switch (event.key.key) {
                // Arrow keys always processed for game control
                case SDLK_UP: m_keyUp = true; break;
                case SDLK_DOWN: m_keyDown = true; break;
                case SDLK_LEFT: m_keyLeft = true; break;
                case SDLK_RIGHT: m_keyRight = true; break;
                // WASD/QE only when ImGui doesn't want keyboard
                case SDLK_W: if (!imguiWantsKeyboard) m_keyW = true; break;
                case SDLK_A: if (!imguiWantsKeyboard) m_keyA = true; break;
                case SDLK_S: if (!imguiWantsKeyboard) m_keyS = true; break;
                case SDLK_D: if (!imguiWantsKeyboard) m_keyD = true; break;
                case SDLK_Q: if (!imguiWantsKeyboard) m_keyQ = true; break;
                case SDLK_E: if (!imguiWantsKeyboard) m_keyE = true; break;
                default: break;
            }
        } else if (event.type == SDL_EVENT_KEY_UP) {
            switch (event.key.key) {
                case SDLK_W: m_keyW = false; break;
                case SDLK_A: m_keyA = false; break;
                case SDLK_S: m_keyS = false; break;
                case SDLK_D: m_keyD = false; break;
                case SDLK_Q: m_keyQ = false; break;
                case SDLK_E: m_keyE = false; break;
                case SDLK_UP: m_keyUp = false; break;
                case SDLK_DOWN: m_keyDown = false; break;
                case SDLK_LEFT: m_keyLeft = false; break;
                case SDLK_RIGHT: m_keyRight = false; break;
                default: break;
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !imguiWantsMouse) {
            if (event.button.button == SDL_BUTTON_RIGHT) m_rightMouseDown = true;
            else if (event.button.button == SDL_BUTTON_MIDDLE) m_middleMouseDown = true;
            else if (event.button.button == SDL_BUTTON_LEFT) m_leftMouseDown = true;
            m_lastMouseX = event.button.x;
            m_lastMouseY = event.button.y;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (event.button.button == SDL_BUTTON_RIGHT) m_rightMouseDown = false;
            else if (event.button.button == SDL_BUTTON_MIDDLE) m_middleMouseDown = false;
            else if (event.button.button == SDL_BUTTON_LEFT) m_leftMouseDown = false;
        } else if (event.type == SDL_EVENT_MOUSE_MOTION && !imguiWantsMouse) {
            // Accumulate mouse delta for scene input
            m_mouseDeltaX += event.motion.x - m_lastMouseX;
            m_mouseDeltaY += event.motion.y - m_lastMouseY;
            m_lastMouseX = event.motion.x;
            m_lastMouseY = event.motion.y;
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL && !imguiWantsMouse) {
            m_scrollDelta += event.wheel.y;
        } else if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
            m_windowMinimized = true;
        } else if (event.type == SDL_EVENT_WINDOW_RESTORED) {
            m_windowMinimized = false;
        } else if (event.type == SDL_EVENT_WINDOW_HIDDEN) {
            m_windowMinimized = true;
        } else if (event.type == SDL_EVENT_WINDOW_SHOWN || event.type == SDL_EVENT_WINDOW_EXPOSED) {
            if (m_windowMinimized) {
                m_windowMinimized = false;
                m_renderer->waitIdle();
            }
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            m_renderer->waitIdle();
            m_keyW = m_keyA = m_keyS = m_keyD = m_keyQ = m_keyE = false;
            m_keyUp = m_keyDown = m_keyLeft = m_keyRight = false;
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
            m_renderer->waitIdle();
        }
    }
}

void Application::processInput(float deltaTime) {
    // Pass input to game scene
    if (m_gameScene) {
        InputState input;
        input.leftMouseDown = m_leftMouseDown;
        input.rightMouseDown = m_rightMouseDown;
        input.middleMouseDown = m_middleMouseDown;
        input.mouseX = m_lastMouseX;
        input.mouseY = m_lastMouseY;
        input.mouseDeltaX = m_mouseDeltaX;
        input.mouseDeltaY = m_mouseDeltaY;
        input.scrollDelta = m_scrollDelta;
        input.keyW = m_keyW;
        input.keyA = m_keyA;
        input.keyS = m_keyS;
        input.keyD = m_keyD;
        input.keyQ = m_keyQ;
        input.keyE = m_keyE;
        input.keyUp = m_keyUp;
        input.keyDown = m_keyDown;
        input.keyLeft = m_keyLeft;
        input.keyRight = m_keyRight;
        input.deltaTime = deltaTime;
        m_gameScene->onInput(input);

        // Reset accumulated input after passing to scene
        m_mouseDeltaX = 0.0f;
        m_mouseDeltaY = 0.0f;
        m_scrollDelta = 0.0f;
    }
}

void Application::update(float deltaTime) {
    m_time += deltaTime;
    ++m_frameCount;

    // Update game scene
    if (m_gameScene) {
        m_gameScene->onUpdate(deltaTime);

        // Get updated scene data
        m_sceneData = m_gameScene->getSceneData();
        m_meshInstances = m_gameScene->getMeshInstances();

        // NOTE: Instance upload moved to render() after beginFrame() fence wait
        // to avoid updating the buffer while GPU is still reading it

        // Update camera from scene
        m_cameraEye = m_gameScene->getCameraEye();
        m_cameraTarget = m_gameScene->getCameraTarget();
        m_cameraUp = m_gameScene->getCameraUp();
    }

    // FPS calculation
    m_fpsAccumulator += deltaTime;
    ++m_fpsFrameCount;
    if (m_fpsAccumulator >= 0.5f) {
        m_fps = static_cast<float>(m_fpsFrameCount) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_fpsFrameCount = 0;
    }
}

void Application::render() {
    // Compute camera basis vectors from the three PGA points (GLU-style camera)
    // Extract x, y, z coordinates from the stored TriVectors
    float eyeX = m_cameraEye.e032();
    float eyeY = m_cameraEye.e013();
    float eyeZ = m_cameraEye.e021();

    float targetX = m_cameraTarget.e032();
    float targetY = m_cameraTarget.e013();
    float targetZ = m_cameraTarget.e021();

    float upRefX = m_cameraUp.e032();
    float upRefY = m_cameraUp.e013();
    float upRefZ = m_cameraUp.e021();

    // Compute forward direction (from eye to target)
    float fx = targetX - eyeX;
    float fy = targetY - eyeY;
    float fz = targetZ - eyeZ;
    float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
    fx /= flen; fy /= flen; fz /= flen;

    // Up hint is now a direction vector directly (not a reference point)
    float hx = upRefX;
    float hy = upRefY;
    float hz = upRefZ;
    float hlen = std::sqrt(hx*hx + hy*hy + hz*hz);
    hx /= hlen; hy /= hlen; hz /= hlen;

    // Compute right vector (cross product of forward and up hint)
    float rx = fy * hz - fz * hy;
    float ry = fz * hx - fx * hz;
    float rz = fx * hy - fy * hx;
    float rlen = std::sqrt(rx*rx + ry*ry + rz*rz);
    rx /= rlen; ry /= rlen; rz /= rlen;

    // Recompute true up vector (cross product of right and forward)
    float ux = ry * fz - rz * fy;
    float uy = rz * fx - rx * fz;
    float uz = rx * fy - ry * fx;

    // Store camera rotation matrix (row-major)
    // Row 0: right vector (X axis in camera space)
    // Row 1: up vector (Y axis in camera space)
    // Row 2: forward vector (-Z axis in camera space, but we pass -forward to match convention)
    m_cameraRotation[0] = rx;
    m_cameraRotation[1] = ry;
    m_cameraRotation[2] = rz;
    m_cameraRotation[3] = ux;
    m_cameraRotation[4] = uy;
    m_cameraRotation[5] = uz;
    m_cameraRotation[6] = -fx;  // -forward because camera looks down -Z
    m_cameraRotation[7] = -fy;
    m_cameraRotation[8] = -fz;

    // Package camera position
    float cameraPosition[3] = {eyeX, eyeY, eyeZ};

    // Begin frame (waits for previous frame's fence)
    m_renderer->beginFrame();

    // Upload instances AFTER fence wait to avoid updating while GPU is reading
    // This ensures the previous frame has finished using the buffer
    m_renderer->uploadInstances(m_meshInstances);
    m_renderer->updateSpheres(m_sceneData.spheres);
    m_renderer->updatePlanes(m_sceneData.planes);

    // Build ImGui UI
    ImGui::Begin("Controls");
    ImGui::Text("FPS: %.1f", m_fps);
    ImGui::Text("Frame: %u", m_frameCount);
    ImGui::Text("Camera: (%.2f, %.2f, %.2f)", eyeX, eyeY, eyeZ);
    ImGui::SliderFloat("FOV", &m_cameraFov, 20.0f, 120.0f);
    ImGui::End();

    // Call scene's custom ImGui
    if (m_gameScene) {
        m_gameScene->onGui();
    }

    // Render scene with camera rotation matrix and position
    // Use game scene's meshes (m_meshes is not populated - it's a legacy member)
    m_renderer->renderScene(m_sceneData, m_gameScene->getMeshes(), m_meshInstances,
                           m_cameraRotation.data(), cameraPosition, m_time, m_cameraFov);

    // End frame
    m_renderer->endFrame();
}

void Application::cleanup() {
    // Cleanup game scene
    if (m_gameScene) {
        m_gameScene->onShutdown();
        m_gameScene.reset();
    }

    // VulkanRenderer cleanup handled by unique_ptr

    // Cleanup SDL
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
}
