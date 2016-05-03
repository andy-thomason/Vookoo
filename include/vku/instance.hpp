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

class instance : public resource<VkInstance, instance> {
public:
  instance() : resource((VkInstance)VK_NULL_HANDLE) {
  }

  /// instance that does not own its pointer
  instance(VkInstance value) : resource(value, VK_NULL_HANDLE) {
  }

  /// instance that does owns (and creates) its pointer
  instance(const char *name, bool enableValidation = false) : resource((VkDevice)VK_NULL_HANDLE) {
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

    VkInstance inst = VK_NULL_HANDLE;
    VkResult err = vkCreateInstance(&instanceCreateInfo, VK_NULL_HANDLE, &inst);
    if (err) {
      #ifdef _WIN32
        MessageBox(NULL, "Could not open the vulan driver", "oops", MB_ICONHAND);
      #endif
    }
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

  VkSurfaceKHR createSurface(void *window, void *connection) {
    VkSurfaceKHR result = VK_NULL_HANDLE;
    // Create surface depending on OS
    #if defined(_WIN32)
      VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.hinstance = (HINSTANCE)connection;
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
      surfaceCreateInfo.connection = connection;
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


} // vku

#endif
