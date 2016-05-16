////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: device, wraps VkDevice
// 

#ifndef VKU_DEVICE_INCLUDED
#define VKU_DEVICE_INCLUDED

#include <vector>

namespace vku {

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


} // vku

#endif
