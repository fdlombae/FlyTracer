#ifndef ENEMY_MANAGER_H
#define ENEMY_MANAGER_H
#include <string>
#include <string_view>
#include <vector>

#include "FlyFish.h"
#include "GlobalConstants.h"
#include "GameScene.h"

class Enemy
{
public:
    Enemy(std::string_view meshName, Motor const& translation, GameScene& scene);
    void RotateTowardsPoint(TriVector const&);
    TriVector GetOrigin() const;
    void UpdateMeshTransform();
    void ResolveCharacterCollision(TriVector const& characterOrigin);

private:
    std::string const m_meshName{};
    Motor m_translation{/*Set in OnInit()*/}, m_enemyRotation{ Motor::Rotation(0.f, yAxis) };
    static BiVector const m_initialDirection;
    GameScene& m_scene;

};

class EnemyManager
{
public:
    explicit EnemyManager(GameScene& scene);
    void AddEnemy(TriVector const& position);
    void Update(float deltaSec, TriVector const& characterOrigin);

private:
    GameScene& m_scene;
    std::vector<Enemy> m_enemies;

};

#endif
