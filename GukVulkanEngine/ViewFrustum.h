#pragma once

#include <array>
#include <glm/glm.hpp>

namespace guk {


struct Plane
{
    glm::vec3 normal;
    float distance;
};

class ViewFrustum
{
  public:
    void create(const glm::mat4& vpMat);
    bool culling(const glm::vec3& min, const glm::vec3& max, const glm::mat4& mMat) const;

    static std::array<glm::vec3, 8> corners(const glm::vec3& min, const glm::vec3& max);

  private:
    std::array<Plane, 6> planes_;
};

} // namespace guk