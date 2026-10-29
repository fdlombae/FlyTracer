#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Milliseconds between two GPU timestamps. Only the low validBits bits of a timestamp are
// meaningful, so the counter may have wrapped around between the two.
[[nodiscard]] constexpr double TimestampMilliseconds(uint64_t start, uint64_t end, uint32_t validBits,
                                                     double nanosecondsPerTick) noexcept {
    const uint64_t mask = validBits >= 64 ? ~uint64_t{0} : (uint64_t{1} << validBits) - 1;
    return static_cast<double>((end - start) & mask) * nanosecondsPerTick * 1e-6;
}

struct TimingSummary {
    double median{0.0};
    double percentile95{0.0};  // Nearest rank: at least 95% of the durations are at most this
};

[[nodiscard]] inline TimingSummary Summarize(std::vector<double> durations) {
    if (durations.empty()) return {};
    std::ranges::sort(durations);
    const size_t count = durations.size();
    const double median = count % 2 == 1 ? durations[count / 2]
                                         : 0.5 * (durations[count / 2 - 1] + durations[count / 2]);
    const auto rank = static_cast<size_t>(std::ceil(0.95 * static_cast<double>(count)));
    return {median, durations[std::max<size_t>(rank, 1) - 1]};
}
