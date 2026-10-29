#pragma once

#include "VulkanContext.h"
#include "VulkanHelpers.h"
#include <optional>
#include <vector>

// Measures how long the GPU spends on a range of commands, with a pair of timestamp queries per
// frame in flight. Without timestamps that resolve within a command buffer it measures nothing.
class GpuTimer {
public:
    GpuTimer(const VulkanContext& context, uint32_t frameCount);

    // Around the timed commands in the frame's command buffer
    void Begin(VkCommandBuffer cmdBuffer, uint32_t frame);
    void End(VkCommandBuffer cmdBuffer, uint32_t frame);

    // The duration last recorded for this frame, once its command buffer has completed
    [[nodiscard]] std::optional<double> Milliseconds(uint32_t frame) const;

private:
    VkDevice m_device;
    VulkanHelpers::UniqueQueryPool m_queryPool;
    uint32_t m_validBits{0};
    double m_nanosecondsPerTick{1.0};
    std::vector<bool> m_recorded;
};
