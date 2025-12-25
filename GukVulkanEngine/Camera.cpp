#include "Camera.h"
#include "Window.h"

namespace guk {
Camera::Camera()
{
    view_ =
        glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));

    view_ = glm::mat4(1.0f);

    projective_ = glm::perspectiveRH_ZO(glm::radians(fov), Window::aspectRatio, znear, zfar);
    projective_[1][1] *= -1;
}
void Camera::rotate(float yaw, float pitch)
{
    rotation_ += glm::vec3(-pitch, -yaw, 0.f) * rotationSpeed;

    glm::mat4 rotMat(1.f);
    rotMat = glm::rotate(rotMat, glm::radians(rotation_.x), glm::vec3(1.f, 0.f, 0.f));
    rotMat = glm::rotate(rotMat, glm::radians(rotation_.y), glm::vec3(0.f, 1.f, 0.f));

    glm::mat4 transMat = glm::translate(glm::mat4(1.f), position_);
    view_ = rotMat * transMat;
}

void Camera::updateScene(SceneUniform& sceneUniform)
{
    sceneUniform.view = view_;
    sceneUniform.proj = projective_;
}

} // namespace guk