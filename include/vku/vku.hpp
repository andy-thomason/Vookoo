////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: main include file
// 

#ifndef VKU_INCLUDED
#define VKU_INCLUDED


#ifdef _WIN32
  #define VK_USE_PLATFORM_WIN32_KHR 1
  #pragma comment(lib, "vulkan-1.lib")
  #define _CRT_SECURE_NO_WARNINGS
#else
  #define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vk_platform.h>

#include <cstring>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <algorithm>
#include <numeric>

// derived from https://github.com/SaschaWillems/Vulkan
//
// Many thanks to Sascha, without who this would be a challenge!

namespace vku {

  inline float deg_to_rad(float deg) { return deg * (3.1415927f / 180); }

  template <class WindowHandle, class Window> Window *map_window(WindowHandle handle, Window *win) {
    static std::unordered_map<WindowHandle, Window *> map;
    auto iter = map.find(handle);
    if (iter == map.end()) {
      if (win != nullptr) map[handle] = win;
      return win;
    } else {
      return iter->second;
    }
  };

  #ifdef VK_USE_PLATFORM_WIN32_KHR
    inline static HINSTANCE connection() { return GetModuleHandle(NULL); }
  #else
    inline static xcb_connection_t *connection() {
      static xcb_connection_t *connection = nullptr;
      if (connection == nullptr) {

        const xcb_setup_t *setup;
        xcb_screen_iterator_t iter;
        int scr = 10;

        setup = xcb_get_setup(connection);
        iter = xcb_setup_roots_iterator(setup);
        while (scr-- > 0) {
          xcb_screen_next(&iter);
          connection = xcb_connect(NULL, &scr);
          if (connection == NULL) {
            printf("Could not find a compatible Vulkan ICD!\n");
            fflush(stdout);
            exit(1);
          }
        }
      }
      return connection;
    }
  #endif

  #ifdef VK_USE_PLATFORM_WIN32_KHR
    template <class Window> static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      Window *win = map_window<HWND, window>(hWnd, nullptr);
      //printf("%04x %p %p\n", uMsg, hWnd, win);
      if (win) win->handleMessages(hWnd, uMsg, wParam, lParam);
      return (DefWindowProc(hWnd, uMsg, wParam, lParam));
    }
  #endif

class error : public std::runtime_error {
  const char *error_name(VkResult err) {
    switch (err) {
      case VK_SUCCESS: return "VK_SUCCESS";
      case VK_NOT_READY: return "VK_NOT_READY";
      case VK_TIMEOUT: return "VK_TIMEOUT";
      case VK_EVENT_SET: return "VK_EVENT_SET";
      case VK_EVENT_RESET: return "VK_EVENT_RESET";
      case VK_INCOMPLETE: return "VK_INCOMPLETE";
      case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
      case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
      case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
      case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
      case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
      case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
      case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
      case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
      case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
      case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
      case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
      case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
      case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
      case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
      case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
      case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
      case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
      default: return "UNKNOWN ERROR";
    }
  }

  const char *what(VkResult err, const char *file, int line) {
    snprintf(text_, sizeof(text_), "error: %s at %s:%d", error_name(err), file, line);
    return text_;
  }

  char text_[256];
public:
  error(VkResult err, const char *file, int line) : std::runtime_error(what(err, file, line)) {
  }
};

/// This resource class is the base class of most of the vku wrappers.
/// It wraps a single Vulkan interface plus related interfaces.
/// The primary Vulkan interface is castable to a Vulkan handle of various kinds.
///
/// Exactly one resource object "owns" the resource. I considered using reference counting,
/// but the overhead is pretty rotten, I may return to this later.
/// A downside of this is that you cannot put these objects in vectors.
///
template <class VulkanType, class ParentClass>
class resource {
public:
  resource() : value_(VK_NULL_HANDLE), ownsResource(false), dev_(VK_NULL_HANDLE) {
  }

  resource(VulkanType value, VkDevice dev = VK_NULL_HANDLE) : value_(value), ownsResource(false), dev_(dev) {
  }

  resource(VkDevice dev) : dev_(dev), ownsResource(false) {
  }

  // every resource is moveable transfering ownership of the
  // object to the copy. The former owner loses ownership.
  void operator=(resource &&rhs) {
    clear();
    value_ = rhs.value_;
    dev_ = rhs.dev_;
    ownsResource = rhs.ownsResource;
    rhs.value_ = VK_NULL_HANDLE;
    rhs.ownsResource = false;
  }

  ~resource() {
    clear();
  }

  operator VulkanType() const {
    return value_;
  }

  VulkanType get() const { return value_; }
  VkDevice dev() const { return dev_; }
  resource &set(VulkanType value, bool owns) { value_ = value; ownsResource = owns; return *this; }

  void clear() {
    if (value_ && ownsResource) ((ParentClass*)this)->destroy();
    value_ = VK_NULL_HANDLE; ownsResource = false;
  }
private:
  // resources have a value, a device and an ownership flag.
  VulkanType value_ = VK_NULL_HANDLE;
  VkDevice dev_ = VK_NULL_HANDLE;
  bool ownsResource = false;
};


/// Device wrapper.
/// This incorporates a VkDevice along with the physical device and a number of queues.
class device {
public:
  device() {
  }

  device(VkDevice dev, VkPhysicalDevice physicalDevice_) : dev(dev), physicalDevice_(physicalDevice_) {
  }

  uint32_t getMemoryType(uint32_t typeBits, VkFlags properties) const {
    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &deviceMemoryProperties);

    for (uint32_t i = 0; i < 32; i++) {
      if (typeBits & (1<<i)) {
        if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
          return i;
        }
      }
    }
    return ~(uint32_t)0;
  }

  VkFormat getSupportedDepthFormat()
  {
    // Since all depth formats may be optional, we need to find a suitable depth format to use
    // Start with the highest precision packed format
    static const VkFormat depthFormats[] = { 
      VK_FORMAT_D32_SFLOAT_S8_UINT, 
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D24_UNORM_S8_UINT, 
      VK_FORMAT_D16_UNORM_S8_UINT, 
      VK_FORMAT_D16_UNORM
    };

    for (size_t i = 0; i != sizeof(depthFormats)/sizeof(depthFormats[0]); ++i) {
      VkFormat format = depthFormats[i];
      VkFormatProperties formatProps;
      vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &formatProps);
      // Format must support depth stencil attachment for optimal tiling
      if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        return format;
      }
    }

    return VK_FORMAT_UNDEFINED;
  }

  // todo: get two queues
  uint32_t getGraphicsQueueNodeIndex(VkSurfaceKHR surface) {
    // Get queue properties
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueCount, NULL);

    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueCount, queueProps.data());

    // Iterate over each queue to learn whether it supports presenting:
    std::vector<VkBool32> supportsPresent(queueCount);
    for (uint32_t i = 0; i < queueCount; i++)  {
      vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface, &supportsPresent[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueCount; i++) 
    {
      if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) 
      {
        if (graphicsQueueNodeIndex == UINT32_MAX) 
        {
          graphicsQueueNodeIndex = i;
        }

        if (supportsPresent[i] == VK_TRUE) 
        {
          return i;
        }
      }
    }

    /*if (presentQueueNodeIndex == UINT32_MAX) 
    {  
      // If there's no queue that supports both present and graphics
      // try to find a separate present queue
      for (uint32_t i = 0; i < queueCount; ++i) 
      {
        if (supportsPresent[i] == VK_TRUE) 
        {
          presentQueueNodeIndex = i;
          break;
        }
      }
    }*/
    return ~(uint32_t)0;
  }

  std::pair<VkFormat, VkColorSpaceKHR> getSurfaceFormat(VkSurfaceKHR surface) {
    // Get list of supported formats
    uint32_t formatCount = 0;
    VkResult err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount, NULL);
    if (err) throw error(err, __FILE__, __LINE__);

    std::vector<VkSurfaceFormatKHR> surfFormats(formatCount);
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount, surfFormats.data());
    if (err) throw error(err, __FILE__, __LINE__);

    return formatCount == 0 || surfFormats[0].format == VK_FORMAT_UNDEFINED ?
      std::pair<VkFormat, VkColorSpaceKHR>(VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR) :
      std::pair<VkFormat, VkColorSpaceKHR>(surfFormats[0].format, surfFormats[0].colorSpace)
    ;
  }

  operator VkDevice() const { return dev; }
  VkPhysicalDevice physicalDevice() const { return physicalDevice_; }

  void waitIdle() const {
    vkDeviceWaitIdle(dev);
  }

