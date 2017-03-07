////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016, 2017
//
// Vookoo: instance, wraps VkInstance
// 

#ifndef VKU_INSTANCE_INCLUDED
#define VKU_INSTANCE_INCLUDED

#include <vku/resource.hpp>
#include <vku/device.hpp>
#include <array>
#include <vector>
#include <string.h>

#ifndef VOOKOO_ENABLE_VALIDATION
  #ifndef NDEBUG
    #define VOOKOO_ENABLE_VALIDATION 1
  #endif
#endif

namespace vku {

class instance {
public:
  void destroy() {
    vkDestroyInstance(instance_, nullptr);
  }

  vku::device &device() { return dev_; }
  VkQueue queue() const { return queue_; }
  VkInstance get() const { return instance_; }

  // singleton, created on first use.
  static instance &singleton() {
    static instance theInstance;
    return theInstance;
  }

  ~instance() {
  }

  bool layerExists(const char *layerName) {
    for (VkLayerProperties &p : layerProperties_) {
      if (!strcmp(p.layerName, layerName)) {
        return true;
      }
    }
    return false;
  }

  bool instanceExtensionExists(const char *extensionName) {
    for (VkExtensionProperties &p : instanceExtensionProperties_) {
      if (!strcmp(p.extensionName, extensionName)) {
        return true;
      }
    }
    return false;
  }

  bool deviceExtensionExists(const char *extensionName) {
    for (VkExtensionProperties &p : deviceExtensionProperties_) {
      if (!strcmp(p.extensionName, extensionName)) {
        return true;
      }
    }
    return false;
  }

  uint32_t graphicsQueueIndex() const {
    return graphicsQueueIndex_;
  }

private:
  // There is only one instance. It is created with a singleton instance::get()
  instance() {
    // start by doing a full dump of layers and extensions enabled in Vulkan.
    uint32_t num_layers = 0;
    vkEnumerateInstanceLayerProperties(&num_layers, nullptr);
    layerProperties_.resize(num_layers);
    vkEnumerateInstanceLayerProperties(&num_layers, layerProperties_.data());

    for (auto &p : layerProperties_) {
      uint32_t num_exts = 0;
      vkEnumerateInstanceExtensionProperties(p.layerName, &num_exts, nullptr);
      size_t old_size = instanceExtensionProperties_.size();
      instanceExtensionProperties_.resize(old_size + num_exts);
      vkEnumerateInstanceExtensionProperties(p.layerName, &num_exts, instanceExtensionProperties_.data() + old_size);
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "vku";
    appInfo.pEngineName = "vku";

    // Temporary workaround for drivers not supporting SDK 1.0.3 upon launch
    // todo : Use VK_API_VERSION 
    appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 2);

    std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };
    std::vector<const char*> instanceLayers;

    #if defined(VK_KHR_WIN32_SURFACE_EXTENSION_NAME)
      instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(VK_KHR_XCB_SURFACE_EXTENSION_NAME)
      instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    #elif defined(VK_KHR_MIR_SURFACE_EXTENSION_NAME)
      instanceExtensions.push_back(VK_KHR_MIR_SURFACE_EXTENSION_NAME);
    #endif

    // todo : check if all extensions are present

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    if (enableValidation) {
      instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
      instanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
    }

    // todo: filter out extensions and layers that do not exist.
    instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    instanceCreateInfo.enabledLayerCount = (uint32_t)instanceLayers.size();
    instanceCreateInfo.ppEnabledLayerNames = instanceLayers.data();
    VkResult err = vkCreateInstance(&instanceCreateInfo, nullptr, &instance_);
    if (err) {
      #ifdef _WIN32
        MessageBox(nullptr, "Could not open the vulkan driver", "oops", MB_ICONHAND);
      #endif
    }
    if (err) throw error(err, __FILE__, __LINE__);

    // Physical device
    uint32_t gpuCount = 0;
    // Get number of available physical devices
    err = vkEnumeratePhysicalDevices(instance_, &gpuCount, nullptr);
    if (err) throw error(err, __FILE__, __LINE__);

    if (gpuCount == 0) {
      throw(std::runtime_error("no Vulkan devices found"));
    }

    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    err = vkEnumeratePhysicalDevices(instance_, &gpuCount, physicalDevices.data());
    if (err) throw error(err, __FILE__, __LINE__);

    // Note : 
    // This example will always use the first physical device reported, 
    // change the vector index if you have multiple Vulkan devices installed 
    // and want to use another one
    // Find a queue that supports graphics operations
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[0], &queueCount, nullptr);
    //assert(queueCount >= 1);

    std::vector<VkQueueFamilyProperties> queueProps;
    queueProps.resize(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[0], &queueCount, queueProps.data());

    uint32_t qi = 0;
    graphicsQueueIndex_ = ~(uint32_t)0;
    computeQueueIndex_ = ~(uint32_t)0;
    for (qi = 0; qi < queueCount; qi++)
    {
      if (queueProps[qi].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        graphicsQueueIndex_ = qi;
      }
      if (queueProps[qi].queueFlags & VK_QUEUE_COMPUTE_BIT) {
        computeQueueIndex_ = qi;
      }
    }
    if (graphicsQueueIndex_ == ~(uint32_t)0) std::runtime_error("no graphics queue found");

    // Vulkan device
    std::array<float, 1> queuePriorities = { 0.0f };
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueIndex_;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = queuePriorities.data();

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = nullptr;

	  std::vector<const char *> deviceLayers;
	  std::vector<const char *> extensions;

    extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    if (enableValidation) {
      deviceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
    }

    deviceCreateInfo.enabledLayerCount = (uint32_t)deviceLayers.size();
    deviceCreateInfo.ppEnabledLayerNames = deviceLayers.data();
    deviceCreateInfo.enabledExtensionCount = (uint32_t)extensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = extensions.data();

    VkDevice dev = VK_NULL_HANDLE;
    err = vkCreateDevice(physicalDevices[0], &deviceCreateInfo, nullptr, &dev);
    if (err) throw error(err, __FILE__, __LINE__);

    dev_ = vku::device(dev, physicalDevices[0]);

    if (enableValidation) {
      auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT");
      if (vkCreateDebugReportCallbackEXT) {
        VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
        callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        callbackCreateInfo.flags =
          VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
          VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT
        ;
        callbackCreateInfo.pfnCallback = &debugCallback;

        VkDebugReportCallbackEXT callback = VK_NULL_HANDLE;
        err = vkCreateDebugReportCallbackEXT(instance_, &callbackCreateInfo, nullptr, &callback);
        if (err) throw error(err, __FILE__, __LINE__);
      }
    }

    // Get the graphics queue
    vkGetDeviceQueue(dev_, graphicsQueueIndex_, 0, &queue_);
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData
  ) {
    printf("debugCallback: %s\n", pMessage);
    return VK_FALSE;
  }

  #ifdef VOOKOO_ENABLE_VALIDATION
    bool enableValidation = true;
  #else
    bool enableValidation = false;
  #endif
  vku::device dev_;
  VkQueue queue_;
  VkInstance instance_;

  uint32_t graphicsQueueIndex_;
  uint32_t computeQueueIndex_;

  std::vector<VkLayerProperties> layerProperties_;
  std::vector<VkExtensionProperties> instanceExtensionProperties_;
  std::vector<VkExtensionProperties> deviceExtensionProperties_;
};

} // vku

#endif
