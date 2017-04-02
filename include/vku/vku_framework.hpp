
////////////////////////////////////////////////////////////////////////////////
//
// Demo framework for the Vookoo for the Vookoo high level C++ Vulkan interface.
//
// (C) Andy Thomason 2017 MIT License
//
// This is an optional demo framework for the Vookoo high level C++ Vulkan interface.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef VKU_FRAMEWORK_HPP
#define VKU_FRAMEWORK_HPP

#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#define VKU_SURFACE "VK_KHR_win32_surface"
#pragma warning(disable : 4005)
#else
#define VK_USE_PLATFORM_XLIB_KHR
#define GLFW_EXPOSE_NATIVE_X11
#define VKU_SURFACE "VK_KHR_xlib_surface"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// Undo damage done by windows.h
#undef APIENTRY
#undef None
#undef max
#undef min

#include <array>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <thread>
#include <chrono>
#include <functional>
#include <cstddef>

#include <vulkan/vulkan.hpp>
#include <vku/vku.hpp>

namespace vku {

// This class provides an optional interface to the vulkan instance, devices and queues.
// It is not used by any of the other classes directly and so can be safely ignored if Vookoo
// is embedded in an engine.
class Framework {
public:
  Framework() {
  }

  // Construct a framework containing the instance, a device and one or more queues.
  Framework(const std::string &name) {
    std::vector<const char *> layers;
    layers.push_back("VK_LAYER_LUNARG_standard_validation");

    std::vector<const char *> instance_extensions;
    instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    instance_extensions.push_back(VKU_SURFACE);
    instance_extensions.push_back("VK_KHR_surface");

    auto appinfo = vk::ApplicationInfo{};
    instance_ = vk::createInstanceUnique(vk::InstanceCreateInfo{
        {}, &appinfo, (uint32_t)layers.size(),
        layers.data(), (uint32_t)instance_extensions.size(),
        instance_extensions.data()});

    auto ci = vk::DebugReportCallbackCreateInfoEXT{
        //vk::DebugReportFlagBitsEXT::eInformation |
            vk::DebugReportFlagBitsEXT::eWarning |
            vk::DebugReportFlagBitsEXT::ePerformanceWarning |
            vk::DebugReportFlagBitsEXT::eError,
            //vk::DebugReportFlagBitsEXT::eDebug,
        &debugCallback};
    const VkDebugReportCallbackCreateInfoEXT &cir = ci;

    auto vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)instance_->getProcAddr(
            "vkCreateDebugReportCallbackEXT");

    VkDebugReportCallbackEXT cb;
    vkCreateDebugReportCallbackEXT(
        *instance_, &(const VkDebugReportCallbackCreateInfoEXT &)ci,
        nullptr, &cb);
    callback_ = cb;

    auto pds = instance_->enumeratePhysicalDevices();
    physical_device_ = pds[0];
    auto qprops = physical_device_.getQueueFamilyProperties();
    graphicsQueueFamilyIndex_ = 0;
    computeQueueFamilyIndex_ = 0;
    for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
      auto &qprop = qprops[qi];
      if (qprop.queueFlags & vk::QueueFlagBits::eGraphics) {
        graphicsQueueFamilyIndex_ = qi;
      }
      if (qprop.queueFlags & vk::QueueFlagBits::eCompute) {
        computeQueueFamilyIndex_ = qi;
      }
    }

    memprops_ = physical_device_.getMemoryProperties();

    // todo: find optimal texture format
    // auto rgbaprops = physical_device_.getFormatProperties(vk::Format::eR8G8B8A8Unorm);

    std::vector<const char *> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    float queue_priorities[] = {0.0f};
    std::vector<vk::DeviceQueueCreateInfo> qci;

    qci.emplace_back(vk::DeviceQueueCreateFlags{}, graphicsQueueFamilyIndex_, 1,
                     queue_priorities);

    if (computeQueueFamilyIndex_ != graphicsQueueFamilyIndex_) {
      qci.emplace_back(vk::DeviceQueueCreateFlags{}, computeQueueFamilyIndex_, 1,
                       queue_priorities);
    };

    float graphicsQueue_priorities[] = {0.0f};
    device_ = physical_device_.createDeviceUnique(vk::DeviceCreateInfo{
        {}, (uint32_t)qci.size(), qci.data(),
        (uint32_t)layers.size(), layers.data(),
        (uint32_t)device_extensions.size(), device_extensions.data()});

