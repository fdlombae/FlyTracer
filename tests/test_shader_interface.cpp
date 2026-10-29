#include <gtest/gtest.h>
#include "GpuScene.h"
#include "ShaderInterface.h"
#include "render_helpers.h"
#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

// The GPU structs, bindings and push constants seen from both sides: tests/shaders/interface_test.comp
// reads them with the raytracer's GLSL declarations (common.glsl, bindings.glsl). Every 32-bit word
// of the C++ data holds its binding and position, so a member at another offset, an array with
// another stride or a buffer at another binding shows up as a wrong word.

namespace {

struct Binding {
    uint32_t binding;
    size_t structSize;   // Bytes per element in C++
    size_t memberWords;  // The words GLSL reads per element: all but the C++ padding at the end
};

const std::array kBindings{
    Binding{BINDING_VERTICES, sizeof(Gpu::Vertex), 8},
    Binding{BINDING_TRIANGLES, sizeof(Triangle), 4},
    Binding{BINDING_EDGES, sizeof(Gpu::Edge), 6},
    Binding{BINDING_EDGE_TRIANGLES, sizeof(Gpu::EdgeTriangle), 4},
    Binding{BINDING_MESH_PAIRS, sizeof(Gpu::NodePair), 16},
    Binding{BINDING_MESH_ROOT_PAIRS, sizeof(uint32_t), 1},
    Binding{BINDING_MATERIALS, sizeof(Gpu::Material), 10},
    Binding{BINDING_SPHERES, sizeof(Gpu::Sphere), 5},
    Binding{BINDING_PLANES, sizeof(Gpu::Plane), 5},
    Binding{BINDING_LIGHTS, sizeof(Gpu::Light), 13},
    Binding{BINDING_INSTANCES, sizeof(Gpu::MeshInstance), 15},
    Binding{BINDING_SCENE_PAIRS, sizeof(Gpu::NodePair), 16},
    Binding{BINDING_DEBUG_LINES, sizeof(Gpu::DebugLine), 12},
};
constexpr size_t kPushConstantWords = 18;
constexpr uint32_t kPushConstantTag = 0xFFu;

uint32_t Word(uint32_t tag, size_t index) {
    return tag << 16 | static_cast<uint32_t>(index);
}

} // anonymous namespace

TEST(ShaderInterfaceTest, GlslMatchesCpp) {
    const auto context = TryCreateContext();
    if (!context) GTEST_SKIP() << "No usable Vulkan device";

    // Two elements per buffer; the shader reads the second
    std::vector<VulkanHelpers::Buffer> inputs;
    std::vector<std::pair<uint32_t, VkBuffer>> buffers;
    std::vector<uint32_t> expected;
    for (const Binding& binding : kBindings) {
        std::vector<uint32_t> words(2 * binding.structSize / sizeof(uint32_t));
        for (size_t i = 0; i < words.size(); ++i) {
            words[i] = Word(binding.binding, i);
        }
        VulkanHelpers::Buffer& buffer = inputs.emplace_back(MakeHostBuffer(*context, words.size() * sizeof(uint32_t)));
        buffer.write(words.data(), buffer.size());
        buffers.emplace_back(binding.binding, buffer.buffer());
        for (size_t member = 0; member < binding.memberWords; ++member) {
            expected.push_back(Word(binding.binding, binding.structSize / sizeof(uint32_t) + member));
        }
    }
    static_assert(sizeof(Gpu::PushConstants) == kPushConstantWords * sizeof(uint32_t));
    std::array<uint32_t, kPushConstantWords> pushConstants{};
    for (size_t i = 0; i < pushConstants.size(); ++i) {
        pushConstants[i] = Word(kPushConstantTag, i);
        expected.push_back(pushConstants[i]);
    }
    const VulkanHelpers::Buffer output = MakeHostBuffer(*context, expected.size() * sizeof(uint32_t));
    buffers.emplace_back(BINDING_COUNT, output.buffer());

    RunCompute(*context, "interface_test.comp", buffers, std::as_bytes(std::span(pushConstants)), 1);

    const auto* words = static_cast<const uint32_t*>(output.mapped());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(words[i], expected[i]) << "word " << i << ": binding " << (expected[i] >> 16) << ", position "
                                         << (expected[i] & 0xFFFFu) << " expected, binding " << (words[i] >> 16)
                                         << ", position " << (words[i] & 0xFFFFu) << " read";
    }
}
