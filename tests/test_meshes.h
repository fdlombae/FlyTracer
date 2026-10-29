#pragma once

#include "Mesh.h"
#include "PGA.h"
#include <array>
#include <cstdint>
#include <vector>

// Small meshes built in code for the tests

inline Mesh MakeMesh(const std::vector<TriVector>& positions, const std::vector<std::array<uint32_t, 3>>& triangles) {
    Mesh mesh;
    for (const TriVector& position : positions) {
        mesh.AddVertex({.position = position});
    }
    for (const auto& [a, b, c] : triangles) {
        mesh.AddTriangle(Triangle{{a, b, c}, 0});
    }
    return mesh;
}

// A closed box with the given sizes along x, y and z, centered on the origin and then moved by the
// motor; its triangles are wound counter-clockwise seen from outside
inline Mesh MakeBox(float x, float y, float z, const Motor& motor = Motor{1, 0, 0, 0, 0, 0, 0, 0}) {
    std::vector<TriVector> corners;
    for (uint32_t corner = 0; corner < 8; ++corner) {
        const TriVector local((corner & 1 ? 0.5f : -0.5f) * x, (corner & 2 ? 0.5f : -0.5f) * y,
                              (corner & 4 ? 0.5f : -0.5f) * z);
        corners.push_back(ApplyMotor(motor, local));
    }
    const TriVector center = ApplyMotor(motor, TriVector(0.0f, 0.0f, 0.0f));

    std::vector<std::array<uint32_t, 3>> triangles;
    for (uint32_t axis = 0; axis < 3; ++axis) {
        for (uint32_t side = 0; side < 2; ++side) {
            const uint32_t u = 1u << ((axis + 1) % 3);
            const uint32_t v = 1u << ((axis + 2) % 3);
            const uint32_t base = side << axis;
            const std::array<uint32_t, 4> quad{base, base | u, base | u | v, base | v};
            for (const auto& triangle : {std::array{quad[0], quad[1], quad[2]}, std::array{quad[0], quad[2], quad[3]}}) {
                // Wind it counter-clockwise from outside: its join's normal then points inwards
                const Vector plane = corners[triangle[0]] & corners[triangle[1]] & corners[triangle[2]];
                const bool outwards = (center & plane) > 0.0f;
                triangles.push_back(outwards ? triangle : std::array{triangle[0], triangle[2], triangle[1]});
            }
        }
    }
    return MakeMesh(corners, triangles);
}
