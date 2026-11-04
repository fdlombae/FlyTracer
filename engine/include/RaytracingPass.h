#pragma once

#include "Camera.h"
#include "GameScene.h"
#include "GpuScene.h"
#include "GpuTimer.h"
#include "SceneEncoding.h"
#include "ShaderInterface.h"
#include "VulkanContext.h"
#include "VulkanHelpers.h"
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// The compute shader raytracer (raytracer.comp). It accumulates samples while the view and scene
// stay the same, and writes the tone-mapped, sRGB-encoded result to the output image.
class RaytracingPass {
public:
    static constexpr uint32_t kFramesInFlight = 2;
    static constexpr uint32_t kMaxTextures = MAX_TEXTURES;

    RaytracingPass(const VulkanContext& context, VkExtent2D extent, const std::string& shaderDir,
                   uint32_t maxBounces);

    RaytracingPass(const RaytracingPass&) = delete;
    RaytracingPass& operator=(const RaytracingPass&) = delete;

    // Geometry, BVHs and textures of all meshes. Record also does this when the meshes changed, but
    // calling it after loading the scene keeps the upload out of the first frame.
    void UploadMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes);
    // Recreates the images at a new size; the GPU must be idle
    void Resize(VkExtent2D extent);

    // Updates this frame's scene data and records the dispatch.
    // The output image is in GENERAL layout before and after.
    void Record(VkCommandBuffer cmdBuffer, uint32_t frameIndex, const GameScene& scene);

    [[nodiscard]] const VulkanHelpers::Image& Output() const noexcept { return m_output; }
    // GPU time of the dispatch recorded kFramesInFlight frames ago, measured when Record reused its frame
    [[nodiscard]] std::optional<double> DispatchMilliseconds() const noexcept { return m_dispatchMilliseconds; }

private:
    // Scene data the CPU rewrites every frame, one copy per frame in flight
    struct FrameBuffers {
        VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
        VulkanHelpers::Buffer materials;
        VulkanHelpers::Buffer spheres;
        VulkanHelpers::Buffer planes;
        VulkanHelpers::Buffer lights;
        VulkanHelpers::Buffer instances;
        VulkanHelpers::Buffer scenePairs;
        VulkanHelpers::Buffer debugLines;
    };

    void createImages(VkExtent2D extent);
    void createDescriptors(const std::string& shaderDir);
    template<typename T>
    void writeFrameBuffer(VulkanHelpers::Buffer& buffer, VkDescriptorSet set, uint32_t binding,
                          const std::vector<T>& data);
    // Restarts accumulation when anything that affects the image changed since the last frame
    void updateSampleIndex(std::vector<std::byte> frameState);

    const VulkanContext& m_context;
    uint32_t m_maxBounces;

    VulkanHelpers::Image m_accumulation;  // Running average of the linear radiance
    VulkanHelpers::Image m_output;        // Tone-mapped sRGB, blitted to the swapchain

    VulkanHelpers::UniqueDescriptorSetLayout m_descriptorSetLayout;
    VulkanHelpers::UniqueDescriptorPool m_descriptorPool;
    VulkanHelpers::UniquePipelineLayout m_pipelineLayout;
    VulkanHelpers::UniquePipeline m_pipeline;
    VulkanHelpers::UniqueSampler m_sampler;

    // Static mesh data
    VulkanHelpers::Buffer m_vertices;
    VulkanHelpers::Buffer m_triangles;
    VulkanHelpers::Buffer m_edges;
    VulkanHelpers::Buffer m_edgeTriangles;
    VulkanHelpers::Buffer m_meshPairs;
    VulkanHelpers::Buffer m_meshRootPairs;
    std::vector<VulkanHelpers::Image> m_textures;  // The first one is white, for untextured slots
    MeshTable m_meshTable;

    std::array<FrameBuffers, kFramesInFlight> m_frames;
    GpuTimer m_timer;
    std::optional<double> m_dispatchMilliseconds;

    Motor m_cameraMotor{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<std::byte> m_previousFrameState;
    uint32_t m_sampleIndex{0};
    uint32_t m_frameIndex{0};
};
