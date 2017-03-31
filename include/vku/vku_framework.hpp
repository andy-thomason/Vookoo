
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
    s.instance_ = vk::createInstanceUnique(vk::InstanceCreateInfo{
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
        (PFN_vkCreateDebugReportCallbackEXT)s.instance_->getProcAddr(
            "vkCreateDebugReportCallbackEXT");

    VkDebugReportCallbackEXT cb;
    vkCreateDebugReportCallbackEXT(
        *s.instance_, &(const VkDebugReportCallbackCreateInfoEXT &)ci,
        nullptr, &cb);
    s.callback_ = cb;

    auto pds = s.instance_->enumeratePhysicalDevices();
    s.physical_device_ = pds[0];
    auto qprops = s.physical_device_.getQueueFamilyProperties();
    s.graphicsQueueFamilyIndex_ = 0;
    s.computeQueueFamilyIndex_ = 0;
    for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
      auto &qprop = qprops[qi];
      if (qprop.queueFlags & vk::QueueFlagBits::eGraphics) {
        s.graphicsQueueFamilyIndex_ = qi;
      }
      if (qprop.queueFlags & vk::QueueFlagBits::eCompute) {
        s.computeQueueFamilyIndex_ = qi;
      }
    }

    s.memprops = s.physical_device_.getMemoryProperties();
    for (uint32_t i = 0; i != s.memprops.memoryTypeCount; ++i) {
      std::cout << "type" << i << " heap" << s.memprops.memoryTypes[i].heapIndex << " " << vk::to_string(s.memprops.memoryTypes[i].propertyFlags) << "\n";
    }
    for (uint32_t i = 0; i != s.memprops.memoryHeapCount; ++i) {
      std::cout << "heap" << vk::to_string(s.memprops.memoryHeaps[i].flags) << " " << s.memprops.memoryHeaps[i].size << "\n";
    }

    // todo: find optimal texture format
    // auto rgbaprops = s.physical_device_.getFormatProperties(vk::Format::eR8G8B8A8Unorm);

    std::vector<const char *> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    float queue_priorities[] = {0.0f};
    std::vector<vk::DeviceQueueCreateInfo> qci;

    qci.emplace_back(vk::DeviceQueueCreateFlags{}, s.graphicsQueueFamilyIndex_, 1,
                     queue_priorities);

    if (s.computeQueueFamilyIndex_ != s.graphicsQueueFamilyIndex_) {
      qci.emplace_back(vk::DeviceQueueCreateFlags{}, s.computeQueueFamilyIndex_, 1,
                       queue_priorities);
    };

    float graphicsQueue_priorities[] = {0.0f};
    s.device_ = s.physical_device_.createDeviceUnique(vk::DeviceCreateInfo{
        {}, (uint32_t)qci.size(), qci.data(),
        (uint32_t)layers.size(), layers.data(),
        (uint32_t)device_extensions.size(), device_extensions.data()});

    s.graphicsQueue_ = s.device_->getQueue(s.graphicsQueueFamilyIndex_, 0);
    s.computeQueue_ = s.device_->getQueue(s.computeQueueFamilyIndex_, 0);

    vk::PipelineCacheCreateInfo pipelineCacheInfo{};
    s.pipelineCache_ = s.device_->createPipelineCacheUnique(pipelineCacheInfo);

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
    s.descriptorPool_ = s.device_->createDescriptorPoolUnique(descriptorPoolInfo);

    s.ok_ = true;
  }

  /// Get the Vulkan instance.
  const vk::Instance instance() const { return *s.instance_; }

  /// Get the Vulkan device.
  const vk::Device device() const { return *s.device_; }

  /// Get the queue used to submit graphics jobs
  const vk::Queue graphicsQueue() const { return s.graphicsQueue_; }

  /// Get the queue used to submit compute jobs
  const vk::Queue computeQueue() const { return s.computeQueue_; }

  /// Get the physical device.
  const vk::PhysicalDevice &physicalDevice() const { return s.physical_device_; }

  /// Get the default pipeline cache (you can use your own if you like).
  const vk::PipelineCache pipelineCache() const { return *s.pipelineCache_; }

  /// Get the default descriptor pool (you can use your own if you like).
  const vk::DescriptorPool descriptorPool() const { return *s.descriptorPool_; }

  /// Get the family index for the graphics queues.
  uint32_t graphicsQueueFamilyIndex() const { return s.graphicsQueueFamilyIndex_; }

  /// Get the family index for the compute queues.
  uint32_t computeQueueFamilyIndex() const { return s.computeQueueFamilyIndex_; }

  const vk::PhysicalDeviceMemoryProperties &memprops() const { return s.memprops; }

  /// Clean up the framework satisfying the Vulkan verification layers.
  ~Framework() {
    if (s.device_) {
      s.device_->waitIdle();
      if (s.pipelineCache_) {
        s.pipelineCache_.reset();
      }
      if (s.descriptorPool_) {
        s.descriptorPool_.reset();
      }
      s.device_.reset();
    }

    if (s.instance_) {
      auto vkDestroyDebugReportCallbackEXT =
          (PFN_vkDestroyDebugReportCallbackEXT)s.instance_->getProcAddr(
              "vkDestroyDebugReportCallbackEXT");
      vkDestroyDebugReportCallbackEXT(*s.instance_, s.callback_, nullptr);
      s.instance_.reset();
    }
  }

  /// Returns true if the Framework has been built correctly.
  bool ok() const { return s.ok_; }

  /// Allows the framework to be moved to an aggregate object.
  /// eg. Framework f; f = Framework("fred");
  void operator=(Framework &&rhs) {
    s = std::move(rhs.s);
    rhs.s.ok_ = false;
  }

