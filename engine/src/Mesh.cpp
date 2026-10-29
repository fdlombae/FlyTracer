#include "Mesh.h"
#include "PGA.h"
#include <tiny_obj_loader.h>
#include <iostream>
#include <map>
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <array>
#include <numbers>

namespace {

// The triangles and an apex form tetrahedra that, with the signs of their volumes, add up to the
// solid. The apex is arbitrary; one inside the bounds keeps the numbers small.
template<typename Visit>
void forEachTetrahedron(const Mesh& mesh, const TriVector& apex, Visit visit) {
    for (const Triangle& triangle : mesh.Triangles()) {
        const TriVector& p0 = mesh.Vertices()[triangle.indices[0]].position;
        const TriVector& p1 = mesh.Vertices()[triangle.indices[1]].position;
        const TriVector& p2 = mesh.Vertices()[triangle.indices[2]].position;
        // The join of the apex with the triangle's plane is six times their tetrahedron's volume
        visit(p0, p1, p2, (apex & (p0 & p1 & p2)) / 6.0f);
    }
}

TriVector boundsCenter(const Mesh& mesh) {
    const AABB bounds = mesh.Bounds();
    return TriVector(bounds.Centroid(0), bounds.Centroid(1), bounds.Centroid(2));
}

// The plane through the origin perpendicular to a direction
Vector planeAlong(const TriVector& direction) {
    return Vector(0.0f, direction.e032(), direction.e013(), direction.e021());
}

const std::array<Vector, 3> kAxisPlanes{Vector(0.0f, 1.0f, 0.0f, 0.0f), Vector(0.0f, 0.0f, 1.0f, 0.0f),
                                        Vector(0.0f, 0.0f, 0.0f, 1.0f)};

// A symmetric linear map is given by the images of the axis planes, map(x) = sum_j (x | e_j) frame[j].
// The map rotated by R is R map(~R x R) ~R; its frame holds the images of the axis planes again.
std::array<Vector, 3> rotateFrame(const Motor& rotor, const std::array<Vector, 3>& frame) {
    std::array<Vector, 3> rotated;
    for (size_t i = 0; i < 3; ++i) {
        const Vector axis = ApplyMotor(~rotor, kAxisPlanes[i]);
        Vector image(0.0f, 0.0f, 0.0f, 0.0f);
        for (size_t j = 0; j < 3; ++j) {
            image += frame[j] * (axis | kAxisPlanes[j]);
        }
        rotated[i] = ApplyMotor(rotor, image);
    }
    return rotated;
}

// Jacobi's method with rotors ("Clean up your Mesh!", section 5c): every step rotates around one
// axis by the angle that zeroes one off-diagonal element, until the frame is diagonal
PrincipalInertia principalFrame(std::array<Vector, 3> frame) {
    const std::array<BiVector, 3> axisLines{BiVector(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f),
                                            BiVector(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f),
                                            BiVector(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f)};
    // (p, q, k): rotating around axis k by a right-handed angle turns axis p towards axis q
    constexpr std::array<std::array<size_t, 3>, 3> kPlanes{{{0, 1, 2}, {1, 2, 0}, {2, 0, 1}}};
    const float scale = frame[0].Norm() + frame[1].Norm() + frame[2].Norm();

    Motor rotor{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    for (int sweep = 0; sweep < 50; ++sweep) {
        float offDiagonal = 0.0f;
        for (const auto& [p, q, k] : kPlanes) {
            offDiagonal += std::abs(frame[p] | kAxisPlanes[q]);
        }
        if (offDiagonal <= 1e-7f * scale) break;

        for (const auto& [p, q, k] : kPlanes) {
            const float pq = frame[p] | kAxisPlanes[q];
            if (std::abs(pq) <= 1e-12f * scale) continue;
            const float pp = frame[p] | kAxisPlanes[p];
            const float qq = frame[q] | kAxisPlanes[q];
            // The smallest such angle, at most 45 degrees, so nearly diagonal frames are not swapped around
            const float angle = 0.5f * std::atan(2.0f * pq / (qq - pp));
            const Motor step = Motor::Rotation(angle * RAD_TO_DEG, axisLines[k]);
            frame = rotateFrame(step, frame);
            rotor = step * rotor;
        }
    }
    // The rotor diagonalizes the map, so its reverse turns the axes into the principal axes
    return {NormalizedMotor(~rotor),
            {frame[0] | kAxisPlanes[0], frame[1] | kAxisPlanes[1], frame[2] | kAxisPlanes[2]}};
}

} // anonymous namespace

bool Mesh::LoadFromFile(const std::string& filename) {
    std::string mtlBasePath = std::filesystem::path(filename).parent_path().string();
    if (!mtlBasePath.empty()) {
        mtlBasePath += "/";
    }

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> objMaterials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &objMaterials, &warn, &err, filename.c_str(), mtlBasePath.c_str())) {
        std::cerr << "Failed to load " << filename << ": " << err << std::endl;
        return false;
    }
    if (!warn.empty()) {
        std::cout << "Warning loading " << filename << ": " << warn << std::endl;
    }
    // tinyobjloader only warns about positions and texture coordinates that do not exist
    const auto validIndex = [](const tinyobj::index_t& index, const tinyobj::attrib_t& attrib) {
        return index.vertex_index >= 0 && static_cast<size_t>(index.vertex_index) < attrib.vertices.size() / 3 &&
               (index.texcoord_index < 0 || static_cast<size_t>(index.texcoord_index) < attrib.texcoords.size() / 2);
    };
    for (const auto& shape : shapes) {
        if (!std::ranges::all_of(shape.mesh.indices, [&](const auto& index) { return validIndex(index, attrib); })) {
            std::cerr << "Failed to load " << filename << ": a face refers to a missing vertex" << std::endl;
            return false;
        }
    }

    Clear();
    // OBJ materials become PBR materials
    for (const auto& mat : objMaterials) {
        Scene::Material& material = m_materials.emplace_back(Scene::Material::PBR(
            Scene::Color(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]), 0.0f, RoughnessFromShininess(mat.shininess)));
        material.shininess = mat.shininess;
        if (!mat.diffuse_texname.empty()) {
            material.texturePath = mtlBasePath + mat.diffuse_texname;
        }
    }
    if (m_materials.empty()) {
        m_materials.push_back(Scene::Material::PBR(Scene::Color(0.8f, 0.8f, 0.8f)));
    }

    // Face corners with the same position, texture coordinate and normal indices share a vertex
    std::map<std::array<int, 3>, uint32_t> vertexMap;
    const auto vertexOf = [&](const tinyobj::index_t& index) {
        Vertex vertex{};
        const float* p = &attrib.vertices[3 * index.vertex_index];
        vertex.position = TriVector(p[0], p[1], p[2]);
        // Some OBJ files reference normals (f v/vt/vn) without defining them (no vn lines)
        if (index.normal_index >= 0 && static_cast<size_t>(3 * index.normal_index + 2) < attrib.normals.size()) {
            const float* n = &attrib.normals[3 * index.normal_index];
            vertex.normal = TriVector(n[0], n[1], n[2], 0.0f);
        }
        // OBJ has v = 0 at the bottom, image row 0 is the top
        if (index.texcoord_index >= 0) {
            vertex.texCoord[0] = attrib.texcoords[2 * index.texcoord_index];
            vertex.texCoord[1] = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];
        }
        return vertex;
    };

    for (const auto& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
            const size_t cornerCount = shape.mesh.num_face_vertices[face];
            if (cornerCount != 3) {
                std::cerr << "Warning: Skipping non-triangle face" << std::endl;
                indexOffset += cornerCount;
                continue;
            }

            const int materialId = shape.mesh.material_ids[face];
            Triangle triangle{.materialIndex = materialId >= 0 ? static_cast<uint32_t>(materialId) : 0};
            for (size_t corner = 0; corner < 3; ++corner) {
                const tinyobj::index_t index = shape.mesh.indices[indexOffset + corner];
                const auto [it, isNew] = vertexMap.try_emplace({index.vertex_index, index.texcoord_index, index.normal_index},
                                                               static_cast<uint32_t>(m_vertices.size()));
                triangle.indices[corner] = it->second;
                if (isNew) {
                    m_vertices.push_back(vertexOf(index));
                }
            }
            m_triangles.push_back(triangle);
            indexOffset += cornerCount;
        }
    }

    if (std::ranges::any_of(m_vertices, [](const Vertex& v) { return v.normal.VNorm() < 1e-3f; })) {
        ComputeNormals();
    }
    return !m_vertices.empty();
}

