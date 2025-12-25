#include "Device.h"
#include "Logger.h"

#include <fstream>
#include <sstream>

namespace guk {

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

Device::Device(const std::vector<const char*>& instanceExtensions)
{
    createInstance(instanceExtensions);
    selectPhysicalDevice();
    createDevice();
    createPipelineCache();
    createCommandPool();
    createDescriptorPool();
    createSamplers();
}

Device::~Device()
{
    for (const auto& sampler : samplers_) {
        vkDestroySampler(device_, sampler, nullptr);
    }

    vkDestroyDescriptorPool(device_, descPool_, nullptr);
    vkDestroyCommandPool(device_, cmdPool_, nullptr);
    vkDestroyPipelineCache(device_, cache_, nullptr);
    vkDestroyDevice(device_, nullptr);

    if (debugUtilsMessenger) {
        vkDestroyDebugUtilsMessenger(instance_, debugUtilsMessenger, nullptr);
    }

    vkDestroyInstance(instance_, nullptr);
}

VkInstance Device::instance() const
{
    return instance_;
}

VkPhysicalDevice Device::physical() const
{
    return physicalDevice_;
}

VkDevice Device::get() const
{
    return device_;
}

VkQueue Device::queue() const
{
    return queue_;
}

VkPipelineCache Device::cache() const
{
    return cache_;
}

VkFormat Device::depthStencilFormat() const
{
    return depthStencilFmt_;
}

VkSampleCountFlagBits Device::smapleCount() const
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);

    VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts &
                                properties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_4_BIT) {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT) {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

void Device::checkSurfaceSupport(VkSurfaceKHR surface) const
{
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, queueFaimlyIdx_, surface, &presentSupport);

    if (!presentSupport) {
        exitLog("Separate graphics and presenting queues are not supported yet!");
    }
}

uint32_t Device::getMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperty) const
{
    VkPhysicalDeviceMemoryProperties memory{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memory);

    for (uint32_t i = 0; i < memory.memoryTypeCount; i++) {
        if ((memoryType & 1) &&
            (memory.memoryTypes[i].propertyFlags & memoryProperty) == memoryProperty) {
            return i;
        }

        memoryType >>= 1;
    }

    return uint32_t(0);
}

VkCommandBuffer Device::cmdBuffers(uint32_t index) const
{
    return cmdBuffers_[index];
}

VkCommandBuffer Device::beginCmd() const
{
    VkCommandBufferAllocateInfo cmdAI{};
    cmdAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAI.commandPool = cmdPool_;
    cmdAI.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAI, &cmd));

    VkCommandBufferBeginInfo cmdBI{};
    cmdBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBI));

    return cmd;
}

void Device::submitWait(VkCommandBuffer cmd) const
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

const VkDescriptorPool& Device::descriptorPool() const
{
    return descPool_;
}

VkSampler Device::samplerAnisoRepeat() const
{
    return samplers_[0];
}

VkSampler Device::samplerAnisoClamp() const
{
    return samplers_[1];
}

VkSampler Device::samplerLinearRepeat() const
{
    return samplers_[2];
}

VkSampler Device::samplerLinearClamp() const
{
    return samplers_[3];
}

void Device::createInstance(std::vector<const char*> extensions)
{
#ifdef NDEBUG
    bool enableValidation = false;
#else
    bool enableValidation = true;
#endif //  NDEBUG

    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCIE{};

    if (enableValidation) {
        // validation layer
        uint32_t layerCount{};
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        if (find_if(availableLayers.begin(), availableLayers.end(), [=](VkLayerProperties layer) {
                return strcmp(layer.layerName, validationLayer_) == 0;
            }) == availableLayers.end()) {

            exitLog("validation layers requestd, but not available!");
        }

        // debug extension
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        // debug message
        debugMessengerCIE.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugMessengerCIE.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugMessengerCIE.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugMessengerCIE.pfnUserCallback = debugUtilsMessageCallback;
    }

    uint32_t extCount{};
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExtensions.data());

    for (const char* extension : extensions) {
        if (find_if(availableExtensions.begin(), availableExtensions.end(),
                    [=](VkExtensionProperties ext) {
                        return strcmp(ext.extensionName, extension) == 0;
                    }) == availableExtensions.end()) {

            exitLog("debug extension requestd, but not available!");
        }
    }

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Guk Vulkan Engine";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "Guk Vulkan Engine";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCI{};
    instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.pApplicationInfo = &applicationInfo;
    instanceCI.enabledLayerCount = 1;
    instanceCI.ppEnabledLayerNames = &validationLayer_;
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceCI.ppEnabledExtensionNames = extensions.data();
    instanceCI.pNext = enableValidation ? &debugMessengerCIE : nullptr;

    VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &instance_));

    // create debug messenger
    if (enableValidation) {
        vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        vkDestroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));

        if (vkCreateDebugUtilsMessenger) {
            VK_CHECK(vkCreateDebugUtilsMessenger(instance_, &debugMessengerCIE, nullptr,
                                                 &debugUtilsMessenger));
        }
    }
}

