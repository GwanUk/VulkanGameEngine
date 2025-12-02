#include "Window.h"
#include "Logger.h"

namespace guk {

Window::Window()
{
    if (!glfwInit()) {
        exitLog("failed to init glfw");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    constexpr float aspectRatio = 16.f / 9.f;
    constexpr float outRatio = 0.8f;

    const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    uint32_t displayWidth = videoMode->width;
    uint32_t displayHeight = videoMode->height;

    uint32_t width{}, height{};
    if (displayWidth > displayHeight) {
        height = static_cast<uint32_t>(displayHeight * outRatio);
        width = static_cast<uint32_t>(height * aspectRatio);
    } else {
        width = static_cast<uint32_t>(displayWidth * outRatio);
        height = static_cast<uint32_t>(width / aspectRatio);
    }

    window_ = glfwCreateWindow(width, height, "Guk Vulkan Engine", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        exitLog("failed to create glfw window");
    }

    glfwSetErrorCallback([](int error, const char* description) {
        exitLog("GLFW Error (%i): %s\n", error, description);
    });

    uint32_t posX = (displayWidth - width) / 2;
    uint32_t posY = (displayHeight - height) / 2;
    glfwSetWindowPos(window_, posX, posY);
}

Window::~Window()
{
    glfwDestroyWindow(window_);
    glfwTerminate();
}

std::vector<const char*> Window::getRequiredExts() const
{
    uint32_t extCount{};
    const char** exts = glfwGetRequiredInstanceExtensions(&extCount);

    return std::vector<const char*>(exts, exts + extCount);
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const
{
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window_, nullptr, &surface));

    return surface;
}

VkExtent2D Window::getFramebufferSize() const
{
    int width{}, height{};
    glfwGetFramebufferSize(window_, &width, &height);
    return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(window_);
}

void Window::pollEvents() const
{
    glfwPollEvents();
}

bool Window::isMinimized() const
{
    int width{}, height{};
    glfwGetWindowSize(window_, &width, &height);
    return !width || !height;
}

void Window::waitEvents() const
{
    glfwWaitEvents();
}

void Window::setUserPointer(void* pointer) const
{
    glfwSetWindowUserPointer(window_, pointer);
}

void Window::setKeyCallback(GLFWkeyfun callback) const
{
    glfwSetKeyCallback(window_, callback);
}

void Window::setMouseButtonCallback(GLFWmousebuttonfun callback) const
{
    glfwSetMouseButtonCallback(window_, callback);
}

void Window::setCursorPosCallback(GLFWcursorposfun callback) const
{
    glfwSetCursorPosCallback(window_, callback);
}

void Window::setScrollCallback(GLFWscrollfun callback) const
{
    glfwSetScrollCallback(window_, callback);
}

void Window::setFramebufferSizeCallback(GLFWframebuffersizefun callback) const
{
    glfwSetFramebufferSizeCallback(window_, callback);
}

} // namespace guk