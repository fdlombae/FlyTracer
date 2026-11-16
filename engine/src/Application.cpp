#include "Application.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

struct KeyBinding {
    SDL_Keycode sdlKey;
    Key key;
    bool alwaysForGame;  // Also delivered while an ImGui widget has keyboard focus
};

constexpr std::array kKeyBindings{
    KeyBinding{SDLK_UP, Key::Up, true},
    KeyBinding{SDLK_DOWN, Key::Down, true},
    KeyBinding{SDLK_LEFT, Key::Left, true},
    KeyBinding{SDLK_RIGHT, Key::Right, true},
    KeyBinding{SDLK_SPACE, Key::Space, true},
    KeyBinding{SDLK_LSHIFT, Key::Shift, true},
    KeyBinding{SDLK_RSHIFT, Key::Shift, true},
    KeyBinding{SDLK_LCTRL, Key::Ctrl, true},
    KeyBinding{SDLK_RCTRL, Key::Ctrl, true},
    KeyBinding{SDLK_LALT, Key::Alt, true},
    KeyBinding{SDLK_RALT, Key::Alt, true},
    KeyBinding{SDLK_ESCAPE, Key::Escape, true},
    KeyBinding{SDLK_W, Key::W, false},
    KeyBinding{SDLK_A, Key::A, false},
    KeyBinding{SDLK_S, Key::S, false},
    KeyBinding{SDLK_D, Key::D, false},
    KeyBinding{SDLK_Q, Key::Q, false},
    KeyBinding{SDLK_E, Key::E, false},
    KeyBinding{SDLK_H, Key::H, false},
    KeyBinding{SDLK_R, Key::R, false},
    KeyBinding{SDLK_P, Key::P, false},
    KeyBinding{SDLK_G, Key::G, false},
    KeyBinding{SDLK_V, Key::V, false},
    KeyBinding{SDLK_F, Key::F, false},
    KeyBinding{SDLK_T, Key::T, false},
    KeyBinding{SDLK_L, Key::L, false},
    KeyBinding{SDLK_0, Key::Num0, false},
    KeyBinding{SDLK_1, Key::Num1, false},
    KeyBinding{SDLK_2, Key::Num2, false},
    KeyBinding{SDLK_3, Key::Num3, false},
    KeyBinding{SDLK_4, Key::Num4, false},
    KeyBinding{SDLK_5, Key::Num5, false},
    KeyBinding{SDLK_6, Key::Num6, false},
    KeyBinding{SDLK_7, Key::Num7, false},
    KeyBinding{SDLK_8, Key::Num8, false},
    KeyBinding{SDLK_9, Key::Num9, false},
};

// The left and right modifier keys are one key for the game, which is down while either of them is
bool otherModifierDown(Key key, SDL_Keymod modifiers) {
    switch (key) {
        case Key::Shift: return (modifiers & SDL_KMOD_SHIFT) != 0;
        case Key::Ctrl: return (modifiers & SDL_KMOD_CTRL) != 0;
        case Key::Alt: return (modifiers & SDL_KMOD_ALT) != 0;
        default: return false;
    }
}

const KeyBinding* findKeyBinding(SDL_Keycode sdlKey) {
    const auto it = std::ranges::find(kKeyBindings, sdlKey, &KeyBinding::sdlKey);
    return it != kKeyBindings.end() ? &*it : nullptr;
}

std::optional<MouseButton> toMouseButton(Uint8 sdlButton) {
    switch (sdlButton) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        default: return std::nullopt;
    }
}

// A long stall (breakpoint, window drag) would otherwise make animations jump
constexpr double kMaxDeltaTime = 0.1;

