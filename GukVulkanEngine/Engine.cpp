#include "Engine.h"
#include "Util.h"
#include "BufferObject.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <fstream>
#include <chrono>

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

    createSyncObjects();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createTextureImage();
    createColorAttahcment();
    createDepthStencilAttachment();

    createDescriptorPool();
    createDescriptorSetLayout();
    allocateDescriptorSets();
    createPipelineLayout();
    createPipeline();
}

Engine::~Engine()
{
    vkDestroyPipeline(device_, pipeline_, nullptr);
    vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
    vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);

    vkDestroyImageView(device_, depthStencilAttahcmentView_, nullptr);
    vkDestroyImage(device_, depthStencilAttahcment_, nullptr);
    vkFreeMemory(device_, depthStencilAttahcmentMemory_, nullptr);

    vkDestroyImageView(device_, colorAttahcmentView_, nullptr);
    vkDestroyImage(device_, colorAttahcment_, nullptr);
    vkFreeMemory(device_, colorAttahcmentMemory_, nullptr);

    vkDestroySampler(device_, textureSampler_, nullptr);
    vkDestroyImageView(device_, textureImageView_, nullptr);
    vkDestroyImage(device_, textureImage_, nullptr);
    vkFreeMemory(device_, textureImageMemory_, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device_, uniformBuffers_[i], nullptr);
        vkFreeMemory(device_, uniformBuffersMemory_[i], nullptr);
    }

    vkDestroyBuffer(device_, indexBuffer_, nullptr);
    vkFreeMemory(device_, indexBufferMemory_, nullptr);
    vkDestroyBuffer(device_, vertexBuffer_, nullptr);
    vkFreeMemory(device_, vertexBufferMemory_, nullptr);

    for (uint32_t i = 0; i < swapchainImageCount_; i++) {
        vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
        vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
    }

    for (const auto& fence : inFlightFences_) {
        vkDestroyFence(device_, fence, nullptr);
    }

    for (const auto& swapChainImageView : swapChainImageViews_) {
        vkDestroyImageView(device_, swapChainImageView, nullptr);
    }
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);

    vkDestroyPipelineCache(device_, pipelineCache_, nullptr);
    vkDestroyCommandPool(device_, commandPool_, nullptr);

    if (debugUtilsMessenger && vkDestroyDebugUtilsMessenger) {
        vkDestroyDebugUtilsMessenger(instance_, debugUtilsMessenger, nullptr);
    }

    vkDestroyDevice(device_, nullptr);
    vkDestroyInstance(instance_, nullptr);

    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Engine::run()
{
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        drawFrame();
    }

    vkDeviceWaitIdle(device_);
}

VkCommandBuffer Engine::beginCommand()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return commandBuffer;
}

void Engine::submitAndWait(VkCommandBuffer commandBuffer)
{
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkCommandBufferSubmitInfo CommandBufferSubmitInfo{};
    CommandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    CommandBufferSubmitInfo.commandBuffer = commandBuffer;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &CommandBufferSubmitInfo;

    VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence;

    VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr, &fence));

    VK_CHECK(vkQueueSubmit2(queue_, 1, &submitInfo, fence));

    VK_CHECK(vkWaitForFences(device_, 1, &fence, VK_TRUE, 1000000000));
    vkDestroyFence(device_, fence, nullptr);
}

uint32_t Engine::getMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperty)
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

VkShaderModule Engine::createShaderModuleFromSpvfile(const std::string& spvFilename)
{
    std::ifstream ifs(spvFilename, std::ios::ate | std::ios::binary);
    if (!ifs.is_open()) {
        exitLog("failed to open file! [{}]", spvFilename);
    }

    size_t fileSize = (size_t)ifs.tellg();
    std::vector<char> buffer(fileSize);

    ifs.seekg(0);
    ifs.read(buffer.data(), fileSize);
    ifs.close();

    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = buffer.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device_, &shaderModuleCreateInfo, nullptr, &shaderModule));

    return shaderModule;
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

    vkDestroyImageView(device_, depthStencilAttahcmentView_, nullptr);
    vkDestroyImage(device_, depthStencilAttahcment_, nullptr);
    vkFreeMemory(device_, depthStencilAttahcmentMemory_, nullptr);

    vkDestroyImageView(device_, colorAttahcmentView_, nullptr);
    vkDestroyImage(device_, colorAttahcment_, nullptr);
    vkFreeMemory(device_, colorAttahcmentMemory_, nullptr);

    createColorAttahcment();
    createDepthStencilAttachment();
}

