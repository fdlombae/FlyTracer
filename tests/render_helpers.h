#pragma once

#include <gtest/gtest.h>
#include "Application.h"
#include "RaytracingPass.h"
#include "VulkanContext.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

// Headless rendering for the image tests, and compute shaders for the shader tests

struct RgbImage {
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint8_t> pixels;  // RGB
};

inline void WritePpm(const std::string& path, const RgbImage& image) {
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << image.width << " " << image.height << "\n255\n";
    file.write(reinterpret_cast<const char*>(image.pixels.data()), static_cast<std::streamsize>(image.pixels.size()));
}

inline std::optional<RgbImage> ReadPpm(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::string magic;
    RgbImage image;
    int maxValue = 0;
    if (!(file >> magic >> image.width >> image.height >> maxValue) || magic != "P6") {
        return std::nullopt;
    }
    file.get();  // The single whitespace after the header
    image.pixels.resize(size_t{image.width} * image.height * 3);
    file.read(reinterpret_cast<char*>(image.pixels.data()), static_cast<std::streamsize>(image.pixels.size()));
    return image;
}

// A Vulkan context, or nothing when there is no usable GPU: the image tests skip themselves then
inline std::unique_ptr<VulkanContext> TryCreateContext() {
    try {
        return std::make_unique<VulkanContext>();
    } catch (const std::exception&) {
        return nullptr;
    }
}

// One command buffer at a time, waited for
inline VulkanHelpers::CommandSubmitter MakeSubmitter(const VulkanContext& context) {
    return {context.Device(), context.CommandPool(), context.Queue()};
}

inline VulkanHelpers::Buffer MakeHostBuffer(const VulkanContext& context, VkDeviceSize size,
                                            VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
    return {context.Device(), context.PhysicalDevice(), size, usage, VulkanHelpers::kHostVisible};
}

// Runs a compute shader from build/bin/test_shaders over groupCount workgroups, with storage buffers
// at the given bindings and the push constants, and waits for its results
inline void RunCompute(const VulkanContext& context, const std::string& shaderName,
                       const std::vector<std::pair<uint32_t, VkBuffer>>& buffers,
                       std::span<const std::byte> pushConstants, uint32_t groupCount) {
    const VkDevice device = context.Device();
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (const uint32_t binding : buffers | std::views::keys) {
        bindings.push_back({binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
    }
    const auto setLayout = VulkanHelpers::createDescriptorSetLayout(device, bindings);
    const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(buffers.size())};
    const auto pool = VulkanHelpers::createDescriptorPool(device, 1, {&poolSize, 1});
    const VkDescriptorSet set = VulkanHelpers::allocateDescriptorSet(device, pool.get(), setLayout.get());
    for (const auto& [binding, buffer] : buffers) {
        VulkanHelpers::writeStorageBufferDescriptor(device, set, binding, buffer);
    }
    const auto layout = VulkanHelpers::createPipelineLayout(device, setLayout.get(),
                                                            static_cast<uint32_t>(pushConstants.size()));
    const auto pipeline = VulkanHelpers::createComputePipeline(
        device, layout.get(), ExecutableRelativePath("test_shaders/" + shaderName + ".spv").string());

    MakeSubmitter(context).Submit([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout.get(), 0, 1, &set, 0, nullptr);
        vkCmdPushConstants(cmd, layout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           static_cast<uint32_t>(pushConstants.size()), pushConstants.data());
        vkCmdDispatch(cmd, groupCount, 1, 1);
        VulkanHelpers::shaderWritesToHostBarrier(cmd);
    });
}

// The raytracer's output image
inline RgbImage ReadOutput(const VulkanContext& context, const RaytracingPass& raytracing,
                           VulkanHelpers::CommandSubmitter& submitter) {
    const VkExtent2D size = raytracing.Output().extent();
    const VkDeviceSize byteCount = VkDeviceSize{size.width} * size.height * 4;
    const VulkanHelpers::Buffer readback = MakeHostBuffer(context, byteCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    submitter.Submit([&](VkCommandBuffer cmd) {
        const VkImage output = raytracing.Output().image();
        VulkanHelpers::transitionImageLayout2(cmd, output,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        const VkBufferImageCopy region{
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageExtent = {size.width, size.height, 1},
        };
        vkCmdCopyImageToBuffer(cmd, output, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer(), 1, &region);
        // Back to GENERAL for the next frame's dispatch
        VulkanHelpers::transitionImageLayout2(cmd, output,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    });

    RgbImage image{size.width, size.height, {}};
    const auto* rgba = static_cast<const uint8_t*>(readback.mapped());
    for (VkDeviceSize i = 0; i < byteCount; i += 4) {
        image.pixels.insert(image.pixels.end(), rgba + i, rgba + i + 3);
    }
    return image;
}

// Accumulates samples of an initialized scene, then reads the image back
inline RgbImage RenderImage(const VulkanContext& context, const GameScene& scene, VkExtent2D size, uint32_t samples) {
    RaytracingPass raytracing(context, size, ExecutableRelativePath("shaders").string(), 3);
    raytracing.UploadMeshes(scene.GetMeshes());

    VulkanHelpers::CommandSubmitter submitter = MakeSubmitter(context);
    for (uint32_t frame = 0; frame < samples; ++frame) {
        submitter.Submit([&](VkCommandBuffer cmd) {
            raytracing.Record(cmd, frame % RaytracingPass::kFramesInFlight, scene);
        });
    }
    return ReadOutput(context, raytracing, submitter);
}

struct ImageDifference {
    double mean{0.0};               // Per channel, 0 to 255
    double differentFraction{0.0};  // Of the pixels with a channel more than 16 apart
};

inline ImageDifference Compare(const RgbImage& a, const RgbImage& b) {
    double total = 0.0;
    size_t different = 0;
    for (size_t i = 0; i < a.pixels.size(); i += 3) {
        int pixelDifference = 0;
        for (size_t channel = 0; channel < 3; ++channel) {
            const int difference = std::abs(int{a.pixels[i + channel]} - int{b.pixels[i + channel]});
            pixelDifference = std::max(pixelDifference, difference);
            total += difference;
        }
        different += pixelDifference > 16 ? 1 : 0;
    }
    return {total / static_cast<double>(a.pixels.size()),
            static_cast<double>(different) / (static_cast<double>(a.width) * a.height)};
}

// Compares with the reference image <referenceBase>.ppm, which is recorded when it is missing. Small
// differences between GPUs and drivers are fine; a changed image is not, and is written to
// <referenceBase>_actual.ppm.
inline void ExpectMatchesReference(const RgbImage& rendered, const std::string& referenceBase) {
    const std::optional<RgbImage> reference = ReadPpm(referenceBase + ".ppm");
    if (!reference) {
        WritePpm(referenceBase + ".ppm", rendered);
        GTEST_SKIP() << "Recorded a new reference image: " << referenceBase << ".ppm";
    }
    ASSERT_EQ(reference->width, rendered.width);
    ASSERT_EQ(reference->height, rendered.height);

    const ImageDifference difference = Compare(rendered, *reference);
    if (difference.mean >= 1.0 || difference.differentFraction >= 0.005) {
        WritePpm(referenceBase + "_actual.ppm", rendered);
    }
    EXPECT_LT(difference.mean, 1.0);
    EXPECT_LT(difference.differentFraction, 0.005);
}