// Runs glslc on the raytracing shader in the source tree, as the build does (the paths come from
// CMakeLists.txt). glslc prints its errors to the console.
bool compileRaytracingShader(const std::string& shaderDirectory) {
    const auto quoted = [](const std::string& text) { return '"' + text + '"'; };
    const std::string sourceDirectory = FLYTRACER_SHADER_SOURCE_DIR;
    std::string command = quoted(FLYTRACER_GLSLC) + " --target-env=vulkan1.3 -I" + quoted(sourceDirectory) +
                          " -I" + quoted(FLYTRACER_ENGINE_INCLUDE_DIR) + " " +
                          quoted(sourceDirectory + "/raytracer.comp") + " -o " +
                          quoted(shaderDirectory + "/raytracer.comp.spv");
#ifdef _WIN32
    command = quoted(command);  // cmd.exe strips the outer quotes of a command that starts with one
#endif
    return std::system(command.c_str()) == 0;
}

} // anonymous namespace

std::filesystem::path ExecutableRelativePath(const std::filesystem::path& path) {
    if (path.is_absolute()) return path;
    const char* basePath = SDL_GetBasePath();
    return basePath != nullptr ? std::filesystem::path(basePath) / path : path;
}

Application::Application(const FlyTracer::AppConfig& config, std::unique_ptr<GameScene> scene)
    : m_gameScene(std::move(scene))
    , m_shaderDirectory(ExecutableRelativePath(config.shaderDirectory).string())
    , m_renderScale(config.renderScale)
{
    if (!m_gameScene) {
        throw std::invalid_argument("Application requires a scene");
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        throw std::runtime_error("Failed to initialize SDL: " + std::string(SDL_GetError()));
    }

    // The window size is in points; with high pixel density the renderer uses every display pixel
    SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config.fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }
    m_window = SDL_CreateWindow(config.windowTitle.c_str(), config.windowWidth, config.windowHeight, windowFlags);
    if (!m_window) {
        SDL_Quit();
        throw std::runtime_error("Failed to create window: " + std::string(SDL_GetError()));
    }
    SDL_SyncWindow(m_window);

    try {
        m_renderer = std::make_unique<VulkanRenderer>(m_window, VulkanRenderer::Config{
            .vsync = config.vsync,
            .maxBounces = static_cast<uint32_t>(std::max(config.maxRayBounces, 1)),
            .renderScale = config.renderScale,
            .shaderDir = m_shaderDirectory,
        });

        m_gameScene->Init();
        m_renderer->UploadMeshes(m_gameScene->GetMeshes());
    } catch (...) {
        m_gameScene->Shutdown();
        m_renderer.reset();
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        throw;
    }
}

Application::~Application() noexcept {
    try {
        m_gameScene->Shutdown();
    } catch (const std::exception& e) {
        std::cerr << "Scene shutdown failed: " << e.what() << '\n';
    }
    m_gameScene.reset();
    m_renderer.reset();  // Uses the window's surface, so it goes first
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Application::run() {
    m_running = true;
    Uint64 lastTime = SDL_GetPerformanceCounter();
    const auto frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    while (m_running) {
        // Nothing is drawn while minimized: wait for events instead of spinning
        if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) {
            SDL_WaitEventTimeout(nullptr, 100);
        }
        const Uint64 currentTime = SDL_GetPerformanceCounter();
        const double deltaTime = std::min(static_cast<double>(currentTime - lastTime) / frequency, kMaxDeltaTime);
        lastTime = currentTime;

        handleEvents();
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(m_window, &width, &height);
        m_input.SetWindowSize(static_cast<float>(width), static_cast<float>(height));
        m_input.SetDeltaTime(static_cast<float>(deltaTime));
        m_gameScene->HandleInput(m_input);
        m_input.EndFrame();

        m_gameScene->Update(static_cast<float>(deltaTime));
        render(static_cast<float>(deltaTime));
    }

    m_renderer->WaitIdle();
}

