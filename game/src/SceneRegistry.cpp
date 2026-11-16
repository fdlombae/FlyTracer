#include "SceneRegistry.h"
#include "DebugDemoScene.h"
#include "TestBoxScene.h"
#include <algorithm>
#include <iostream>

namespace {

template<typename SceneType>
std::unique_ptr<GameScene> create(const std::string& resourceDir) {
    return std::make_unique<SceneType>(resourceDir);
}

// A new scene only needs a line here (and its .cpp in CMakeLists.txt)
constexpr SceneEntry kScenes[]{
    {"testbox", create<TestBoxScene>},
    {"debug", create<DebugDemoScene>},
};

} // anonymous namespace

std::span<const SceneEntry> Scenes() {
    return kScenes;
}

std::unique_ptr<GameScene> CreateScene(std::string_view name, const std::string& resourceDir) {
    const auto it = std::ranges::find(kScenes, name, &SceneEntry::name);
    if (it != std::end(kScenes)) {
        return it->create(resourceDir);
    }
    std::cerr << "Unknown scene '" << name << "', using " << kScenes[0].name << "\n";
    return kScenes[0].create(resourceDir);
}
