#pragma once

#include "GameScene.h"
#include <memory>
#include <span>
#include <string>
#include <string_view>

struct SceneEntry {
    std::string_view name;
    std::unique_ptr<GameScene> (*create)(const std::string& resourceDir);
};

// The scenes the --scene argument can choose, listed in SceneRegistry.cpp. The first one is the default.
[[nodiscard]] std::span<const SceneEntry> Scenes();

// The default scene for an unknown name
[[nodiscard]] std::unique_ptr<GameScene> CreateScene(std::string_view name, const std::string& resourceDir);