void Application::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        const bool imguiWantsMouse = ImGui::GetIO().WantCaptureMouse;
        const bool imguiWantsKeyboard = ImGui::GetIO().WantCaptureKeyboard;

        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    m_running = false;
                }
                if (event.key.key == SDLK_F5 && !event.key.repeat) {
                    m_reloadShaderRequested = true;
                }
                if (const KeyBinding* binding = findKeyBinding(event.key.key);
                    binding && (binding->alwaysForGame || !imguiWantsKeyboard)) {
                    m_input.SetKey(binding->key, true);
                }
                break;
            case SDL_EVENT_KEY_UP:
                if (const KeyBinding* binding = findKeyBinding(event.key.key);
                    binding && !otherModifierDown(binding->key, event.key.mod)) {
                    m_input.SetKey(binding->key, false);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (const auto button = toMouseButton(event.button.button); button && !imguiWantsMouse) {
                    m_input.SetMouseButton(*button, true);
                    m_input.SetMousePosition(event.button.x, event.button.y);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (const auto button = toMouseButton(event.button.button)) {
                    m_input.SetMouseButton(*button, false);
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (!imguiWantsMouse) {
                    m_input.MoveMouse(event.motion.x, event.motion.y);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (!imguiWantsMouse) {
                    m_input.Scroll(event.wheel.y);
                }
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                m_renderer->OnWindowResized();
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                // Key and button releases now go to another window
                m_input.ReleaseAll();
                break;
            default:
                break;
        }
    }
}

void Application::render(float deltaTime) {
    if (!m_renderer->BeginFrame()) {
        return;
    }
    m_gameScene->DrawGui();
    drawRendererWindow(deltaTime);
    if (std::exchange(m_reloadShaderRequested, false)) {
        reloadShader();
    }
    m_renderer->EndFrame(*m_gameScene);
}

void Application::reloadShader() {
    try {
        if (!compileRaytracingShader(m_shaderDirectory)) {
            throw std::runtime_error("it does not compile, see the console");
        }
        m_renderer->ReloadShader();
        m_shaderStatus = "Shader reloaded";
    } catch (const std::exception& e) {
        m_shaderStatus = std::string("Shader not reloaded: ") + e.what();
    }
    std::cout << m_shaderStatus << "\n";
}

void Application::drawRendererWindow(float deltaTime) {
    if (const auto milliseconds = m_renderer->RaytracingMilliseconds()) {
        m_gpuMillisecondsSum += *milliseconds;
        ++m_gpuSampleCount;
    }
    ++m_frameCount;
    m_sampleTime += deltaTime;
    if (m_sampleTime >= 0.5f) {
        m_frameMilliseconds = 1000.0 * m_sampleTime / m_frameCount;
        if (m_gpuSampleCount > 0) {
            m_gpuMilliseconds = m_gpuMillisecondsSum / m_gpuSampleCount;
        }
        m_gpuMillisecondsSum = 0.0;
        m_gpuSampleCount = 0;
        m_frameCount = 0;
        m_sampleTime = 0.0f;
    }

    // Kept in the bottom left corner, out of the way of the scenes' windows
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10.0f, viewport->WorkPos.y + viewport->WorkSize.y - 10.0f),
                            ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::Begin("Renderer", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNavInputs);
    const VkExtent2D extent = m_renderer->RenderExtent();
    ImGui::Text("Frame: %.2f ms at %u x %u", m_frameMilliseconds, extent.width, extent.height);
    if (m_gpuMilliseconds) {
        ImGui::Text("Raytracing (GPU): %.2f ms", *m_gpuMilliseconds);
    }

    if (ImGui::CollapsingHeader("Settings")) {
        int bounces = static_cast<int>(m_renderer->MaxBounces());
        if (ImGui::SliderInt("Bounces", &bounces, 1, 8, "%d", ImGuiSliderFlags_AlwaysClamp)) {
            m_renderer->SetMaxBounces(static_cast<uint32_t>(bounces));
        }
        float exposure = m_renderer->Exposure();
        if (ImGui::SliderFloat("Exposure", &exposure, 0.25f, 4.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) {
            m_renderer->SetExposure(exposure);
        }
        // Applied when the slider is released: every change recreates the raytracer's images
        ImGui::SliderFloat("Render scale", &m_renderScale, VulkanRenderer::kMinRenderScale, 1.0f, "%.2f",
                           ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            m_renderer->SetRenderScale(m_renderScale);
        }
        if (ImGui::Button("Reload shader (F5)")) {
            m_reloadShaderRequested = true;
        }
        if (!m_shaderStatus.empty()) {
            ImGui::TextUnformatted(m_shaderStatus.c_str());
        }
    }
    ImGui::End();
}