private:
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
      uint64_t object, size_t location, int32_t messageCode,
      const char *pLayerPrefix, const char *pMessage, void *pUserData) {
    printf("%08x debugCallback: %s\n", flags, pMessage);
    return VK_FALSE;
  }

  struct State {
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
    vk::PhysicalDeviceMemoryProperties memprops;
    bool ok_ = false;
  };

  State s;
};

/// This class wraps a window, a surface and a swap chain for that surface.
class Window {
public:
  Window() { s = {}; }

  Window(const vk::Instance &instance, const vk::Device &device, const vk::PhysicalDevice &physicalDevice, uint32_t graphicsQueueFamilyIndex, GLFWwindow *window) {
    s = {};
    s.device = device;

#ifdef VK_USE_PLATFORM_WIN32_KHR
    auto module = GetModuleHandle(nullptr);
    auto handle = glfwGetWin32Window(window);
    auto ci = vk::Win32SurfaceCreateInfoKHR{{}, module, handle};
    s.surface_ = instance.createWin32SurfaceKHRUnique(ci);
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
    auto display = glfwGetX11Display();
    auto x11window = glfwGetX11Window(window);
    auto ci = vk::XlibSurfaceCreateInfoKHR{{}, display, x11window};
    s.surface_ = instance.createXlibSurfaceKHRUnique(ci);
#endif

    s.presentQueueFamily_ = 0;
    auto &pd = physicalDevice;
    auto qprops = pd.getQueueFamilyProperties();
    for (uint32_t qi = 0; qi != qprops.size(); ++qi) {
      auto &qprop = qprops[qi];
      VkBool32 presentSupport = false;
      std::cout << vk::to_string(qprop.queueFlags) << " nq=" << qprop.queueCount << "\n";
      if (pd.getSurfaceSupportKHR(qi, *s.surface_)) {
        s.presentQueue_ = device.getQueue(qi, 0);
        s.presentQueueFamily_ = qi;
      }
    }

    if (!s.presentQueue_) {
      std::cout << "No Vulkan queues found\n";
      return;
    }

    auto fmts = pd.getSurfaceFormatsKHR(*s.surface_);
    s.swapchainImageFormat_ = fmts[0].format;
    s.swapchainColorSpace_ = fmts[0].colorSpace;
    if (fmts.size() == 1 && s.swapchainImageFormat_ == vk::Format::eUndefined) {
      s.swapchainImageFormat_ = vk::Format::eB8G8R8A8Unorm;
      s.swapchainColorSpace_ = vk::ColorSpaceKHR::eSrgbNonlinear;
    } else {
      for (auto &fmt : fmts) {
        std::cout << "format=" << vk::to_string(fmt.format)
                  << " colorSpace=" << vk::to_string(fmt.colorSpace) << "\n";
        if (fmt.format == vk::Format::eB8G8R8A8Unorm) {
          s.swapchainImageFormat_ = fmt.format;
          s.swapchainColorSpace_ = fmt.colorSpace;
        }
      }
    }

    auto surfaceCaps = pd.getSurfaceCapabilitiesKHR(*s.surface_);
    s.width_ = surfaceCaps.currentExtent.width;
    s.height_ = surfaceCaps.currentExtent.height;

    auto presentModes = pd.getSurfacePresentModesKHR(*s.surface_);
    vk::PresentModeKHR presentMode = presentModes[0];
    auto bestpm = vk::PresentModeKHR::eFifo;
    for (auto pm : presentModes) {
      std::cout << vk::to_string(pm) << "\n";
      if (pm == bestpm) presentMode = pm;
    }

    vk::SwapchainCreateInfoKHR swapinfo{};
    std::array<uint32_t, 2> queueFamilyIndices = { graphicsQueueFamilyIndex, s.presentQueueFamily_ };
    bool sameQueues = queueFamilyIndices[0] == queueFamilyIndices[1];
    vk::SharingMode sharingMode = !sameQueues ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive;
    swapinfo.imageExtent = surfaceCaps.currentExtent;
    swapinfo.surface = *s.surface_;
    swapinfo.minImageCount = surfaceCaps.minImageCount + 1;
    swapinfo.imageFormat = s.swapchainImageFormat_;
    swapinfo.imageColorSpace = s.swapchainColorSpace_;
    swapinfo.imageExtent = surfaceCaps.currentExtent;
    swapinfo.imageArrayLayers = 1;
    swapinfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapinfo.imageSharingMode = sharingMode;
    swapinfo.queueFamilyIndexCount = !sameQueues ? 2 : 0;
    swapinfo.pQueueFamilyIndices = queueFamilyIndices.data();
    swapinfo.preTransform = surfaceCaps.currentTransform;;
    swapinfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapinfo.presentMode = bestpm;
    swapinfo.clipped = 1;
    swapinfo.oldSwapchain = vk::SwapchainKHR{};
    s.swapchain_ = device.createSwapchainKHRUnique(swapinfo);

    s.images_ = device.getSwapchainImagesKHR(*s.swapchain_);
    for (auto &img : s.images_) {
      vk::ImageViewCreateInfo ci{};
      ci.image = img;
      ci.viewType = vk::ImageViewType::e2D;
      ci.format = s.swapchainImageFormat_;
      ci.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
      s.imageViews_.emplace_back(device.createImageView(ci));
    }

    // This subpass dependency handles the transition from ePresentSrcKHR to eColorAttachmentOptimal
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
    vk::AttachmentDescription attachmentDesc{};
    attachmentDesc.format = s.swapchainImageFormat_;
    attachmentDesc.samples = vk::SampleCountFlagBits::e1;
    attachmentDesc.loadOp = vk::AttachmentLoadOp::eClear;
    attachmentDesc.storeOp = vk::AttachmentStoreOp::eStore;
    attachmentDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachmentDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachmentDesc.initialLayout = vk::ImageLayout::eUndefined;
    attachmentDesc.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vku::RenderpassMaker rpm;
    rpm.beginSubpass(vk::PipelineBindPoint::eGraphics);
    rpm.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal);
    rpm.attachmentDescription(attachmentDesc);
    rpm.subpassDependency(dependency);
    s.renderPass_ = rpm.createUnique(device);

