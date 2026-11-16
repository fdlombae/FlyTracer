#include <gtest/gtest.h>
#include "ImageFile.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

// A file of its own for every test: CTest runs the tests as separate processes at the same time
std::filesystem::path testPath() {
    const std::string test = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    return std::filesystem::temp_directory_path() / ("flytracer_" + test + ".png");
}

}

// Saved RGB pixels come back as RGBA bytes in that order, with an opaque alpha
TEST(ImageFileTest, LoadsPngAsRgba) {
    const std::filesystem::path path = testPath();
    std::array<uint8_t, 6> rgb{255, 0, 0, 0, 128, 255};
    const SurfacePtr source(SDL_CreateSurfaceFrom(2, 1, SDL_PIXELFORMAT_RGB24, rgb.data(), 6), SDL_DestroySurface);
    ASSERT_TRUE(SDL_SavePNG(source.get(), path.string().c_str()));

    const SurfacePtr image = LoadRgbaPng(path.string());
    std::filesystem::remove(path);
    ASSERT_TRUE(image);
    EXPECT_EQ(image->w, 2);
    EXPECT_EQ(image->h, 1);
    EXPECT_EQ(image->pitch, 2 * 4);
    const std::array<uint8_t, 8> expected{255, 0, 0, 255, 0, 128, 255, 255};
    EXPECT_EQ(std::memcmp(image->pixels, expected.data(), expected.size()), 0);
}

TEST(ImageFileTest, MissingOrInvalidFileIsNull) {
    const std::filesystem::path path = testPath();
    EXPECT_FALSE(LoadRgbaPng((std::filesystem::temp_directory_path() / "flytracer_missing.png").string()));
    std::ofstream(path) << "not a png";
    EXPECT_FALSE(LoadRgbaPng(path.string()));
    std::filesystem::remove(path);
}
