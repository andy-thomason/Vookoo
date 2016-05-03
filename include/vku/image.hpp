////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: main include file
// 

#ifndef VKU_IMAGE_INCLUDED
#define VKU_IMAGE_INCLUDED


#ifdef _WIN32
  #define VK_USE_PLATFORM_WIN32_KHR 1
  #pragma comment(lib, "vulkan-1.lib")
  #define _CRT_SECURE_NO_WARNINGS
#else
  #define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vku/resource.hpp>

// derived from https://github.com/SaschaWillems/Vulkan
//
// Many thanks to Sascha, without who this would be a challenge!

namespace vku {

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

  void setImageLayout(const vku::commandBuffer &cmdBuf, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout) {
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

} // vku

#endif
