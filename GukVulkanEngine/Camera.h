#pragma once

#include "DataStructures.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace guk {
class Camera
{
  public:
    Camera();

    void rotate(float yaw, float pitch);
    void updateScene(SceneUniform& sceneUniform);

  private:
    float fov{75.f};
    float znear{0.1f};
    float zfar{256.f};

    float rotationSpeed{0.1f};
    float movementSpeed{10.f};

    glm::vec3 position_{0.f, 0.f, 2.5f};
    glm::vec3 rotation_{};

    glm::mat4 view_;
    glm::mat4 projective_;
};
} // namespace guk