void Engine::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    SceneUniform ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
    ubo.view =
        glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
    ubo.proj = glm::perspective(glm::radians(45.f),
                                swapchainImageExtent_.width / (float)swapchainImageExtent_.height,
                                0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
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

    glfwSetErrorCallback([](int error, const char* description) {
        exitLog("GLFW Error (%i): %s\n", error, description);
    });

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
        app->glfwFramebufferResized_ = true;
    });

    glfwSetKeyCallback(window_,
                       [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                           if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                               glfwSetWindowShouldClose(window, GLFW_TRUE);
                           }
                       });

    uint32_t windowPosX = (displayWidth - windowWidth) / 2;
    uint32_t windowPosY = (displayHeight - windowHeight) / 2;
    glfwSetWindowPos(window_, windowPosX, windowPosY);
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
    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);

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

void Engine::createSyncObjects()
{
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

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

void Engine::createVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo stagingBufferCI{};
    stagingBufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferCI.size = bufferSize;
    stagingBufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device_, &stagingBufferCI, nullptr, &stagingBuffer));

    VkMemoryRequirements stagingBufferMRs;
    vkGetBufferMemoryRequirements(device_, stagingBuffer, &stagingBufferMRs);

    VkMemoryAllocateInfo stagingBufferMA{};
    stagingBufferMA.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingBufferMA.allocationSize = stagingBufferMRs.size;
    stagingBufferMA.memoryTypeIndex = getMemoryTypeIndex(stagingBufferMRs.memoryTypeBits,
                                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(device_, &stagingBufferMA, nullptr, &stagingBufferMemory));
    VK_CHECK(vkBindBufferMemory(device_, stagingBuffer, stagingBufferMemory, 0));

    void* data;
    VK_CHECK(vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data));
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device_, stagingBufferMemory);

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device_, &bufferCreateInfo, nullptr, &vertexBuffer_));

    VkMemoryRequirements mrs;
    vkGetBufferMemoryRequirements(device_, vertexBuffer_, &mrs);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = mrs.size;
    memoryAllocateInfo.memoryTypeIndex =
        getMemoryTypeIndex(mrs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &vertexBufferMemory_));
    VK_CHECK(vkBindBufferMemory(device_, vertexBuffer_, vertexBufferMemory_, 0));

    VkCommandBuffer commandBuffer = beginCommand();

    VkBufferCopy bufferCopy{};
    bufferCopy.srcOffset = 0;
    bufferCopy.dstOffset = 0;
    bufferCopy.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, vertexBuffer_, 1, &bufferCopy);

    submitAndWait(commandBuffer);

    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingBufferMemory, nullptr);
}

void Engine::createIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo stagingBufferCreateInfo{};
    stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferCreateInfo.size = bufferSize;
    stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device_, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements stagingBufferMemoryRequirements;
    vkGetBufferMemoryRequirements(device_, stagingBuffer, &stagingBufferMemoryRequirements);

    VkMemoryAllocateInfo stagingBufferMemoryAllocateInfo{};
    stagingBufferMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingBufferMemoryAllocateInfo.allocationSize = stagingBufferMemoryRequirements.size;
    stagingBufferMemoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(
        stagingBufferMemoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(
        vkAllocateMemory(device_, &stagingBufferMemoryAllocateInfo, nullptr, &stagingBufferMemory));
    VK_CHECK(vkBindBufferMemory(device_, stagingBuffer, stagingBufferMemory, 0));

    void* data;
    VK_CHECK(vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data));
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device_, stagingBufferMemory);

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device_, &bufferCreateInfo, nullptr, &indexBuffer_));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device_, indexBuffer_, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex =
        getMemoryTypeIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &indexBufferMemory_));
    VK_CHECK(vkBindBufferMemory(device_, indexBuffer_, indexBufferMemory_, 0));

    VkCommandBuffer commandBuffer = beginCommand();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, indexBuffer_, 1, &copyRegion);

    submitAndWait(commandBuffer);

    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingBufferMemory, nullptr);
}

