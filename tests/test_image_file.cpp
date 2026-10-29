#include <gtest/gtest.h>
#include "ImageFile.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace {

const std::filesystem::path kPath = std::filesystem::temp_directory_path() / "flytracer_test_image.png";

}

// Saved RGB pixels come back as RGBA bytes in that order, with an opaque alpha
TEST(ImageFileTest, LoadsPngAsRgba) {
    std::array<uint8_t, 6> rgb{255, 0, 0, 0, 128, 255};
    const SurfacePtr source(SDL_CreateSurfaceFrom(2, 1, SDL_PIXELFORMAT_RGB24, rgb.data(), 6), SDL_DestroySurface);
    ASSERT_TRUE(SDL_SavePNG(source.get(), kPath.string().c_str()));

    const SurfacePtr image = LoadRgbaPng(kPath.string());
    std::filesystem::remove(kPath);
    ASSERT_TRUE(image);
    EXPECT_EQ(image->w, 2);
    EXPECT_EQ(image->h, 1);
    EXPECT_EQ(image->pitch, 2 * 4);
    const std::array<uint8_t, 8> expected{255, 0, 0, 255, 0, 128, 255, 255};
    EXPECT_EQ(std::memcmp(image->pixels, expected.data(), expected.size()), 0);
}

TEST(ImageFileTest, MissingOrInvalidFileIsNull) {
    EXPECT_FALSE(LoadRgbaPng((std::filesystem::temp_directory_path() / "flytracer_missing.png").string()));
    std::ofstream(kPath) << "not a png";
    EXPECT_FALSE(LoadRgbaPng(kPath.string()));
    std::filesystem::remove(kPath);
}
