#include "SceneRegistry.h"
#include "BiReflectionScene.h"
#include "DebugDemoScene.h"
#include "HornetScene.h"
#include "PluckerScene.h"
#include "QuaternionScene.h"
#include "TestBoxScene.h"
#include <iostream>

std::unique_ptr<GameScene> CreateScene(std::string_view name, const std::string& resourceDir) {
    if (name == "testbox") {
        return std::make_unique<TestBoxScene>(resourceDir);
    } else if (name == "debug") {
        return std::make_unique<DebugDemoScene>(resourceDir);
    } else if (name == "bireflection") {
        return std::make_unique<BiReflectionScene>(resourceDir);
    } else if (name == "birot") {
        return std::make_unique<BiReflectionScene>(resourceDir, BiReflectionScene::DemoMode::Rotation);
    } else if (name == "bitrans") {
        return std::make_unique<BiReflectionScene>(resourceDir, BiReflectionScene::DemoMode::Translation);
    } else if (name == "quaternion") {
        return std::make_unique<QuaternionScene>(resourceDir);
    } else if (name == "plucker") {
        return std::make_unique<PluckerScene>(resourceDir);
    } else if (name == "hornet") {
        return std::make_unique<HornetScene>(resourceDir);
    }
    std::cerr << "Unknown scene '" << name << "', using debug scene\n";
    return std::make_unique<DebugDemoScene>(resourceDir);
}