void Engine::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(SceneUniform);

    uniformBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory_.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped_.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device_, &bufferCreateInfo, nullptr, &uniformBuffers_[i]));

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device_, uniformBuffers_[i], &memoryRequirements);

        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(
            vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &uniformBuffersMemory_[i]));
        VK_CHECK(vkBindBufferMemory(device_, uniformBuffers_[i], uniformBuffersMemory_[i], 0));

        VK_CHECK(vkMapMemory(device_, uniformBuffersMemory_[i], 0, bufferSize, 0,
                             &uniformBuffersMapped_[i]));
    }
}

void Engine::createTextureImage()
{
    int width, height, channels;
    stbi_uc* pixels =
        stbi_load("textures/blender_uv_grid_2k.png", &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        exitLog("failed to load texture image!");
        return;
    }

    VkDeviceSize size = width * height * 4;
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device_, &bufferCreateInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device_, stagingBuffer, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(
        memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &stagingBufferMemory));
    VK_CHECK(vkBindBufferMemory(device_, stagingBuffer, stagingBufferMemory, 0));

    void* data;
    VK_CHECK(vkMapMemory(device_, stagingBufferMemory, 0, size, 0, &data));
    memcpy(data, pixels, static_cast<size_t>(size));
    vkUnmapMemory(device_, stagingBufferMemory);

    stbi_image_free(pixels);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                      VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &textureImage_));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, textureImage_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        getMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &textureImageMemory_));
    VK_CHECK(vkBindImageMemory(device_, textureImage_, textureImageMemory_, 0));

    VkCommandBuffer commandBuffer = beginCommand();

    VkImageMemoryBarrier2 imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = textureImage_;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = mipLevels;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, textureImage_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice_, VK_FORMAT_R8G8B8A8_SRGB,
                                        &formatProperties);

    if (!(formatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        exitLog("texture image format does not support linear blitting!");
    }

    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = textureImage_;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;

    int32_t mipWidth = width;
    int32_t mipHeight = height;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.subresourceRange.baseMipLevel = i - 1;

        vkCmdPipelineBarrier2(commandBuffer, &dependency);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1,
                              1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer, textureImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       textureImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                       VK_FILTER_LINEAR);

        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier2(commandBuffer, &dependency);

        if (mipWidth > 1) {
            mipWidth /= 2;
        }
        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;

    vkCmdPipelineBarrier2(commandBuffer, &dependency);

    submitAndWait(commandBuffer);

    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingBufferMemory, nullptr);

    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = textureImage_;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageViewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                      VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(device_, &imageViewCreateInfo, nullptr, &textureImageView_));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = physicalDeviceProperties_.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.mipLodBias = 0.f;

    VK_CHECK(vkCreateSampler(device_, &samplerInfo, nullptr, &textureSampler_));
}

void Engine::createColorAttahcment()
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapchainImageExtent_.width;
    imageInfo.extent.height = swapchainImageExtent_.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = swapchainImageFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = sampleCount_;
    imageInfo.flags = 0;

    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &colorAttahcment_));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, colorAttahcment_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        getMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &colorAttahcmentMemory_));
    VK_CHECK(vkBindImageMemory(device_, colorAttahcment_, colorAttahcmentMemory_, 0));

    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = colorAttahcment_;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = swapchainImageFormat_;
    imageViewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                      VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(device_, &imageViewCreateInfo, nullptr, &colorAttahcmentView_));
}

