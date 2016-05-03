////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_SWAP_CHAIN_INCLUDED
#define VKU_SWAP_CHAIN_INCLUDED

#include <vku/resource.hpp>
#include <vku/instance.hpp>

namespace vku {

class swapChain : public resource<VkSwapchainKHR, swapChain> {
public:
  /// semaphore that does not own its pointer
  swapChain(VkSwapchainKHR value = VK_NULL_HANDLE, VkDevice dev = VK_NULL_HANDLE) : resource(value, dev) {
  }

  /// semaphore that does owns (and creates) its pointer
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
    }
    else {
      preTransform = surfCaps.currentTransform;
    }

    VkSwapchainCreateInfoKHR swapchainCI = {};
    swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCI.pNext = NULL;
    swapchainCI.surface = surface;
    swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
    swapchainCI.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    swapchainCI.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
    swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.queueFamilyIndexCount = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCI.queueFamilyIndexCount = 0;
    swapchainCI.pQueueFamilyIndices = NULL;
    swapchainCI.presentMode = swapchainPresentMode;
    swapchainCI.oldSwapchain = oldSwapchain;
    swapchainCI.clipped = true;
    swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainKHR res;
    err = vkCreateSwapchainKHR(dev, &swapchainCI, VK_NULL_HANDLE, &res);
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
    presentInfo.pNext = NULL;
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
    renderPass_ = layout.create(dev());

    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = depthStencilView;

    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = NULL;
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
      VkResult err = vkCreateFramebuffer(dev(), &frameBufferCreateInfo, VK_NULL_HANDLE, &frameBuffers_[i]);
    }
  }

  VkFramebuffer frameBuffer(size_t i) const { return frameBuffers_[i]; }
  VkRenderPass renderPass() const { return renderPass_; }

  void destroy() {
    vkDestroySwapchainKHR(dev(), get(), VK_NULL_HANDLE);
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

class buffer {
public:
  buffer(VkDevice dev = VK_NULL_HANDLE, VkBuffer buf = VK_NULL_HANDLE) : buf_(buf), dev(dev) {
  }

  buffer(device dev, void *init, VkDeviceSize size, VkBufferUsageFlags usage) : dev(dev), size_(size) {
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = usage;
    VkResult err = vkCreateBuffer(dev, &bufInfo, VK_NULL_HANDLE, &buf_);
    if (err) throw error(err, __FILE__, __LINE__);

    ownsBuffer = true;

    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    vkGetBufferMemoryRequirements(dev, buf_, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = dev.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

     err = vkAllocateMemory(dev, &memAlloc, VK_NULL_HANDLE, &mem);
    if (err) throw error(err, __FILE__, __LINE__);

    if (init) {
      void *dest = map();
      std::memcpy(dest, init, size);
      unmap();
    }
    bind();
  }

  buffer(VkBufferCreateInfo bufInfo, VkDevice dev = VK_NULL_HANDLE) : dev(dev) {
    vkCreateBuffer(dev, &bufInfo, VK_NULL_HANDLE, &buf_);
  }

  // RAII move operator
  buffer &operator=(buffer &&rhs) {
    dev = rhs.dev;
    buf_ = rhs.buf_;
    mem = rhs.mem;
    size_ = rhs.size_;
    rhs.dev = VK_NULL_HANDLE;
    rhs.mem = VK_NULL_HANDLE;
    rhs.buf_ = VK_NULL_HANDLE;

    rhs.ownsBuffer = false;
    return *this;
  }

  ~buffer() {
    if (buf_ && ownsBuffer) {
      vkDestroyBuffer(dev, buf_, VK_NULL_HANDLE);
      buf_ = VK_NULL_HANDLE;
    }
  }

  void *map() {
    void *dest = VK_NULL_HANDLE;
    VkResult err = vkMapMemory(dev, mem, 0, size(), 0, &dest);
    if (err) throw error(err, __FILE__, __LINE__);
    return dest;
  }

  void unmap() {
    vkUnmapMemory(dev, mem);
  }

  void bind() {
    VkResult err = vkBindBufferMemory(dev, buf_, mem, 0);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  size_t size() const {
    return size_;
  }

  //operator VkBuffer() const { return buf_; }

  VkBuffer buf() const { return buf_; }

  VkDescriptorBufferInfo desc() const {
    VkDescriptorBufferInfo d = {};
    d.buffer = buf_;
    d.range = size_;
    return d;
  }

private:
  VkBuffer buf_ = VK_NULL_HANDLE;
  VkDevice dev = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  size_t size_;
  bool ownsBuffer = false;
};


} // vku

#endif
