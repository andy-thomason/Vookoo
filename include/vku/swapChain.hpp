////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016, 2017
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_SWAP_CHAIN_INCLUDED
#define VKU_SWAP_CHAIN_INCLUDED

#include <vku/resource.hpp>
#include <vku/instance.hpp>
#include <vku/renderPassLayout.hpp>

namespace vku {

class swapChain : public resource<VkSwapchainKHR, swapChain> {
public:
  /// swap chain that does not own its pointer
  swapChain(VkSwapchainKHR value = VK_NULL_HANDLE, VkDevice dev = VK_NULL_HANDLE) : resource(value, dev) {
  }

  /// swap chain that does own (and creates) its pointer
  swapChain(const vku::device dev, uint32_t width, uint32_t height, VkSurfaceKHR surface, VkCommandBuffer buf) : resource(dev) {
    VkResult err;
    VkSwapchainKHR oldSwapchain = *this;

    // Get physical device surface properties and formats
    VkSurfaceCapabilitiesKHR surfCaps;
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev.physicalDevice(), surface, &surfCaps);
    if (err) throw error(err, __FILE__, __LINE__);

    uint32_t presentModeCount;
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(dev.physicalDevice(), surface, &presentModeCount, NULL);
    if (err) throw error(err, __FILE__, __LINE__);

    // todo : replace with vector?
    VkPresentModeKHR *presentModes = (VkPresentModeKHR *)malloc(presentModeCount * sizeof(VkPresentModeKHR));

    err = vkGetPhysicalDeviceSurfacePresentModesKHR(dev.physicalDevice(), surface, &presentModeCount, presentModes);
    if (err) throw error(err, __FILE__, __LINE__);

    VkExtent2D swapchainExtent = {};
    // width and height are either both -1, or both not -1.
    if (surfCaps.currentExtent.width == -1)
    {
      // If the surface size is undefined, the size is set to
      // the size of the images requested.
      width_ = swapchainExtent.width = width;
      height_ = swapchainExtent.height = height;
    }
    else
    {
      // If the surface size is defined, the swap chain size must match
      swapchainExtent = surfCaps.currentExtent;
      width_ = surfCaps.currentExtent.width;
      height_ = surfCaps.currentExtent.height;
    }

    // Try to use mailbox mode
    // Low latency and non-tearing
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (size_t i = 0; i < presentModeCount; i++) 
    {
      if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) 
      {
        swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        break;
      }
      if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)) 
      {
        swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
      }
    }

    // Determine the number of images
    uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
    if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
    {
      desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
    }

    VkSurfaceTransformFlagsKHR preTransform;
    if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
      preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
      preTransform = surfCaps.currentTransform;
    }

    VkSwapchainCreateInfoKHR swapchainCI = {};
    swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCI.surface = surface;
    swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
    swapchainCI.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    swapchainCI.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
    swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.queueFamilyIndexCount = 0;
    swapchainCI.pQueueFamilyIndices = nullptr;
    swapchainCI.presentMode = swapchainPresentMode;
    swapchainCI.oldSwapchain = oldSwapchain;
    swapchainCI.clipped = true;
    swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainKHR res;
    err = vkCreateSwapchainKHR(dev, &swapchainCI, nullptr, &res);
    if (err) throw error(err, __FILE__, __LINE__);
    set(res, true);

    surface_ = surface;

    build_images(buf);
  }

  void build_images(VkCommandBuffer buf);

  swapChain &operator=(swapChain &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    width_ = rhs.width_;
    height_ = rhs.height_;
    swapchainImages = std::move(rhs.swapchainImages);
    swapchainViews = std::move(rhs.swapchainViews);
    return *this;
  }

  void present(VkQueue queue, uint32_t currentBuffer)
  {
    VkPresentInfoKHR presentInfo = {};
    VkSwapchainKHR sc = *this;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &sc;
    presentInfo.pImageIndices = &currentBuffer;
    VkResult err = vkQueuePresentKHR(queue, &presentInfo);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }

  size_t imageCount() const { return swapchainImages.size(); }
  VkImage image(size_t i) const { return swapchainImages[i]; }
  VkImageView view(size_t i) const { return swapchainViews[i]; }

  uint32_t acquireNextImage(VkSemaphore presentCompleteSemaphore) const {
    uint32_t currentBuffer = 0;
    VkResult err = vkAcquireNextImageKHR(dev(), get(), UINT64_MAX, presentCompleteSemaphore, (VkFence)VK_NULL_HANDLE, &currentBuffer);
    if (err) throw error(err, __FILE__, __LINE__);
    return currentBuffer;
  }


  void setupFrameBuffer(VkImageView depthStencilView, VkFormat depthFormat) {
    VkImageView attachments[2];

    depthFormat_ = depthFormat;

    vku::renderPassLayout layout;
    uint32_t color = layout.addAttachment(colorFormat_);
    uint32_t depth = layout.addAttachment(depthFormat);
    layout.addSubpass(color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    renderPass_ = layout.createRenderPass(dev());

    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = depthStencilView;

    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.renderPass = renderPass_;
    frameBufferCreateInfo.attachmentCount = 2;
    frameBufferCreateInfo.pAttachments = attachments;
    frameBufferCreateInfo.width = width_;
    frameBufferCreateInfo.height = height_;
    frameBufferCreateInfo.layers = 1;

    // Create frame buffers for every swap chain image
    frameBuffers_.resize(swapchainViews.size());
    for (uint32_t i = 0; i < frameBuffers_.size(); i++)
    {
      attachments[0] = swapchainViews[i];
      VkResult err = vkCreateFramebuffer(dev(), &frameBufferCreateInfo, nullptr, &frameBuffers_[i]);
    }
  }

  VkFramebuffer frameBuffer(size_t i) const { return frameBuffers_[i]; }
  VkRenderPass renderPass() const { return renderPass_; }

  void destroy() {
    vkDestroySwapchainKHR(dev(), get(), nullptr);
  }

private:
  uint32_t width_;
  uint32_t height_;

  VkFormat colorFormat_ = VK_FORMAT_B8G8R8A8_UNORM;;
  VkFormat depthFormat_ = VK_FORMAT_B8G8R8A8_UNORM;;
  VkColorSpaceKHR colorSpace_ = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  uint32_t queueNodeIndex_ = UINT32_MAX;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkRenderPass renderPass_;

  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainViews;
  std::vector<VkFramebuffer> frameBuffers_;
};


} // vku

#endif