void Mesh::Clear() {
    m_bvh = {};
    m_vertices.clear();
    m_triangles.clear();
    m_materials.clear();
}

void Mesh::ComputeNormals() {
    // Vertices at the same position share their normal, also where a texture seam splits them
    std::map<std::array<float, 3>, TriVector> normals;
    const auto normalAt = [&normals](const TriVector& p) -> TriVector& {
        return normals.try_emplace({p.e032(), p.e013(), p.e021()}, 0.0f, 0.0f, 0.0f, 0.0f).first->second;
    };

    // The angle at corner p between the edges to q and r, from the lines p & q and p & r:
    // their inner product is minus the dot product of their directions
    const auto cornerAngle = [](const TriVector& p, const TriVector& q, const TriVector& r) {
        const BiVector toQ = p & q;
        const BiVector toR = p & r;
        const float lengths = toQ.Norm() * toR.Norm();
        return lengths < 1e-8f ? 0.0f : std::acos(std::clamp(-(toQ | toR) / lengths, -1.0f, 1.0f));
    };

    for (const auto& tri : m_triangles) {
        const TriVector& p0 = m_vertices[tri.indices[0]].position;
        const TriVector& p1 = m_vertices[tri.indices[1]].position;
        const TriVector& p2 = m_vertices[tri.indices[2]].position;

        // The join of the corners; for corners wound counter-clockwise its normal points inwards
        const Vector face = (p0 & p1 & p2) * -1.0f;
        if (face.Norm() < 1e-8f) continue;
        const TriVector faceNormal = PlaneNormal(face) * (1.0f / face.Norm());

        normalAt(p0) += faceNormal * cornerAngle(p0, p1, p2);
        normalAt(p1) += faceNormal * cornerAngle(p1, p2, p0);
        normalAt(p2) += faceNormal * cornerAngle(p2, p0, p1);
    }

    for (auto& v : m_vertices) {
        const TriVector& sum = normalAt(v.position);
        const float length = sum.VNorm();
        v.normal = length > 1e-8f ? sum * (1.0f / length) : TriVector(0.0f, 1.0f, 0.0f, 0.0f);
    }
}

