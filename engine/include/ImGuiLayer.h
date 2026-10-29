#pragma once

#include "Swapchain.h"
#include "VulkanContext.h"
#include "VulkanHelpers.h"
#include <SDL3/SDL.h>

// Dear ImGui on top of the raytraced image, drawn with dynamic rendering into the swapchain image
class ImGuiLayer {
public:
    ImGuiLayer(SDL_Window* window, const VulkanContext& context, const Swapchain& swapchain);
    ~ImGuiLayer() noexcept;

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void OnSwapchainRecreated(const Swapchain& swapchain);
    void NewFrame();
    // Loads the image, which must be in COLOR_ATTACHMENT_OPTIMAL, and draws on top of it
    void Record(VkCommandBuffer cmdBuffer, VkImageView view, VkExtent2D extent);

private:
    VkDevice m_device;
    VkFormat m_colorFormat;  // ImGui keeps a pointer to it
    VulkanHelpers::UniqueDescriptorPool m_descriptorPool;
};
