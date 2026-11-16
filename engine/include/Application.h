#pragma once

#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "Config.h"
#include "GameScene.h"
#include "Input.h"
#include "VulkanRenderer.h"

// Relative paths are resolved against the executable's directory, so the program
// finds its shaders and resources no matter where it is started from
[[nodiscard]] std::filesystem::path ExecutableRelativePath(const std::filesystem::path& path);

class Application {
public:
    Application(const FlyTracer::AppConfig& config, std::unique_ptr<GameScene> scene);
    ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();

private:
    void handleEvents();
    void render(float deltaTime);
    void drawRendererWindow(float deltaTime);
    // Compiles engine/shaders/raytracer.comp again and loads it; the current shader stays on errors
    void reloadShader();

    SDL_Window* m_window{nullptr};
    std::unique_ptr<VulkanRenderer> m_renderer;
    std::unique_ptr<GameScene> m_gameScene;
    Input m_input;
    bool m_running{false};

    std::string m_shaderDirectory;
    std::string m_shaderStatus;
    bool m_reloadShaderRequested{false};
    float m_renderScale;  // The slider's value, applied when it is released

    // Frame time and GPU raytracing time, averaged over half a second like the scenes' FPS
    double m_frameMilliseconds{0.0};
    uint32_t m_frameCount{0};
    std::optional<double> m_gpuMilliseconds;
    double m_gpuMillisecondsSum{0.0};
    uint32_t m_gpuSampleCount{0};
    float m_sampleTime{0.0f};
};
