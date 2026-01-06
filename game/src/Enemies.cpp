#include "Enemies.h"

#include "GameScene.h"
#include "Collisions.h"
#include "Utils.h"

///////////////////////////////////////////////////////////////////////////
/// Enemy
///////////////////////////////////////////////////////////////////////////
BiVector const Enemy::m_initialDirection{ zAxis };

Enemy::Enemy(std::string_view const meshName, Motor const& translation, GameScene& scene)
    : m_meshName{ meshName }
    , m_translation{ translation }
    , m_scene{ scene }
{

}

void Enemy::RotateTowardsPoint(TriVector const& point)
{
    // 1. Retrieving character and enemy origins, and enemy view direction
    TriVector const origin{ GetOrigin().Normalized() };
    BiVector const enemyViewDirection{ (m_enemyRotation * m_initialDirection * ~m_enemyRotation).Grade2().Normalized() };

    // 2. Calculating final enemy's direction
    BiVector const finalEnemyViewDirection{ (origin & point).Normalized() };

    // 3. Getting the angle between direction vectors
    // 3.1. Getting cos of the angle between the direction vectors
    // NOTE: enemyViewDirection and finalEnemyViewDirection are normalized, so there's no need to divide by the product of norms
    float cos{ (enemyViewDirection | finalEnemyViewDirection) };

    // Clamping dot to [-1, 1], because std::acos will result in NaN outside of it
    // NOTE: The value can get outside of this range due to floating point precision errors
    if (cos < -1) cos = -1;
    else if (cos > 1) cos = 1;

    // 3.2. Getting sin of the angle between the direction vectors
    // NOTE: Not using meet directly, because that alone will result in void
    BiVector const wedge{(enemyViewDirection * finalEnemyViewDirection).Grade2()};
    float const sin{ GetEuclideanSign(wedge) * wedge.Norm() };

    // 3.3. Finding the angle
    float const directionRadians{ std::atan2(sin, cos) };

    // 4. Creating rotation motor and applying it to the enemy's mesh
    Motor const R{ Motor::Rotation(directionRadians * RAD_TO_DEG, yAxis) };
    m_enemyRotation = R * m_enemyRotation;
}

TriVector Enemy::GetOrigin() const
{
    return (m_translation * TriVector{0.f,  0.f, 0.f} * ~m_translation).Grade3().Normalized();
}

void Enemy::UpdateMeshTransform()
{
    if (MeshInstance* pEnemyMesh = m_scene.FindInstance(m_meshName); pEnemyMesh)
    {
        pEnemyMesh->transform = m_enemyRotation * m_translation;
    }
}

void Enemy::ResolveCharacterCollision(TriVector const& characterOrigin)
{
    // 1. Getting bottom spheres
    Sphere const enemyBottomSphere{ Capsule(GetOrigin(),  capsuleColliderRadius, capsuleColliderHeight).GetBottomSphereOrigin(), capsuleColliderRadius };
    Sphere const characterBottomSphere{ Capsule(characterOrigin,  capsuleColliderRadius, capsuleColliderHeight).GetBottomSphereOrigin(), capsuleColliderRadius };
    // 2. Handling sphere collision
    if (auto const translations{ ProcessCollision(enemyBottomSphere, characterBottomSphere) };
        translations.has_value())
    {
        // 3. Resolving the collision
        m_translation = translations.value().first * m_translation;
    }
}

///////////////////////////////////////////////////////////////////////////
/// Enemy manager
///////////////////////////////////////////////////////////////////////////
Enemies::Enemies(GameScene& scene)
    : m_scene(scene)
{}


void Enemies::AddEnemy(TriVector const& position)
{
    std::string enemyMeshName{ "enemy" + std::to_string(m_enemies.size()) };
    uint32_t const enemyMeshId{ m_scene.LoadMesh("capsule.obj", "capsule.png") };// TODO: Make common for all enemies
    m_scene.AddMeshInstance(enemyMeshId, position, enemyMeshName);
    if (MeshInstance* pEnemyMesh{ m_scene.FindInstance(enemyMeshName) }; pEnemyMesh)
    {
        pEnemyMesh->scale = capsuleScale;
        m_enemies.emplace_back(enemyMeshName, pEnemyMesh->transform, m_scene);
    }
}

void Enemies::Update([[maybe_unused]] float deltaSec, TriVector const& characterOrigin)
{
    for (Enemy& enemy : m_enemies)
    {
        enemy.RotateTowardsPoint(characterOrigin);
        enemy.UpdateMeshTransform();
        enemy.ResolveCharacterCollision(characterOrigin);
    }
}
