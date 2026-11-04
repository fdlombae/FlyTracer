#include "VulkanContext.h"
#include "VulkanHelpers.h"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using VulkanHelpers::check;
using VulkanHelpers::enumerate;

namespace {

#ifdef NDEBUG
constexpr bool kEnableValidation = false;
#else
constexpr bool kEnableValidation = true;
#endif

bool isEnvEnabled(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && std::string_view(value) == "1";
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    [[maybe_unused]] void* userData)
{
    const char* label = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "\033[31m[ERROR]\033[0m"
                      : (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "\033[33m[WARNING]\033[0m"
                      : "\033[36m[INFO]\033[0m";
    std::cerr << "Vulkan " << label << " ";
    if (callbackData->pMessageIdName != nullptr) {
        std::cerr << "[" << callbackData->pMessageIdName << "] ";
    }
    std::cerr << callbackData->pMessage << '\n';
    return VK_FALSE;
}

bool isLayerAvailable(const char* name) {
    return std::ranges::any_of(enumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties),
                               [name](const VkLayerProperties& layer) { return strcmp(name, layer.layerName) == 0; });
}

int deviceTypeScore(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 1000;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 100;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 50;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return 10;
        default: return 1;
    }
}

} // anonymous namespace

VulkanContext::VulkanContext(SDL_Window* window) {
    try {
        createInstance(window != nullptr);
        if (window != nullptr && !SDL_Vulkan_CreateSurface(window, m_instance, nullptr, &m_surface)) {
            throw std::runtime_error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
        }
        selectPhysicalDevice();
        createDevice();

        const VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_queueFamily,
        };
        check(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "vkCreateCommandPool");
    } catch (...) {
        destroy();
        throw;
    }
}

VulkanContext::~VulkanContext() noexcept {
    destroy();
}

void VulkanContext::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_instance == VK_NULL_HANDLE) return;
    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        destroyMessenger(m_instance, m_debugMessenger, nullptr);
    }
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::WaitIdle() const {
    check(vkDeviceWaitIdle(m_device), "vkDeviceWaitIdle");
}

void VulkanContext::createInstance(bool withSurface) {
    std::vector<const char*> extensions;
    if (withSurface) {
        uint32_t sdlExtensionCount = 0;
        const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        if (sdlExtensions == nullptr) {
            throw std::runtime_error("Failed to get SDL Vulkan extensions: " + std::string(SDL_GetError()));
        }
        extensions.assign(sdlExtensions, sdlExtensions + sdlExtensionCount);
    }

    const VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "FlyTracer",
        .pEngineName = "FlyTracer",
        .apiVersion = VK_API_VERSION_1_4,
    };
    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
    };

    const VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
    };
    // FLYTRACER_GPU_VALIDATION=1 additionally enables GPU-assisted validation. The messenger in the
    // chain also reports messages from vkCreateInstance and vkDestroyInstance.
    const VkValidationFeatureEnableEXT gpuValidation = VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT;
    const bool gpuValidationEnabled = isEnvEnabled("FLYTRACER_GPU_VALIDATION");
    const VkValidationFeaturesEXT validationFeatures{
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .pNext = &debugCreateInfo,
        .enabledValidationFeatureCount = gpuValidationEnabled ? 1u : 0u,
        .pEnabledValidationFeatures = &gpuValidation,
    };

    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    const bool validation = kEnableValidation && isLayerAvailable(validationLayer);
    if (validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        createInfo.pNext = &validationFeatures;
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayer;
    } else if (kEnableValidation) {
        std::cerr << "Warning: " << validationLayer << " is not installed\n";
    }
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    check(vkCreateInstance(&createInfo, nullptr, &m_instance), "vkCreateInstance");

    if (validation) {
        auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        check(createMessenger(m_instance, &debugCreateInfo, nullptr, &m_debugMessenger),
              "vkCreateDebugUtilsMessengerEXT");
    }
}

void VulkanContext::selectPhysicalDevice() {
    int bestScore = 0;
    for (VkPhysicalDevice device : enumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, m_instance)) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_3) {
            continue;  // Synchronization2 and dynamic rendering are used as core Vulkan 1.3
        }

        // One queue family does compute (raytracing), graphics (ImGui) and present
        const auto queueFamilies =
            enumerate<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, device);
        constexpr VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        const int score = deviceTypeScore(properties.deviceType);
        for (uint32_t i = 0; i < queueFamilies.size() && score > bestScore; ++i) {
            VkBool32 presentSupport = VK_TRUE;
            if (m_surface != VK_NULL_HANDLE) {
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
            }
            if ((queueFamilies[i].queueFlags & required) == required && presentSupport) {
                bestScore = score;
                m_physicalDevice = device;
                m_queueFamily = i;
            }
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a Vulkan 1.3 GPU with a graphics and compute queue");
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    std::cout << "Selected GPU: " << properties.deviceName << "\n";
}

void VulkanContext::createDevice() {
    const float queuePriority = 1.0f;
    const VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    std::vector<const char*> extensions;
    if (m_surface != VK_NULL_HANDLE) {
        extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    // Only enable what is used: optional features like robustBufferAccess cost performance.
    // Texture indices vary per pixel, so the texture array needs non-uniform indexing. The raytracer's
    // buffers are tightly packed (scalar block layout). ImGui draws with dynamic rendering.
    VkPhysicalDeviceVulkan13Features vulkan13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features vulkan12Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &vulkan13Features,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .scalarBlockLayout = VK_TRUE,
    };
    const VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &vulkan12Features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };
    check(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device), "vkCreateDevice");
    vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
}