void Engine::createDepthStencilAttachment()
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapchainImageExtent_.width;
    imageInfo.extent.height = swapchainImageExtent_.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthStencilFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = sampleCount_;
    imageInfo.flags = 0;

    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &depthStencilAttahcment_));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, depthStencilAttahcment_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        getMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &depthStencilAttahcmentMemory_));
    VK_CHECK(vkBindImageMemory(device_, depthStencilAttahcment_, depthStencilAttahcmentMemory_, 0));

    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = depthStencilAttahcment_;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = depthStencilFormat_;
    imageViewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                      VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    VK_CHECK(
        vkCreateImageView(device_, &imageViewCreateInfo, nullptr, &depthStencilAttahcmentView_));
}

void Engine::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> descriptorPoolSize(2);
    descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorPoolSize[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    descriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSize[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo descriptorPoolCI{};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(descriptorPoolSize.size());
    descriptorPoolCI.pPoolSizes = descriptorPoolSize.data();
    descriptorPoolCI.maxSets = MAX_FRAMES_IN_FLIGHT;

    VK_CHECK(vkCreateDescriptorPool(device_, &descriptorPoolCI, nullptr, &descriptorPool_));
}

void Engine::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboDescriptorSetLayoutBinding{};
    uboDescriptorSetLayoutBinding.binding = 0;
    uboDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboDescriptorSetLayoutBinding.descriptorCount = 1;
    uboDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding{};
    samplerDescriptorSetLayoutBinding.binding = 1;
    samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerDescriptorSetLayoutBinding.descriptorCount = 1;
    samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings{
        uboDescriptorSetLayoutBinding, samplerDescriptorSetLayoutBinding};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
    descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
    descriptorSetLayoutCI.pBindings = descriptorSetLayoutBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device_, &descriptorSetLayoutCI, nullptr,
                                         &descriptorSetLayout_));
}

void Engine::allocateDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(MAX_FRAMES_IN_FLIGHT,
                                                            descriptorSetLayout_);
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool_;
    descriptorSetAllocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

    descriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(device_, &descriptorSetAllocateInfo, descriptorSets_.data()));

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(SceneUniform);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView_;
        imageInfo.sampler = textureSampler_;

        std::vector<VkWriteDescriptorSet> descriptorWrites(2);

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets_[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pTexelBufferView = nullptr;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets_[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pImageInfo = &imageInfo;
        descriptorWrites[1].pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}

void Engine::createPipelineLayout()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout_;

    VK_CHECK(vkCreatePipelineLayout(device_, &pipelineLayoutCI, nullptr, &pipelineLayout_));
}

