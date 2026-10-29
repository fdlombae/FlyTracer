#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <cstdint>

// Instance, device and the single queue used for compute, graphics and present.
// Without a window (headless, e.g. for image tests) no surface is created.
class VulkanContext {
public:
    explicit VulkanContext(SDL_Window* window = nullptr);
    ~VulkanContext() noexcept;

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    [[nodiscard]] VkInstance Instance() const noexcept { return m_instance; }
    [[nodiscard]] VkSurfaceKHR Surface() const noexcept { return m_surface; }
    [[nodiscard]] VkPhysicalDevice PhysicalDevice() const noexcept { return m_physicalDevice; }
    [[nodiscard]] VkDevice Device() const noexcept { return m_device; }
    [[nodiscard]] VkQueue Queue() const noexcept { return m_queue; }
    [[nodiscard]] uint32_t QueueFamily() const noexcept { return m_queueFamily; }
    [[nodiscard]] VkCommandPool CommandPool() const noexcept { return m_commandPool; }

    void WaitIdle() const;

private:
    void createInstance(bool withSurface);
    void selectPhysicalDevice();
    void createDevice();
    void destroy() noexcept;

    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_queue{VK_NULL_HANDLE};
    uint32_t m_queueFamily{0};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};
};