public:
  VkDevice dev;
  VkPhysicalDevice physicalDevice_;
};

class instance : public resource<VkInstance, instance> {
public:
  instance() : resource((VkInstance)VK_NULL_HANDLE) {
  }

  /// instance that does not own its pointer
  instance(VkInstance value) : resource(value, VK_NULL_HANDLE) {
  }

  /// instance that does owns (and creates) its pointer
  instance(const char *name, bool enableValidation = true) : resource((VkDevice)VK_NULL_HANDLE) {
    // sadly none of these seem to work on the windows version
	  static const char *validationLayerNames[] = 
	  {
		  "VK_LAYER_LUNARG_threading",
		  "VK_LAYER_LUNARG_mem_tracker",
		  "VK_LAYER_LUNARG_object_tracker",
		  "VK_LAYER_LUNARG_draw_state",
		  "VK_LAYER_LUNARG_param_checker",
		  "VK_LAYER_LUNARG_swapchain",
		  "VK_LAYER_LUNARG_device_limits",
		  "VK_LAYER_LUNARG_image",
		  "VK_LAYER_GOOGLE_unique_objects",
	  };

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "vku";
    appInfo.pEngineName = "vku";

    // Temporary workaround for drivers not supporting SDK 1.0.3 upon launch
    // todo : Use VK_API_VERSION 
    appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 2);

    std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    #if defined(VK_KHR_WIN32_SURFACE_EXTENSION_NAME)
      enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(VK_KHR_XCB_SURFACE_EXTENSION_NAME)
      enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    #elif defined(VK_KHR_MIR_SURFACE_EXTENSION_NAME)
      enabledExtensions.push_back(VK_KHR_MIR_SURFACE_EXTENSION_NAME);
    #endif

    // todo : check if all extensions are present

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    if (enabledExtensions.size() > 0)
    {
      if (enableValidation)
      {
        enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
      }
      instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
      instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    }
    if (enableValidation)
    {
      instanceCreateInfo.enabledLayerCount = 0; //sizeof(validationLayerNames)/sizeof(validationLayerNames[0]);
      instanceCreateInfo.ppEnabledLayerNames = validationLayerNames;
    }

    VkInstance inst = VK_NULL_HANDLE;
    VkResult err = vkCreateInstance(&instanceCreateInfo, VK_NULL_HANDLE, &inst);
    if (err) throw error(err, __FILE__, __LINE__);
    set(inst, true);

    // Physical device
    uint32_t gpuCount = 0;
    // Get number of available physical devices
    err = vkEnumeratePhysicalDevices(get(), &gpuCount, VK_NULL_HANDLE);
    if (err) throw error(err, __FILE__, __LINE__);

    if (gpuCount == 0) {
      throw(std::runtime_error("no Vulkan devices found"));
    }

    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    err = vkEnumeratePhysicalDevices(get(), &gpuCount, physicalDevices.data());
    if (err) throw error(err, __FILE__, __LINE__);

    // Note : 
    // This example will always use the first physical device reported, 
    // change the vector index if you have multiple Vulkan devices installed 
    // and want to use another one
    physicalDevice_ = physicalDevices[0];

    // Find a queue that supports graphics operations
    uint32_t graphicsQueueIndex = 0;
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice(), &queueCount, NULL);
    //assert(queueCount >= 1);

    std::vector<VkQueueFamilyProperties> queueProps;
    queueProps.resize(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice(), &queueCount, queueProps.data());

    for (graphicsQueueIndex = 0; graphicsQueueIndex < queueCount; graphicsQueueIndex++)
    {
      if (queueProps[graphicsQueueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        break;
    }
    //assert(graphicsQueueIndex < queueCount);

    // Vulkan device
    std::array<float, 1> queuePriorities = { 0.0f };
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = queuePriorities.data();

    std::vector<const char*> enabledDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = NULL;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = NULL;

    if (enabledExtensions.size() > 0)
    {
      deviceCreateInfo.enabledExtensionCount = (uint32_t)enabledDeviceExtensions.size();
      deviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
    }
    if (enableValidation)
    {
      deviceCreateInfo.enabledLayerCount = 0; //sizeof(validationLayerNames)/sizeof(validationLayerNames[0]);
      deviceCreateInfo.ppEnabledLayerNames = validationLayerNames;
    }

    err = vkCreateDevice(physicalDevice_, &deviceCreateInfo, VK_NULL_HANDLE, &dev_);
    if (err) throw error(err, __FILE__, __LINE__);

    // Get the graphics queue
    vkGetDeviceQueue(dev_, graphicsQueueIndex, 0, &queue_);
  }

  VkSurfaceKHR createSurface(void *window) {
    VkSurfaceKHR result = VK_NULL_HANDLE;
    // Create surface depending on OS
    #if defined(_WIN32)
      VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.hinstance = connection();
      surfaceCreateInfo.hwnd = (HWND)window;
      VkResult err = vkCreateWin32SurfaceKHR(get(), &surfaceCreateInfo, VK_NULL_HANDLE, &result);
    #elif defined(__ANDROID__)
      VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.window = window;
      VkResult err = vkCreateAndroidSurfaceKHR(get(), &surfaceCreateInfo, NULL, &result);
    #else
      VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.connection = connection();
      surfaceCreateInfo.window = (xcb_window_t)(intptr_t)window;
      VkResult err = vkCreateXcbSurfaceKHR(get(), &surfaceCreateInfo, VK_NULL_HANDLE, &result);
    #endif
    if (err) throw error(err, __FILE__, __LINE__);
    return result;
  }

  void destroy() {
    vkDestroyInstance(get(), VK_NULL_HANDLE);
  }

  VkPhysicalDevice physicalDevice() const { return physicalDevice_; }

  vku::device device() const { return vku::device(dev_, physicalDevice_); }
  VkQueue queue() const { return queue_; }

public:
  bool enableValidation = false;
  VkPhysicalDevice physicalDevice_;
  VkDevice dev_;
  VkQueue queue_;
};

class renderPassLayout {
public:
  uint32_t addAttachment(VkFormat format) {
    VkAttachmentDescription ad = {};
    ad.format = format;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ad.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ad.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    uint32_t res = (uint32_t)attachments.size();
    attachments.push_back(ad);
    return res;
  }