bool Mesh::HasUVCoordinates() const noexcept {
    return std::ranges::any_of(m_vertices, [](const Vertex& v) { return v.texCoord[0] != 0.0f || v.texCoord[1] != 0.0f; });
}

AABB Mesh::Bounds() const noexcept {
    AABB bounds;
    for (const Vertex& v : m_vertices) {
        bounds.Grow(v.position);
    }
    return bounds;
}

std::vector<uint32_t> Mesh::WeldedVertexIds() const {
    std::map<std::array<float, 3>, uint32_t> ids;
    std::vector<uint32_t> welded;
    welded.reserve(m_vertices.size());
    for (const Vertex& v : m_vertices) {
        const std::array<float, 3> position{v.position.e032(), v.position.e013(), v.position.e021()};
        welded.push_back(ids.try_emplace(position, static_cast<uint32_t>(ids.size())).first->second);
    }
    return welded;
}

float Mesh::Volume() const {
    float volume = 0.0f;
    forEachTetrahedron(*this, boundsCenter(*this), [&](const TriVector&, const TriVector&, const TriVector&,
                                                       float tetrahedronVolume) { volume += tetrahedronVolume; });
    return volume;
}

// Every tetrahedron's centroid (p0 + p1 + p2 + apex) / 4, weighted by its volume
TriVector Mesh::CenterOfMass() const {
    const TriVector apex = boundsCenter(*this);
    TriVector weightedSum(0.0f, 0.0f, 0.0f, 0.0f);
    forEachTetrahedron(*this, apex, [&](const TriVector& p0, const TriVector& p1, const TriVector& p2, float volume) {
        weightedSum += (p0 + p1 + p2 + apex) * volume;
    });
    return Euclidean(weightedSum);
}

