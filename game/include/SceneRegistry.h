#pragma once

#include "GameScene.h"
#include <array>
#include <memory>
#include <string>
#include <string_view>

// The scenes the --scene argument can choose
inline constexpr auto kSceneNames = std::to_array<std::string_view>(
    {"bireflection", "birot", "bitrans", "quaternion", "plucker", "hornet", "testbox", "debug"});

// The debug scene for an unknown name
[[nodiscard]] std::unique_ptr<GameScene> CreateScene(std::string_view name, const std::string& resourceDir);