void Engine::createPipeline()
{
    VkShaderModule vertexShaderModule = createShaderModuleFromSpvfile("shaders/test.vert.spv");
    VkShaderModule fragmentShaderModule = createShaderModuleFromSpvfile("shaders/test.frag.spv");

    VkPipelineShaderStageCreateInfo pipelineVertexShaderStageCI{};
    pipelineVertexShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineVertexShaderStageCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineVertexShaderStageCI.module = vertexShaderModule;
    pipelineVertexShaderStageCI.pName = "main";

    VkPipelineShaderStageCreateInfo pipelineFragmentShaderStageCI{};
    pipelineFragmentShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineFragmentShaderStageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineFragmentShaderStageCI.module = fragmentShaderModule;
    pipelineFragmentShaderStageCI.pName = "main";

    VkPipelineShaderStageCreateInfo pipelineShaderStageCIs[] = {pipelineVertexShaderStageCI,
                                                                pipelineFragmentShaderStageCI};

    auto vertexInputBindingDescrption = Vertex::getBindingDescrption();
    auto vertexInputAttributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCI{};
    pipelineVertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStateCI.vertexBindingDescriptionCount = 1;
    pipelineVertexInputStateCI.pVertexBindingDescriptions = &vertexInputBindingDescrption;
    pipelineVertexInputStateCI.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vertexInputAttributeDescriptions.size());
    pipelineVertexInputStateCI.pVertexAttributeDescriptions =
        vertexInputAttributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCI{};
    pipelineInputAssemblyStateCI.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineInputAssemblyStateCI.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCI{};
    pipelineRasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationStateCI.depthClampEnable = VK_FALSE;
    pipelineRasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
    pipelineRasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
    pipelineRasterizationStateCI.lineWidth = 1.f;
    pipelineRasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineRasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineRasterizationStateCI.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCI{};
    pipelineMultisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateCI.sampleShadingEnable = VK_FALSE;
    pipelineMultisampleStateCI.rasterizationSamples = sampleCount_;

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCI{};
    pipelineDepthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilStateCI.depthTestEnable = VK_TRUE;
    pipelineDepthStencilStateCI.depthWriteEnable = VK_TRUE;
    pipelineDepthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDepthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
    pipelineDepthStencilStateCI.minDepthBounds = 0.f;
    pipelineDepthStencilStateCI.maxDepthBounds = 1.f;
    pipelineDepthStencilStateCI.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
    pipelineColorBlendAttachmentState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
    pipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    pipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    pipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipelineColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCI{};
    pipelineColorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendStateCI.logicOpEnable = VK_FALSE;
    pipelineColorBlendStateCI.logicOp = VK_LOGIC_OP_COPY;
    pipelineColorBlendStateCI.attachmentCount = 1;
    pipelineColorBlendStateCI.pAttachments = &pipelineColorBlendAttachmentState;
    pipelineColorBlendStateCI.blendConstants[0] = 0.f;
    pipelineColorBlendStateCI.blendConstants[1] = 0.f;
    pipelineColorBlendStateCI.blendConstants[2] = 0.f;
    pipelineColorBlendStateCI.blendConstants[3] = 0.f;

    VkPipelineViewportStateCreateInfo pipelineViewportStateCI{};
    pipelineViewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportStateCI.viewportCount = 1;
    pipelineViewportStateCI.scissorCount = 1;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCI{};
    pipelineDynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    pipelineDynamicStateCI.pDynamicStates = dynamicStates.data();

    VkPipelineRenderingCreateInfo pipelineRenderingCI{};
    pipelineRenderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCI.colorAttachmentCount = 1;
    pipelineRenderingCI.pColorAttachmentFormats = &swapchainImageFormat_;
    pipelineRenderingCI.depthAttachmentFormat = depthStencilFormat_;
    pipelineRenderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo graphicsPipelineCI{};
    graphicsPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCI.pNext = &pipelineRenderingCI;
    graphicsPipelineCI.stageCount = 2;
    graphicsPipelineCI.pStages = pipelineShaderStageCIs;
    graphicsPipelineCI.pVertexInputState = &pipelineVertexInputStateCI;
    graphicsPipelineCI.pInputAssemblyState = &pipelineInputAssemblyStateCI;
    graphicsPipelineCI.pViewportState = &pipelineViewportStateCI;
    graphicsPipelineCI.pRasterizationState = &pipelineRasterizationStateCI;
    graphicsPipelineCI.pMultisampleState = &pipelineMultisampleStateCI;
    graphicsPipelineCI.pDepthStencilState = &pipelineDepthStencilStateCI;
    graphicsPipelineCI.pColorBlendState = &pipelineColorBlendStateCI;
    graphicsPipelineCI.pDynamicState = &pipelineDynamicStateCI;
    graphicsPipelineCI.layout = pipelineLayout_;
    graphicsPipelineCI.renderPass = VK_NULL_HANDLE;
    graphicsPipelineCI.subpass = 0;
    graphicsPipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    graphicsPipelineCI.basePipelineIndex = -1;

    VK_CHECK(vkCreateGraphicsPipelines(device_, pipelineCache_, 1, &graphicsPipelineCI, nullptr,
                                       &pipeline_));

    vkDestroyShaderModule(device_, vertexShaderModule, nullptr);
    vkDestroyShaderModule(device_, fragmentShaderModule, nullptr);
}

