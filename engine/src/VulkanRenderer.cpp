#include "VulkanRenderer.h"
#include <algorithm>
#include <stdexcept>

using VulkanHelpers::check;

VulkanRenderer::VulkanRenderer(SDL_Window* window, const Config& config)
    : m_window(window)
    , m_context(window)
    , m_swapchain(m_context, windowPixels(), config.vsync)
    , m_raytracing(m_context, m_swapchain.Extent(), config.shaderDir, config.maxBounces)
    , m_imgui(window, m_context, m_swapchain)
{
    for (Frame& frame : m_frames) {
        frame.commandBuffer = VulkanHelpers::allocateCommandBuffer(m_context.Device(), m_context.CommandPool());
        frame.imageAvailable = VulkanHelpers::createSemaphore(m_context.Device());
        frame.inFlight = VulkanHelpers::createFence(m_context.Device(), true);
    }
    m_imagesInFlight.assign(m_swapchain.ImageCount(), VK_NULL_HANDLE);
}

VulkanRenderer::~VulkanRenderer() noexcept {
    vkDeviceWaitIdle(m_context.Device());
}

void VulkanRenderer::WaitIdle() const {
    m_context.WaitIdle();
}

void VulkanRenderer::UploadMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes) {
    m_raytracing.UploadMeshes(meshes);
}

VkExtent2D VulkanRenderer::windowPixels() const {
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

void VulkanRenderer::recreateSwapchain() {
    m_context.WaitIdle();
    m_swapchain.Recreate(windowPixels());
    m_imgui.OnSwapchainRecreated(m_swapchain);
    m_raytracing.Resize(m_swapchain.Extent());
    m_imagesInFlight.assign(m_swapchain.ImageCount(), VK_NULL_HANDLE);
    m_swapchainOutdated = false;
}

bool VulkanRenderer::BeginFrame() {
    const VkExtent2D pixels = windowPixels();
    if (pixels.width == 0 || pixels.height == 0) {
        return false;  // Minimized
    }
    if (m_swapchainOutdated) {
        recreateSwapchain();
    }

    Frame& frame = m_frames[m_currentFrame];
    check(vkWaitForFences(m_context.Device(), 1, frame.inFlight.address(), VK_TRUE, UINT64_MAX), "vkWaitForFences");

    const VkResult result = vkAcquireNextImageKHR(m_context.Device(), m_swapchain.Handle(), UINT64_MAX,
                                                  frame.imageAvailable.get(), VK_NULL_HANDLE, &m_imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_swapchainOutdated = true;
        return false;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        m_swapchainOutdated = true;  // Still usable for this frame
    } else {
        check(result, "vkAcquireNextImageKHR");
    }

    // The acquired image may still be in use by the other frame in flight
    if (m_imagesInFlight[m_imageIndex] != VK_NULL_HANDLE) {
        check(vkWaitForFences(m_context.Device(), 1, &m_imagesInFlight[m_imageIndex], VK_TRUE, UINT64_MAX),
              "vkWaitForFences");
    }
    m_imagesInFlight[m_imageIndex] = frame.inFlight.get();
    check(vkResetFences(m_context.Device(), 1, frame.inFlight.address()), "vkResetFences");

    m_imgui.NewFrame();
    return true;
}

void VulkanRenderer::EndFrame(const GameScene& scene) {
    Frame& frame = m_frames[m_currentFrame];
    const VkCommandBuffer cmdBuffer = frame.commandBuffer;
    const VkImage swapchainImage = m_swapchain.Image(m_imageIndex);

    VulkanHelpers::beginOneTimeCommands(cmdBuffer);
    m_raytracing.Record(cmdBuffer, m_currentFrame, scene);
    recordBlitToSwapchain(cmdBuffer);
    m_imgui.Record(cmdBuffer, m_swapchain.View(m_imageIndex), m_swapchain.Extent());
    VulkanHelpers::transitionImageLayout2(cmdBuffer, swapchainImage,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    check(vkEndCommandBuffer(cmdBuffer), "vkEndCommandBuffer");

    // The acquire semaphore is waited on where the swapchain image is first touched: the blit.
    // ALL_COMMANDS includes the transition to PRESENT_SRC.
    const VkSemaphore renderFinished = m_swapchain.RenderFinished(m_imageIndex);
    const VkSemaphoreSubmitInfo waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = frame.imageAvailable.get(),
        .stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
    };
    const VkSemaphoreSubmitInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = renderFinished,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    VulkanHelpers::submit(m_context.Queue(), cmdBuffer, &waitInfo, &signalInfo, frame.inFlight.get());

    const VkSwapchainKHR swapchain = m_swapchain.Handle();
    const VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &m_imageIndex,
    };
    const VkResult result = vkQueuePresentKHR(m_context.Queue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_swapchainOutdated = true;
    } else {
        check(result, "vkQueuePresentKHR");
    }

    m_currentFrame = (m_currentFrame + 1) % RaytracingPass::kFramesInFlight;
}

// The raytracer renders at the swapchain size; the blit converts RGBA to the swapchain's BGRA
void VulkanRenderer::recordBlitToSwapchain(VkCommandBuffer cmdBuffer) {
    const VkImage output = m_raytracing.Output().image();
    const VkImage swapchainImage = m_swapchain.Image(m_imageIndex);

    VulkanHelpers::transitionImageLayout2(cmdBuffer, output,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    // The source stage matches the acquire semaphore wait stage, so the transition happens after acquire
    VulkanHelpers::transitionImageLayout2(cmdBuffer, swapchainImage,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    const auto corner = [](VkExtent2D extent) {
        return VkOffset3D{static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1};
    };
    const VkImageBlit region{
        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .srcOffsets = {{}, corner(m_raytracing.Output().extent())},
        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .dstOffsets = {{}, corner(m_swapchain.Extent())},
    };
    vkCmdBlitImage(cmdBuffer, output, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);

    VulkanHelpers::transitionImageLayout2(cmdBuffer, swapchainImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    // Back to GENERAL for the next frame's dispatch, which must wait for this blit
    VulkanHelpers::transitionImageLayout2(cmdBuffer, output,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}
