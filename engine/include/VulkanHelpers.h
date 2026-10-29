#pragma once

#include <vulkan/vulkan.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace VulkanHelpers {

inline void check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(what) + " failed (VkResult " + std::to_string(result) + ")");
    }
}

// The two calls of a Vulkan enumeration, e.g. enumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, instance):
// the count, then the elements
template<typename T, typename Function, typename... Args>
[[nodiscard]] std::vector<T> enumerate(Function function, Args... args) {
    uint32_t count = 0;
    function(args..., &count, nullptr);
    std::vector<T> elements(count);
    function(args..., &count, elements.data());
    elements.resize(count);
    return elements;
}

// Owns a handle created from a VkDevice and destroys it with the matching vkDestroy* function
template<typename Handle, auto Destroy>
class DeviceHandle {
public:
    DeviceHandle() noexcept = default;
    DeviceHandle(VkDevice device, Handle handle) noexcept : m_device(device), m_handle(handle) {}

    DeviceHandle(const DeviceHandle&) = delete;
    DeviceHandle& operator=(const DeviceHandle&) = delete;

    DeviceHandle(DeviceHandle&& other) noexcept
        : m_device(std::exchange(other.m_device, VK_NULL_HANDLE))
        , m_handle(std::exchange(other.m_handle, VK_NULL_HANDLE)) {}

    DeviceHandle& operator=(DeviceHandle&& other) noexcept {
        if (this != &other) {
            reset();
            m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
            m_handle = std::exchange(other.m_handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    ~DeviceHandle() { reset(); }

    [[nodiscard]] Handle get() const noexcept { return m_handle; }
    [[nodiscard]] const Handle* address() const noexcept { return &m_handle; }

private:
    void reset() noexcept {
        if (m_handle != VK_NULL_HANDLE) {
            Destroy(m_device, m_handle, nullptr);
            m_handle = VK_NULL_HANDLE;
        }
    }

    VkDevice m_device{VK_NULL_HANDLE};
    Handle m_handle{VK_NULL_HANDLE};
};

using UniqueBuffer = DeviceHandle<VkBuffer, vkDestroyBuffer>;
using UniqueImage = DeviceHandle<VkImage, vkDestroyImage>;
using UniqueDeviceMemory = DeviceHandle<VkDeviceMemory, vkFreeMemory>;
using UniqueSemaphore = DeviceHandle<VkSemaphore, vkDestroySemaphore>;
using UniqueFence = DeviceHandle<VkFence, vkDestroyFence>;
using UniqueImageView = DeviceHandle<VkImageView, vkDestroyImageView>;
using UniqueSampler = DeviceHandle<VkSampler, vkDestroySampler>;
using UniqueDescriptorPool = DeviceHandle<VkDescriptorPool, vkDestroyDescriptorPool>;
using UniqueDescriptorSetLayout = DeviceHandle<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>;
using UniquePipelineLayout = DeviceHandle<VkPipelineLayout, vkDestroyPipelineLayout>;
using UniquePipeline = DeviceHandle<VkPipeline, vkDestroyPipeline>;
using UniqueSwapchain = DeviceHandle<VkSwapchainKHR, vkDestroySwapchainKHR>;
using UniqueQueryPool = DeviceHandle<VkQueryPool, vkDestroyQueryPool>;

[[nodiscard]] inline UniqueSemaphore createSemaphore(VkDevice device) {
    const VkSemaphoreCreateInfo createInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore;
    check(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore), "vkCreateSemaphore");
    return {device, semaphore};
}

[[nodiscard]] inline UniqueFence createFence(VkDevice device, bool signaled) {
    const VkFenceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags{0},
    };
    VkFence fence;
    check(vkCreateFence(device, &createInfo, nullptr, &fence), "vkCreateFence");
    return {device, fence};
}

[[nodiscard]] inline UniqueImageView createImageView(VkDevice device, VkImage image, VkFormat format,
                                                     uint32_t mipLevels = 1) {
    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = mipLevels, .layerCount = 1},
    };
    VkImageView imageView;
    check(vkCreateImageView(device, &viewInfo, nullptr, &imageView), "vkCreateImageView");
    return {device, imageView};
}

[[nodiscard]] inline UniqueDescriptorSetLayout createDescriptorSetLayout(
    VkDevice device, std::span<const VkDescriptorSetLayoutBinding> bindings) {
    const VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    VkDescriptorSetLayout layout;
    check(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout), "vkCreateDescriptorSetLayout");
    return {device, layout};
}