    graphicsQueue_ = device_->getQueue(graphicsQueueFamilyIndex_, 0);
    computeQueue_ = device_->getQueue(computeQueueFamilyIndex_, 0);

    vk::PipelineCacheCreateInfo pipelineCacheInfo{};
    pipelineCache_ = device_->createPipelineCacheUnique(pipelineCacheInfo);

    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.emplace_back(vk::DescriptorType::eUniformBuffer, 128);
    poolSizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, 128);

    // Create an arbitrary number of descriptors in a pool.
    // Allow the descriptors to be freed, possibly not optimal behaviour.
    vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descriptorPoolInfo.maxSets = 256;
    descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPool_ = device_->createDescriptorPoolUnique(descriptorPoolInfo);

    ok_ = true;
  }

  void dumpCaps(std::ostream &os) const {
    os << "Memory Types\n";
    for (uint32_t i = 0; i != memprops_.memoryTypeCount; ++i) {
      os << "  type" << i << " heap" << memprops_.memoryTypes[i].heapIndex << " " << vk::to_string(memprops_.memoryTypes[i].propertyFlags) << "\n";
    }
    os << "Heaps\n";
    for (uint32_t i = 0; i != memprops_.memoryHeapCount; ++i) {
      os << "  heap" << vk::to_string(memprops_.memoryHeaps[i].flags) << " " << memprops_.memoryHeaps[i].size << "\n";
    }
  }

  /// Get the Vulkan instance.
  const vk::Instance instance() const { return *instance_; }

  /// Get the Vulkan device.
  const vk::Device device() const { return *device_; }

  /// Get the queue used to submit graphics jobs
  const vk::Queue graphicsQueue() const { return graphicsQueue_; }

  /// Get the queue used to submit compute jobs
  const vk::Queue computeQueue() const { return computeQueue_; }

  /// Get the physical device.
  const vk::PhysicalDevice &physicalDevice() const { return physical_device_; }

  /// Get the default pipeline cache (you can use your own if you like).
  const vk::PipelineCache pipelineCache() const { return *pipelineCache_; }

  /// Get the default descriptor pool (you can use your own if you like).
  const vk::DescriptorPool descriptorPool() const { return *descriptorPool_; }

  /// Get the family index for the graphics queues.
  uint32_t graphicsQueueFamilyIndex() const { return graphicsQueueFamilyIndex_; }

  /// Get the family index for the compute queues.
  uint32_t computeQueueFamilyIndex() const { return computeQueueFamilyIndex_; }

  const vk::PhysicalDeviceMemoryProperties &memprops() const { return memprops_; }

  /// Clean up the framework satisfying the Vulkan verification layers.
  ~Framework() {
    if (device_) {
      device_->waitIdle();
      if (pipelineCache_) {
        pipelineCache_.reset();
      }
      if (descriptorPool_) {
        descriptorPool_.reset();
      }
      device_.reset();
    }

    if (instance_) {
      auto vkDestroyDebugReportCallbackEXT =
          (PFN_vkDestroyDebugReportCallbackEXT)instance_->getProcAddr(
              "vkDestroyDebugReportCallbackEXT");
      vkDestroyDebugReportCallbackEXT(*instance_, callback_, nullptr);
      instance_.reset();
    }
  }

  /// Returns true if the Framework has been built correctly.
  bool ok() const { return ok_; }

private:
  // Report any errors or warnings.
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
      uint64_t object, size_t location, int32_t messageCode,
      const char *pLayerPrefix, const char *pMessage, void *pUserData) {
    printf("%08x debugCallback: %s\n", flags, pMessage);
    return VK_FALSE;
  }

  vk::UniqueInstance instance_;
  vk::UniqueDevice device_;
  vk::Queue graphicsQueue_;
  vk::Queue computeQueue_;
  vk::DebugReportCallbackEXT callback_;
  vk::PhysicalDevice physical_device_;
  vk::UniquePipelineCache pipelineCache_;
  vk::UniqueDescriptorPool descriptorPool_;
  uint32_t graphicsQueueFamilyIndex_;
  uint32_t computeQueueFamilyIndex_;
  vk::PhysicalDeviceMemoryProperties memprops_;
  bool ok_ = false;
};

/// This class wraps a window, a surface and a swap chain for that surface.
class Window {
public:
  Window() {
  }

