#include "Engine.h"

#include "GuiRenderer.h"
#include "SceneRenderer.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <imgui.h>

namespace guk {

#ifdef NDEBUG
static constexpr bool enableValidation = false;
#else
static constexpr bool enableValidation = true;
#endif //  NDEBUG

static const char* VALIDATION_LAYER_NAME{"VK_LAYER_KHRONOS_validation"};
static const char* SWAPCHAIN_EXTENSION_NAME{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

static PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger{};
static PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger{};
static VkDebugUtilsMessengerEXT debugUtilsMessenger{};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::stringstream debugMessage{};
    if (pCallbackData->pMessageIdName) {
        debugMessage << "[" << pCallbackData->messageIdNumber << "]["
                     << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;
    } else {
        debugMessage << "[" << pCallbackData->messageIdNumber << "] : " << pCallbackData->pMessage;
    }

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        log("[VERBOSE] {}", debugMessage.str());
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        log("[INFO] {}", debugMessage.str());
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        log("[WARNING] {}", debugMessage.str());
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        exitLog("[ERROR] {}", debugMessage.str());
    }

    return VK_FALSE;
}

Engine::Engine()
{
    createWindow();
    createInstance();
    selectPhysicalDevice();
    createDevice();

    createCommandPool();
    allocateCommandBuffers();
    determineDepthStencilFormat();
    selectSmapleCount();
    createPipelineCache();

    createSwapChain();
    createDescriptorPool();
    createSyncObjects();

    sceneRenderer_ = new SceneRenderer(*this);
    guiRenderer_ = new GuiRenderer(*this);
}

Engine::~Engine()
{
    delete guiRenderer_;
    delete sceneRenderer_;

    for (uint32_t i = 0; i < swapchainImageCount_; i++) {
        vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
        vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
    }

    for (const auto& fence : inFlightFences_) {
        vkDestroyFence(device_, fence, nullptr);
    }

    vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);

    for (const auto& swapChainImageView : swapChainImageViews_) {
        vkDestroyImageView(device_, swapChainImageView, nullptr);
    }
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);

    vkDestroyPipelineCache(device_, pipelineCache_, nullptr);
    vkDestroyCommandPool(device_, commandPool_, nullptr);

    vkDestroyDevice(device_, nullptr);

    if (debugUtilsMessenger && vkDestroyDebugUtilsMessenger) {
        vkDestroyDebugUtilsMessenger(instance_, debugUtilsMessenger, nullptr);
    }
    vkDestroyInstance(instance_, nullptr);

    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Engine::run()
{
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        updateGui();
        sceneRenderer_->updateUniform(currentFrame_);

        drawFrame();
    }

    vkDeviceWaitIdle(device_);
}

VkCommandBuffer Engine::beginCommand() const
{
    VkCommandBufferAllocateInfo cmdAI{};
    cmdAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAI.commandPool = commandPool_;
    cmdAI.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAI, &cmd));

    VkCommandBufferBeginInfo cmdBI{};
    cmdBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBI));

    return cmd;
}

void Engine::submitAndWait(VkCommandBuffer cmd) const
{
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdSI{};
    cmdSI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSI.commandBuffer = cmd;

    VkSubmitInfo2 si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    si.commandBufferInfoCount = 1;
    si.pCommandBufferInfos = &cmdSI;

    VkFenceCreateInfo fenceCI{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence;

    VK_CHECK(vkCreateFence(device_, &fenceCI, nullptr, &fence));

    VK_CHECK(vkQueueSubmit2(queue_, 1, &si, fence));

    VK_CHECK(vkWaitForFences(device_, 1, &fence, VK_TRUE, 1000000000));
    vkDestroyFence(device_, fence, nullptr);
}

uint32_t Engine::getMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperty) const
{
    for (uint32_t i = 0; i < physicalDeviceMemoryProperties_.memoryTypeCount; i++) {
        if ((memoryType & 1) && (physicalDeviceMemoryProperties_.memoryTypes[i].propertyFlags &
                                 memoryProperty) == memoryProperty) {
            return i;
        }

        memoryType >>= 1;
    }

    return uint32_t(0);
}

VkShaderModule Engine::createShaderModule(const std::string& spv) const
{
    std::ifstream ifs(spv, std::ios::ate | std::ios::binary);
    if (!ifs.is_open()) {
        exitLog("failed to open file! [{}]", spv);
    }

    size_t size = (size_t)ifs.tellg();
    std::vector<char> buffer(size);

    ifs.seekg(0);
    ifs.read(buffer.data(), size);
    ifs.close();

    VkShaderModuleCreateInfo shaderModuleCI{};
    shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCI.codeSize = buffer.size();
    shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device_, &shaderModuleCI, nullptr, &shaderModule));

    return shaderModule;
}