void Engine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkImageMemoryBarrier2 colorAttahcmentBarrier{};
    colorAttahcmentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    colorAttahcmentBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorAttahcmentBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    colorAttahcmentBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorAttahcmentBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    colorAttahcmentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttahcmentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttahcmentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorAttahcmentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorAttahcmentBarrier.image = colorAttahcment_;
    colorAttahcmentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorAttahcmentBarrier.subresourceRange.baseMipLevel = 0;
    colorAttahcmentBarrier.subresourceRange.levelCount = 1;
    colorAttahcmentBarrier.subresourceRange.baseArrayLayer = 0;
    colorAttahcmentBarrier.subresourceRange.layerCount = 1;

    VkImageMemoryBarrier2 swapchainImageBarrier{};
    swapchainImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    swapchainImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    swapchainImageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
    swapchainImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    swapchainImageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    swapchainImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainImageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    swapchainImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainImageBarrier.image = swapchainImages_[imageIndex];
    swapchainImageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchainImageBarrier.subresourceRange.baseMipLevel = 0;
    swapchainImageBarrier.subresourceRange.levelCount = 1;
    swapchainImageBarrier.subresourceRange.baseArrayLayer = 0;
    swapchainImageBarrier.subresourceRange.layerCount = 1;

    VkImageMemoryBarrier2 depthStencilBarrier{};
    depthStencilBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    depthStencilBarrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    depthStencilBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthStencilBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    depthStencilBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthStencilBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStencilBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStencilBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthStencilBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthStencilBarrier.image = depthStencilAttahcment_;
    depthStencilBarrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    depthStencilBarrier.subresourceRange.baseMipLevel = 0;
    depthStencilBarrier.subresourceRange.levelCount = 1;
    depthStencilBarrier.subresourceRange.baseArrayLayer = 0;
    depthStencilBarrier.subresourceRange.layerCount = 1;

    std::vector<VkImageMemoryBarrier2> imageBarriers{colorAttahcmentBarrier, swapchainImageBarrier,
                                                     depthStencilBarrier};
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
    dependencyInfo.pImageMemoryBarriers = imageBarriers.data();

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorAttahcmentView_;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.clearValue.color = {0.f, 0.f, 0.f, 1.f};
    colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachment.resolveImageView = swapChainImageViews_[imageIndex];
    colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkRenderingAttachmentInfo depthStecilAttachment{};
    depthStecilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthStecilAttachment.imageView = depthStencilAttahcmentView_;
    depthStecilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStecilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStecilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthStecilAttachment.clearValue.depthStencil = {1.f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0},
                                {swapchainImageExtent_.width, swapchainImageExtent_.height}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthStecilAttachment;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(swapchainImageExtent_.width);
    viewport.height = static_cast<float>(swapchainImageExtent_.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainImageExtent_;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexbuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexbuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &descriptorSets_[currentFrame_], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    vkCmdEndRendering(commandBuffer);

    VkImageMemoryBarrier2 presentBarrier{};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    presentBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    presentBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    presentBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    presentBarrier.dstAccessMask = VK_ACCESS_2_NONE;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.image = swapchainImages_[imageIndex];
    presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    presentBarrier.subresourceRange.baseMipLevel = 0;
    presentBarrier.subresourceRange.levelCount = 1;
    presentBarrier.subresourceRange.baseArrayLayer = 0;
    presentBarrier.subresourceRange.layerCount = 1;

    VkDependencyInfo presentDependencyInfo{};
    presentDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    presentDependencyInfo.imageMemoryBarrierCount = 1;
    presentDependencyInfo.pImageMemoryBarriers = &presentBarrier;

    vkCmdPipelineBarrier2(commandBuffer, &presentDependencyInfo);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void Engine::drawFrame()
{
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                            imageAvailableSemaphores_[currentSemaphore_],
                                            VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        exitLog("failed to acquire swap chain image!");
    }

    updateUniformBuffer(currentFrame_);

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
    recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

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
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(queue_, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        glfwFramebufferResized_) {
        glfwFramebufferResized_ = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        exitLog("failed to present swap chain image!");
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    currentSemaphore_ = (currentSemaphore_ + 1) % swapchainImageCount_;
}

} // namespace guk
