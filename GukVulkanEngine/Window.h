#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

namespace guk {

class Window
{
  public:
    Window();
    ~Window();

    std::vector<const char*> getRequiredExts() const;
    VkSurfaceKHR createSurface(VkInstance instance) const;
    VkExtent2D getFramebufferSize() const;

    bool shouldClose() const;
    void pollEvents() const;

    bool isMinimized() const;
    void waitEvents() const;

    void setUserPointer(void* pointer) const;
    void setKeyCallback(GLFWkeyfun callback) const;
    void setMouseButtonCallback(GLFWmousebuttonfun callback) const;
    void setCursorPosCallback(GLFWcursorposfun callback) const;
    void setScrollCallback(GLFWscrollfun callback) const;
    void setFramebufferSizeCallback(GLFWframebuffersizefun callback) const;
  
  private:
    GLFWwindow* window_{};
};

} // namespace guk
