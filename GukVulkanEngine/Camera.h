#pragma once

#include "DataStructures.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace guk {
class Camera
{
  public:
    Camera();

    void updateView();
    void update(float deltaTime);
    void updateScene(SceneUniform& sceneUniform) const;

    void rotate(float dx, float dy);

    bool left{false};
    bool right{false};
    bool forward{false};
    bool backward{false};
    bool up{false};
    bool down{false};

    glm::vec3 pos();
    glm::vec3 rot();
    glm::vec3 dir();

  private:
    float fov{75.f};
    float znear{0.1f};
    float zfar{256.f};

    float rotationSpeed{0.1f};
    float movementSpeed{3.f};

    glm::vec3 position_{1.f, 0.f, 3.f};
    glm::vec3 rotation_{0.f, 25.f, 0.f};
    glm::vec3 forwardDir_{};
    glm::vec3 rightDir_{};
    glm::vec3 upDir_{0.0f, 1.0f, 0.0f};

    glm::mat4 view_{1.f};
    glm::mat4 perspective_;
};
} // namespace guk