  Window(const vk::Instance &instance, const vk::Device &device, const vk::PhysicalDevice &physicalDevice, uint32_t graphicsQueueFamilyIndex, GLFWwindow *window) {
    device_ = device;

#ifdef VK_USE_PLATFORM_WIN32_KHR
    auto module = GetModuleHandle(nullptr);
    auto handle = glfwGetWin32Window(window);
    auto ci = vk::Win32SurfaceCreateInfoKHR{{}, module, handle};
    surface_ = instance.createWin32SurfaceKHRUnique(ci);
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
    auto display = glfwGetX11Display();
    auto x11window = glfwGetX11Window(window);
    auto ci = vk::XlibSurfaceCreateInfoKHR{{}, display, x11window};
    surface_ = instance.createXlibSurfaceKHRUnique(ci);
#endif

    presentQueueFamily_ = 0;
    auto &pd = physicalDevice;
    auto qprops = pd.getQueueFamilyProperties();
    for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
      auto &qprop = qprops[qi];
      VkBool32 presentSupport = false;
      if (pd.getSurfaceSupportKHR(qi, *surface_)) {
        presentQueue_ = device.getQueue(qi, 0);
        presentQueueFamily_ = qi;
      }
    }

    if (!presentQueue_) {
      std::cout << "No Vulkan queues found\n";
      return;
    }

    auto fmts = pd.getSurfaceFormatsKHR(*surface_);
    swapchainImageFormat_ = fmts[0].format;
    swapchainColorSpace_ = fmts[0].colorSpace;
    if (fmts.size() == 1 && swapchainImageFormat_ == vk::Format::eUndefined) {
      swapchainImageFormat_ = vk::Format::eB8G8R8A8Unorm;
      swapchainColorSpace_ = vk::ColorSpaceKHR::eSrgbNonlinear;
    } else {
      for (auto &fmt : fmts) {
        if (fmt.format == vk::Format::eB8G8R8A8Unorm) {
          swapchainImageFormat_ = fmt.format;
          swapchainColorSpace_ = fmt.colorSpace;
        }
      }
    }

    auto surfaceCaps = pd.getSurfaceCapabilitiesKHR(*surface_);
    width_ = surfaceCaps.currentExtent.width;
    height_ = surfaceCaps.currentExtent.height;

    auto presentModes = pd.getSurfacePresentModesKHR(*surface_);
    vk::PresentModeKHR presentMode = presentModes[0];
    auto bestpm = vk::PresentModeKHR::eMailbox;
    for (auto pm : presentModes) {
      if (pm == bestpm) presentMode = pm;
    }
    std::cout << "using " << vk::to_string(presentMode) << "\n";

    vk::SwapchainCreateInfoKHR swapinfo{};
    std::array<uint32_t, 2> queueFamilyIndices = { graphicsQueueFamilyIndex, presentQueueFamily_ };
    bool sameQueues = queueFamilyIndices[0] == queueFamilyIndices[1];
    vk::SharingMode sharingMode = !sameQueues ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive;
    swapinfo.imageExtent = surfaceCaps.currentExtent;
    swapinfo.surface = *surface_;
    swapinfo.minImageCount = surfaceCaps.minImageCount + 1;
    swapinfo.imageFormat = swapchainImageFormat_;
    swapinfo.imageColorSpace = swapchainColorSpace_;
    swapinfo.imageExtent = surfaceCaps.currentExtent;
    swapinfo.imageArrayLayers = 1;
    swapinfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapinfo.imageSharingMode = sharingMode;
    swapinfo.queueFamilyIndexCount = !sameQueues ? 2 : 0;
    swapinfo.pQueueFamilyIndices = queueFamilyIndices.data();
    swapinfo.preTransform = surfaceCaps.currentTransform;;
    swapinfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapinfo.presentMode = presentMode;
    swapinfo.clipped = 1;
    swapinfo.oldSwapchain = vk::SwapchainKHR{};
    swapchain_ = device.createSwapchainKHRUnique(swapinfo);

    images_ = device.getSwapchainImagesKHR(*swapchain_);
    for (auto &img : images_) {
      vk::ImageViewCreateInfo ci{};
      ci.image = img;
      ci.viewType = vk::ImageViewType::e2D;
      ci.format = swapchainImageFormat_;
      ci.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
      imageViews_.emplace_back(device.createImageView(ci));
    }

    auto memprops = physicalDevice.getMemoryProperties();
    depthStencilImage_ = vku::DepthStencilImage(device, memprops, width_, height_);

    // This subpass dependency handles the transition from ePresentSrcKHR to eUndefined
    // at the start of rendering.
    vk::SubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = {};
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite;