[[nodiscard]] inline UniqueDescriptorPool createDescriptorPool(VkDevice device, uint32_t maxSets,
                                                               std::span<const VkDescriptorPoolSize> sizes,
                                                               VkDescriptorPoolCreateFlags flags = 0) {
    const VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = flags,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data(),
    };
    VkDescriptorPool pool;
    check(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
    return {device, pool};
}

// Freed with its pool
[[nodiscard]] inline VkDescriptorSet allocateDescriptorSet(VkDevice device, VkDescriptorPool pool,
                                                           VkDescriptorSetLayout layout) {
    const VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    VkDescriptorSet set;
    check(vkAllocateDescriptorSets(device, &allocInfo, &set), "vkAllocateDescriptorSets");
    return set;
}

// One descriptor set and push constants for the compute stage
[[nodiscard]] inline UniquePipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout,
                                                               uint32_t pushConstantSize) {
    const VkPushConstantRange pushConstants{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = pushConstantSize};
    const VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &setLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstants,
    };
    VkPipelineLayout layout;
    check(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout");
    return {device, layout};
}

[[nodiscard]] inline uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                                             VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

inline constexpr VkMemoryPropertyFlags kHostVisible =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

// Memory the CPU writes every frame and the GPU reads: device-local when the CPU can map it
// (unified memory, resizable BAR), otherwise plain host memory
[[nodiscard]] inline VkMemoryPropertyFlags hostWriteMemoryProperties(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    const std::span types(memProperties.memoryTypes, memProperties.memoryTypeCount);
    const bool mappableDeviceMemory = std::ranges::any_of(types, [](const VkMemoryType& type) {
        return (type.propertyFlags & kHostVisible) == kHostVisible &&
               (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    });
    return mappableDeviceMemory ? kHostVisible | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : kHostVisible;
}

[[nodiscard]] inline UniqueDeviceMemory allocateMemory(VkDevice device, VkPhysicalDevice physicalDevice,
                                                       const VkMemoryRequirements& requirements,
                                                       VkMemoryPropertyFlags properties) {
    const VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, requirements.memoryTypeBits, properties),
    };
    VkDeviceMemory memory;
    check(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "vkAllocateMemory");
    return {device, memory};
}

// A Vulkan buffer with its own memory; host-visible buffers stay mapped for their whole lifetime
class Buffer {
public:
    Buffer() noexcept = default;

    Buffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
           VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
        : m_size(size)
    {
        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VkBuffer buffer;
        check(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "vkCreateBuffer");
        m_buffer = UniqueBuffer(device, buffer);

        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        m_memory = allocateMemory(device, physicalDevice, requirements, properties);
        check(vkBindBufferMemory(device, buffer, m_memory.get(), 0), "vkBindBufferMemory");
        if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            check(vkMapMemory(device, m_memory.get(), 0, VK_WHOLE_SIZE, 0, &m_mapped), "vkMapMemory");
        }
    }

    // Copies into the mapped memory of a host-visible buffer
    void write(const void* data, VkDeviceSize size) {
        if (m_mapped == nullptr || size > m_size) {
            throw std::runtime_error("Buffer write out of range or buffer not host visible");
        }
        std::memcpy(m_mapped, data, static_cast<size_t>(size));
    }

    [[nodiscard]] VkBuffer buffer() const noexcept { return m_buffer.get(); }
    [[nodiscard]] VkDeviceSize size() const noexcept { return m_size; }
    // Mapped memory of a host-visible buffer, nullptr otherwise
    [[nodiscard]] const void* mapped() const noexcept { return m_mapped; }

private:
    // The buffer is declared last, so it is destroyed before its memory is freed
    UniqueDeviceMemory m_memory;
    UniqueBuffer m_buffer;
    VkDeviceSize m_size{0};
    void* m_mapped{nullptr};
};

// A device-local 2D image with its own memory and a view over all its mip levels
class Image {
public:
    Image() noexcept = default;

    Image(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
          VkImageUsageFlags usage, uint32_t mipLevels = 1)
        : m_extent(extent), m_mipLevels(mipLevels)
    {
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {extent.width, extent.height, 1},
            .mipLevels = mipLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        VkImage image;
        check(vkCreateImage(device, &imageInfo, nullptr, &image), "vkCreateImage");
        m_image = UniqueImage(device, image);

        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(device, image, &requirements);
        m_memory = allocateMemory(device, physicalDevice, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkBindImageMemory(device, image, m_memory.get(), 0), "vkBindImageMemory");
        m_view = createImageView(device, image, format, mipLevels);
    }

