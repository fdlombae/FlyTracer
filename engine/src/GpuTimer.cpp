#include "GpuTimer.h"
#include "GpuTiming.h"
#include <array>

using VulkanHelpers::check;

GpuTimer::GpuTimer(const VulkanContext& context, uint32_t frameCount)
    : m_device(context.Device()), m_recorded(frameCount, false)
{
    const auto families = VulkanHelpers::enumerate<VkQueueFamilyProperties>(
        vkGetPhysicalDeviceQueueFamilyProperties, context.PhysicalDevice());
    m_validBits = families.at(context.QueueFamily()).timestampValidBits;
    if (m_validBits == 0) return;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(context.PhysicalDevice(), &properties);
    m_nanosecondsPerTick = properties.limits.timestampPeriod;

    const VkQueryPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2 * frameCount,
    };
    VkQueryPool pool;
    check(vkCreateQueryPool(m_device, &poolInfo, nullptr, &pool), "vkCreateQueryPool");
    m_queryPool = VulkanHelpers::UniqueQueryPool(m_device, pool);
}

// Both timestamps are taken when all earlier compute work has finished
void GpuTimer::Begin(VkCommandBuffer cmdBuffer, uint32_t frame) {
    if (m_validBits == 0) return;
    vkCmdResetQueryPool(cmdBuffer, m_queryPool.get(), 2 * frame, 2);
    vkCmdWriteTimestamp2(cmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, m_queryPool.get(), 2 * frame);
}

void GpuTimer::End(VkCommandBuffer cmdBuffer, uint32_t frame) {
    if (m_validBits == 0) return;
    vkCmdWriteTimestamp2(cmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, m_queryPool.get(), 2 * frame + 1);
    m_recorded[frame] = true;
}

std::optional<double> GpuTimer::Milliseconds(uint32_t frame) const {
    if (m_validBits == 0 || !m_recorded[frame]) return std::nullopt;
    std::array<uint64_t, 2> ticks{};
    const VkResult result = vkGetQueryPoolResults(m_device, m_queryPool.get(), 2 * frame, 2, sizeof(ticks),
                                                  ticks.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    if (result == VK_NOT_READY) return std::nullopt;
    check(result, "vkGetQueryPoolResults");
    // Some drivers (KosmicKrisp on Metal) give every timestamp in a command buffer the same value
    if (ticks[0] == ticks[1]) return std::nullopt;
    return TimestampMilliseconds(ticks[0], ticks[1], m_validBits, m_nanosecondsPerTick);
}