void Engine::createWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    constexpr float aspectRatio = 16.f / 9.f;
    constexpr float outRatio = 0.8f;

    const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    uint32_t displayWidth = videoMode->width;
    uint32_t displayHeight = videoMode->height;

    uint32_t windowWidth{}, windowHeight{};
    if (displayWidth > displayHeight) {
        windowHeight = static_cast<uint32_t>(displayHeight * outRatio);
        windowWidth = static_cast<uint32_t>(windowHeight * aspectRatio);
    } else {
        windowWidth = static_cast<uint32_t>(displayWidth * outRatio);
        windowHeight = static_cast<uint32_t>(windowWidth / aspectRatio);
    }

    window_ = glfwCreateWindow(windowWidth, windowHeight, "Guk Vulkan Engine", nullptr, nullptr);

    uint32_t windowPosX = (displayWidth - windowWidth) / 2;
    uint32_t windowPosY = (displayHeight - windowHeight) / 2;
    glfwSetWindowPos(window_, windowPosX, windowPosY);

    glfwSetErrorCallback([](int error, const char* description) {
        exitLog("GLFW Error (%i): %s\n", error, description);
    });

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        app->framebufferResized_ = true;
    });

    glfwSetKeyCallback(window_,
                       [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                           if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                               glfwSetWindowShouldClose(window, GLFW_TRUE);
                           }
                       });

    glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button, int action, int mods) {
        auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (action == GLFW_PRESS) {
            switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                app->mouseState_.buttons.left = true;
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                app->mouseState_.buttons.right = true;
                break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                app->mouseState_.buttons.middle = true;
                break;
            }
        } else if (action == GLFW_RELEASE) {
            switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                app->mouseState_.buttons.left = false;
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                app->mouseState_.buttons.right = false;
                break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                app->mouseState_.buttons.middle = false;
                break;
            }
        }
    });

    glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double xpos, double ypos) {
        auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        app->mouseState_.position = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));
    });
}

void Engine::createInstance()
{
    uint32_t glfwExtensionCount{};
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> instanceExtensions(glfwExtensions,
                                                glfwExtensions + glfwExtensionCount);

    if (enableValidation) {
        uint32_t layerPropertiesCount{};
        vkEnumerateInstanceLayerProperties(&layerPropertiesCount, nullptr);
        std::vector<VkLayerProperties> layerPropertiesList(layerPropertiesCount);
        vkEnumerateInstanceLayerProperties(&layerPropertiesCount, layerPropertiesList.data());
        // validation layer check
        for (uint32_t i = 0; i < layerPropertiesCount; i++) {
            if (strcmp(layerPropertiesList[i].layerName, VALIDATION_LAYER_NAME) == 0) {
                break;
            }

            if (i == layerPropertiesCount - 1) {
                exitLog("validation layers requestd, but not available!");
            }
        }

        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Guk Vulkan Engine";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "Guk Vulkan Engine";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = &VALIDATION_LAYER_NAME;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
    if (enableValidation) {
        debugUtilsMessengerCreateInfo.sType =
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsMessengerCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugUtilsMessengerCreateInfo.pfnUserCallback = debugUtilsMessageCallback;

        instanceCreateInfo.pNext = &debugUtilsMessengerCreateInfo;
    }

    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance_));

    VK_CHECK(glfwCreateWindowSurface(instance_, window_, nullptr, &surface_));

    // instance debug utils messenger object
    if (enableValidation) {
        vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        vkDestroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));

        if (vkCreateDebugUtilsMessenger) {
            VK_CHECK(vkCreateDebugUtilsMessenger(instance_, &debugUtilsMessengerCreateInfo, nullptr,
                                                 &debugUtilsMessenger));
        }
    }
}

void Engine::selectPhysicalDevice()
{
    uint32_t gpuCount{};
    vkEnumeratePhysicalDevices(instance_, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    vkEnumeratePhysicalDevices(instance_, &gpuCount, physicalDevices.data());

    physicalDevice_ = physicalDevices[0];

    uint32_t queueFamilyCount{};
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties{queueFamilyCount};
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount,
                                             queueFamilyProperties.data());
    VkQueueFlags queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_, &presentSupport);
        if ((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags && presentSupport) {
            queueFaimlyIndex_ = i;
            break;
        }

        if (i == queueFamilyCount - 1) {
            exitLog("failed to select queue family index!");
        }
    }

    uint32_t extensionCount{};
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extensionCount,
                                         extensionProperties.data());
    for (uint32_t i = 0; i < extensionCount; i++) {
        if (strcmp(extensionProperties[i].extensionName, SWAPCHAIN_EXTENSION_NAME) == 0) {
            break;
        }

        if (i == extensionCount - 1) {
            exitLog("swapchain extension requestd, but not available!");
        }
    }

    const VkFormat formats[] = {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM};
    for (const auto& format : formats) {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &formatProperties);

        if (!(formatProperties.optimalTilingFeatures &
              VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            exitLog("texture image format does not support linear blitting!");
        }
    }

    vkGetPhysicalDeviceProperties(physicalDevice_, &physicalDeviceProperties_);
    vkGetPhysicalDeviceFeatures(physicalDevice_, &physicalDeviceFeatures_);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &physicalDeviceMemoryProperties_);
}