  void addSubpass(uint32_t colorAttachment, VkImageLayout colorLayout, uint32_t depthAttachment, VkImageLayout depthLayout) {
    subpass s = {};
    s.num_color = 1;
    s.color[0].attachment = 0;
    s.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    s.depth.attachment = 1;
    s.depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    s.desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses.push_back(s);
  }

  VkRenderPass create(VkDevice device) {
    std::vector<VkSubpassDescription> descs;
    for (size_t i = 0; i != subpasses.size(); ++i) {
      subpass &s = subpasses[i];
      s.desc.colorAttachmentCount = s.num_color;
      s.desc.pColorAttachments = s.color;
      s.desc.pDepthStencilAttachment = &s.depth;
      descs.push_back(s.desc);
    }

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = (uint32_t)attachments.size();
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = (uint32_t)descs.size();
    renderPassInfo.pSubpasses = descs.data();

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult err = vkCreateRenderPass(device, &renderPassInfo, VK_NULL_HANDLE, &renderPass);
    return renderPass;
  }

private:
  struct subpass {
    uint32_t num_color;
    VkAttachmentReference color[6];
    VkAttachmentReference depth;
    VkSubpassDescription desc;
  };
  std::vector<VkAttachmentDescription> attachments;
  std::vector<subpass> subpasses;
  std::vector<VkAttachmentReference> refs;
};

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

class descriptorPool {
public:
  descriptorPool() {
  }

  descriptorPool(VkDevice dev, uint32_t num_uniform_buffers) : dev_(dev) {
    // We need to tell the API the number of max. requested descriptors per type
    VkDescriptorPoolSize typeCounts[1];
    // This example only uses one descriptor type (uniform buffer) and only
    // requests one descriptor of this type
    typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    typeCounts[0].descriptorCount = num_uniform_buffers;
    // For additional types you need to add new entries in the type count list
    // E.g. for two combined image samplers :
    // typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // typeCounts[1].descriptorCount = 2;

    // Create the global descriptor pool
    // All descriptors used in this example are allocated from this pool
    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = NULL;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = typeCounts;
    // Set the max. number of sets that can be requested
    // Requesting descriptors beyond maxSets will result in an error
    descriptorPoolInfo.maxSets = num_uniform_buffers * 2;

    VkResult err = vkCreateDescriptorPool(dev_, &descriptorPoolInfo, VK_NULL_HANDLE, &pool_);
    if (err) throw error(err, __FILE__, __LINE__);

    ownsResource_ = true;
  }

  // allocate a descriptor set for a buffer
  VkWriteDescriptorSet *allocateDescriptorSet(const buffer &buffer, const VkDescriptorSetLayout *layout, VkDescriptorSet *descriptorSets) {
    // Update descriptor sets determining the shader binding points
    // For every binding point used in a shader there needs to be one
    // descriptor set matching that binding point

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = layout;

    VkResult err = vkAllocateDescriptorSets(dev_, &allocInfo, descriptorSets);
    if (err) throw error(err, __FILE__, __LINE__);

    // Binding 0 : Uniform buffer
    VkDescriptorBufferInfo desc = buffer.desc();
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSets[0];
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &desc;
    // Binds this uniform buffer to binding point 0
    writeDescriptorSet.dstBinding = 0;

    return &writeDescriptorSet;
  }

  ~descriptorPool() {
    if (pool_ && ownsResource_) {
      vkDestroyDescriptorPool(dev_, pool_, VK_NULL_HANDLE);
      ownsResource_ = false;
    }
  }

  descriptorPool &operator=(descriptorPool &&rhs) {
    ownsResource_ = true;
    pool_ = rhs.pool_;
    rhs.ownsResource_ = false;
    dev_ = rhs.dev_;
    return *this;
  }

  operator VkDescriptorPool() { return pool_; }
private:
  VkDevice dev_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  bool ownsResource_ = false;
  VkWriteDescriptorSet writeDescriptorSet = {};
};


class shaderModule : public resource<VkShaderModule, shaderModule> {
public:
  shaderModule() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// descriptor pool that does not own its pointer
  shaderModule(VkShaderModule value, VkDevice dev) : resource(value, dev) {
  }

  shaderModule(VkDevice dev, const std::string &filename, VkShaderStageFlagBits stage) : resource(dev) {
    std::ifstream input(filename, std::ios::binary);
    auto b = std::istreambuf_iterator<char>(input);
    auto e = std::istreambuf_iterator<char>();
    if (b == e) throw(std::runtime_error("shaderModule(): shader file empty or not found"));
    std::vector<uint8_t> buf(b, e);
    create(buf.data(), buf.data() + buf.size(), stage);
  }

  shaderModule(VkDevice dev, const uint8_t *b, const uint8_t *e, VkShaderStageFlagBits stage) : resource(dev) {
    create(b, e, stage);
  }

  /// descriptor pool that owns (and creates) its pointer
  void create(const uint8_t *b, const uint8_t *e, VkShaderStageFlagBits stage) {
    std::vector<uint8_t> buf;
    if (*b != 0x03) {
      buf.resize(12);
      ((uint32_t *)buf.data())[0] = 0x07230203; 
      ((uint32_t *)buf.data())[1] = 0;
      ((uint32_t *)buf.data())[2] = stage;
    }
    while (b != e) buf.push_back(*b++);
    buf.push_back(0);

    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = buf.size();
    moduleCreateInfo.pCode = (uint32_t*)buf.data();
    moduleCreateInfo.flags = 0;

    VkShaderModule shaderModule;
    VkResult err = vkCreateShaderModule(dev(), &moduleCreateInfo, NULL, &shaderModule);
    if (err) throw error(err, __FILE__, __LINE__);
    set(shaderModule, true);
  }

  void destroy() {
    if (get()) vkDestroyShaderModule(dev(), get(), VK_NULL_HANDLE);
  }

  shaderModule &operator=(shaderModule &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    return *this;
  }
};

class pipelineCache : public resource<VkPipelineCache, pipelineCache> {
public:
  pipelineCache() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// descriptor pool that does not own its pointer
  pipelineCache(VkPipelineCache value, VkDevice dev) : resource(value, dev) {
  }

  /// descriptor pool that does owns (and creates) its pointer
  pipelineCache(VkDevice dev) : resource(dev) {
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkPipelineCache cache;
    VkResult err = vkCreatePipelineCache(dev, &pipelineCacheCreateInfo, VK_NULL_HANDLE, &cache);
    if (err) throw error(err, __FILE__, __LINE__);
    set(cache, true);
  }

  void destroy() {
    if (get()) vkDestroyPipelineCache(dev(), get(), VK_NULL_HANDLE);
  }

  pipelineCache &operator=(pipelineCache &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    return *this;
  }
};

class pipelineCreateHelper {
public:
  pipelineCreateHelper() {
    // Vertex input state
    // Describes the topoloy used with this pipeline
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // This pipeline renders vertex data as triangle lists
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Rasterization state
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // Solid polygon mode
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    // No culling
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;

    // Color blend state
    // Describes blend modes and color masks
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    // One blend attachment state
    // Blending is not used in this example
    blendAttachmentState[0].colorWriteMask = 0xf;
    blendAttachmentState[0].blendEnable = VK_FALSE;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blendAttachmentState;

    // Viewport state
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    // One viewport
    viewportState.viewportCount = 1;
    // One scissor rectangle
    viewportState.scissorCount = 1;

    // Enable dynamic states
    // Describes the dynamic states to be used with this pipeline
    // Dynamic states can be set even after the pipeline has been created
    // So there is no need to create new pipelines just for changing
    // a viewport's dimensions or a scissor box
    // The dynamic state properties themselves are stored in the command buffer
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

    // Depth and stencil state
    // Describes depth and stenctil test and compare ops
    // Basic depth compare setup with depth writes and depth test enabled
    // No stencil used 
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = depthStencilState.back;

    // Multi sampling state
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.pSampleMask = NULL;
    // No multi sampling used in this example
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  }

