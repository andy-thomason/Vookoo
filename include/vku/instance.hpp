////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: instance, wraps VkInstance
// 

#ifndef VKU_INSTANCE_INCLUDED
#define VKU_INSTANCE_INCLUDED

#include <vku/resource.hpp>
#include <vku/device.hpp>
#include <array>
#include <vector>

namespace vku {

class instance {
public:
  VkSurfaceKHR createSurface(void *window, void *connection) {
    VkSurfaceKHR result = VK_NULL_HANDLE;
    // Create surface depending on OS
    #if defined(_WIN32)
      VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.hinstance = (HINSTANCE)connection;
      surfaceCreateInfo.hwnd = (HWND)window;
      VkResult err = vkCreateWin32SurfaceKHR(instance_, &surfaceCreateInfo, VK_NULL_HANDLE, &result);
    #elif defined(__ANDROID__)
      VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.window = window;
      VkResult err = vkCreateAndroidSurfaceKHR(instance_, &surfaceCreateInfo, NULL, &result);
    #else
      VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.connection = connection;
      surfaceCreateInfo.window = (xcb_window_t)(intptr_t)window;
      VkResult err = vkCreateXcbSurfaceKHR(instance_, &surfaceCreateInfo, VK_NULL_HANDLE, &result);
    #endif
    if (err) throw error(err, __FILE__, __LINE__);
    return result;
  }

  void destroy() {
    vkDestroyInstance(instance_, VK_NULL_HANDLE);
  }

  VkPhysicalDevice physicalDevice() const { return physicalDevice_; }

  vku::device device() const { return vku::device(dev_, physicalDevice_); }
  VkQueue queue() const { return queue_; }
  VkInstance inst() const { return instance_; }

  // singleton, created on first use.
  static instance &get() {
    static instance theInstance;
    return theInstance;
  }

private:
  // There is only one instance. It is created with a singleton instance::get()
  instance() {
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
      instanceCreateInfo.enabledLayerCount = sizeof(validationLayerNames)/sizeof(validationLayerNames[0]);
      instanceCreateInfo.ppEnabledLayerNames = validationLayerNames;
    }

    VkResult err = vkCreateInstance(&instanceCreateInfo, VK_NULL_HANDLE, &instance_);
    if (err) {
      #ifdef _WIN32
        MessageBox(NULL, "Could not open the vulkan driver", "oops", MB_ICONHAND);
      #endif
    }
    if (err) throw error(err, __FILE__, __LINE__);

    // Physical device
    uint32_t gpuCount = 0;
    // Get number of available physical devices
    err = vkEnumeratePhysicalDevices(instance_, &gpuCount, VK_NULL_HANDLE);
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

  bool enableValidation = false;
  VkPhysicalDevice physicalDevice_;
  VkDevice dev_;
  VkQueue queue_;
  VkInstance instance_;
};

} // vku

#endif
