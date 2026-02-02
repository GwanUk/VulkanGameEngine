#include "Camera.h"
#include "Window.h"

namespace guk {
Camera::Camera()
{
    perspective_ = glm::perspectiveRH_ZO(glm::radians(fov_), Window::aspectRatio, znear_, zfar_);
    perspective_[1][1] *= -1;
}

void Camera::updateView()
{
    forwardDir_.x = -cos(glm::radians(rotation_.x)) * sin(glm::radians(rotation_.y));
    forwardDir_.y = sin(glm::radians(rotation_.x));
    forwardDir_.z = -cos(glm::radians(rotation_.x)) * cos(glm::radians(rotation_.y));
    forwardDir_ = glm::normalize(forwardDir_);
    rightDir_ = glm::normalize(glm::cross(forwardDir_, worldUp_));
    upDir_ = glm::normalize(glm::cross(rightDir_, forwardDir_));

    glm::mat4 rot =
        glm::transpose(glm::mat4(glm::vec4(rightDir_, 0.f), glm::vec4(upDir_, 0.f),
                                 glm::vec4(-forwardDir_, 0.f), glm::vec4(0.f, 0.f, 0.f, 1.f)));
    glm::mat4 trans = glm::translate(glm::mat4(1.f), -position_);

    if (firstPersonMode_) {
        view_ = rot * trans;
    } else {
        view_ = trans * rot;
    }
}

void Camera::update(float deltaTime)
{
    glm::vec3 moveDir(0.f);

    if (keyState_.forward) {
        moveDir += forwardDir_;
    }
    if (keyState_.backward) {
        moveDir -= forwardDir_;
    }
    if (keyState_.right) {
        moveDir += rightDir_;
    }
    if (keyState_.left) {
        moveDir -= rightDir_;
    }

    moveDir.y = 0.f;
    if (glm::dot(moveDir, moveDir) > 0.f) {
        moveDir = glm::normalize(moveDir);
    }

    if (keyState_.up) {
        moveDir += worldUp_;
    }
    if (keyState_.down) {
        moveDir -= worldUp_;
    }

    position_ += moveDir * movementSpeed_ * deltaTime;

    updateView();
}

void Camera::rotate(float dx, float dy)
{
    rotation_ += glm::vec3(-dy, -dx, 0.f) * rotationSpeed_;
    rotation_.x = glm::clamp(rotation_.x, -89.9f, 89.9f);

    updateView();
}

void Camera::writeScene(SceneUniform& sceneUniform) const
{
    sceneUniform.view = view_;
    sceneUniform.proj = perspective_;
    sceneUniform.cameraPos = position_;
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