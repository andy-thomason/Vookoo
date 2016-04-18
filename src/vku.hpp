
#ifndef VKU_INCLUDED
#define VKU_INCLUDED


#ifdef _WIN32
  #define VK_USE_PLATFORM_WIN32_KHR
  #pragma comment(lib, "vulkan/vulkan-1.lib")
  #define _CRT_SECURE_NO_WARNINGS

#endif

#include "../vulkan/vulkan.h"
#include "../glm/glm.hpp"

#include <cstring>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <chrono>

// derived from https://github.com/SaschaWillems/Vulkan
//
// Many thanks to Sascha, without who this would be a challenge!

namespace vku {

class error : public std::runtime_error {
  const char *what(VkResult err) {
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
public:
  error(VkResult err) : std::runtime_error(what(err)) {
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
  resource() : value_(nullptr), ownsResource(false), dev_(nullptr) {
  }

  resource(VulkanType value, VkDevice dev = nullptr) : value_(value), ownsResource(false), dev_(dev) {
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
    rhs.value_ = nullptr;
    rhs.ownsResource = false;
  }

  ~resource() {
    clear();
  }

  operator VulkanType() {
    return value_;
  }

  VulkanType get() const { return value_; }
  VkDevice dev() const { return dev_; }
  resource &set(VulkanType value, bool owns) { value_ = value; ownsResource = owns; return *this; }

  void clear() {
    if (value_ && ownsResource) ((ParentClass*)this)->destroy();
    value_ = nullptr; ownsResource = false;
  }
private:
  // resources have a value, a device and an ownership flag.
  VulkanType value_ = nullptr;
  VkDevice dev_ = nullptr;
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
    if (err) throw error(err);

		std::vector<VkSurfaceFormatKHR> surfFormats(formatCount);
		err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount, surfFormats.data());
    if (err) throw error(err);

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
  instance() : resource((VkInstance)nullptr) {
  }

  /// instance that does not own its pointer
  instance(VkInstance value) : resource(value, nullptr) {
  }

  /// instance that does owns (and creates) its pointer
  instance(const char *name) : resource((VkDevice)nullptr) {
    bool enableValidation = false;
	  VkApplicationInfo appInfo = {};
	  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	  appInfo.pApplicationName = "vku";
	  appInfo.pEngineName = "vku";

	  // Temporary workaround for drivers not supporting SDK 1.0.3 upon launch
	  // todo : Use VK_API_VERSION 
	  appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 2);

	  std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    #ifdef _WIN32
      enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #else
      // todo : linux/android
      enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
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
		  //instanceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount; // todo : change validation layer names!
		  //instanceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
	  }

    VkInstance inst = nullptr;
	  vkCreateInstance(&instanceCreateInfo, nullptr, &inst);
    set(inst, true);

	  // Physical device
	  uint32_t gpuCount = 0;
	  // Get number of available physical devices
	  VkResult err = vkEnumeratePhysicalDevices(get(), &gpuCount, nullptr);
    if (err) throw error(err);

    if (gpuCount == 0) {
      throw(std::runtime_error("no Vulkan devices found"));
    }

	  // Enumerate devices
	  std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	  err = vkEnumeratePhysicalDevices(get(), &gpuCount, physicalDevices.data());
    if (err) throw error(err);

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
		  //deviceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount; // todo : validation layer names
		  //deviceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
	  }

