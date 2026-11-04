#include "ImGuiLayer.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

ImGuiLayer::ImGuiLayer(SDL_Window* window, const VulkanContext& context, const Swapchain& swapchain)
    : m_device(context.Device()), m_colorFormat(swapchain.Format())
{
    const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    m_descriptorPool = VulkanHelpers::createDescriptorPool(m_device, 1, {&poolSize, 1},
                                                           VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForVulkan(window);

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = context.Instance(),
        .PhysicalDevice = context.PhysicalDevice(),
        .Device = m_device,
        .QueueFamily = context.QueueFamily(),
        .Queue = context.Queue(),
        .DescriptorPool = m_descriptorPool.get(),
        .MinImageCount = swapchain.ImageCount(),
        .ImageCount = swapchain.ImageCount(),
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &m_colorFormat,
        },
    };
    ImGui_ImplVulkan_Init(&initInfo);
}

ImGuiLayer::~ImGuiLayer() noexcept {
    vkDeviceWaitIdle(m_device);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::OnSwapchainRecreated(const Swapchain& swapchain) {
    ImGui_ImplVulkan_SetMinImageCount(swapchain.ImageCount());
}

void ImGuiLayer::NewFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::Record(VkCommandBuffer cmdBuffer, VkImageView view, VkExtent2D extent) {
    ImGui::Render();

    const VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    const VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    };
    vkCmdBeginRendering(cmdBuffer, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
    vkCmdEndRendering(cmdBuffer);
}
