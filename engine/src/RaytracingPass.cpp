#include "RaytracingPass.h"
#include "ImageFile.h"
#include "PGA.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <span>

namespace {

constexpr VkFormat kAccumulationFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr VkFormat kOutputFormat = VK_FORMAT_R8G8B8A8_UNORM;

// The bytes of the ranges, one after the other
std::vector<std::byte> concatenatedBytes(const auto&... ranges) {
    std::vector<std::byte> bytes;
    (bytes.append_range(std::as_bytes(std::span(ranges))), ...);
    return bytes;
}

} // anonymous namespace

RaytracingPass::RaytracingPass(const VulkanContext& context, VkExtent2D extent, const std::string& shaderDir,
                               uint32_t maxBounces)
    : m_context(context), m_shaderPath(shaderDir + "/raytracer.comp.spv"), m_maxBounces(maxBounces),
      m_timer(context, kFramesInFlight)
{
    createDescriptors();
    ReloadShader();
    createImages(extent);
    UploadMeshes({});  // Valid (empty) mesh data until the scene's meshes arrive
}

void RaytracingPass::createDescriptors() {
    const VkDevice device = m_context.Device();

    // The scene set is pushed into every frame's command buffer, so no descriptor sets have to be
    // kept up to date per frame in flight
    std::array<VkDescriptorSetLayoutBinding, BINDING_COUNT> bindings{};
    for (uint32_t binding = 0; binding < BINDING_COUNT; ++binding) {
        bindings[binding] = {binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    }
    bindings[BINDING_ACCUMULATION_IMAGE].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[BINDING_OUTPUT_IMAGE].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    m_sceneSetLayout = VulkanHelpers::createDescriptorSetLayout(
        device, bindings, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT);

    // The texture array is too large to push (maxPushDescriptors is only guaranteed to be 32) and
    // changes only with the meshes
    const VkDescriptorSetLayoutBinding textureBinding{BINDING_TEXTURES, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      kMaxTextures, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    m_textureSetLayout = VulkanHelpers::createDescriptorSetLayout(device, {&textureBinding, 1});
    const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxTextures};
    m_descriptorPool = VulkanHelpers::createDescriptorPool(device, 1, {&poolSize, 1});
    m_textureSet = VulkanHelpers::allocateDescriptorSet(device, m_descriptorPool.get(), m_textureSetLayout.get());

    const std::array setLayouts{m_sceneSetLayout.get(), m_textureSetLayout.get()};
    static_assert(SET_SCENE == 0 && SET_TEXTURES == 1);
    m_pipelineLayout = VulkanHelpers::createPipelineLayout(device, setLayouts, sizeof(Gpu::PushConstants));

    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .maxLod = VK_LOD_CLAMP_NONE,
    };
    VkSampler sampler;
    VulkanHelpers::check(vkCreateSampler(device, &samplerInfo, nullptr, &sampler), "vkCreateSampler");
    m_sampler = VulkanHelpers::UniqueSampler(device, sampler);
}

void RaytracingPass::ReloadShader() {
    m_context.WaitIdle();
    m_pipeline = VulkanHelpers::createComputePipeline(m_context.Device(), m_pipelineLayout.get(), m_shaderPath);
    m_context.SetName(VK_OBJECT_TYPE_PIPELINE, m_pipeline.get(), "Raytracer pipeline");
    m_previousFrameState.clear();
}