	  err = vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &dev_);
    if (err) throw error(err);

	  // Get the graphics queue
	  vkGetDeviceQueue(dev_, graphicsQueueIndex, 0, &queue_);
  }

  VkSurfaceKHR createSurface(void *connection, void *window) {
    VkSurfaceKHR result = nullptr;
		// Create surface depending on OS
    #if defined(_WIN32)
		  VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
		  surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		  surfaceCreateInfo.hinstance = (HINSTANCE)connection;
		  surfaceCreateInfo.hwnd = (HWND)window;
		  VkResult err = vkCreateWin32SurfaceKHR(get(), &surfaceCreateInfo, nullptr, &result);
    #elif defined(__ANDROID__)
		  VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
		  surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
		  surfaceCreateInfo.window = window;
		  VkResult err = vkCreateAndroidSurfaceKHR(get(), &surfaceCreateInfo, NULL, &result);
    #else
		  VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
		  surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
		  surfaceCreateInfo.connection = connection;
		  surfaceCreateInfo.window = window;
		  VkResult err = vkCreateXcbSurfaceKHR(get(), &surfaceCreateInfo, nullptr, &result);
    #endif
    if (err) throw error(err);
    return result;
  }

  void destroy() {
    vkDestroyInstance(get(), nullptr);
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

    VkRenderPass renderPass = nullptr;
	  VkResult err = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
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
  swapChain(VkSwapchainKHR value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// semaphore that does owns (and creates) its pointer
  swapChain(const vku::device dev, uint32_t width, uint32_t height, VkSurfaceKHR surface, VkCommandBuffer buf) : resource(dev) {
	  VkResult err;
	  VkSwapchainKHR oldSwapchain = *this;

	  // Get physical device surface properties and formats
	  VkSurfaceCapabilitiesKHR surfCaps;
	  err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev.physicalDevice(), surface, &surfCaps);
    if (err) throw error(err);

	  uint32_t presentModeCount;
	  err = vkGetPhysicalDeviceSurfacePresentModesKHR(dev.physicalDevice(), surface, &presentModeCount, NULL);
    if (err) throw error(err);

	  // todo : replace with vector?
	  VkPresentModeKHR *presentModes = (VkPresentModeKHR *)malloc(presentModeCount * sizeof(VkPresentModeKHR));

	  err = vkGetPhysicalDeviceSurfacePresentModesKHR(dev.physicalDevice(), surface, &presentModeCount, presentModes);
    if (err) throw error(err);

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
	  err = vkCreateSwapchainKHR(dev, &swapchainCI, nullptr, &res);
	  if (err) throw error(err);
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
    if (err) throw error(err);
	}

  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }

  size_t imageCount() { return swapchainImages.size(); }
  VkImage image(size_t i) { return swapchainImages[i]; }
  VkImageView view(size_t i) { return swapchainViews[i]; }

  uint32_t acquireNextImage(VkSemaphore presentCompleteSemaphore) {
    uint32_t currentBuffer = 0;
		VkResult err = vkAcquireNextImageKHR(dev(), get(), UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, &currentBuffer);
    if (err) throw error(err);
    return currentBuffer;
  }

  #if 0
  void setupDepthStencil()
  {
    depthStencil.image = vku::image(device, width, height, depthFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	  /*VkImageCreateInfo image = {};
	  image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	  image.pNext = NULL;
	  image.imageType = VK_IMAGE_TYPE_2D;
	  image.format = depthFormat;
	  image.extent = { width, height, 1 };
	  image.mipLevels = 1;
	  image.arrayLayers = 1;
	  image.samples = VK_SAMPLE_COUNT_1_BIT;
	  image.tiling = VK_IMAGE_TILING_OPTIMAL;
	  image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	  image.flags = 0;*/

	  VkMemoryAllocateInfo mem_alloc = {};
	  mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	  mem_alloc.pNext = NULL;
	  mem_alloc.allocationSize = 0;
	  mem_alloc.memoryTypeIndex = 0;

	  VkImageViewCreateInfo depthStencilView = {};
	  depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	  depthStencilView.pNext = NULL;
	  depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	  depthStencilView.format = depthFormat;
	  depthStencilView.flags = 0;
	  depthStencilView.subresourceRange = {};
	  depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	  depthStencilView.subresourceRange.baseMipLevel = 0;
	  depthStencilView.subresourceRange.levelCount = 1;
	  depthStencilView.subresourceRange.baseArrayLayer = 0;
	  depthStencilView.subresourceRange.layerCount = 1;

	  VkMemoryRequirements memReqs;
	  VkResult err;

	  //err = vkCreateImage(device, &image, nullptr, &depthStencil.image);
	  //assert(!err);

	  vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);
	  mem_alloc.allocationSize = memReqs.size;
	  getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mem_alloc.memoryTypeIndex);
	  err = vkAllocateMemory(device, &mem_alloc, nullptr, &depthStencil.mem);
	  assert(!err);

	  err = vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0);
	  assert(!err);
	  vkTools::setImageLayout(setupCmdBuffer, depthStencil.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	  depthStencilView.image = depthStencil.image;
	  err = vkCreateImageView(device, &depthStencilView, nullptr, &depthStencil.view);
	  assert(!err);
  }
  #endif


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
  VkSurfaceKHR surface_ = nullptr;
  VkRenderPass renderPass_;

  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainViews;
  std::vector<VkFramebuffer> frameBuffers_;
};

class buffer {
public:
  buffer(VkDevice dev = nullptr, VkBuffer buf = nullptr) : buf_(buf), dev(dev) {
  }

  buffer(VkDevice dev, VkBufferCreateInfo *bufInfo) : dev(dev), size_(bufInfo->size) {
		vkCreateBuffer(dev, bufInfo, nullptr, &buf_);
    ownsBuffer = true;
  }

