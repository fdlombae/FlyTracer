#pragma once

#include "Config.h"
#include "GameScene.h"
#include "ImGuiLayer.h"
#include "RaytracingPass.h"
#include "Swapchain.h"
#include "VulkanContext.h"
#include "VulkanHelpers.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Renders a GameScene into an SDL window: raytracing pass, blit to the swapchain, ImGui on top
class VulkanRenderer {
public:
    struct Config {
        bool vsync{true};
        uint32_t maxBounces{3};
        float renderScale{1.0f};
        std::string shaderDir{"shaders"};
    };

    static constexpr float kMinRenderScale = FlyTracer::AppConfig::kMinRenderScale;

    VulkanRenderer(SDL_Window* window, const Config& config);
    ~VulkanRenderer() noexcept;

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Geometry, BVHs, materials and textures; call before the first frame
    void UploadMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes);

    // Returns false when no swapchain image is available (e.g. while resizing); skip the frame then.
    // Between BeginFrame and EndFrame the ImGui frame is open.
    [[nodiscard]] bool BeginFrame();
    void EndFrame(const GameScene& scene);

    void OnWindowResized() noexcept { m_swapchainOutdated = true; }

    // The raytracer renders at the window's size in pixels times the render scale (kMinRenderScale to 1);
    // the image is stretched over the window. Changing it waits for the GPU.
    void SetRenderScale(float scale);
    [[nodiscard]] float RenderScale() const noexcept { return m_renderScale; }
    [[nodiscard]] VkExtent2D RenderExtent() const noexcept { return m_raytracing.Output().extent(); }
    void SetMaxBounces(uint32_t maxBounces) noexcept { m_raytracing.SetMaxBounces(maxBounces); }
    [[nodiscard]] uint32_t MaxBounces() const noexcept { return m_raytracing.MaxBounces(); }
    void SetExposure(float exposure) noexcept { m_raytracing.SetExposure(exposure); }
    [[nodiscard]] float Exposure() const noexcept { return m_raytracing.Exposure(); }
    // Loads the compiled raytracing shader again; throws and keeps the current one when that fails
    void ReloadShader() { m_raytracing.ReloadShader(); }

    [[nodiscard]] std::optional<double> RaytracingMilliseconds() const noexcept {
        return m_raytracing.DispatchMilliseconds();
    }
    void WaitIdle() const;

private:
    struct Frame {
        VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
        VulkanHelpers::UniqueSemaphore imageAvailable;
        VulkanHelpers::UniqueFence inFlight;
    };

    [[nodiscard]] VkExtent2D windowPixels() const;
    [[nodiscard]] VkExtent2D scaledExtent() const;
    void recreateSwapchain();
    void recordBlitToSwapchain(VkCommandBuffer cmdBuffer);

    SDL_Window* m_window;
    float m_renderScale;
    // Declaration order is construction order; destruction runs backwards, so the context goes last
    VulkanContext m_context;
    Swapchain m_swapchain;
    RaytracingPass m_raytracing;
    ImGuiLayer m_imgui;
    std::array<Frame, RaytracingPass::kFramesInFlight> m_frames;
    std::vector<VkFence> m_imagesInFlight;  // Fence of the frame that last used each swapchain image

    uint32_t m_currentFrame{0};
    uint32_t m_imageIndex{0};
    bool m_swapchainOutdated{false};
};
