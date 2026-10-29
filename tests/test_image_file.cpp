#include <gtest/gtest.h>
#include "ImageFile.h"
#include "temp_file.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>

// Saved RGB pixels come back as RGBA bytes in that order, with an opaque alpha
TEST(ImageFileTest, LoadsPngAsRgba) {
    std::array<uint8_t, 6> rgb{255, 0, 0, 0, 128, 255};
    const SurfacePtr source(SDL_CreateSurfaceFrom(2, 1, SDL_PIXELFORMAT_RGB24, rgb.data(), 6), SDL_DestroySurface);
    const TempFile file(".png");
    ASSERT_TRUE(SDL_SavePNG(source.get(), file.String().c_str()));

    const SurfacePtr image = LoadRgbaPng(file.String());
    ASSERT_TRUE(image);
    EXPECT_EQ(image->w, 2);
    EXPECT_EQ(image->h, 1);
    EXPECT_EQ(image->pitch, 2 * 4);
    const std::array<uint8_t, 8> expected{255, 0, 0, 255, 0, 128, 255, 255};
    EXPECT_EQ(std::memcmp(image->pixels, expected.data(), expected.size()), 0);
}

TEST(ImageFileTest, MissingOrInvalidFileIsNull) {
    const TempFile file(".png");
    EXPECT_FALSE(LoadRgbaPng(file.String()));
    std::ofstream(file.Path()) << "not a png";
    EXPECT_FALSE(LoadRgbaPng(file.String()));
}