void Device::selectPhysicalDevice()
{
    uint32_t gpuCount{};
    vkEnumeratePhysicalDevices(instance_, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    vkEnumeratePhysicalDevices(instance_, &gpuCount, physicalDevices.data());

    physicalDevice_ = physicalDevices[0];

    // queue family index
    uint32_t qFamilyCnt{};
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qFamilyCnt, nullptr);
    std::vector<VkQueueFamilyProperties> qFamilies{qFamilyCnt};
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qFamilyCnt, qFamilies.data());

    VkQueueFlags qFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    for (uint32_t i = 0; i < qFamilyCnt; i++) {
        if ((qFamilies[i].queueFlags & qFlags) == qFlags) {
            queueFaimlyIdx_ = i;
            break;
        }

        if (i == qFamilyCnt - 1) {
            exitLog("failed to select queue family index!");
        }
    }

    // swapchain extesion
    uint32_t extCnt{};
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extCnt, nullptr);
    std::vector<VkExtensionProperties> exts(extCnt);
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extCnt, exts.data());

    if (find_if(exts.begin(), exts.end(), [=](VkExtensionProperties ext) {
            return strcmp(ext.extensionName, swapchainExtension_) == 0;
        }) == exts.end()) {

        exitLog("swapchain extension requestd, but not available!");
    }

    // depth stencil format
    std::vector<VkFormat> dsfmts = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
                                    VK_FORMAT_D16_UNORM_S8_UINT};

    auto dsFmtIt = find_if(dsfmts.begin(), dsfmts.end(), [=](VkFormat format) {
        VkFormatProperties fmt{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &fmt);
        return fmt.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    });
    if (dsFmtIt == dsfmts.end()) {
        exitLog("depth stencil format requestd, but not available!");
    }

    depthStencilFmt_ = *dsFmtIt;
}

void Device::createDevice()
{
    const float queuePriority = 1.f;
    VkDeviceQueueCreateInfo queueCI{};
    queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCI.queueFamilyIndex = queueFaimlyIdx_;
    queueCI.queueCount = 1;
    queueCI.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};
    vkGetPhysicalDeviceFeatures(physicalDevice_, &deviceFeatures);

    VkPhysicalDeviceVulkan13Features deviceFeatures13{};
    deviceFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    deviceFeatures13.dynamicRendering = VK_TRUE;
    deviceFeatures13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features = deviceFeatures;
    deviceFeatures2.pNext = &deviceFeatures13;

    VkDeviceCreateInfo deviceCI{};
    deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCI.queueCreateInfoCount = 1;
    deviceCI.pQueueCreateInfos = &queueCI;
    deviceCI.pEnabledFeatures = nullptr;
    deviceCI.enabledExtensionCount = 1;
    deviceCI.ppEnabledExtensionNames = &swapchainExtension_;
    deviceCI.pNext = &deviceFeatures2;

    VK_CHECK(vkCreateDevice(physicalDevice_, &deviceCI, nullptr, &device_));

    // queue
    vkGetDeviceQueue(device_, queueFaimlyIdx_, 0, &queue_);
}

void Device::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCI{};
    pipelineCacheCI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    VK_CHECK(vkCreatePipelineCache(device_, &pipelineCacheCI, nullptr, &cache_));
}

void Device::createCommandPool()
{
    VkCommandPoolCreateInfo commandPoolCI{};
    commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCI.queueFamilyIndex = queueFaimlyIdx_;

    VK_CHECK(vkCreateCommandPool(device_, &commandPoolCI, nullptr, &cmdPool_));

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = cmdPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers_.size());

    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, cmdBuffers_.data()));
}

void Device::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> descPoolSize(2);
    descPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descPoolSize[0].descriptorCount = 3 * MAX_FRAMES_IN_FLIGHT;
    descPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descPoolSize[1].descriptorCount = 5 * MAX_FRAMES_IN_FLIGHT + 1;

    VkDescriptorPoolCreateInfo descPoolCI{};
    descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolCI.poolSizeCount = static_cast<uint32_t>(descPoolSize.size());
    descPoolCI.pPoolSizes = descPoolSize.data();
    descPoolCI.maxSets = 3 * MAX_FRAMES_IN_FLIGHT + 1;

    VK_CHECK(vkCreateDescriptorPool(device_, &descPoolCI, nullptr, &descPool_));
}

void Device::createSamplers()
{
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(physicalDevice_, &features);
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);

    // Aniso Repeat
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.mipLodBias = 0.f;
    samplerCI.anisotropyEnable = features.samplerAnisotropy;
    samplerCI.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerCI.compareEnable = VK_FALSE;
    samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCI.minLod = 0.f;
    samplerCI.maxLod = VK_LOD_CLAMP_NONE;
    samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCI.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(device_, &samplerCI, nullptr, &samplers_[0]));

    // Aniso Clamp
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VK_CHECK(vkCreateSampler(device_, &samplerCI, nullptr, &samplers_[1]));

    // Linear Repeat
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.anisotropyEnable = VK_FALSE;
    samplerCI.maxAnisotropy = 1.f;

    VK_CHECK(vkCreateSampler(device_, &samplerCI, nullptr, &samplers_[2]));

    // Linear Clamp
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VK_CHECK(vkCreateSampler(device_, &samplerCI, nullptr, &samplers_[3]));
}

VkShaderModule Device::createShaderModule(const std::string& spv) const
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

} // namespace guk