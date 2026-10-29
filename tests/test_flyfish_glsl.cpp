#include <gtest/gtest.h>
#include "FlyFish.h"
#include "PGA.h"
#include "render_helpers.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <span>
#include <string>
#include <vector>

// Runs engine/shaders/flyfish.glsl on the GPU (through tests/shaders/flyfish_test.comp) and compares
// every function with the FlyFish operator it stands for, on random inputs.

namespace {

constexpr uint32_t kCases = 256;
constexpr size_t kInputFloats = 44;   // INPUT_FLOATS in flyfish_test.comp
constexpr size_t kOutputFloats = 70;  // OUTPUT_FLOATS in flyfish_test.comp

struct Result {
    const char* name;
    size_t floatCount;
};

// In the order flyfish_test.comp writes them
constexpr std::array kResults{
    Result{"join(point, point)", 6},     Result{"join(point, line)", 4},   Result{"join(line, point)", 4},
    Result{"join(point, plane)", 1},     Result{"join(plane, point)", 1},  Result{"join(line, line)", 1},
    Result{"meet(plane, plane)", 6},     Result{"meet(line, plane)", 4},   Result{"meet(plane, line)", 4},
    Result{"inner(point, line)", 4},     Result{"inner(plane, plane)", 1}, Result{"geometricProduct", 8},
    Result{"sandwich(motor, point)", 4}, Result{"sandwich(motor, direction)", 4},
    Result{"reverse(motor)", 8},         Result{"reflectInPlane(point)", 4},
    Result{"reflectInPlane(direction)", 4}, Result{"norm(line)", 1},   Result{"norm(plane)", 1},
};
static_assert([] {
    size_t count = 0;
    for (const Result& result : kResults) count += result.floatCount;
    return count;
}() == kOutputFloats);

template<typename Element>
void append(std::vector<float>& floats, const Element& element) {
    for (const float coefficient : element) {
        floats.push_back(coefficient);
    }
}

void append(std::vector<float>& floats, float value) {
    floats.push_back(value);
}

struct Cases {
    std::vector<float> inputs;
    std::vector<float> expected;
};

Cases MakeCases() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> r(-2.0f, 2.0f);
    const auto random = [&] { return r(rng); };

    Cases cases;
    for (uint32_t i = 0; i < kCases; ++i) {
        const TriVector p(random(), random(), random());
        const TriVector q(random(), random(), random());
        const TriVector d(random(), random(), random(), 0.0f);
        const BiVector lineA(random(), random(), random(), random(), random(), random());
        const BiVector lineB(random(), random(), random(), random(), random(), random());
        const Vector planeA(random(), random(), random(), random());
        const Vector planeB(random(), random(), random(), random());
        const Vector unitPlane = Vector(random(), random(), random(), random()).Normalized();
        // A screw motion: a rotation around a line off the origin, then a translation in any direction
        const BiVector axis = TriVector(random(), random(), random()) & TriVector(random(), random(), random());
        const Motor motor = TranslationMotor(random(), random(), random()) * Motor::Rotation(90.0f * random(), axis);

        append(cases.inputs, p);
        append(cases.inputs, q);
        append(cases.inputs, d);
        append(cases.inputs, lineA);
        append(cases.inputs, lineB);
        append(cases.inputs, planeA);
        append(cases.inputs, planeB);
        append(cases.inputs, unitPlane);
        append(cases.inputs, motor);

        append(cases.expected, p & q);
        append(cases.expected, p & lineA);
        append(cases.expected, lineA & p);
        append(cases.expected, p & planeA);
        append(cases.expected, planeA & p);
        append(cases.expected, lineA & lineB);
        append(cases.expected, planeA ^ planeB);
        append(cases.expected, lineA ^ planeA);
        append(cases.expected, planeA ^ lineA);
        append(cases.expected, p | lineA);
        append(cases.expected, planeA | planeB);
        append(cases.expected, planeA * planeB);
        append(cases.expected, (motor * p * ~motor).Grade3());
        append(cases.expected, (motor * d * ~motor).Grade3());
        append(cases.expected, ~motor);
        append(cases.expected, (unitPlane * p * ~unitPlane).Grade3());
        append(cases.expected, (unitPlane * d * ~unitPlane).Grade3());
        append(cases.expected, lineA.Norm());
        append(cases.expected, planeA.Norm());
    }
    return cases;
}

std::vector<float> RunOnGpu(const VulkanContext& context, const std::vector<float>& inputs) {
    VulkanHelpers::Buffer input = MakeHostBuffer(context, sizeof(float) * inputs.size());
    input.write(inputs.data(), input.size());
    const VulkanHelpers::Buffer output = MakeHostBuffer(context, sizeof(float) * kCases * kOutputFloats);
    RunCompute(context, "flyfish_test.comp", {{0, input.buffer()}, {1, output.buffer()}},
               std::as_bytes(std::span(&kCases, 1)), (kCases + 63) / 64);

    const auto* outputs = static_cast<const float*>(output.mapped());
    return {outputs, outputs + kCases * kOutputFloats};
}

} // anonymous namespace

TEST(FlyFishGlslTest, MatchesFlyFish) {
    const Cases cases = MakeCases();
    ASSERT_EQ(cases.inputs.size(), kCases * kInputFloats);
    ASSERT_EQ(cases.expected.size(), kCases * kOutputFloats);

    const auto context = TryCreateContext();
    if (!context) GTEST_SKIP() << "No usable Vulkan device";
    const std::vector<float> outputs = RunOnGpu(*context, cases.inputs);

    for (uint32_t testCase = 0; testCase < kCases; ++testCase) {
        size_t index = testCase * kOutputFloats;
        for (const Result& result : kResults) {
            for (size_t i = 0; i < result.floatCount; ++i, ++index) {
                const float expected = cases.expected[index];
                ASSERT_NEAR(outputs[index], expected, 1e-4f * std::max(1.0f, std::abs(expected)))
                    << result.name << ", coefficient " << i << ", case " << testCase;
            }
        }
    }
}
