#include "BoltManager.h"
#include "Utils.h"

Bolt::Bolt(TriVector const& A, BiVector const& trajectory)
    : m_trajectory{ trajectory }
    , m_A{ A }
    , m_B{ OffsetPointAlongLine(A, trajectory, m_length) }
{}

void Bolt::Update(float const deltaSec)
{
    float const distance{ m_speed * deltaSec };
    m_A = OffsetPointAlongLine(m_A, m_trajectory, distance);
    m_B = OffsetPointAlongLine(m_B, m_trajectory, distance);
}

std::pair<TriVector, TriVector> Bolt::GetPoints() const
{
    return std::make_pair(m_A, m_B);
}