  pipelineCreateHelper &attrib(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription desc = {};
    desc.location = location;
    desc.binding = binding;
    desc.format = format;
    desc.offset = offset;
    attributeDescriptions.push_back(desc);
    return *this;
  }

  pipelineCreateHelper &binding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX) {
    VkVertexInputBindingDescription desc = {};
    desc.binding = binding;
    desc.stride = stride;
    desc.inputRate = inputRate;
    bindingDescriptions.push_back(desc);
    return *this;
  }

  pipelineCreateHelper &uniformBuffers(uint32_t count, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    layoutBindings_.push_back(layoutBinding);
    return *this;
  }

  pipelineCreateHelper &shader(const vku::shaderModule &module, VkShaderStageFlagBits stage, const char *entrypoint="main") {
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;
    shaderStage.module = module.get();
    shaderStage.pName = entrypoint;
    shaderStages_.push_back(shaderStage);
    return *this;
  }

  // todo: make more of these
  pipelineCreateHelper &topology(VkPrimitiveTopology value) {
    inputAssemblyState.topology = value;
    return *this;
  }

  // querying functions
  VkDescriptorSetLayoutCreateInfo *getDescriptorSetLayout() {
    descriptorLayout_.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout_.bindingCount = (uint32_t)layoutBindings_.size();
    descriptorLayout_.pBindings = layoutBindings_.data();
    return &descriptorLayout_;
  }

  VkGraphicsPipelineCreateInfo *get(VkRenderPass renderPass, VkPipelineLayout pipelineLayout) {
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.pNext = VK_NULL_HANDLE;
    vi.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
    vi.pVertexBindingDescriptions = bindingDescriptions.data();
    vi.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
    vi.pVertexAttributeDescriptions = attributeDescriptions.data();

    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    // The layout used for this pipeline
    pipelineCreateInfo.layout = pipelineLayout;
    // Renderpass this pipeline is attached to
    pipelineCreateInfo.renderPass = renderPass;

    // Assign states
    // Two shader stages
    pipelineCreateInfo.stageCount = (uint32_t)shaderStages_.size();
    // Assign pipeline state create information
    pipelineCreateInfo.pVertexInputState = &vi;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pStages = shaderStages_.data();
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.pDynamicState = &dynamicState;

    return &pipelineCreateInfo;
  }

  /*pipelineCreateHelper &operator=(pipelineCreateHelper && rhs) {
    vi = rhs.vi;
    descriptorLayout_ = rhs.descriptorLayout_;
    bindingDescriptions = std::move(rhs.bindingDescriptions);
    attributeDescriptions = std::move(rhs.attributeDescriptions);
    layoutBindings_ = std::move(rhs.layoutBindings_);
    shaderStages_ = std::move(rhs.shaderStages_);
    dynamicStateEnables = std::move(rhs.dynamicStateEnables);
  }*/

  pipelineCreateHelper &operator=(pipelineCreateHelper && rhs) = default;
private:
  VkPipelineVertexInputStateCreateInfo vi = {};
  VkDescriptorSetLayoutCreateInfo descriptorLayout_ = {};
  std::vector<VkVertexInputBindingDescription> bindingDescriptions;
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings_;
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages_;
  std::vector<VkDynamicState> dynamicStateEnables;

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
  VkPipelineRasterizationStateCreateInfo rasterizationState = {};
  VkPipelineColorBlendStateCreateInfo colorBlendState = {};
  VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
  VkPipelineViewportStateCreateInfo viewportState = {};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
  VkPipelineMultisampleStateCreateInfo multisampleState = {};
  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
};

class pipeline {
public:
  pipeline() {
  }

  pipeline(
    const vku::device &device,
    VkRenderPass renderPass,
    const vku::pipelineCache &pipelineCache,
    pipelineCreateHelper &pipelineCreateHelper
  ) : dev_(device) {
    VkResult err = vkCreateDescriptorSetLayout(device, pipelineCreateHelper.getDescriptorSetLayout(), NULL, &descriptorSetLayout);
    if (err) throw error(err, __FILE__, __LINE__);

    // Create the pipeline layout that is used to generate the rendering pipelines that
    // are based on this descriptor set layout
    // In a more complex scenario you would have different pipeline layouts for different
    // descriptor set layouts that could be reused
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = NULL;
    pPipelineLayoutCreateInfo.setLayoutCount = 1;
    pPipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

    err = vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, VK_NULL_HANDLE, &pipelineLayout);
    if (err) throw error(err, __FILE__, __LINE__);

    auto info = pipelineCreateHelper.get(renderPass, pipelineLayout);

    // Create rendering pipeline
    err = vkCreateGraphicsPipelines(device, pipelineCache, 1, info, VK_NULL_HANDLE, &pipe_);
    if (err) throw error(err, __FILE__, __LINE__);

    ownsData = true;
  }

  pipeline &operator=(pipeline &&rhs) {
    pipe_ = rhs.pipe_;
    pipelineLayout = rhs.pipelineLayout;
    descriptorSet = rhs.descriptorSet;
    descriptorSetLayout = rhs.descriptorSetLayout;
    dev_ = rhs.dev_;
    shaderModules = std::move(shaderModules);
    ownsData = true;
    rhs.ownsData = false;
    return *this;
  }

  ~pipeline() {
    if (ownsData) {
      vkDestroyPipeline(dev_, pipe_, VK_NULL_HANDLE);
      vkDestroyPipelineLayout(dev_, pipelineLayout, VK_NULL_HANDLE);
      vkDestroyDescriptorSetLayout(dev_, descriptorSetLayout, VK_NULL_HANDLE);
    }
  }

  void allocateDescriptorSets(descriptorPool &descPool, uint32_t count=1) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    descriptorSet = VK_NULL_HANDLE;
    VkResult err = vkAllocateDescriptorSets(dev_, &allocInfo, &descriptorSet);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  void updateDescriptorSets(buffer &uniformVS) {
    VkWriteDescriptorSet writeDescriptorSet = {};

    // Binding 0 : Uniform buffer
    VkDescriptorBufferInfo desc = uniformVS.desc();
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &desc;
    // Binds this uniform buffer to binding point 0
    writeDescriptorSet.dstBinding = 0;

    vkUpdateDescriptorSets(dev_, 1, &writeDescriptorSet, 0, NULL);
  }

  VkPipeline pipe() { return pipe_; }
  VkPipelineLayout layout() const { return pipelineLayout; }
  VkDescriptorSet *descriptorSets() { return &descriptorSet; }
  VkDescriptorSetLayout *descriptorLayouts() { return &descriptorSetLayout; }

private:

  VkPipeline pipe_ = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDevice dev_ = VK_NULL_HANDLE;
  std::vector<VkShaderModule> shaderModules;
  bool ownsData = false;
};

class commandPool : public resource<VkCommandPool, commandPool> {
public:
  commandPool() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// command pool that does not own its pointer
  commandPool(VkCommandPool value, VkDevice dev) : resource(value, dev) {
  }