  buffer(device dev, void *init, VkDeviceSize size, VkBufferUsageFlags usage) : dev(dev), size_(size) {
    VkBufferCreateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size = size;
		bufInfo.usage = usage;
		VkResult err = vkCreateBuffer(dev, &bufInfo, nullptr, &buf_);
    if (err) throw error(err);

    ownsBuffer = true;

		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		vkGetBufferMemoryRequirements(dev, buf_, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = dev.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

 		err = vkAllocateMemory(dev, &memAlloc, nullptr, &mem);
    if (err) throw error(err);

		if (init) {
		  void *dest = map();
      std::memcpy(dest, init, size);
      unmap();
    }
    bind();
  }

  buffer(VkBufferCreateInfo bufInfo, VkDevice dev = nullptr) : dev(dev) {
		vkCreateBuffer(dev, &bufInfo, nullptr, &buf_);
  }

  // RAII move operator
  buffer &operator=(buffer &&rhs) {
    dev = rhs.dev;
    buf_ = rhs.buf_;
    mem = rhs.mem;
    size_ = rhs.size_;
    rhs.dev = nullptr;
    rhs.mem = nullptr;
    rhs.buf_ = nullptr;

    rhs.ownsBuffer = false;
    return *this;
  }

  ~buffer() {
    if (buf_ && ownsBuffer) {
      vkDestroyBuffer(dev, buf_, nullptr);
      buf_ = nullptr;
    }
  }

  void *map() {
    void *dest = nullptr;
    VkResult err = vkMapMemory(dev, mem, 0, size(), 0, &dest);
    if (err) throw error(err);
    return dest;
  }

  void unmap() {
		vkUnmapMemory(dev, mem);
  }

  void bind() {
		VkResult err = vkBindBufferMemory(dev, buf_, mem, 0);
    if (err) throw error(err);
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
  VkBuffer buf_ = nullptr;
  VkDevice dev = nullptr;
  VkDeviceMemory mem = nullptr;
  size_t size_;
  bool ownsBuffer = false;
};

class vertexInputState {
public:
  vertexInputState() {
  }

  vertexInputState &operator=(vertexInputState && rhs) {
    vi = rhs.vi;
    bindingDescriptions = std::move(rhs.bindingDescriptions);
    attributeDescriptions = std::move(rhs.attributeDescriptions);
  }

  vertexInputState &attrib(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription desc = {};
    desc.location = location;
    desc.binding = binding;
    desc.format = format;
    desc.offset = offset;
    attributeDescriptions.push_back(desc);
    return *this;
  }

  vertexInputState &binding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX) {
    VkVertexInputBindingDescription desc = {};
    desc.binding = binding;
    desc.stride = stride;
    desc.inputRate = inputRate;
    bindingDescriptions.push_back(desc);
    return *this;
  }

  VkPipelineVertexInputStateCreateInfo *get() {
		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vi.pNext = nullptr;
		vi.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
		vi.pVertexBindingDescriptions = bindingDescriptions.data();
		vi.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
		vi.pVertexAttributeDescriptions = attributeDescriptions.data();
    return &vi;
  }

private:
	VkPipelineVertexInputStateCreateInfo vi;
	std::vector<VkVertexInputBindingDescription> bindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
};

class descriptorPool {
public:
  descriptorPool() {
  }

  descriptorPool(VkDevice dev) : dev_(dev) {
		// We need to tell the API the number of max. requested descriptors per type
		VkDescriptorPoolSize typeCounts[1];
		// This example only uses one descriptor type (uniform buffer) and only
		// requests one descriptor of this type
		typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		typeCounts[0].descriptorCount = 2;
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
		descriptorPoolInfo.maxSets = 2;

		VkResult err = vkCreateDescriptorPool(dev_, &descriptorPoolInfo, nullptr, &pool_);
    if (err) throw error(err);

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
    if (err) throw error(err);

		// Binding 0 : Uniform buffer
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSets[0];
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &buffer.desc();
		// Binds this uniform buffer to binding point 0
		writeDescriptorSet.dstBinding = 0;

    return &writeDescriptorSet;
  }

