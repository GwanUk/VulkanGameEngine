#include "Camera.h"
#include "Window.h"

namespace guk {
Camera::Camera()
{
    perspective_ = glm::perspectiveRH_ZO(glm::radians(fov), Window::aspectRatio, znear, zfar);
    perspective_[1][1] *= -1;
}

void Camera::updateView()
{
    glm::mat4 rot(1.f);
    rot = glm::rotate(rot, -glm::radians(rotation_.x), glm::vec3(1.f, 0.f, 0.f));
    rot = glm::rotate(rot, -glm::radians(rotation_.y), glm::vec3(0.f, 1.f, 0.f));

    glm::mat4 trans = glm::translate(glm::mat4(1.f), -position_);

    if (firstPersonMode_) {
        view_ = rot * trans;
    } else {
        view_ = trans * rot;
    }
}

void Camera::update(float deltaTime)
{
    forwardDir_.x = -cos(glm::radians(rotation_.x)) * sin(glm::radians(rotation_.y));
    forwardDir_.y = sin(glm::radians(rotation_.x));
    forwardDir_.z = -cos(glm::radians(rotation_.x)) * cos(glm::radians(rotation_.y));
    forwardDir_ = glm::normalize(forwardDir_);
    rightDir_ = glm::normalize(glm::cross(forwardDir_, upDir_));

    if (keyState_.forward) {
        position_ += forwardDir_ * movementSpeed * deltaTime;
    }
    if (keyState_.backward) {
        position_ -= forwardDir_ * movementSpeed * deltaTime;
    }
    if (keyState_.right) {
        position_ += rightDir_ * movementSpeed * deltaTime;
    }
    if (keyState_.left) {
        position_ -= rightDir_ * movementSpeed * deltaTime;
    }
    if (keyState_.up) {
        position_ += upDir_ * movementSpeed * deltaTime;
    }
    if (keyState_.down) {
        position_ -= upDir_ * movementSpeed * deltaTime;
    }

    updateView();
}

void Camera::updateScene(SceneUniform& sceneUniform) const
{
    sceneUniform.view = view_;
    sceneUniform.proj = perspective_;
    sceneUniform.cameraPos = position_;
}

void Camera::rotate(float dx, float dy)
{
    rotation_ += glm::vec3(-dy, -dx, 0.f) * rotationSpeed;
    updateView();
}

glm::vec3 Camera::pos()
{
    return position_;
}

glm::vec3 Camera::rot()
{
    return rotation_;
}

glm::vec3 Camera::dir()
{
    return forwardDir_;
}

} // namespace guk