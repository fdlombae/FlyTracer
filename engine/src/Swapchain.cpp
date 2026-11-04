#include "Swapchain.h"
#include <algorithm>
#include <iostream>

using VulkanHelpers::check;
using VulkanHelpers::enumerate;

namespace {

// FIFO waits for vsync and is always supported; otherwise prefer mailbox over immediate
VkPresentModeKHR choosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool vsync) {
    if (vsync) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    const auto modes = enumerate<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, physicalDevice, surface);
    for (VkPresentModeKHR preferred : {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR}) {
        if (std::ranges::contains(modes, preferred)) {
            return preferred;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR chooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    const auto formats = enumerate<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, physicalDevice, surface);
    for (VkFormat preferred : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}) {
        const auto it = std::ranges::find_if(formats, [preferred](const VkSurfaceFormatKHR& format) {
            return format.format == preferred && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
        if (it != formats.end()) {
            return *it;
        }
    }
    return formats.at(0);
}

} // anonymous namespace

Swapchain::Swapchain(const VulkanContext& context, VkExtent2D windowPixels, bool vsync)
    : m_context(context), m_vsync(vsync)
{
    Recreate(windowPixels);
}

void Swapchain::Recreate(VkExtent2D windowPixels) {
    const VkPhysicalDevice physicalDevice = m_context.PhysicalDevice();
    const VkSurfaceKHR surface = m_context.Surface();
    const VkDevice device = m_context.Device();

    VkSurfaceCapabilitiesKHR capabilities;
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities),
          "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = std::clamp(windowPixels.width, capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
        extent.height = std::clamp(windowPixels.height, capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(physicalDevice, surface);
    const VkPresentModeKHR presentMode = choosePresentMode(physicalDevice, surface, m_vsync);
    const VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = m_swapchain.get(),
    };

    // Views of the old images go before the old swapchain, which is replaced right after creation
    m_views.clear();
    m_renderFinished.clear();
    VkSwapchainKHR swapchain;
    check(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain), "vkCreateSwapchainKHR");
    m_swapchain = VulkanHelpers::UniqueSwapchain(device, swapchain);
    m_images = enumerate<VkImage>(vkGetSwapchainImagesKHR, device, swapchain);
    m_format = surfaceFormat.format;
    m_extent = extent;

    for (VkImage image : m_images) {
        m_views.push_back(VulkanHelpers::createImageView(device, image, m_format));
        m_renderFinished.push_back(VulkanHelpers::createSemaphore(device));
    }

    std::cout << "Swapchain " << extent.width << "x" << extent.height << ", " << m_images.size() << " images, "
              << (presentMode == VK_PRESENT_MODE_FIFO_KHR ? "vsync" : "no vsync") << "\n";
}