void RaytracingPass::createImages(VkExtent2D extent) {
    const VkDevice device = m_context.Device();
    m_accumulation = VulkanHelpers::Image(device, m_context.PhysicalDevice(), extent, kAccumulationFormat,
                                          VK_IMAGE_USAGE_STORAGE_BIT);
    m_output = VulkanHelpers::Image(device, m_context.PhysicalDevice(), extent, kOutputFormat,
                                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    VulkanHelpers::CommandSubmitter(device, m_context.CommandPool(), m_context.Queue()).Submit([&](VkCommandBuffer cmd) {
        for (const VulkanHelpers::Image* image : {&m_accumulation, &m_output}) {
            VulkanHelpers::transitionImageLayout2(cmd, image->image(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        }
    });

    m_context.SetName(VK_OBJECT_TYPE_IMAGE, m_accumulation.image(), "Accumulation image");
    m_context.SetName(VK_OBJECT_TYPE_IMAGE, m_output.image(), "Raytracer output image");
    m_previousFrameState.clear();
    m_sampleIndex = 0;
}

void RaytracingPass::Resize(VkExtent2D extent) {
    createImages(extent);
}

void RaytracingPass::UploadMeshes(const std::vector<std::unique_ptr<Mesh>>& meshes) {
    m_context.WaitIdle();
    MeshData data = EncodeMeshes(meshes, kMaxTextures);
    std::vector<std::string> texturePaths = data.table.texturePaths;
    if (texturePaths.size() + 1 > kMaxTextures) {
        std::cerr << "Warning: only the first " << kMaxTextures - 1 << " textures are used\n";
        texturePaths.resize(kMaxTextures - 1);
    }

    const VkDevice device = m_context.Device();
    const VkPhysicalDevice physicalDevice = m_context.PhysicalDevice();
    VulkanHelpers::StagingUploader uploader(device, physicalDevice, m_context.CommandPool(), m_context.Queue());
    m_vertices = uploader.upload(data.vertices);
    m_triangles = uploader.upload(data.triangles);
    m_edges = uploader.upload(data.edges);
    m_edgeTriangles = uploader.upload(data.edgeTriangles);
    m_meshPairs = uploader.upload(data.pairs);
    m_meshRootPairs = uploader.upload(data.rootPairs);

    // Textures are sRGB images: the sampler returns linear colors. Mip levels down to 1x1.
    const auto addTexture = [&](VkExtent2D extent, const void* pixels) {
        const VulkanHelpers::Image& texture = m_textures.emplace_back(
            device, physicalDevice, extent, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            std::bit_width(std::max(extent.width, extent.height)));
        uploader.uploadTexture(texture, pixels);
    };
    const uint32_t white = 0xFFFFFFFF;
    m_textures.clear();
    m_textures.reserve(texturePaths.size() + 1);
    addTexture({1, 1}, &white);
    for (const std::string& path : texturePaths) {
        if (const SurfacePtr image = LoadRgbaPng(path)) {
            addTexture({static_cast<uint32_t>(image->w), static_cast<uint32_t>(image->h)}, image->pixels);
        } else {
            std::cerr << "Warning: Failed to load texture: " << path << " (" << SDL_GetError() << ")\n";
            addTexture({1, 1}, &white);
        }
    }
    uploader.submit();

    const std::pair<const VulkanHelpers::Buffer&, const char*> meshBuffers[]{
        {m_vertices, "Mesh vertices"},
        {m_triangles, "Mesh triangles"},
        {m_edges, "Mesh edges"},
        {m_edgeTriangles, "Mesh edge triangles"},
        {m_meshPairs, "Mesh BVH node pairs"},
        {m_meshRootPairs, "Mesh root pairs"},
    };
    for (const auto& [buffer, name] : meshBuffers) {
        m_context.SetName(VK_OBJECT_TYPE_BUFFER, buffer.buffer(), name);
    }
    // Unused slots show the white texture
    std::array<VkDescriptorImageInfo, kMaxTextures> textures{};
    for (uint32_t slot = 0; slot < kMaxTextures; ++slot) {
        const VulkanHelpers::Image& texture = slot < m_textures.size() ? m_textures[slot] : m_textures.front();
        textures[slot] = {m_sampler.get(), texture.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }
    VulkanHelpers::writeImageDescriptors(device, m_textureSet, BINDING_TEXTURES,
                                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textures);
    m_meshTable = std::move(data.table);
    m_previousFrameState.clear();
}

template<typename T>
void RaytracingPass::writeFrameBuffer(VulkanHelpers::Buffer& buffer, const std::vector<T>& data, const char* name) {
    const VkDeviceSize dataSize = sizeof(T) * data.size();
    if (buffer.buffer() == VK_NULL_HANDLE || buffer.size() < dataSize) {
        const VkDeviceSize capacity = std::max({dataSize, 2 * buffer.size(), VkDeviceSize{sizeof(T)}});
        buffer = VulkanHelpers::Buffer(m_context.Device(), m_context.PhysicalDevice(), capacity,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                       VulkanHelpers::hostWriteMemoryProperties(m_context.PhysicalDevice()));
        m_context.SetName(VK_OBJECT_TYPE_BUFFER, buffer.buffer(), name);
    }
    if (dataSize > 0) {
        buffer.write(data.data(), dataSize);
    }
}

void RaytracingPass::pushSceneDescriptors(VkCommandBuffer cmdBuffer, const FrameBuffers& frame) const {
    const std::pair<uint32_t, VkImageView> images[]{
        {BINDING_ACCUMULATION_IMAGE, m_accumulation.view()},
        {BINDING_OUTPUT_IMAGE, m_output.view()},
    };
    const std::pair<uint32_t, VkBuffer> buffers[]{
        {BINDING_VERTICES, m_vertices.buffer()},
        {BINDING_TRIANGLES, m_triangles.buffer()},
        {BINDING_EDGES, m_edges.buffer()},
        {BINDING_EDGE_TRIANGLES, m_edgeTriangles.buffer()},
        {BINDING_MESH_PAIRS, m_meshPairs.buffer()},
        {BINDING_MESH_ROOT_PAIRS, m_meshRootPairs.buffer()},
        {BINDING_MATERIALS, frame.materials.buffer()},
        {BINDING_SPHERES, frame.spheres.buffer()},
        {BINDING_PLANES, frame.planes.buffer()},
        {BINDING_LIGHTS, frame.lights.buffer()},
        {BINDING_INSTANCES, frame.instances.buffer()},
        {BINDING_SCENE_PAIRS, frame.scenePairs.buffer()},
        {BINDING_DEBUG_LINES, frame.debugLines.buffer()},
    };
    static_assert(std::size(images) + std::size(buffers) == BINDING_COUNT);

    std::array<VkDescriptorImageInfo, std::size(images)> imageInfos;
    std::array<VkDescriptorBufferInfo, std::size(buffers)> bufferInfos;
    std::array<VkWriteDescriptorSet, BINDING_COUNT> writes;
    size_t writeCount = 0;
    for (size_t i = 0; i < std::size(images); ++i) {
        imageInfos[i] = {.imageView = images[i].second, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
        writes[writeCount++] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = images[i].first,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imageInfos[i],
        };
    }
    for (size_t i = 0; i < std::size(buffers); ++i) {
        bufferInfos[i] = {.buffer = buffers[i].second, .range = VK_WHOLE_SIZE};
        writes[writeCount++] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = buffers[i].first,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &bufferInfos[i],
        };
    }
    vkCmdPushDescriptorSet(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout.get(), SET_SCENE,
                           static_cast<uint32_t>(writes.size()), writes.data());
}

void RaytracingPass::updateSampleIndex(std::vector<std::byte> frameState) {
    if (frameState == m_previousFrameState) {
        ++m_sampleIndex;
    } else {
        m_sampleIndex = 0;
        m_previousFrameState = std::move(frameState);
    }
}

void RaytracingPass::Record(VkCommandBuffer cmdBuffer, uint32_t frameIndex, const GameScene& scene) {
    // The renderer has waited for this frame's previous use, so its buffers are free to update
    // and its timestamps are available
    FrameBuffers& frame = m_frames[frameIndex];
    m_dispatchMilliseconds = m_timer.Milliseconds(frameIndex);

    // Meshes added or changed since the last upload; this waits for the GPU
    if (MeshesChanged(m_meshTable, scene.GetMeshes())) {
        UploadMeshes(scene.GetMeshes());
    }
    const FrameData data = EncodeFrame(scene, m_meshTable);
    writeFrameBuffer(frame.materials, data.materials, "Materials");
    writeFrameBuffer(frame.spheres, data.spheres, "Spheres");
    writeFrameBuffer(frame.planes, data.planes, "Planes");
    writeFrameBuffer(frame.lights, data.lights, "Lights");
    writeFrameBuffer(frame.instances, data.instances, "Instances");
    writeFrameBuffer(frame.scenePairs, data.scenePairs, "Scene BVH node pairs");
    writeFrameBuffer(frame.debugLines, data.debugLines, "Debug lines");

    // A degenerate camera keeps the previous orientation
    const Camera& camera = scene.GetCamera();
    m_cameraMotor = CameraMotor(camera).value_or(m_cameraMotor);
    const auto [cameraRotor, cameraDual] = Gpu::Encode(m_cameraMotor);
    Gpu::PushConstants pushConstants{
        .cameraRotor = cameraRotor,
        .cameraDual = cameraDual,
        .tanHalfFov = std::tan(0.5f * camera.fov * DEG_TO_RAD),
        .sphereCount = static_cast<uint32_t>(data.spheres.size()),
        .planeCount = static_cast<uint32_t>(data.planes.size()),
        .lightCount = static_cast<uint32_t>(data.lights.size()),
        .instanceCount = static_cast<uint32_t>(data.instances.size()),
        .maxBounces = m_maxBounces,
        .exposure = camera.exposure * m_exposure,
    };

    // Debug lines are drawn after accumulation, so they don't restart it
    updateSampleIndex(concatenatedBytes(data.materials, data.spheres, data.planes, data.lights, data.instances,
                                        std::span(&pushConstants, 1)));
    pushConstants.debugLineCount = static_cast<uint32_t>(data.debugLines.size());
    pushConstants.sampleIndex = m_sampleIndex;
    pushConstants.frameIndex = m_frameIndex++;

    m_context.BeginLabel(cmdBuffer, "Raytracing");
    // The previous frame's dispatch wrote the accumulation image this one reads
    VulkanHelpers::transitionImageLayout2(cmdBuffer, m_accumulation.image(),
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

    const VkExtent2D extent = m_output.extent();
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline.get());
    pushSceneDescriptors(cmdBuffer, frame);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout.get(),
                            SET_TEXTURES, 1, &m_textureSet, 0, nullptr);
    vkCmdPushConstants(cmdBuffer, m_pipelineLayout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(pushConstants), &pushConstants);
    m_timer.Begin(cmdBuffer, frameIndex);
    vkCmdDispatch(cmdBuffer, (extent.width + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE,
                  (extent.height + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1);
    m_timer.End(cmdBuffer, frameIndex);
    m_context.EndLabel(cmdBuffer);
}
