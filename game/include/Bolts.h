#ifndef BOLT_MANAGER_H
#define BOLT_MANAGER_H

#include <vector>

#include "FlyFish.h"

class Bolt
{
public:
    Bolt(TriVector const& A, BiVector const& trajectory);
    void Update(float deltaSec);
    std::pair<TriVector, TriVector> GetPoints() const;
    float GetAccumulatedDistance() const;

private:
    BiVector m_trajectory{};// Both offset and direction
    TriVector m_A{}, m_B{};
    static float constexpr m_length{ 2.f },
        m_speed{ 100.f };// Units/sec
    float m_accumulatedDistance{};
};

class Bolts
{
public:
    void AddBolt(Bolt const&);
    std::vector<Bolt> const& GetBolts() const;
    void Update(float deltaSec);

private:
    std::vector<Bolt> m_bolts;
    float const m_maxDistance{ 500.f };

};

#endif