    for (int i = 0; i != s.imageViews_.size(); ++i) {
      vk::ImageView attachment{s.imageViews_[i]};
      vk::FramebufferCreateInfo fbci{{}, *s.renderPass_, 1, &attachment, s.width_, s.height_, 1 };
      s.framebuffers_.push_back(device.createFramebufferUnique(fbci));
    }

    vk::SemaphoreCreateInfo sci;
    s.imageAcquireSemaphore_ = device.createSemaphoreUnique(sci);
    s.commandCompleteSemaphore_ = device.createSemaphoreUnique(sci);

    vk::CommandPoolCreateInfo cpci{ {}, graphicsQueueFamilyIndex };
    s.commandPool_ = device.createCommandPoolUnique(cpci);

    vk::CommandBufferAllocateInfo cbai{ *s.commandPool_, vk::CommandBufferLevel::ePrimary, (uint32_t)s.framebuffers_.size() };
    s.commandBuffers_ = device.allocateCommandBuffersUnique(cbai);
    for (int i = 0; i != s.commandBuffers_.size(); ++i) {
      vk::FenceCreateInfo fci;
      fci.flags = vk::FenceCreateFlagBits::eSignaled;
      s.commandBufferFences_.emplace_back(device.createFence(fci));
    }

    s.ok_ = true;
  }

  void setRenderCommands(const std::function<void (vk::CommandBuffer cb, int imageIndex)> &func) {
    for (int i = 0; i != s.commandBuffers_.size(); ++i) {
      vk::CommandBuffer cb = *s.commandBuffers_[i];
      vk::CommandBufferBeginInfo bi{};
      cb.begin(bi);

      std::array<float, 4> clearColorValue = {0, 0, 1, 1};
      vk::ClearValue clearColor{clearColorValue};
      vk::RenderPassBeginInfo rpbi;
      rpbi.renderPass = *s.renderPass_;
      rpbi.framebuffer = *s.framebuffers_[i];
      rpbi.renderArea = vk::Rect2D{{0, 0}, {s.width_, s.height_}};
      rpbi.clearValueCount = 1;
      rpbi.pClearValues = &clearColor;
      cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
      func(cb, i); 
      cb.endRenderPass();

      cb.end();
    }
  }

  void draw(const vk::Device &device, const vk::Queue &graphicsQueue) const {
    static auto start = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::high_resolution_clock::now();
    auto delta = time - start;
    start = time;
    //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(delta).count() << "us frame time\n";

    auto umax = std::numeric_limits<uint64_t>::max();
    uint32_t imageIndex = 0;
    device.acquireNextImageKHR(*s.swapchain_, umax, *s.imageAcquireSemaphore_, vk::Fence(), &imageIndex);

    vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::Semaphore ccSema = *s.commandCompleteSemaphore_;
    vk::Semaphore iaSema = *s.imageAcquireSemaphore_;
    vk::CommandBuffer cb = *s.commandBuffers_[imageIndex];
    vk::SubmitInfo submit;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &iaSema;
    submit.pWaitDstStageMask = &waitStages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &ccSema;
    
    vk::Fence cbFence = s.commandBufferFences_[imageIndex];
    device.waitForFences(cbFence, 1, umax);
    device.resetFences(cbFence);
    graphicsQueue.submit(1, &submit, cbFence);


    vk::PresentInfoKHR presentInfo;
    vk::SwapchainKHR swapchain = *s.swapchain_;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &ccSema;
    s.presentQueue_.presentKHR(presentInfo);
  }

  uint32_t presentQueueFamily() const { return s.presentQueueFamily_; }
  bool ok() const { return s.ok_; }
  vk::RenderPass renderPass() const { return *s.renderPass_; }
  const std::vector<vk::UniqueFramebuffer> &framebuffers() const { return s.framebuffers_; }

  ~Window() {
    for (auto &iv : s.imageViews_) {
      s.device.destroyImageView(iv);
    }
    for (auto &f : s.commandBufferFences_) {
      s.device.destroyFence(f);
    }
  }

  uint32_t width() const { return s.width_; }
  uint32_t height() const { return s.height_; }
  vk::Format swapchainImageFormat() const { return s.swapchainImageFormat_; }
  vk::ColorSpaceKHR swapchainColorSpace() const { return s.swapchainColorSpace_; }
  const vk::SwapchainKHR swapchain() const { return *s.swapchain_; }
  const std::vector<vk::ImageView> &imageViews() const { return s.imageViews_; }
  const std::vector<vk::Image> &images() const { return s.images_; }
  const std::vector<vk::UniqueCommandBuffer> &commandBuffers() const { return s.commandBuffers_; }
  const std::vector<vk::Fence> &commandBufferFences() const { return s.commandBufferFences_; }
  vk::Semaphore imageAcquireSemaphore() const { return *s.imageAcquireSemaphore_; }
  vk::Semaphore presentSemaphore() const { return *s.commandCompleteSemaphore_; }
  vk::CommandPool commandPool() const { return *s.commandPool_; }

  void operator=(Window &&rhs) {
    s = std::move(rhs.s);
    rhs.s.ok_ = false;
  }

private:
  struct WindowState {
    vk::UniqueSurfaceKHR surface_;
    vk::Queue presentQueue_;
    vk::UniqueSwapchainKHR swapchain_;
    vk::UniqueRenderPass renderPass_;
    vk::UniqueSemaphore imageAcquireSemaphore_;
    vk::UniqueSemaphore commandCompleteSemaphore_;
    std::vector<vk::ImageView> imageViews_;
    std::vector<vk::Image> images_;
    std::vector<vk::Fence> commandBufferFences_;
    std::vector<vk::UniqueFramebuffer> framebuffers_;
    vk::UniqueCommandPool commandPool_;
    std::vector<vk::UniqueCommandBuffer> commandBuffers_;
    uint32_t presentQueueFamily_ = 0;
    uint32_t width_;
    uint32_t height_;
    vk::Format swapchainImageFormat_ = vk::Format::eB8G8R8A8Unorm;
    vk::ColorSpaceKHR swapchainColorSpace_ = vk::ColorSpaceKHR::eSrgbNonlinear;
    vk::Device device;
    bool ok_;
  };

  WindowState s;
};

} // namespace vku

#endif // VKU_FRAMEWORK_HPP