void Engine::createDevice()
{
    const float queuePriority = 1.f;
    VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.queueFamilyIndex = queueFaimlyIndex_;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceVulkan13Features physicalDeviceFeatures13{};
    physicalDeviceFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    physicalDeviceFeatures13.dynamicRendering = VK_TRUE;
    physicalDeviceFeatures13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
    physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalDeviceFeatures2.features = physicalDeviceFeatures_;
    physicalDeviceFeatures2.pNext = &physicalDeviceFeatures13;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = nullptr;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = &SWAPCHAIN_EXTENSION_NAME;
    deviceCreateInfo.pNext = &physicalDeviceFeatures2;

    VK_CHECK(vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_));

    vkGetDeviceQueue(device_, queueFaimlyIndex_, 0, &queue_);
}

void Engine::createCommandPool()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFaimlyIndex_;
    VK_CHECK(vkCreateCommandPool(device_, &commandPoolCreateInfo, nullptr, &commandPool_));
}

void Engine::allocateCommandBuffers()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()));
}

void Engine::determineDepthStencilFormat()
{
    std::vector<VkFormat> depthStencilFormatList = {
        VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT};

    for (const auto& depthStencilFormat : depthStencilFormatList) {
        VkFormatProperties formatProperties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, depthStencilFormat, &formatProperties);
        if (formatProperties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            depthStencilFormat_ = depthStencilFormat;
            return;
        }
    }

    exitLog("depth stencil format requestd, but not available!");
}

void Engine::selectSmapleCount()
{
    VkSampleCountFlags sampleCount = physicalDeviceProperties_.limits.framebufferColorSampleCounts &
                                     physicalDeviceProperties_.limits.framebufferDepthSampleCounts;
    if (sampleCount & VK_SAMPLE_COUNT_4_BIT) {
        sampleCount_ = VK_SAMPLE_COUNT_4_BIT;
    } else if (sampleCount & VK_SAMPLE_COUNT_2_BIT) {
        sampleCount_ = VK_SAMPLE_COUNT_2_BIT;
    } else {
        sampleCount_ = VK_SAMPLE_COUNT_1_BIT;
    }
}

void Engine::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VK_CHECK(vkCreatePipelineCache(device_, &pipelineCacheCreateInfo, nullptr, &pipelineCache_));
}

void Engine::createSwapChain()
{
    VkSwapchainKHR oldSwapchain{swapchain_};

    VkSurfaceCapabilitiesKHR surfaceCapabiliteis{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &surfaceCapabiliteis);

    swapchainImageExtent_ = surfaceCapabiliteis.currentExtent;
    if (swapchainImageExtent_.width == uint32_t(-1)) {
        int width{}, height{};
        glfwGetFramebufferSize(window_, &width, &height);
        swapchainImageExtent_.width =
            std::clamp(static_cast<uint32_t>(width), surfaceCapabiliteis.minImageExtent.width,
                       surfaceCapabiliteis.maxImageExtent.width);
        swapchainImageExtent_.height =
            std::clamp(static_cast<uint32_t>(height), surfaceCapabiliteis.minImageExtent.height,
                       surfaceCapabiliteis.maxImageExtent.height);
    }

    uint32_t surfaceFormatsCount{};
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &surfaceFormatsCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &surfaceFormatsCount,
                                         surfaceFormats.data());
    for (uint32_t i = 0; i < surfaceFormatsCount; i++) {
        if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            swapchainImageFormat_ = surfaceFormats[i].format;
            break;
        }

        if (i == surfaceFormatsCount - 1) {
            exitLog("surface format requestd, but not available!");
        }
    }

    uint32_t presentModeCount{};
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount,
                                              nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount,
                                              presentModes.data());

    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            break;
        }

        if (i == presentModeCount - 1) {
            exitLog("present mode requestd, but not available!");
        }
    }

    uint32_t imageCount = surfaceCapabiliteis.minImageCount + 1;
    if (surfaceCapabiliteis.maxImageCount > 0 && imageCount > surfaceCapabiliteis.maxImageCount) {
        imageCount = surfaceCapabiliteis.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = surface_;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = swapchainImageFormat_;
    swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainCreateInfo.imageExtent = swapchainImageExtent_;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.preTransform = surfaceCapabiliteis.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = oldSwapchain;

    VK_CHECK(vkCreateSwapchainKHR(device_, &swapchainCreateInfo, nullptr, &swapchain_));

    if (oldSwapchain) {
        for (const auto& swapChainImageView : swapChainImageViews_) {
            vkDestroyImageView(device_, swapChainImageView, nullptr);
        }
        vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &swapchainImageCount_, nullptr);
    swapchainImages_.resize(swapchainImageCount_);
    vkGetSwapchainImagesKHR(device_, swapchain_, &swapchainImageCount_, swapchainImages_.data());

    swapChainImageViews_.resize(swapchainImageCount_);
    for (uint32_t i = 0; i < swapchainImageCount_; i++) {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = swapchainImages_[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = swapchainImageFormat_;
        imageViewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                          VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        VK_CHECK(
            vkCreateImageView(device_, &imageViewCreateInfo, nullptr, &swapChainImageViews_[i]));
    }
}

void Engine::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);

    createSwapChain();

    sceneRenderer_->createAttachments();
}