  ~descriptorPool() {
    if (pool_ && ownsResource_) {
      vkDestroyDescriptorPool(dev_, pool_, nullptr);
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
  VkDevice dev_ = nullptr;
  VkDescriptorPool pool_ = nullptr;
  bool ownsResource_ = false;
  VkWriteDescriptorSet writeDescriptorSet = {};
};


class pipeline {
public:
  pipeline() {
  }

  pipeline(VkDevice device, VkRenderPass renderPass, VkPipelineVertexInputStateCreateInfo *vertexInputState, VkPipelineCache pipelineCache) : dev_(device) {
		// Setup layout of descriptors used in this example
		// Basically connects the different shader stages to descriptors
		// for binding uniform buffers, image samplers, etc.
		// So every shader binding should map to one descriptor set layout
		// binding

		// Binding 0 : Uniform buffer (Vertex shader)
		VkDescriptorSetLayoutBinding layoutBinding = {};
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = 1;
		layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBinding.pImmutableSamplers = NULL;

		VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
		descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayout.pNext = NULL;
		descriptorLayout.bindingCount = 1;
		descriptorLayout.pBindings = &layoutBinding;

		VkResult err = vkCreateDescriptorSetLayout(device, &descriptorLayout, NULL, &descriptorSetLayout);
		if (err) throw error(err);

		// Create the pipeline layout that is used to generate the rendering pipelines that
		// are based on this descriptor set layout
		// In a more complex scenario you would have different pipeline layouts for different
		// descriptor set layouts that could be reused
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
		pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pPipelineLayoutCreateInfo.pNext = NULL;
		pPipelineLayoutCreateInfo.setLayoutCount = 1;
		pPipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

		err = vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		if (err) throw error(err);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};

		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		// The layout used for this pipeline
		pipelineCreateInfo.layout = pipelineLayout;
		// Renderpass this pipeline is attached to
		pipelineCreateInfo.renderPass = renderPass;

		// Vertex input state
		// Describes the topoloy used with this pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
		inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		// This pipeline renders vertex data as triangle lists
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// Rasterization state
		VkPipelineRasterizationStateCreateInfo rasterizationState = {};
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
		VkPipelineColorBlendStateCreateInfo colorBlendState = {};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		// One blend attachment state
		// Blending is not used in this example
		VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
		blendAttachmentState[0].colorWriteMask = 0xf;
		blendAttachmentState[0].blendEnable = VK_FALSE;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = blendAttachmentState;

		// Viewport state
		VkPipelineViewportStateCreateInfo viewportState = {};
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
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		// The dynamic state properties themselves are stored in the command buffer
		std::vector<VkDynamicState> dynamicStateEnables;
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables.data();
		dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

		// Depth and stencil state
		// Describes depth and stenctil test and compare ops
		VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
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
		VkPipelineMultisampleStateCreateInfo multisampleState = {};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.pSampleMask = NULL;
		// No multi sampling used in this example
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Load shaders
		VkPipelineShaderStageCreateInfo shaderStages[2] = { {},{} };

#ifdef USE_GLSL
		shaderStages[0] = loadShaderGLSL("data/shaders/triangle.vert", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShaderGLSL("data/shaders/triangle.frag", VK_SHADER_STAGE_FRAGMENT_BIT);
#else
		shaderStages[0] = loadShader("data/shaders/triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader("data/shaders/triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
#endif

		// Assign states
		// Two shader stages
		pipelineCreateInfo.stageCount = 2;
		// Assign pipeline state create information
		pipelineCreateInfo.pVertexInputState = vertexInputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pStages = shaderStages;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.pDynamicState = &dynamicState;

		// Create rendering pipeline
		err = vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipe_);
		if (err) throw error(err);

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
		  vkDestroyPipeline(dev_, pipe_, nullptr);
		  vkDestroyPipelineLayout(dev_, pipelineLayout, nullptr);
		  vkDestroyDescriptorSetLayout(dev_, descriptorSetLayout, nullptr);
    }
  }

  void allocateDescriptorSets(descriptorPool &descPool) {
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

    descriptorSet = nullptr;
		VkResult err = vkAllocateDescriptorSets(dev_, &allocInfo, &descriptorSet);
		if (err) throw error(err);
  }

  void updateDescriptorSets(buffer &uniformVS) {
		VkWriteDescriptorSet writeDescriptorSet = {};

		// Binding 0 : Uniform buffer
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &uniformVS.desc();
		// Binds this uniform buffer to binding point 0
		writeDescriptorSet.dstBinding = 0;

		vkUpdateDescriptorSets(dev_, 1, &writeDescriptorSet, 0, NULL);
  }

  VkPipeline pipe() { return pipe_; }
  VkPipelineLayout layout() const { return pipelineLayout; }
  VkDescriptorSet *descriptorSets() { return &descriptorSet; }
  VkDescriptorSetLayout *descriptorLayouts() { return &descriptorSetLayout; }

private:
  VkPipelineShaderStageCreateInfo loadShader(const char * fileName, VkShaderStageFlagBits stage)
  {
    std::ifstream input(fileName, std::ios::binary);
    auto &b = std::istreambuf_iterator<char>(input);
    auto &e = std::istreambuf_iterator<char>();
    std::vector<char> buf(b, e);

		VkShaderModuleCreateInfo moduleCreateInfo = {};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = buf.size();
		moduleCreateInfo.pCode = (uint32_t*)buf.data();
		moduleCreateInfo.flags = 0;
		VkShaderModule shaderModule;
		VkResult err = vkCreateShaderModule(dev_, &moduleCreateInfo, NULL, &shaderModule);
    if (err) throw error(err);

	  VkPipelineShaderStageCreateInfo shaderStage = {};
	  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	  shaderStage.stage = stage;
	  shaderStage.module = shaderModule;
	  shaderStage.pName = "main"; // todo : make param
	  shaderModules.push_back(shaderStage.module);
	  return shaderStage;
  }

  VkPipelineShaderStageCreateInfo loadShaderGLSL(const char * fileName, VkShaderStageFlagBits stage)
  {
    std::ifstream input(fileName, std::ios::binary);
    auto &b = std::istreambuf_iterator<char>(input);
    auto &e = std::istreambuf_iterator<char>();

    std::vector<char> buf;
    buf.resize(12);
		((uint32_t *)buf.data())[0] = 0x07230203; 
		((uint32_t *)buf.data())[1] = 0;
		((uint32_t *)buf.data())[2] = stage;
    while (b != e) buf.push_back(*b++);
    buf.push_back(0);

		VkShaderModuleCreateInfo moduleCreateInfo = {};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = buf.size();
		moduleCreateInfo.pCode = (uint32_t*)buf.data();
		moduleCreateInfo.flags = 0;

		VkShaderModule shaderModule;
		VkResult err = vkCreateShaderModule(dev_, &moduleCreateInfo, NULL, &shaderModule);
    if (err) throw error(err);

	  VkPipelineShaderStageCreateInfo shaderStage = {};
	  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	  shaderStage.stage = stage;
	  shaderStage.module = shaderModule;
	  shaderStage.pName = "main";
	  shaderModules.push_back(shaderStage.module);
	  return shaderStage;
  }

  VkPipeline pipe_ = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;
	VkDescriptorSet descriptorSet = nullptr;
	VkDescriptorSetLayout descriptorSetLayout = nullptr;
  VkDevice dev_ = nullptr;
	std::vector<VkShaderModule> shaderModules;
  bool ownsData = false;
};

class commandPool : public resource<VkCommandPool, commandPool> {
public:
  commandPool() : resource(nullptr, nullptr) {
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
	  VkResult err = vkCreateCommandPool(dev, &cmdPoolInfo, nullptr, &cmdPool);
    if (err) throw error(err);
    set(cmdPool, true);
  }

  void destroy() {
    if (get()) vkDestroyCommandPool(dev(), get(), nullptr);
  }

  commandPool &operator=(commandPool &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    return *this;
  }
};

class pipelineCache : public resource<VkPipelineCache, pipelineCache> {
public:
  pipelineCache() : resource(nullptr, nullptr) {
  }