    // At the start, the buffer is in eUndefined layout.
    // We will clear the buffer at the start (eClear)
    // We will write the result out at the end (eStore)
    // After this renderpass, we will switch to ePresentSrcKHR ready for the swap.
    vk::AttachmentDescription colourDesc{};
    colourDesc.format = swapchainImageFormat_;
    colourDesc.samples = vk::SampleCountFlagBits::e1;
    colourDesc.loadOp = vk::AttachmentLoadOp::eClear;
    colourDesc.storeOp = vk::AttachmentStoreOp::eStore;
    colourDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colourDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colourDesc.initialLayout = vk::ImageLayout::eUndefined;
    colourDesc.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    // At the start, the buffer is in eUndefined layout.
    // We will clear the buffer at the start (eClear)
    vk::AttachmentDescription depthDesc{};
    depthDesc.format = depthStencilImage_.format();
    depthDesc.samples = vk::SampleCountFlagBits::e1;
    depthDesc.loadOp = vk::AttachmentLoadOp::eClear;
    depthDesc.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depthDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depthDesc.initialLayout = vk::ImageLayout::eUndefined;
    depthDesc.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vku::RenderpassMaker rpm;
    rpm.attachmentDescription(colourDesc);
    rpm.attachmentDescription(depthDesc);
    rpm.beginSubpass(vk::PipelineBindPoint::eGraphics);
    rpm.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);
    rpm.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);
    rpm.subpassDependency(dependency);
    renderPass_ = rpm.createUnique(device);

    for (int i = 0; i != imageViews_.size(); ++i) {
      vk::ImageView attachments[2] = {imageViews_[i], depthStencilImage_.imageView()};
      vk::FramebufferCreateInfo fbci{{}, *renderPass_, 2, attachments, width_, height_, 1 };
      framebuffers_.push_back(device.createFramebufferUnique(fbci));
    }

    vk::SemaphoreCreateInfo sci;
    imageAcquireSemaphore_ = device.createSemaphoreUnique(sci);
    commandCompleteSemaphore_ = device.createSemaphoreUnique(sci);
    presubmitSemaphore_ = device.createSemaphoreUnique(sci);

    typedef vk::CommandPoolCreateFlagBits ccbits;

    vk::CommandPoolCreateInfo cpci{ ccbits::eTransient|ccbits::eResetCommandBuffer, graphicsQueueFamilyIndex };
    commandPool_ = device.createCommandPoolUnique(cpci);

    vk::CommandBufferAllocateInfo cbai{ *commandPool_, vk::CommandBufferLevel::ePrimary, (uint32_t)framebuffers_.size() };
    drawBuffers_ = device.allocateCommandBuffersUnique(cbai);
    presubmitBuffers_ = device.allocateCommandBuffersUnique(cbai);
    for (int i = 0; i != drawBuffers_.size(); ++i) {
      vk::FenceCreateInfo fci;
      fci.flags = vk::FenceCreateFlagBits::eSignaled;
      commandBufferFences_.emplace_back(device.createFence(fci));
    }

    ok_ = true;
  }

  void dumpCaps(std::ostream &os, vk::PhysicalDevice pd) const {
    os << "Surface formats\n";
    auto fmts = pd.getSurfaceFormatsKHR(*surface_);
    for (auto &fmt : fmts) {
      auto fmtstr = vk::to_string(fmt.format);
      auto cstr = vk::to_string(fmt.colorSpace);
      os << "format=" << fmtstr << " colorSpace=" << cstr << "\n";
    }

    os << "Present Modes\n";
    auto presentModes = pd.getSurfacePresentModesKHR(*surface_);
    for (auto pm : presentModes) {
      std::cout << vk::to_string(pm) << "\n";
    }
  }

  void setRenderCommands(const std::function<void (vk::CommandBuffer cb, int imageIndex)> &func) {
    for (int i = 0; i != drawBuffers_.size(); ++i) {
      vk::CommandBuffer cb = *drawBuffers_[i];
      vk::CommandBufferBeginInfo bi{};
      cb.begin(bi);

      std::array<float, 4> clearColorValue{0, 0, 1, 1};
      vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
      std::array<vk::ClearValue, 2> clearColours{vk::ClearValue{clearColorValue}, clearDepthValue};
      vk::RenderPassBeginInfo rpbi;
      rpbi.renderPass = *renderPass_;
      rpbi.framebuffer = *framebuffers_[i];
      rpbi.renderArea = vk::Rect2D{{0, 0}, {width_, height_}};
      rpbi.clearValueCount = (uint32_t)clearColours.size();
      rpbi.pClearValues = clearColours.data();
      cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
      func(cb, i); 
      cb.endRenderPass();

      cb.end();
    }
  }

  typedef void (preSubmitFunc_t)(vk::CommandBuffer cb, int imageIndex);

  void draw(const vk::Device &device, const vk::Queue &graphicsQueue, const std::function<void (vk::CommandBuffer pscb, int imageIndex)> &preSubmit) {
    static auto start = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::high_resolution_clock::now();
    auto delta = time - start;
    start = time;
    //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(delta).count() << "us frame time\n";

    auto umax = std::numeric_limits<uint64_t>::max();
    uint32_t imageIndex = 0;
    device.acquireNextImageKHR(*swapchain_, umax, *imageAcquireSemaphore_, vk::Fence(), &imageIndex);

    vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::Semaphore ccSema = *commandCompleteSemaphore_;
    vk::Semaphore iaSema = *imageAcquireSemaphore_;
    vk::Semaphore psSema = *presubmitSemaphore_;
    vk::CommandBuffer cb = *drawBuffers_[imageIndex];
    vk::CommandBuffer pscb = *presubmitBuffers_[imageIndex];

    vk::Fence cbFence = commandBufferFences_[imageIndex];
    device.waitForFences(cbFence, 1, umax);
    device.resetFences(cbFence);

    preSubmit(pscb, imageIndex);

    vk::SubmitInfo submit;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &iaSema;
    submit.pWaitDstStageMask = &waitStages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &pscb;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &psSema;
    graphicsQueue.submit(1, &submit, vk::Fence{});

    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &psSema;
    submit.pWaitDstStageMask = &waitStages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &ccSema;
    graphicsQueue.submit(1, &submit, cbFence);

    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR swapchain = *swapchain_;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &ccSema;
    presentQueue_.presentKHR(presentInfo);
  }

  uint32_t presentQueueFamily() const { return presentQueueFamily_; }
  bool ok() const { return ok_; }
  vk::RenderPass renderPass() const { return *renderPass_; }
  const std::vector<vk::UniqueFramebuffer> &framebuffers() const { return framebuffers_; }

  ~Window() {
    for (auto &iv : imageViews_) {
      device_.destroyImageView(iv);
    }
    for (auto &f : commandBufferFences_) {
      device_.destroyFence(f);
    }
  }

  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }
  vk::Format swapchainImageFormat() const { return swapchainImageFormat_; }
  vk::ColorSpaceKHR swapchainColorSpace() const { return swapchainColorSpace_; }
  const vk::SwapchainKHR swapchain() const { return *swapchain_; }
  const std::vector<vk::ImageView> &imageViews() const { return imageViews_; }
  const std::vector<vk::Image> &images() const { return images_; }
  const std::vector<vk::UniqueCommandBuffer> &commandBuffers() const { return drawBuffers_; }
  const std::vector<vk::Fence> &commandBufferFences() const { return commandBufferFences_; }
  vk::Semaphore imageAcquireSemaphore() const { return *imageAcquireSemaphore_; }
  vk::Semaphore presentSemaphore() const { return *commandCompleteSemaphore_; }
  vk::CommandPool commandPool() const { return *commandPool_; }
  int numImageIndices() const { return (int)images_.size(); }

