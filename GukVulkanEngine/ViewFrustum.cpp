#include "ViewFrustum.h"

namespace guk {

void ViewFrustum::create(const glm::mat4& vpMat)
{
    // -1 ≤ x_ndc ≤ 1
    // -w_clip ≤ x_clip ≤ w_clip
    // w_clip + x_clip = (row3 + row0) · p_world ≥ 0
    // dot(normal, p_world) + distance ≥ 0

    // Left plane
    planes_[0].normal.x = vpMat[0][3] + vpMat[0][0],
    planes_[0].normal.y = vpMat[1][3] + vpMat[1][0],
    planes_[0].normal.z = vpMat[2][3] + vpMat[2][0],
    planes_[0].distance = vpMat[3][3] + vpMat[3][0];
    // Right plane
    planes_[1].normal.x = vpMat[0][3] - vpMat[0][0],
    planes_[1].normal.y = vpMat[1][3] - vpMat[1][0],
    planes_[1].normal.z = vpMat[2][3] - vpMat[2][0],
    planes_[1].distance = vpMat[3][3] - vpMat[3][0];
    // Bottom plane
    planes_[2].normal.x = vpMat[0][3] + vpMat[0][1],
    planes_[2].normal.y = vpMat[1][3] + vpMat[1][1],
    planes_[2].normal.z = vpMat[2][3] + vpMat[2][1],
    planes_[2].distance = vpMat[3][3] + vpMat[3][1];
    // Top plane
    planes_[3].normal.x = vpMat[0][3] - vpMat[0][1],
    planes_[3].normal.y = vpMat[1][3] - vpMat[1][1],
    planes_[3].normal.z = vpMat[2][3] - vpMat[2][1],
    planes_[3].distance = vpMat[3][3] - vpMat[3][1];
    // Near plane
    planes_[4].normal.x = vpMat[0][3] + vpMat[0][2],
    planes_[4].normal.y = vpMat[1][3] + vpMat[1][2],
    planes_[4].normal.z = vpMat[2][3] + vpMat[2][2],
    planes_[4].distance = vpMat[3][3] + vpMat[3][2];
    // Far plane
    planes_[5].normal.x = vpMat[0][3] - vpMat[0][2],
    planes_[5].normal.y = vpMat[1][3] - vpMat[1][2],
    planes_[5].normal.z = vpMat[2][3] - vpMat[2][2],
    planes_[5].distance = vpMat[3][3] - vpMat[3][2];

    for (auto& plane : planes_) {
        float length = glm::length(plane.normal);
        if (length > 1e-4f) {
            plane.normal /= length;
            plane.distance /= length;
        }
    }
}

bool ViewFrustum::culling(const glm::vec3& min, const glm::vec3& max, const glm::mat4& mMat) const
{
    glm::vec3 wMin(std::numeric_limits<float>::max());
    glm::vec3 wMax(std::numeric_limits<float>::lowest());
    for (const auto& coner : corners(min, max)) {
        glm::vec3 wConer = glm::vec3(mMat * glm::vec4(coner, 1.f));
        wMin = glm::min(wMin, wConer);
        wMax = glm::max(wMax, wConer);
    }

    for (const auto& plane : planes_) {
        glm::vec3 pVertex = wMin;
        if (plane.normal.x >= 0) {
            pVertex.x = wMax.x;
        }
        if (plane.normal.y >= 0) {
            pVertex.y = wMax.y;
        }
        if (plane.normal.z >= 0) {
            pVertex.z = wMax.z;
        }

        if (glm::dot(plane.normal, pVertex) + plane.distance < 0) {
            return true;
        }
    }

    return false;
}

std::array<glm::vec3, 8> ViewFrustum::corners(const glm::vec3& min, const glm::vec3& max)
{
    return {glm::vec3{min.x, min.y, min.z}, glm::vec3{max.x, min.y, min.z},
            glm::vec3{min.x, max.y, min.z}, glm::vec3{max.x, max.y, min.z},
            glm::vec3{min.x, min.y, max.z}, glm::vec3{max.x, min.y, max.z},
            glm::vec3{min.x, max.y, max.z}, glm::vec3{max.x, max.y, max.z}};
}

} // namespace guk