  /// descriptor pool that does not own its pointer
  pipelineCache(VkPipelineCache value, VkDevice dev) : resource(value, dev) {
  }

  /// descriptor pool that does owns (and creates) its pointer
  pipelineCache(VkDevice dev) : resource(dev) {
	  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkPipelineCache cache;
	  VkResult err = vkCreatePipelineCache(dev, &pipelineCacheCreateInfo, nullptr, &cache);
    if (err) throw error(err);
    set(cache, true);
  }

  void destroy() {
    if (get()) vkDestroyPipelineCache(dev(), get(), nullptr);
  }

  pipelineCache &operator=(pipelineCache &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    return *this;
  }
};

class cmdBuffer : public resource<VkCommandBuffer, cmdBuffer> {
public:
  cmdBuffer() : resource(nullptr, nullptr) {
  }

  /// command buffer that does not own its pointer
  cmdBuffer(VkCommandBuffer value, VkDevice dev) : resource(value, dev) {
  }

  /// command buffer that does owns (and creates) its pointer
  cmdBuffer(VkDevice dev, VkCommandPool cmdPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) : resource(dev) {
    VkCommandBuffer res = nullptr;
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

  /// change the layout of 
  void addPrePresentationBarrier(VkImage image) const {
		// Add a present memory barrier to the end of the command buffer
		// This will transform the frame buffer color attachment to a
		// new layout for presenting it to the windowing system integration 
		VkImageMemoryBarrier prePresentBarrier = {};
		prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		prePresentBarrier.pNext = NULL;
		prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		prePresentBarrier.dstAccessMask = 0;
		prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		prePresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };			
		prePresentBarrier.image = image;

		VkImageMemoryBarrier *pMemoryBarrier = &prePresentBarrier;
		vkCmdPipelineBarrier(
			get(), 
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
			0,
			0, nullptr,
			0, nullptr,
			1, &prePresentBarrier);
  }

  
  /*void addPostPresentationBarrier(VkImage image) const {
	  VkImageMemoryBarrier postPresentBarrier = {};
	  postPresentBarrier.srcAccessMask = 0;
	  postPresentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	  postPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	  postPresentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	  postPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  postPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  postPresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	  postPresentBarrier.image = image;

	  vkCmdPipelineBarrier(
		  get(),
		  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		  0,
		  0, NULL, // No memory barriers,
		  0, NULL, // No buffer barriers,
		  1, &postPresentBarrier
    );
  }*/


  void endCommandBuffer() const {
    vkEndCommandBuffer(get());
  }

  void pipelineBarrier(VkImage image) const {
		// Add a post present image memory barrier
		// This will transform the frame buffer color attachment back
		// to it's initial layout after it has been presented to the
		// windowing system
		// See buildCommandBuffers for the pre present barrier that 
		// does the opposite transformation 
		VkImageMemoryBarrier postPresentBarrier = {};
		postPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		postPresentBarrier.pNext = NULL;
		postPresentBarrier.srcAccessMask = 0;
		postPresentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		postPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		postPresentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		postPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postPresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		postPresentBarrier.image = image;

		// Use dedicated command buffer from example base class for submitting the post present barrier
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		// Put post present barrier into command buffer
		vkCmdPipelineBarrier(
			get(),
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &postPresentBarrier
    );
  }

	// Create an image memory barrier for changing the layout of
	// an image and put it into an active command buffer
	// See chapter 11.4 "Image Layout" for details
	//todo : rename
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

		// Source layouts (old)

