#pragma once

#include "VulkanContext.h"
#include "VulkanHelpers.h"
#include <vector>

// Swapchain images in a non-sRGB format: the raytracer encodes sRGB itself, and ImGui's
// colors are already sRGB. Recreate it when the window size changes.
class Swapchain {
public:
    Swapchain(const VulkanContext& context, VkExtent2D windowPixels, bool vsync);

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // The caller must make sure the GPU no longer uses the old images
    void Recreate(VkExtent2D windowPixels);

    [[nodiscard]] VkSwapchainKHR Handle() const noexcept { return m_swapchain.get(); }
    [[nodiscard]] VkFormat Format() const noexcept { return m_format; }
    [[nodiscard]] VkExtent2D Extent() const noexcept { return m_extent; }
    [[nodiscard]] uint32_t ImageCount() const noexcept { return static_cast<uint32_t>(m_images.size()); }
    [[nodiscard]] VkImage Image(uint32_t index) const { return m_images.at(index); }
    [[nodiscard]] VkImageView View(uint32_t index) const { return m_views.at(index).get(); }
    // Signaled when rendering into the image is done; a present can still be waiting on it after the
    // frame's fence signals, so these belong to the swapchain images instead of to the frames
    [[nodiscard]] VkSemaphore RenderFinished(uint32_t index) const { return m_renderFinished.at(index).get(); }

private:
    const VulkanContext& m_context;
    bool m_vsync;
    VulkanHelpers::UniqueSwapchain m_swapchain;
    std::vector<VkImage> m_images;
    std::vector<VulkanHelpers::UniqueImageView> m_views;
    std::vector<VulkanHelpers::UniqueSemaphore> m_renderFinished;
    VkFormat m_format{VK_FORMAT_UNDEFINED};
    VkExtent2D m_extent{};
};
