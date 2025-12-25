#include "Swapchain.h"
#include "Logger.h"

#include <algorithm>

namespace guk {

Swapchain::Swapchain(std::unique_ptr<Window>& window, std::shared_ptr<Device> device)
    : device_(device)
{
    surface_ = window->createSurface(device_->instance());
    device_->checkSurfaceSupport(surface_);
    create(window);
}

Swapchain::~Swapchain()
{
    vkDestroySwapchainKHR(device_->get(), swapchain_, nullptr);
    vkDestroySurfaceKHR(device_->instance(), surface_, nullptr);
}

void Swapchain::create(std::unique_ptr<Window>& window)
{
    VkSwapchainKHR oldSwapchain{swapchain_};

    VkSurfaceCapabilitiesKHR capabiliteis{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_->physical(), surface_, &capabiliteis);

    VkExtent2D extent = capabiliteis.currentExtent;
    if (extent.width == uint32_t(-1)) {
        extent = window->getFramebufferSize();
        extent.width = std::clamp(extent.width, capabiliteis.minImageExtent.width,
                                  capabiliteis.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabiliteis.minImageExtent.height,
                                   capabiliteis.maxImageExtent.height);
    }

    uint32_t formatsCount{};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device_->physical(), surface_, &formatsCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatsCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device_->physical(), surface_, &formatsCount,
                                         formats.data());

    VkFormat format{};
    for (uint32_t i = 0; i < formatsCount; i++) {
        if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format = formats[i].format;
            break;
        }

        if (i == formatsCount - 1) {
            exitLog("surface format requestd, but not available!");
        }
    }

    uint32_t modeCount{};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device_->physical(), surface_, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device_->physical(), surface_, &modeCount,
                                              modes.data());

    if (find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) == modes.end()) {
        exitLog("present mode requestd, but not available!");
    }

    uint32_t imageCount = capabiliteis.minImageCount + 1;
    if (capabiliteis.maxImageCount > 0 && imageCount > capabiliteis.maxImageCount) {
        imageCount = capabiliteis.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCI{};
    swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCI.surface = surface_;
    swapchainCI.minImageCount = imageCount;
    swapchainCI.imageFormat = format;
    swapchainCI.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainCI.imageExtent = extent;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCI.preTransform = capabiliteis.currentTransform;
    swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCI.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    swapchainCI.clipped = VK_TRUE;
    swapchainCI.oldSwapchain = oldSwapchain;

    VK_CHECK(vkCreateSwapchainKHR(device_->get(), &swapchainCI, nullptr, &swapchain_));

    if (oldSwapchain) {
        images_.clear();
        vkDestroySwapchainKHR(device_->get(), oldSwapchain, nullptr);
    }

    vkGetSwapchainImagesKHR(device_->get(), swapchain_, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(device_->get(), swapchain_, &imageCount, images.data());

    for (uint32_t i = 0; i < imageCount; i++) {
        images_.push_back(std::make_unique<Image2D>(device_));
        images_[i]->createView(images[i], format, extent.width, extent.height);
    }
}

const VkSwapchainKHR& Swapchain::get() const
{
    return swapchain_;
}

std::shared_ptr<Image2D> Swapchain::image(uint32_t index) const
{
    return images_[index];
}

uint32_t Swapchain::size() const
{
    return static_cast<uint32_t>(images_.size());
}

uint32_t Swapchain::width() const
{
    return images_[0]->width();
}

uint32_t Swapchain::height() const
{
    return images_[0]->height();
}

const VkFormat& Swapchain::format() const
{
    return images_[0]->format();
}

} // namespace guk