		// Undefined layout
		// Only allowed as initial layout!
		// Make sure any writes to the image have been finished
		if (oldImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		// Old layout is color attachment
		// Make sure any writes to the color buffer have been finished
		if (oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) 
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		// Old layout is transfer source
		// Make sure any reads from the image have been finished
		if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		// Old layout is shader read (sampler, input attachment)
		// Make sure any shader reads from the image have been finished
		if (oldImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}

		// Target layouts (new)

		// New layout is transfer destination (copy, blit)
		// Make sure any copyies to the image have been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		// New layout is transfer source (copy, blit)
		// Make sure any reads from and writes to the image have been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		// New layout is color attachment
		// Make sure any writes to the color buffer hav been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		// New layout is depth attachment
		// Make sure any writes to depth/stencil buffer have been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
		{
			imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		// New layout is shader read (sampler, input attachment)
		// Make sure any writes to the image have been finished
		if (newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}


		// Put barrier on top
		VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		// Put barrier inside setup command buffer
		vkCmdPipelineBarrier(
			get(), 
			srcStageFlags, 
			destStageFlags, 
			0, 
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
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
  VkCommandPool pool_ = nullptr;
};

inline void swapChain::build_images(VkCommandBuffer buf) {
  VkDevice d = dev();
  vku::cmdBuffer cb(buf, d);

  uint32_t imageCount = 0;
	VkResult err = vkGetSwapchainImagesKHR(dev(), get(), &imageCount, NULL);
	if (err) throw error(err);

  swapchainImages.resize(imageCount);
  swapchainViews.resize(imageCount);
	err = vkGetSwapchainImagesKHR(dev(), *this, &imageCount, swapchainImages.data());
	if (err) throw error(err);

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
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		colorAttachmentView.image = image(i);

		err = vkCreateImageView(dev(), &colorAttachmentView, nullptr, &swapchainViews[i]);
  	if (err) throw error(err);
  }
}


class semaphore : public resource<VkSemaphore, semaphore> {
public:
  /// semaphore that does not own its pointer
  semaphore(VkSemaphore value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// semaphore that does owns (and creates) its pointer
  semaphore(VkDevice dev) : resource(dev) {
  }

  VkSemaphore create(VkDevice dev) {
	  VkSemaphoreCreateInfo info = {};
	  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	  info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphore res = nullptr;
	  VkResult err = vkCreateSemaphore(dev, &info, nullptr, &res);
	  if (err) throw error(err);
    return res;
  }

  void destroy() {
    vkDestroySemaphore(dev(), get(), nullptr);
  }
};

class queue : public resource<VkQueue, queue> {
public:
  /// queue that does not own its pointer
  queue(VkQueue value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
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
    if (err) throw error(err);
  }

  void waitIdle() const {  
		VkResult err = vkQueueWaitIdle(get());
    if (err) throw error(err);
  }

  VkQueue create(VkDevice dev) {
    return nullptr;
  }

  void destroy() {
  }

};

class image : public resource<VkImage, image> {
public:
  /// image that does not own its pointer
  image(VkImage value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// image that does owns (and creates) its pointer
  image(VkDevice dev, uint32_t width, uint32_t height, VkFormat format=VK_FORMAT_R8G8B8_UNORM, VkImageType type=VK_IMAGE_TYPE_2D, VkImageUsageFlags usage=0) : resource(nullptr, dev) {
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

    VkImage result = nullptr;
	  VkResult err = vkCreateImage(dev, &image, nullptr, &result);
    if (err) throw error(err);

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
	  VkResult err = vkAllocateMemory(device, &mem_alloc, nullptr, &mem_);
    if (err) throw error(err);
  }

  /// bind device memory to the image object
  void bindMemoryToImage() {
  	VkResult err = vkBindImageMemory(dev(), get(), mem(), 0);
    if (err) throw error(err);
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
	  VkResult err = vkCreateImageView(dev(), &viewCreateInfo, nullptr, &view_);
    if (err) throw error(err);
  }

  void destroy() {
	  if (view()) {
      //vkDestroyImageView(dev(), view(), nullptr);
    }

	  if (mem()) {
      //vkFreeMemory(dev(), mem(), nullptr);
    }

	  if (get()) {
      //vkDestroyImage(dev(), get(), nullptr);
    }

    view_ = nullptr;
    mem_ = nullptr;
    set(nullptr, false);
  }

	VkDeviceMemory mem() const { return mem_; }
	VkImageView view() const { return view_; }

	VkDeviceMemory mem_ = nullptr;
	VkImageView view_ = nullptr;

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
  window(bool enableValidation) {
    // Check for validation command line flag
    #ifdef _WIN32
	    for (int32_t i = 0; i < __argc; i++)
	    {
		    if (__argv[i] == std::string("-validation"))
		    {
			    enableValidation = true;
		    }
	    }
    #endif

    #ifndef _WIN32
	    initxcbConnection();
    #endif

    instance = vku::instance("vku");

    vku::device dev = instance.device();
    device = dev;
    queue = instance.queue();

	  // Find a suitable depth format
	  depthFormat = dev.getSupportedDepthFormat();
	  assert(depthFormat != VK_FORMAT_UNDEFINED);
  }

  ~window() {
	  // Clean up Vulkan resources
	  swapChain.clear();

    cmdPool.clear();

    pipelineCache.clear();

    cmdPool.clear();

	  //vkDestroyDevice(device, nullptr); 
    //device.clear();

	  if (enableValidation)
	  {
		  //vkDebug::freeDebugCallback(instance);
	  }

    instance.clear();

    #ifndef _WIN32
	    xcb_destroy_window(connection, window);
	    xcb_disconnect(connection);
    #endif 
  }

  #ifdef _WIN32 
    HWND setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
    {
	    this->windowInstance = hinstance;

	    bool fullscreen = false;

	    // Check command line arguments
	    for (int32_t i = 0; i < __argc; i++)
	    {
		    if (__argv[i] == std::string("-fullscreen"))
		    {
			    fullscreen = true;
		    }
	    }

	    WNDCLASSEX wndClass;

	    wndClass.cbSize = sizeof(WNDCLASSEX);
	    wndClass.style = CS_HREDRAW | CS_VREDRAW;
	    wndClass.lpfnWndProc = wndproc;
	    wndClass.cbClsExtra = 0;
	    wndClass.cbWndExtra = 0;
	    wndClass.hInstance = hinstance;
	    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	    wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	    wndClass.lpszMenuName = NULL;
	    wndClass.lpszClassName = name.c_str();
	    wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

	    if (!RegisterClassEx(&wndClass))
	    {
		    std::cout << "Could not register window class!\n";
		    fflush(stdout);
		    exit(1);
	    }

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

		    if ((width != screenWidth) && (height != screenHeight))
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
		    windowRect.left = (long)screenWidth / 2 - width / 2;
		    windowRect.right = (long)width;
		    windowRect.top = (long)screenHeight / 2 - height / 2;
		    windowRect.bottom = (long)height;
	    }

	    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

	    window_ = CreateWindowEx(0,
		    name.c_str(),
		    title.c_str(),
		    //		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
		    dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		    windowRect.left,
		    windowRect.top,
		    windowRect.right,
		    windowRect.bottom,
		    NULL,
		    NULL,
		    hinstance,
		    NULL);

	    if (!window_) 
	    {
		    printf("Could not create window!\n");
		    fflush(stdout);
		    return 0;
		    exit(1);
	    }

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
		    PostQuitMessage(0);
		    break;
	    case WM_PAINT:
		    ValidateRect(window_, NULL);
		    break;
	    case WM_KEYDOWN:
		    switch (wParam)
		    {
		    case 0x50:
			    paused = !paused;
			    break;
		    case VK_ESCAPE:
			    exit(0);
			    break;
		    }
		    break;
	    case WM_RBUTTONDOWN:
	    case WM_LBUTTONDOWN:
		    mousePos.x = (float)LOWORD(lParam);
		    mousePos.y = (float)HIWORD(lParam);
		    break;
	    case WM_MOUSEMOVE:
		    if (wParam & MK_RBUTTON)
		    {
			    int32_t posx = LOWORD(lParam);
			    int32_t posy = HIWORD(lParam);
			    zoom += (mousePos.y - (float)posy) * .005f * zoomSpeed;
			    mousePos = glm::vec2((float)posx, (float)posy);
			    viewChanged();
		    }
		    if (wParam & MK_LBUTTON)
		    {
			    int32_t posx = LOWORD(lParam);
			    int32_t posy = HIWORD(lParam);
			    rotation.x += (mousePos.y - (float)posy) * 1.25f * rotationSpeed;
			    rotation.y -= (mousePos.x - (float)posx) * 1.25f * rotationSpeed;
			    mousePos = glm::vec2((float)posx, (float)posy);
			    viewChanged();
		    }
		    break;
	    }
    }
  #else // WIN32
    // Linux : Setup window 
    // TODO : Not finished...
    xcb_window_t VulkanExampleBase::setupWindow()
    {
	    uint32_t value_mask, value_list[32];

	    window = xcb_generate_id(connection);

	    value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	    value_list[0] = screen->black_pixel;
	    value_list[1] = XCB_EVENT_MASK_KEY_RELEASE |
		    XCB_EVENT_MASK_EXPOSURE |
		    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		    XCB_EVENT_MASK_POINTER_MOTION |
		    XCB_EVENT_MASK_BUTTON_PRESS |
		    XCB_EVENT_MASK_BUTTON_RELEASE;

	    xcb_create_window(connection,
		    XCB_COPY_FROM_PARENT,
		    window, screen->root,
		    0, 0, width, height, 0,
		    XCB_WINDOW_CLASS_INPUT_OUTPUT,
		    screen->root_visual,
		    value_mask, value_list);

	    /* Magic code that will send notification when window is destroyed */
	    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
	    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookie, 0);

	    xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
	    atom_wm_delete_window = xcb_intern_atom_reply(connection, cookie2, 0);

	    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
		    window, (*reply).atom, 4, 32, 1,
		    &(*atom_wm_delete_window).atom);

	    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
		    window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
		    title.size(), title.c_str());

