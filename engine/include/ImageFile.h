#pragma once

#include <SDL3/SDL_surface.h>
#include <memory>
#include <string>

using SurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

// A PNG file as 8-bit RGBA pixels with tightly packed rows, or null when it cannot be read
inline SurfacePtr LoadRgbaPng(const std::string& path) {
    const SurfacePtr png(SDL_LoadPNG(path.c_str()), SDL_DestroySurface);
    return SurfacePtr(png ? SDL_ConvertSurface(png.get(), SDL_PIXELFORMAT_RGBA32) : nullptr, SDL_DestroySurface);
}
