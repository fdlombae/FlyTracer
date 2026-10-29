#pragma once

#include "GameScene.h"

// A scene with the protected scene API opened to the tests
class OpenScene final : public GameScene {
public:
    OpenScene() : GameScene("") {}

    using GameScene::SetCamera;
    using GameScene::AddMesh;
    using GameScene::GetMesh;
    using GameScene::AddMeshInstance;
    using GameScene::GetInstance;
    using GameScene::FindInstance;
    using GameScene::RemoveMeshInstance;
    using GameScene::GetInstanceIds;
    using GameScene::AddSphere;
    using GameScene::AddPlane;
    using GameScene::AddPointLight;
    using GameScene::AddDirectionalLight;
    using GameScene::GetLight;
    using GameScene::RemoveLight;
    using GameScene::GetLightIds;
};