    [[nodiscard]] VkImage image() const noexcept { return m_image.get(); }
    [[nodiscard]] VkImageView view() const noexcept { return m_view.get(); }
    [[nodiscard]] VkExtent2D extent() const noexcept { return m_extent; }
    [[nodiscard]] uint32_t mipLevels() const noexcept { return m_mipLevels; }

private:
    // Destroyed in reverse order: the view, then the image, then its memory
    UniqueDeviceMemory m_memory;
    UniqueImage m_image;
    UniqueImageView m_view;
    VkExtent2D m_extent{};
    uint32_t m_mipLevels{1};
};

// Image layout transition using synchronization2 (core in Vulkan 1.3)
inline void transitionImageLayout2(
    VkCommandBuffer cmdBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage,
    VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 dstStage,
    VkAccessFlags2 dstAccess,
    uint32_t baseMipLevel = 0,
    uint32_t mipLevelCount = VK_REMAINING_MIP_LEVELS)
{
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStage,
        .srcAccessMask = srcAccess,
        .dstStageMask = dstStage,
        .dstAccessMask = dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = baseMipLevel,
                             .levelCount = mipLevelCount, .layerCount = 1},
    };
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmdBuffer, &dependency);
}

// Makes the shader writes of the commands before it visible to the CPU once they completed
inline void shaderWritesToHostBarrier(VkCommandBuffer cmdBuffer) {
    const VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
    };
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmdBuffer, &dependency);
}

[[nodiscard]] inline VkCommandBuffer allocateCommandBuffer(VkDevice device, VkCommandPool commandPool) {
    const VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer commandBuffer;
    check(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), "vkAllocateCommandBuffers");
    return commandBuffer;
}

// Resets the command buffer and starts recording commands that are submitted once
inline void beginOneTimeCommands(VkCommandBuffer commandBuffer) {
    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    check(vkResetCommandBuffer(commandBuffer, 0), "vkResetCommandBuffer");
    check(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");
}

// One command buffer; waits and signals are optional
inline void submit(VkQueue queue, VkCommandBuffer commandBuffer, const VkSemaphoreSubmitInfo* wait = nullptr,
                   const VkSemaphoreSubmitInfo* signal = nullptr, VkFence fence = VK_NULL_HANDLE) {
    const VkCommandBufferSubmitInfo commandBufferInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffer,
    };
    const VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = wait != nullptr ? 1u : 0u,
        .pWaitSemaphoreInfos = wait,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandBufferInfo,
        .signalSemaphoreInfoCount = signal != nullptr ? 1u : 0u,
        .pSignalSemaphoreInfos = signal,
    };
    check(vkQueueSubmit2(queue, 1, &submitInfo, fence), "vkQueueSubmit2");
}

// Records command buffers and submits them, waiting until the GPU has executed them; one at a time
class CommandSubmitter {
public:
    CommandSubmitter(VkDevice device, VkCommandPool commandPool, VkQueue queue)
        : m_device(device), m_commandPool(commandPool), m_queue(queue)
        , m_commandBuffer(allocateCommandBuffer(device, commandPool)) {}

    CommandSubmitter(const CommandSubmitter&) = delete;
    CommandSubmitter& operator=(const CommandSubmitter&) = delete;