  /// command pool that does owns (and creates) its pointer
  commandPool(VkDevice dev, uint32_t queueFamilyIndex) : resource(dev) {
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool cmdPool;
    VkResult err = vkCreateCommandPool(dev, &cmdPoolInfo, VK_NULL_HANDLE, &cmdPool);
    if (err) throw error(err, __FILE__, __LINE__);
    set(cmdPool, true);
  }

  void destroy() {
    if (get()) vkDestroyCommandPool(dev(), get(), VK_NULL_HANDLE);
  }

  commandPool &operator=(commandPool &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    return *this;
  }
};

class cmdBuffer : public resource<VkCommandBuffer, cmdBuffer> {
public:
  cmdBuffer() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// command buffer that does not own its pointer
  cmdBuffer(VkCommandBuffer value, VkDevice dev) : resource(value, dev) {
  }

  /// command buffer that does owns (and creates) its pointer
  cmdBuffer(VkDevice dev, VkCommandPool cmdPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) : resource(dev) {
    VkCommandBuffer res = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = cmdPool;
    commandBufferAllocateInfo.level = level;
    commandBufferAllocateInfo.commandBufferCount = 1;
    VkResult vkRes = vkAllocateCommandBuffers(dev, &commandBufferAllocateInfo, &res);
    set(res, true);
  }

  void begin(VkRenderPass renderPass, VkFramebuffer framebuffer, int width, int height) const {
    beginCommandBuffer();
    beginRenderPass(renderPass, framebuffer, 0, 0, width, height);
    setViewport(0, 0, (float)width, (float)height);
    setScissor(0, 0, width, height);
  }

  void end(VkImage image) const {
    endRenderPass();
    addPrePresentationBarrier(image);
    endCommandBuffer();
  }

  void beginCommandBuffer() const {
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufInfo.pNext = NULL;
    vkBeginCommandBuffer(get(), &cmdBufInfo);
  }