	    free(reply);

	    xcb_map_window(connection, window);

	    return(window);
    }

    // Initialize XCB connection
    void VulkanExampleBase::initxcbConnection()
    {
	    const xcb_setup_t *setup;
	    xcb_screen_iterator_t iter;
	    int scr;

	    connection = xcb_connect(NULL, &scr);
	    if (connection == NULL) {
		    printf("Could not find a compatible Vulkan ICD!\n");
		    fflush(stdout);
		    exit(1);
	    }

	    setup = xcb_get_setup(connection);
	    iter = xcb_setup_roots_iterator(setup);
	    while (scr-- > 0)
		    xcb_screen_next(&iter);
	    screen = iter.data;
    }

    void VulkanExampleBase::handleEvent(const xcb_generic_event_t *event)
    {
	    switch (event->response_type & 0x7f)
	    {
	    case XCB_CLIENT_MESSAGE:
		    if ((*(xcb_client_message_event_t*)event).data.data32[0] ==
			    (*atom_wm_delete_window).atom) {
			    quit = true;
		    }
		    break;
	    case XCB_MOTION_NOTIFY:
	    {
		    xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
		    if (mouseButtons.left)
		    {
			    rotation.x += (mousePos.y - (float)motion->event_y) * 1.25f;
			    rotation.y -= (mousePos.x - (float)motion->event_x) * 1.25f;
			    viewChanged();
		    }
		    if (mouseButtons.right)
		    {
			    zoom += (mousePos.y - (float)motion->event_y) * .005f;
			    viewChanged();
		    }
		    mousePos = glm::vec2((float)motion->event_x, (float)motion->event_y);
	    }
	    break;
	    case XCB_BUTTON_PRESS:
	    {
		    xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
		    mouseButtons.left = (press->detail & XCB_BUTTON_INDEX_1);
		    mouseButtons.right = (press->detail & XCB_BUTTON_INDEX_3);
	    }
	    break;
	    case XCB_BUTTON_RELEASE:
	    {
		    xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
		    if (press->detail & XCB_BUTTON_INDEX_1)
			    mouseButtons.left = false;
		    if (press->detail & XCB_BUTTON_INDEX_3)
			    mouseButtons.right = false;
	    }
	    break;
	    case XCB_KEY_RELEASE:
	    {
		    const xcb_key_release_event_t *key =
			    (const xcb_key_release_event_t *)event;

		    if (key->detail == 0x9)
			    quit = true;
	    }
	    break;
	    case XCB_DESTROY_NOTIFY:
		    quit = true;
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


  void prepare() {
    VkSurfaceKHR surface = instance.createSurface((void*)windowInstance, (void*)window_);
    uint32_t queueNodeIndex = device.getGraphicsQueueNodeIndex(surface);
    if (queueNodeIndex == ~(uint32_t)0) throw(std::runtime_error("no graphics and present queue available"));
    auto sf = device.getSurfaceFormat(surface);
    //swapChain.colorFormat = sf.first;
    //swapChain.colorSpace = sf.second;

	  if (enableValidation) {
		  //vkDebug::setupDebugging(instance, VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT, NULL);
	  }

    cmdPool = vku::commandPool(device, queueNodeIndex);

    setupCmdBuffer = vku::cmdBuffer(device, cmdPool);
    setupCmdBuffer.beginCommandBuffer();

    swapChain = vku::swapChain(device, width, height, surface, setupCmdBuffer);
    width = swapChain.width();
    height = swapChain.height();

    assert(swapChain.imageCount() <= 2);

    for (size_t i = 0; i != swapChain.imageCount(); ++i) {
      drawCmdBuffers[i] = vku::cmdBuffer(device, cmdPool);
    }

    postPresentCmdBuffer = vku::cmdBuffer(device, cmdPool);

    depthStencil = vku::image(device, width, height, depthFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    depthStencil.allocate(device);
    depthStencil.bindMemoryToImage();
	  depthStencil.setImageLayout(setupCmdBuffer, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    depthStencil.createView();

    pipelineCache = vku::pipelineCache(device);
	  //createPipelineCache();

    swapChain.setupFrameBuffer(depthStencil.view(), depthFormat);

    setupCmdBuffer.endCommandBuffer();
    queue.submit(nullptr, setupCmdBuffer);
    queue.waitIdle();

	  // Recreate setup command buffer for derived class

    setupCmdBuffer = vku::cmdBuffer(device, cmdPool);
    setupCmdBuffer.beginCommandBuffer();

	  // Create a simple texture loader class 
	  //textureLoader = new vkTools::VulkanTextureLoader(instance.physicalDevice(), device, queue, cmdPool);
  }

  void renderLoop() {
  #ifdef _WIN32
	  MSG msg;
	  while (TRUE)
	  {
		  auto tStart = std::chrono::high_resolution_clock::now();
		  PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		  if (msg.message == WM_QUIT)
		  {
			  break;
		  }
		  else
		  {
			  TranslateMessage(&msg);
			  DispatchMessage(&msg);
		  }
		  render();
		  auto tEnd = std::chrono::high_resolution_clock::now();
		  auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		  frameTimer = (float)tDiff / 1000.0f;
		  // Convert to clamped timer value
		  if (!paused)
		  {
			  timer += timerSpeed * frameTimer;
			  if (timer > 1.0)
			  {
				  timer -= 1.0f;
			  }
		  }
	  }
  #else
	  xcb_flush(connection);
	  while (!quit)
	  {
		  auto tStart = std::chrono::high_resolution_clock::now();
		  xcb_generic_event_t *event;
		  event = xcb_poll_for_event(connection);
		  if (event) 
		  {
			  handleEvent(event);
			  free(event);
		  }
		  render();
		  auto tEnd = std::chrono::high_resolution_clock::now();
		  auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		  frameTimer = tDiff / 1000.0f;
  }
  #endif
  }
public:
	virtual void render() = 0;

	bool enableValidation = false;

	// Last frame time, measured using a high performance timer (if available)
	float frameTimer = 1.0f;
  vku::instance instance;
	vku::device device;
  vku::queue queue;
	VkFormat colorformat = VK_FORMAT_B8G8R8A8_UNORM;
	VkFormat depthFormat;
  vku::commandPool cmdPool;
  vku::cmdBuffer setupCmdBuffer;
  vku::cmdBuffer postPresentCmdBuffer;
  vku::cmdBuffer drawCmdBuffers[2];
	uint32_t currentBuffer = 0;
	vku::pipelineCache pipelineCache;
	vku::swapChain swapChain;
	bool prepared = false;
	uint32_t width = 1280;
	uint32_t height = 720;

	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

	float zoom = 0;

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;
	
	bool paused = false;

	// Use to adjust mouse rotation speed
	float rotationSpeed = 1.0f;
	// Use to adjust mouse zoom speed
	float zoomSpeed = 1.0f;

	glm::vec3 rotation = glm::vec3();
	glm::vec2 mousePos;

	std::string title = "Vulkan Example";
	std::string name = "vulkanExample";

  vku::image depthStencil;

  // OS specific 
  #ifdef _WIN32
	  HWND window_;
	  HINSTANCE windowInstance;
  #else
	  struct {
		  bool left = false;
		  bool right = false;
	  } mouseButtons;
	  bool quit;
	  xcb_connection_t *connection;
	  xcb_screen_t *screen;
	  xcb_window_t window_;
	  xcb_intern_atom_reply_t *atom_wm_delete_window;
  #endif	
};


} // vku

#endif