    ~CommandSubmitter() {
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_commandBuffer);
    }

    // Starts recording; End submits
    VkCommandBuffer Begin() {
        beginOneTimeCommands(m_commandBuffer);
        return m_commandBuffer;
    }

    // Returns the milliseconds from the submission until the commands completed
    double End() {
        check(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer");
        const auto start = std::chrono::steady_clock::now();
        submit(m_queue, m_commandBuffer);
        check(vkQueueWaitIdle(m_queue), "vkQueueWaitIdle");
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

    template<typename Record>
    double Submit(Record record) {
        record(Begin());
        return End();
    }

private:
    VkDevice m_device;
    VkCommandPool m_commandPool;
    VkQueue m_queue;
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
};

// Records all staging copies into a single command buffer and submits them with one wait
class StagingUploader {
public:
    StagingUploader(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue)
        : m_device(device), m_physicalDevice(physicalDevice), m_submitter(device, commandPool, queue)
        , m_commandBuffer(m_submitter.Begin()) {}

    // Returns a device-local storage buffer that holds the data once submit() has returned
    template<typename T>
    [[nodiscard]] Buffer upload(const std::vector<T>& data) {
        const VkDeviceSize dataSize = sizeof(T) * data.size();
        Buffer buffer(m_device, m_physicalDevice, std::max<VkDeviceSize>(dataSize, sizeof(T)),
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (dataSize > 0) {
            const VkBufferCopy region{.size = dataSize};
            vkCmdCopyBuffer(m_commandBuffer, stage(data.data(), dataSize).buffer(), buffer.buffer(), 1, &region);
        }
        return buffer;
    }

    // Fills mip level 0 with RGBA8 pixels, then generates the smaller levels by blitting each level
    // into the next. The image ends in SHADER_READ_ONLY_OPTIMAL.
    void uploadTexture(const Image& image, const void* pixels) {
        const VkExtent2D extent = image.extent();
        const Buffer& staging = stage(pixels, VkDeviceSize{extent.width} * extent.height * 4);

        // All levels: level 0 is written by the copy, the others by blits
        transitionImageLayout2(m_commandBuffer, image.image(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        const VkBufferImageCopy region{
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageExtent = {extent.width, extent.height, 1},
        };
        vkCmdCopyBufferToImage(m_commandBuffer, staging.buffer(), image.image(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        auto width = static_cast<int32_t>(extent.width);
        auto height = static_cast<int32_t>(extent.height);
        for (uint32_t level = 1; level < image.mipLevels(); ++level) {
            transitionImageLayout2(m_commandBuffer, image.image(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, level - 1, 1);

            const VkOffset3D source{width, height, 1};
            width = std::max(width / 2, 1);
            height = std::max(height / 2, 1);
            const VkImageBlit blit{
                .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1},
                .srcOffsets = {{}, source},
                .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1},
                .dstOffsets = {{}, {width, height, 1}},
            };
            vkCmdBlitImage(m_commandBuffer,
                           image.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, VK_FILTER_LINEAR);

            transitionImageLayout2(m_commandBuffer, image.image(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, level - 1, 1);
        }

        transitionImageLayout2(m_commandBuffer, image.image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, image.mipLevels() - 1, 1);
    }

    // For recording additional one-time commands (e.g. layout transitions) in the same submission
    [[nodiscard]] VkCommandBuffer commandBuffer() const noexcept { return m_commandBuffer; }

    void submit() {
        m_submitter.End();
        m_stagingBuffers.clear();
    }

private:
    const Buffer& stage(const void* data, VkDeviceSize size) {
        Buffer& staging = m_stagingBuffers.emplace_back(m_device, m_physicalDevice, size,
                                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, kHostVisible);
        staging.write(data, size);
        return staging;
    }

    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    CommandSubmitter m_submitter;
    VkCommandBuffer m_commandBuffer;
    std::vector<Buffer> m_stagingBuffers;
};

inline void writeStorageBufferDescriptor(VkDevice device, VkDescriptorSet set, uint32_t binding, VkBuffer buffer) {
    const VkDescriptorBufferInfo bufferInfo{.buffer = buffer, .range = VK_WHOLE_SIZE};
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &bufferInfo,
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

// Image descriptors of one binding, starting at array element 0
inline void writeImageDescriptors(VkDevice device, VkDescriptorSet set, uint32_t binding, VkDescriptorType type,
                                  std::span<const VkDescriptorImageInfo> images) {
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = binding,
        .descriptorCount = static_cast<uint32_t>(images.size()),
        .descriptorType = type,
        .pImageInfo = images.data(),
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

inline void writeStorageImageDescriptor(VkDevice device, VkDescriptorSet set, uint32_t binding, VkImageView view) {
    const VkDescriptorImageInfo imageInfo{.imageView = view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writeImageDescriptors(device, set, binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, {&imageInfo, 1});
}

[[nodiscard]] inline std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] inline UniquePipeline createComputePipeline(VkDevice device, VkPipelineLayout layout,
                                                          const std::string& spirvPath) {
    const std::vector<char> code = readFile(spirvPath);
    const VkShaderModuleCreateInfo moduleInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    };
    VkShaderModule shaderModule;
    check(vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule), "vkCreateShaderModule");

    const VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaderModule,
            .pName = "main",
        },
        .layout = layout,
    };
    VkPipeline pipeline;
    const VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    vkDestroyShaderModule(device, shaderModule, nullptr);
    check(result, "vkCreateComputePipelines");
    return {device, pipeline};
}

} // namespace VulkanHelpers