// The inertia tensor is a frame of three planes through the origin ("Clean up your Mesh!",
// definition 5.1). First the second moments: secondMoments[k] has the normal (int x_k x, int x_k y,
// int x_k z) over the solid. For a tetrahedron with corners a, b, c and the apex at the origin,
// int x_i x_j = volume / 20 (a_i a_j + b_i b_j + c_i c_j + s_i s_j) with s = a + b + c.
PrincipalInertia Mesh::Inertia() const {
    const TriVector apex = boundsCenter(*this);
    std::array<Vector, 3> secondMoments{};
    float totalVolume = 0.0f;
    forEachTetrahedron(*this, apex, [&](const TriVector& p0, const TriVector& p1, const TriVector& p2, float volume) {
        const std::array corners{p0 - apex, p1 - apex, p2 - apex, p0 + p1 + p2 - apex * 3.0f};
        for (size_t k = 0; k < 3; ++k) {
            for (const TriVector& corner : corners) {
                secondMoments[k] += planeAlong(corner) * (corner[k] * volume / 20.0f);
            }
        }
        totalVolume += volume;
    });

    // Parallel axis theorem: from the apex to the center of mass
    const TriVector offset = CenterOfMass() - apex;
    for (size_t k = 0; k < 3; ++k) {
        secondMoments[k] -= planeAlong(offset) * (offset[k] * totalVolume);
    }

    // Inertia = trace(second moments) - second moments, e.g. I_xx = int (y^2 + z^2)
    const float trace = (secondMoments[0] | kAxisPlanes[0]) + (secondMoments[1] | kAxisPlanes[1]) +
                        (secondMoments[2] | kAxisPlanes[2]);
    std::array<Vector, 3> inertia;
    for (size_t k = 0; k < 3; ++k) {
        inertia[k] = kAxisPlanes[k] * trace - secondMoments[k];
    }
    return principalFrame(inertia);
}

bool Mesh::IsClosed() const {
    if (m_triangles.empty()) return false;
    const std::vector<uint32_t> welded = WeldedVertexIds();
    // Per edge: how often it is run through from its lower to its higher id, minus the other way
    std::map<std::pair<uint32_t, uint32_t>, int> balance;
    for (const Triangle& triangle : m_triangles) {
        for (size_t corner = 0; corner < 3; ++corner) {
            const uint32_t from = welded[triangle.indices[corner]];
            const uint32_t to = welded[triangle.indices[(corner + 1) % 3]];
            balance[std::minmax(from, to)] += from < to ? 1 : -1;
        }
    }
    return std::ranges::all_of(balance, [](const auto& edge) { return edge.second == 0; });
}

// U is the angle around the vertical axis through the center, v the height from the top
void Mesh::GenerateCylindricalUVs() {
    const AABB bounds = Bounds();
    const float height = bounds.Extent(1) > 1e-4f ? bounds.Extent(1) : 1.0f;
    for (auto& v : m_vertices) {
        const float angle = std::atan2(v.position.e032() - bounds.Centroid(0), v.position.e021() - bounds.Centroid(2));
        v.texCoord[0] = angle / (2.0f * std::numbers::pi_v<float>) + 0.5f;
        v.texCoord[1] = (bounds.max[1] - v.position.e013()) / height;
    }
}

void Mesh::StandOnOrigin() {
    const AABB bounds = Bounds();
    const Motor translation = TranslationMotor(-bounds.Centroid(0), -bounds.min[1], -bounds.Centroid(2));
    for (auto& v : m_vertices) {
        v.position = ApplyMotor(translation, v.position);
    }
}

void Mesh::BuildBVH() {
    std::vector<AABB> triangleBounds(m_triangles.size());
    for (size_t i = 0; i < m_triangles.size(); ++i) {
        for (uint32_t vertexIdx : m_triangles[i].indices) {
            triangleBounds[i].Grow(m_vertices[vertexIdx].position);
        }
    }
    m_bvh = ::BuildBVH(triangleBounds, kMaxLeafTriangles);
}

float RoughnessFromShininess(float shininess) noexcept {
    // The usual Blinn-Phong to microfacet conversion gives alpha = sqrt(2 / (Ns + 2)), and the
    // BRDF uses alpha = roughness^2
    return std::sqrt(std::sqrt(2.0f / (std::max(shininess, 0.0f) + 2.0f)));
}
