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
    void rotate(float dx, float dy);
    void writeScene(SceneUniform& sceneUniform) const;

    bool firstPersonMode_{true};
    KeyState keyState_{};

    glm::vec3 pos();
    glm::vec3 rot();
    glm::vec3 dir();

  private:
    float fov_{75.f};
    float znear_{0.1f};
    float zfar_{256.f};
    glm::vec3 worldUp_{0.0f, 1.0f, 0.0f};

    float rotationSpeed_{0.1f};
    float movementSpeed_{3.f};

    glm::vec3 position_{0.f, 0.f, 3.f};
    glm::vec3 rotation_{};
    glm::vec3 rightDir_{};
    glm::vec3 upDir_{};
    glm::vec3 forwardDir_{};

    glm::mat4 view_{1.f};
    glm::mat4 perspective_;
};
} // namespace guk