private:
  vk::UniqueSurfaceKHR surface_;
  vk::Queue presentQueue_;
  vk::UniqueSwapchainKHR swapchain_;
  vk::UniqueRenderPass renderPass_;
  vk::UniqueSemaphore imageAcquireSemaphore_;
  vk::UniqueSemaphore commandCompleteSemaphore_;
  vk::UniqueSemaphore presubmitSemaphore_;
  vku::DepthStencilImage depthStencilImage_;
  std::vector<vk::ImageView> imageViews_;
  std::vector<vk::Image> images_;
  std::vector<vk::Fence> commandBufferFences_;
  std::vector<vk::UniqueFramebuffer> framebuffers_;
  vk::UniqueCommandPool commandPool_;
  std::vector<vk::UniqueCommandBuffer> drawBuffers_;
  std::vector<vk::UniqueCommandBuffer> presubmitBuffers_;
  uint32_t presentQueueFamily_ = 0;
  uint32_t width_;
  uint32_t height_;
  vk::Format swapchainImageFormat_ = vk::Format::eB8G8R8A8Unorm;
  vk::ColorSpaceKHR swapchainColorSpace_ = vk::ColorSpaceKHR::eSrgbNonlinear;
  vk::Device device_;
  bool ok_;
};

} // namespace vku

#endif // VKU_FRAMEWORK_HPP