void Engine::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> descriptorPoolSize(2);
    descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorPoolSize[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    descriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSize[1].descriptorCount = MAX_FRAMES_IN_FLIGHT + 1;

    VkDescriptorPoolCreateInfo descriptorPoolCI{};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(descriptorPoolSize.size());
    descriptorPoolCI.pPoolSizes = descriptorPoolSize.data();
    descriptorPoolCI.maxSets = MAX_FRAMES_IN_FLIGHT + 1;

    VK_CHECK(vkCreateDescriptorPool(device_, &descriptorPoolCI, nullptr, &descriptorPool_));
}

void Engine::createSyncObjects()
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]));
    }

    imageAvailableSemaphores_.resize(swapchainImageCount_);
    renderFinishedSemaphores_.resize(swapchainImageCount_);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < swapchainImageCount_; i++) {
        VK_CHECK(
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]));
        VK_CHECK(
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]));
    }
}

void Engine::updateGui()
{
    ImGuiIO& io = ImGui::GetIO();

    // Update ImGui IO state
    io.DisplaySize =
        ImVec2(float(swapchainImageExtent_.width), float(swapchainImageExtent_.height));
    io.MousePos = ImVec2(mouseState_.position.x, mouseState_.position.y);
    io.MouseDown[0] = mouseState_.buttons.left;
    io.MouseDown[1] = mouseState_.buttons.right;
    io.MouseDown[2] = mouseState_.buttons.middle;

    // Begin GUI frame
    ImGui::NewFrame();

    // Camera info window
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Camera Control")) {
        ImGui::Separator();
        ImGui::Text("Controls:");
        ImGui::Text("Mouse: Look around");
        ImGui::Text("WASD: Move");
        ImGui::Text("QE: Up/Down");
        ImGui::Text("F2: Toggle camera mode");
    }
    ImGui::End();

    ImGui::Render();
}

void Engine::drawFrame()
{
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                            imageAvailableSemaphores_[currentSemaphore_],
                                            VK_NULL_HANDLE, &currentImage_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        exitLog("failed to acquire swap chain image!");
    }

    guiRenderer_->update(currentFrame_);

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);

    sceneRenderer_->draw(commandBuffers_[currentFrame_], currentFrame_);

    VkSemaphoreSubmitInfo waitSemaphoreInfo{};
    waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreInfo.semaphore = imageAvailableSemaphores_[currentSemaphore_];
    waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    waitSemaphoreInfo.value = 0;
    waitSemaphoreInfo.deviceIndex = 0;

    VkSemaphoreSubmitInfo signalSemaphoreInfo{};
    signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfo.semaphore = renderFinishedSemaphores_[currentSemaphore_];
    signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signalSemaphoreInfo.value = 0;
    signalSemaphoreInfo.deviceIndex = 0;

    VkCommandBufferSubmitInfo cmdBufferInfo{};
    cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdBufferInfo.commandBuffer = commandBuffers_[currentFrame_];
    cmdBufferInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdBufferInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    VK_CHECK(vkQueueSubmit2(queue_, 1, &submitInfo, inFlightFences_[currentFrame_]));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores_[currentSemaphore_];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &currentImage_;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(queue_, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        exitLog("failed to present swap chain image!");
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    currentSemaphore_ = (currentSemaphore_ + 1) % swapchainImageCount_;
}

} // namespace guk
