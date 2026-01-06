#include "BoltManager.h"

#include <algorithm>

#include "Utils.h"

///////////////////////////////////////////////////////////////////////////
/// Bolt
///////////////////////////////////////////////////////////////////////////
Bolt::Bolt(TriVector const& A, BiVector const& trajectory)
    : m_trajectory{ trajectory }
    , m_A{ A }
    , m_B{ OffsetPointAlongLine(A, trajectory, m_length) }
{}

void Bolt::Update(float const deltaSec)
{
    float const distance{ m_speed * deltaSec };
    m_accumulatedDistance += distance;
    m_A = OffsetPointAlongLine(m_A, m_trajectory, distance);
    m_B = OffsetPointAlongLine(m_B, m_trajectory, distance);
}

std::pair<TriVector, TriVector> Bolt::GetPoints() const
{
    return std::make_pair(m_A, m_B);
}

float Bolt::GetAccumulatedDistance() const
{
    return m_accumulatedDistance;
}

///////////////////////////////////////////////////////////////////////////
/// Bolt manager
///////////////////////////////////////////////////////////////////////////
void BoltManager::AddBolt(Bolt const& bolt)
{
    m_bolts.push_back(bolt);
}

std::vector<Bolt> const& BoltManager::GetBolts() const
{
    return m_bolts;
}

void BoltManager::Update(float const deltaSec)
{
    // Updating bolts
    for (Bolt& bolt : m_bolts)
    {
        bolt.Update(deltaSec);
    }
    // Deleting far bolts
    std::ranges::remove_if(m_bolts, [this](const Bolt& bolt)
    {
        return bolt.GetAccumulatedDistance() >= m_maxDistance;
    });
}