  void beginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer, int x = 0, int y = 0, int width = 256, int height = 256) const {
    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.025f, 0.025f, 0.025f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = NULL;
    renderPassBeginInfo.framebuffer = framebuffer;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset.x = x;
    renderPassBeginInfo.renderArea.offset.y = y;
    renderPassBeginInfo.renderArea.extent.width = width;
    renderPassBeginInfo.renderArea.extent.height = height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(get(), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void setViewport(float x=0, float y=0, float width=256, float height=256, float minDepth=0, float maxDepth=1) const {
    // Update dynamic viewport state
    VkViewport viewport = {};
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    vkCmdSetViewport(get(), 0, 1, &viewport);
  }

  void setScissor(int x=0, int y=0, int width=256, int height=256) const {
    // Update dynamic scissor state
    VkRect2D scissor = {};
    scissor.offset.x = x;
    scissor.offset.y = y;
    scissor.extent.width = width;
    scissor.extent.height = height;
    vkCmdSetScissor(get(), 0, 1, &scissor);
  }

  void bindPipeline(pipeline &pipe) const {
    // Bind descriptor sets describing shader binding points
    vkCmdBindDescriptorSets(get(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout(), 0, 1, pipe.descriptorSets(), 0, NULL);

    // Bind the rendering pipeline (including the shaders)
    vkCmdBindPipeline(get(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipe());
  }

  void bindVertexBuffer(buffer &buf, int bindId) const {
    VkDeviceSize offsets[] = { 0 };
    VkBuffer bufs[] = { buf.buf() };
    vkCmdBindVertexBuffers(get(), bindId, 1, bufs, offsets);
  }

  void bindIndexBuffer(buffer &buf) const {
    // Bind triangle indices
    vkCmdBindIndexBuffer(get(), buf.buf(), 0, VK_INDEX_TYPE_UINT32);
  }

  void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) const {
    // Draw indexed triangle
    vkCmdDrawIndexed(get(), indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
  }

  void endRenderPass() const {
    vkCmdEndRenderPass(get());
  }

  // Create an image memory barrier for changing the layout of
  // an image and put it into an active command buffer
  // See chapter 11.4 "Image Layout" for details
  void setImageLayout(VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout) const {
    // Create an image barrier object
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange.aspectMask = aspectMask;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.layerCount = 1;

    // Put barrier on top
    VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    switch (oldImageLayout) {
      case VK_IMAGE_LAYOUT_UNDEFINED: {
        // Undefined layout
        // Only allowed as initial layout!
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      } break;
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
        // Old layout is color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      } break;
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
        // Old layout is transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      } break;
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
        // Old layout is shader read (sampler, input attachment)
        // Make sure any shader reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      } break;
      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
        // change the layout back from the surface format to the rendering format.
        srcStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      } break;
      default: {
        throw(std::runtime_error("setImageLayout: unsupported source layout"));
      }
    }

    // Target layouts (new)
    switch (newImageLayout) {
      // New layout is transfer destination (copy, blit)
      // Make sure any copyies to the image have been finished
      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      } break;

      // New layout is transfer source (copy, blit)
      // Make sure any reads from and writes to the image have been finished
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
        imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      } break;

      // New layout is color attachment
      // Make sure any writes to the color buffer hav been finished
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      } break;

      // New layout is depth attachment
      // Make sure any writes to depth/stencil buffer have been finished
      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      } break;

      // New layout is shader read (sampler, input attachment)
      // Make sure any writes to the image have been finished
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      } break;

      // special case for converting to surface presentation
      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
        // wait for all stages to finish
        imageMemoryBarrier.dstAccessMask = 0;
        srcStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      } break;

      default: {
        throw(std::runtime_error("setImageLayout: unsupported destination layout"));
      }
    }

    // Put barrier inside setup command buffer
    vkCmdPipelineBarrier(get(), srcStageFlags, destStageFlags, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &imageMemoryBarrier);
  }

  /// change the layout of an image
  void addPrePresentationBarrier(VkImage image) const {
    setImageLayout(image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    return;
  }

  void endCommandBuffer() const {
    vkEndCommandBuffer(get());
  }

  void addPostPresentBariier(VkImage image) const {
    // Add a post present image memory barrier
    // This will transform the frame buffer color attachment back
    // to it's initial layout after it has been presented to the
    // windowing system
    // See buildCommandBuffers for the pre present barrier that 
    // does the opposite transformation 
    setImageLayout(image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    return;
  }

  cmdBuffer &operator=(cmdBuffer &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    pool_ = rhs.pool_;
    return *this;
  }

  /*cmdBuffer &operator=(const cmdBuffer &rhs) {
    set(rhs.get(), false);
    pool_ = rhs.pool_;
    return *this;
  }*/
  void destroy() {
    if (dev() && pool_) {
      VkCommandBuffer cb = get();
      vkFreeCommandBuffers(dev(), pool_, 1, &cb);
    }
  }

private:
  VkCommandPool pool_ = VK_NULL_HANDLE;
};

inline void swapChain::build_images(VkCommandBuffer buf) {
  VkDevice d = dev();
  vku::cmdBuffer cb(buf, d);

  uint32_t imageCount = 0;
  VkResult err = vkGetSwapchainImagesKHR(dev(), get(), &imageCount, NULL);
  if (err) throw error(err, __FILE__, __LINE__);

  swapchainImages.resize(imageCount);
  swapchainViews.resize(imageCount);
  err = vkGetSwapchainImagesKHR(dev(), *this, &imageCount, swapchainImages.data());
  if (err) throw error(err, __FILE__, __LINE__);

  for (uint32_t i = 0; i < imageCount; i++) {
    VkImageViewCreateInfo colorAttachmentView = {};
    colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorAttachmentView.pNext = NULL;
    colorAttachmentView.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorAttachmentView.components = {
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_A
    };
    colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorAttachmentView.subresourceRange.baseMipLevel = 0;
    colorAttachmentView.subresourceRange.levelCount = 1;
    colorAttachmentView.subresourceRange.baseArrayLayer = 0;
    colorAttachmentView.subresourceRange.layerCount = 1;
    colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorAttachmentView.flags = 0;

    cb.setImageLayout(
      image(i), 
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, 
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    colorAttachmentView.image = image(i);

    err = vkCreateImageView(dev(), &colorAttachmentView, VK_NULL_HANDLE, &swapchainViews[i]);
    if (err) throw error(err, __FILE__, __LINE__);
  }
}


class semaphore : public resource<VkSemaphore, semaphore> {
public:
  /// semaphore that does not own its pointer
  semaphore(VkSemaphore value = VK_NULL_HANDLE, VkDevice dev = VK_NULL_HANDLE) : resource(value, dev) {
  }

  /// semaphore that does owns (and creates) its pointer
  semaphore(VkDevice dev) : resource(dev) {
  }

  VkSemaphore create(VkDevice dev) {
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphore res = VK_NULL_HANDLE;
    VkResult err = vkCreateSemaphore(dev, &info, VK_NULL_HANDLE, &res);
    if (err) throw error(err, __FILE__, __LINE__);
    return res;
  }

  void destroy() {
    vkDestroySemaphore(dev(), get(), VK_NULL_HANDLE);
  }
};

class queue : public resource<VkQueue, queue> {
public:
  /// queue that does not own its pointer
  queue(VkQueue value = VK_NULL_HANDLE, VkDevice dev = VK_NULL_HANDLE) : resource(value, dev) {
  }

  /// queue that does owns (and creates) its pointer
  queue(VkDevice dev) : resource(dev) {
  }

  void submit(VkSemaphore sema, VkCommandBuffer buffer) const {
    // The submit infor strcuture contains a list of
    // command buffers and semaphores to be submitted to a queue
    // If you want to submit multiple command buffers, pass an array
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = sema ? 1 : 0;
    submitInfo.pWaitSemaphores = &sema;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    // Submit to the graphics queue
    VkResult err = vkQueueSubmit(get(), 1, &submitInfo, VK_NULL_HANDLE);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  void waitIdle() const {  
    VkResult err = vkQueueWaitIdle(get());
    if (err) throw error(err, __FILE__, __LINE__);
  }

  VkQueue create(VkDevice dev) {
    return VK_NULL_HANDLE;
  }

  void destroy() {
  }

};

class image : public resource<VkImage, image> {
public:
  /// image that does not own its pointer
  image(VkImage value = VK_NULL_HANDLE, VkDevice dev = VK_NULL_HANDLE) : resource(value, dev) {
  }

  /// image that does owns (and creates) its pointer
  image(VkDevice dev, uint32_t width, uint32_t height, VkFormat format=VK_FORMAT_R8G8B8_UNORM, VkImageType type=VK_IMAGE_TYPE_2D, VkImageUsageFlags usage=0) : resource(VK_NULL_HANDLE, dev) {
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.imageType = type;
    image.format = format;
    image.extent = { width, height, 1 };
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = usage;
    image.flags = 0;

    VkImage result = VK_NULL_HANDLE;
    VkResult err = vkCreateImage(dev, &image, VK_NULL_HANDLE, &result);
    if (err) throw error(err, __FILE__, __LINE__);

    set(result, true);
    format_ = format;
  }

  /// allocate device memory
  void allocate(const vku::device &device) {
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, get(), &memReqs);

    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.allocationSize = memReqs.size;
    mem_alloc.memoryTypeIndex = device.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkResult err = vkAllocateMemory(device, &mem_alloc, VK_NULL_HANDLE, &mem_);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  /// bind device memory to the image object
  void bindMemoryToImage() {
    VkResult err = vkBindImageMemory(dev(), get(), mem(), 0);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  void setImageLayout(const vku::cmdBuffer &cmdBuf, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout) {
    cmdBuf.setImageLayout(get(), aspectMask, oldImageLayout, newImageLayout);
  }

  /// todo: generalise
  void createView() {
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext = NULL;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format_;
    viewCreateInfo.flags = 0;
    viewCreateInfo.subresourceRange = {};
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.image = get();
    VkResult err = vkCreateImageView(dev(), &viewCreateInfo, VK_NULL_HANDLE, &view_);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  void destroy() {
    if (view()) {
      //vkDestroyImageView(dev(), view(), VK_NULL_HANDLE);
    }

    if (mem()) {
      //vkFreeMemory(dev(), mem(), VK_NULL_HANDLE);
    }

    if (get()) {
      //vkDestroyImage(dev(), get(), VK_NULL_HANDLE);
    }

    view_ = VK_NULL_HANDLE;
    mem_ = VK_NULL_HANDLE;
    set(VK_NULL_HANDLE, false);
  }

  VkDeviceMemory mem() const { return mem_; }
  VkImageView view() const { return view_; }

  VkDeviceMemory mem_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;

  image &operator=(image &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    format_ = rhs.format_;
    mem_ = rhs.mem_;
    view_ = rhs.view_;
    return *this;
  }
public:
  VkFormat format_ = VK_FORMAT_UNDEFINED;
};

class window {
public:
  window(int argc, const char **argv, bool enableValidation, uint32_t width, uint32_t height, float zoom, const std::string &title) :
    width_(width), height_(height), zoom_(zoom), title_(title), argc_(argc), argv_(argv)
  {
    // Values not set here are initialized in the base class constructor
    // Check for validation command line flag
    #ifdef _WIN32
      for (int32_t i = 0; i < argc; i++)
      {
        if (argv[i] == std::string("-validation"))
        {
          enableValidation = true;
        }
      }
    #endif

    instance_ = vku::instance("vku");

    vku::device dev = instance_.device();
    device_ = dev;
    queue_ = instance_.queue();

    // Find a suitable depth format
    depthFormat_ = dev.getSupportedDepthFormat();
    assert(depthFormat_ != VK_FORMAT_UNDEFINED);

    setupWindow();
    prepareWindow();
  }

  ~window() {
    // Clean up Vulkan resources
    swapChain_.clear();

    cmdPool_.clear();

    pipelineCache_.clear();

    cmdPool_.clear();

    //vkDestroyDevice(device, VK_NULL_HANDLE); 
    //device.clear();

    if (enableValidation_)
    {
      //vkDebug::freeDebugCallback(instance);
    }

    instance_.clear();

    #ifndef _WIN32
      xcb_destroy_window(connection(), window_);
      //xcb_disconnect(connection);
    #endif 
  }

  #ifdef _WIN32 
    HWND setupWindow()
    {
      bool fullscreen = false;

      // Check command line arguments
      for (int32_t i = 0; i < argc_; i++)
      {
        if (argv_[i] == std::string("-fullscreen"))
        {
          fullscreen = true;
        }
      }

      WNDCLASSEX wndClass;

      wndClass.cbSize = sizeof(WNDCLASSEX);
      wndClass.style = CS_HREDRAW | CS_VREDRAW;
      wndClass.lpfnWndProc = WndProc<vku::window>;
      wndClass.cbClsExtra = 0;
      wndClass.cbWndExtra = 0;
      wndClass.hInstance = connection();
      wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
      wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
      wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
      wndClass.lpszMenuName = NULL;
      wndClass.lpszClassName = name_.c_str();
      wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

      RegisterClassEx(&wndClass);

      int screenWidth = GetSystemMetrics(SM_CXSCREEN);
      int screenHeight = GetSystemMetrics(SM_CYSCREEN);

      if (fullscreen)
      {
        DEVMODE dmScreenSettings;
        memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
        dmScreenSettings.dmSize = sizeof(dmScreenSettings);
        dmScreenSettings.dmPelsWidth = screenWidth;
        dmScreenSettings.dmPelsHeight = screenHeight;
        dmScreenSettings.dmBitsPerPel = 32;
        dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

        if ((width_ != screenWidth) && (height_ != screenHeight))
        {
          if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
          {
            if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
            {
              fullscreen = FALSE;
            }
            else
            {
              return FALSE;
            }
          }
        }

      }

      DWORD dwExStyle;
      DWORD dwStyle;

      if (fullscreen)
      {
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      }
      else
      {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      }

      RECT windowRect;
      if (fullscreen)
      {
        windowRect.left = (long)0;
        windowRect.right = (long)screenWidth;
        windowRect.top = (long)0;
        windowRect.bottom = (long)screenHeight;
      }
      else
      {
        windowRect.left = (long)screenWidth / 2 - width_ / 2;
        windowRect.right = (long)width_;
        windowRect.top = (long)screenHeight / 2 - height_ / 2;
        windowRect.bottom = (long)height_;
      }

      AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

      window_ = CreateWindowEx(0,
        name_.c_str(),
        title_.c_str(),
        //    WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
        dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        windowRect.left,
        windowRect.top,
        windowRect.right,
        windowRect.bottom,
        NULL,
        NULL,
        connection(),
        NULL);

      if (!window_) 
      {
        printf("Could not create window!\n");
        fflush(stdout);
        return 0;
        exit(1);
      }

      map_window(window_, this);

      ShowWindow(window_, SW_SHOW);
      SetForegroundWindow(window_);
      SetFocus(window_);

      return window_;
    }

    void handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      switch (uMsg)
      {
      case WM_CLOSE:
        prepared = false;
        DestroyWindow(hWnd);
        map_window(hWnd, (window*)VK_NULL_HANDLE);
        windowIsClosed_ = true;
        //PostQuitMessage(0);
        break;
      case WM_PAINT:
        ValidateRect(window_, NULL);
        break;
      case WM_KEYDOWN:
        switch (wParam)
        {
        case 0x50:
          paused_ = !paused_;
          break;
        case VK_ESCAPE:
          exit(0);
          break;
        }
        break;
      case WM_RBUTTONDOWN:
      case WM_LBUTTONDOWN:
        mousePos_.x = (float)LOWORD(lParam);
        mousePos_.y = (float)HIWORD(lParam);
        break;
      case WM_MOUSEMOVE:
        if (wParam & MK_RBUTTON)
        {
          int32_t posx = LOWORD(lParam);
          int32_t posy = HIWORD(lParam);
          zoom_ += (mousePos_.y - (float)posy) * .005f * zoomSpeed_;
          mousePos_ = glm::vec2((float)posx, (float)posy);
          viewChanged();
        }
        if (wParam & MK_LBUTTON)
        {
          int32_t posx = LOWORD(lParam);
          int32_t posy = HIWORD(lParam);
          rotation_.x += (mousePos_.y - (float)posy) * 1.25f * rotationSpeed_;
          rotation_.y -= (mousePos_.x - (float)posx) * 1.25f * rotationSpeed_;
          mousePos_ = glm::vec2((float)posx, (float)posy);
          viewChanged();
        }
        break;
      }
    }
  #else // WIN32
    // Linux : Setup window 
    // TODO : Not finished...
    xcb_window_t setupWindow()
    {
      uint32_t value_mask, value_list[32];

      window_ = xcb_generate_id(connection());

      value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
      value_list[0] = screen->black_pixel;
      value_list[1] = XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE;

      xcb_create_window(connection(),
        XCB_COPY_FROM_PARENT,
        window_, screen->root,
        0, 0, width_, height_, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual,
        value_mask, value_list);

      /* Magic code that will send notification when window is destroyed */
      xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection(), 1, 12, "WM_PROTOCOLS");
      xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection(), cookie, 0);

      xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection(), 0, 16, "WM_DELETE_WINDOW");
      atom_wm_delete_window = xcb_intern_atom_reply(connection(), cookie2, 0);

      xcb_change_property(connection(), XCB_PROP_MODE_REPLACE,
        window_, (*reply).atom, 4, 32, 1,
        &(*atom_wm_delete_window).atom);

      xcb_change_property(connection(), XCB_PROP_MODE_REPLACE,
        window_, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
        title_.size(), title_.c_str());

      free(reply);

      xcb_map_window(connection(), window_);

      return(window_);
    }

/*
    // Initialize XCB connection
    void initxcbConnection()
    {
      const xcb_setup_t *setup;
      xcb_screen_iterator_t iter;
      int scr;

      setup = xcb_get_setup(connection());
      iter = xcb_setup_roots_iterator(setup);
      while (scr-- > 0)
        xcb_screen_next(&iter);
      screen = iter.data;
    }
*/

    void handleEvent(const xcb_generic_event_t *event)
    {
      switch (event->response_type & 0x7f)
      {
      case XCB_CLIENT_MESSAGE:
        if ((*(xcb_client_message_event_t*)event).data.data32[0] ==
          (*atom_wm_delete_window).atom) {
          windowIsClosed_ = true;
        }
        break;
      case XCB_MOTION_NOTIFY:
      {
        xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
        if (mouseButtons_.left)
        {
          rotation_.x += (mousePos_.y - (float)motion->event_y) * 1.25f;
          rotation_.y -= (mousePos_.x - (float)motion->event_x) * 1.25f;
          viewChanged();
        }
        if (mouseButtons_.right)
        {
          zoom_ += (mousePos_.y - (float)motion->event_y) * .005f;
          viewChanged();
        }
        mousePos_ = glm::vec2((float)motion->event_x, (float)motion->event_y);
      }
      break;
      case XCB_BUTTON_PRESS:
      {
        xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
        mouseButtons_.left = (press->detail & XCB_BUTTON_INDEX_1);
        mouseButtons_.right = (press->detail & XCB_BUTTON_INDEX_3);
      }
      break;
      case XCB_BUTTON_RELEASE:
      {
        xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
        if (press->detail & XCB_BUTTON_INDEX_1)
          mouseButtons_.left = false;
        if (press->detail & XCB_BUTTON_INDEX_3)
          mouseButtons_.right = false;
      }
      break;
      case XCB_KEY_RELEASE:
      {
        const xcb_key_release_event_t *key =
          (const xcb_key_release_event_t *)event;

        if (key->detail == 0x9) {
          windowIsClosed_ = true;
        }
      }
      break;
      case XCB_DESTROY_NOTIFY:
        windowIsClosed_ = true;
        break;
      default:
        break;
      }
    }
  #endif

  virtual void viewChanged()
  {
    // For overriding on derived class
  }


  void prepareWindow() {
    VkSurfaceKHR surface = instance_.createSurface((void*)(intptr_t)window_);
    uint32_t queueNodeIndex = device_.getGraphicsQueueNodeIndex(surface);
    if (queueNodeIndex == ~(uint32_t)0) throw(std::runtime_error("no graphics and present queue available"));
    auto sf = device_.getSurfaceFormat(surface);
    //swapChain.colorFormat = sf.first;
    //swapChain.colorSpace = sf.second;

    if (enableValidation_) {
      //vkDebug::setupDebugging(instance, VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT, NULL);
    }

    cmdPool_ = vku::commandPool(device_, queueNodeIndex);

    setupCmdBuffer_ = vku::cmdBuffer(device_, cmdPool_);
    setupCmdBuffer_.beginCommandBuffer();

    swapChain_ = vku::swapChain(device_, width_, height_, surface, setupCmdBuffer_);
    width_ = swapChain_.width();
    height_ = swapChain_.height();

    assert(swapChain_.imageCount() <= 2);

    for (size_t i = 0; i != swapChain_.imageCount(); ++i) {
      drawCmdBuffers_[i] = vku::cmdBuffer(device_, cmdPool_);
    }

    postPresentCmdBuffer_ = vku::cmdBuffer(device_, cmdPool_);

    depthStencil_ = vku::image(device_, width_, height_, depthFormat_, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    depthStencil_.allocate(device_);
    depthStencil_.bindMemoryToImage();
    depthStencil_.setImageLayout(setupCmdBuffer_, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    depthStencil_.createView();

    pipelineCache_ = vku::pipelineCache(device_);
    //createPipelineCache();

    swapChain_.setupFrameBuffer(depthStencil_.view(), depthFormat_);

    setupCmdBuffer_.endCommandBuffer();
    queue_.submit(VK_NULL_HANDLE, setupCmdBuffer_);
    queue_.waitIdle();

    // Recreate setup command buffer for derived class

    setupCmdBuffer_ = vku::cmdBuffer(device_, cmdPool_);
    setupCmdBuffer_.beginCommandBuffer();

    // Create a simple texture loader class 
    //textureLoader = new vkTools::VulkanTextureLoader(instance_.physicalDevice(), device, queue, cmdPool);
  }



  static bool poll() {
  #ifdef _WIN32
    MSG msg;
    PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  #else
    xcb_flush(connection());
    xcb_generic_event_t *event = xcb_poll_for_event(connection());
    if (event) {
      vku::window *win = nullptr;
      win->handleEvent(event);
      free(event);
    }
  #endif
    return true;
  }

  glm::mat4 defaultProjectionMatrix() const {
    return glm::perspective(deg_to_rad(60.0f), (float)width() / (float)height(), 0.1f, 256.0f);
  }

  glm::mat4 defaultViewMatrix() const {
    return glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom()));
  }

  glm::mat4 defaultModelMatrix() const {
    glm::mat4 m;
    m = glm::rotate(m, deg_to_rad(rotation().x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, deg_to_rad(rotation().y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, deg_to_rad(rotation().z), glm::vec3(0.0f, 0.0f, 1.0f));
    return m;
  }

  void present() {
    {
      vku::semaphore sema(device_);

      // Get next image in the swap chain (back/front buffer)
      currentBuffer_ = swapChain_.acquireNextImage(sema);

      queue_.submit(sema, drawCmdBuffers_[currentBuffer_]);
    }

    // Present the current buffer to the swap chain
    // This will display the image
    swapChain_.present(queue_, currentBuffer());

    postPresentCmdBuffer_.beginCommandBuffer();
    postPresentCmdBuffer_.addPostPresentBariier(swapChain_.image(currentBuffer()));
    postPresentCmdBuffer_.endCommandBuffer();

    queue_.submit(VK_NULL_HANDLE, postPresentCmdBuffer_);

    queue_.waitIdle();
  }

  const vku::instance &instance() const { return instance_; }
  const vku::device &device() const { return device_; }
  const vku::queue &queue() const { return queue_; }
  const vku::commandPool &cmdPool() const { return cmdPool_; }
  const vku::cmdBuffer &setupCmdBuffer() const { return setupCmdBuffer_; }
  const vku::cmdBuffer &postPresentCmdBuffer() const { return postPresentCmdBuffer_; }
  const vku::cmdBuffer &drawCmdBuffer(size_t i) const { return drawCmdBuffers_[i]; }
  const vku::pipelineCache &pipelineCache() const { return pipelineCache_; }
  const vku::image &depthStencil() const { return depthStencil_; }
  const vku::swapChain &swapChain() const { return swapChain_; }

  const VkFormat colorformat() const { return colorformat_; }
  const VkFormat depthFormat() const { return depthFormat_; }
  const uint32_t currentBuffer() const { return currentBuffer_; }
  const uint32_t width() const { return width_; }
  const uint32_t height() const { return height_; }
  const float frameTimer() const { return frameTimer_; }
  const VkClearColorValue &defaultClearColor() const { return defaultClearColor_; }
  const float zoom() const { return zoom_; }
  const float timer() const { return timer_; }
  const float timerSpeed() const { return timerSpeed_; }
  const bool paused() const { return paused_; }
  const bool windowIsClosed() const { return windowIsClosed_; }
  const bool enableValidation() const { return enableValidation_; }
  const float rotationSpeed() const { return rotationSpeed_; }
  const float zoomSpeed() const { return zoomSpeed_; }
  const glm::vec3 &rotation() const { return rotation_; }
  const glm::vec2 &mousePos() const { return mousePos_; }
  const std::string &title() const { return title_; }
  const std::string &name() const { return name_; }
public:
  virtual void render() = 0;

private:
  vku::instance instance_;
  vku::device device_;
  vku::queue queue_;
  vku::commandPool cmdPool_;
  vku::cmdBuffer setupCmdBuffer_;
  vku::cmdBuffer postPresentCmdBuffer_;
  vku::cmdBuffer drawCmdBuffers_[2];
  vku::pipelineCache pipelineCache_;
  vku::image depthStencil_;
  vku::swapChain swapChain_;

  VkFormat colorformat_ = VK_FORMAT_B8G8R8A8_UNORM;
  VkFormat depthFormat_;
  uint32_t currentBuffer_ = 0;
  bool prepared = false;
  uint32_t width_ = 1280;
  uint32_t height_ = 720;
  int argc_ = 0;
  const char **argv_ = nullptr;

  float frameTimer_ = 1.0f;
  VkClearColorValue defaultClearColor_ = { { 0.025f, 0.025f, 0.025f, 1.0f } };

  float zoom_ = 0;

  // Defines a frame rate independent timer value clamped from -1.0...1.0
  // For use in animations, rotations, etc.
  float timer_ = 0.0f;
  // Multiplier for speeding up (or slowing down) the global timer
  float timerSpeed_ = 0.25f;
  
  bool paused_ = false;
  bool windowIsClosed_ = false;
  bool enableValidation_ = false;

  // Use to adjust mouse rotation speed
  float rotationSpeed_ = 1.0f;
  // Use to adjust mouse zoom speed
  float zoomSpeed_ = 1.0f;

  glm::vec3 rotation_ = glm::vec3();
  glm::vec2 mousePos_;

  std::string title_ = "Vulkan Example";
  std::string name_ = "vulkanExample";

  // OS specific 
  #ifdef _WIN32
    HWND window_;
  #else
    struct {
      bool left = false;
      bool right = false;
    } mouseButtons_;

    xcb_screen_t *screen;
    xcb_window_t window_;
    xcb_intern_atom_reply_t *atom_wm_delete_window;
  #endif  
};



} // vku

#endif
