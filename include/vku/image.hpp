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
#include <vku/commandBuffer.hpp>
#include <stdexcept>

// derived from https://github.com/SaschaWillems/Vulkan
//
// Many thanks to Sascha, without who this would be a challenge!

namespace vku {

class imageLayoutHelper {
public:
  imageLayoutHelper(uint32_t width, uint32_t height) {
    info_.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info_.imageType = VK_IMAGE_TYPE_2D;
    info_.format = VK_FORMAT_R8G8B8_UNORM;
    info_.extent = { width, height, 1 };
    info_.mipLevels = 1;
    info_.arrayLayers = 1;
    info_.samples = VK_SAMPLE_COUNT_1_BIT;
    info_.tiling = VK_IMAGE_TILING_LINEAR;
    info_.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    info_.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info_.flags = 0;
    info_.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  }

  imageLayoutHelper &format(VkFormat value) { info_.format = value; return *this; }
  imageLayoutHelper &width(uint32_t value) { info_.extent.width = value; return *this; }
  imageLayoutHelper &height(uint32_t value) { info_.extent.height = value; return *this; }
  imageLayoutHelper &depth(uint32_t value) { info_.extent.depth = value; return *this; }
  imageLayoutHelper &mipLevels(uint32_t value) { info_.mipLevels = value; return *this; }
  imageLayoutHelper &arrayLayers(uint32_t value) { info_.arrayLayers = value; return *this; }
  imageLayoutHelper &samples(VkSampleCountFlagBits value) { info_.samples = value; return *this; }
  imageLayoutHelper &tiling(VkImageTiling value) { info_.tiling = value; return *this; }
  imageLayoutHelper &flags(VkImageCreateFlags value) { info_.flags = value; return *this; }
  imageLayoutHelper &usage(VkImageUsageFlags value) { info_.usage = value; return *this; }
  imageLayoutHelper &sharingMode(VkSharingMode value) { info_.sharingMode = value; return *this; }
  imageLayoutHelper &initialLayout(VkImageLayout value) { info_.initialLayout = value; return *this; }
  imageLayoutHelper &memoryPropertyFlag(VkMemoryPropertyFlagBits value) { memoryPropertyFlag_ = value; return *this; }

  auto format() { return info_.format; }
  auto width() { return info_.extent.width; }
  auto height() { return info_.extent.height; }
  auto depth() { return info_.extent.depth; }
  auto mipLevels() { return info_.mipLevels; }
  auto arrayLayers() { return info_.arrayLayers; }
  auto samples() { return info_.samples; }
  auto tiling() { return info_.tiling; }
  auto flags() { return info_.flags; }
  auto usage() { return info_.usage; }
  auto sharingMode() { return info_.sharingMode; }
  auto initialLayout() { return info_.initialLayout; }
  auto memoryPropertyFlag() { return memoryPropertyFlag_; }

  VkImageCreateInfo *get() { return &info_; }
private:
  VkImageCreateInfo info_ = {};
  VkMemoryPropertyFlagBits memoryPropertyFlag_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
};

class image : public resource<VkImage, image> {
public:
  /// image that does not own its pointer
  image(VkImage value = VK_NULL_HANDLE, VkDevice dev = VK_NULL_HANDLE) : resource(value, dev) {
  }

  /// image that does own (and creates) its pointer
  image(const vku::device &device, imageLayoutHelper &layout) : resource(VK_NULL_HANDLE, device) {
    VkImage result = VK_NULL_HANDLE;
    VkResult err = vkCreateImage(dev(), layout.get(), nullptr, &result);
    if (err) throw error(err, __FILE__, __LINE__);

    set(result, true);
    format_ = layout.format();

    allocate(device, layout);
    bindMemoryToImage();
    createView(layout);
  }

  void setImageLayout(const vku::commandBuffer &cmdBuf, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout) {
    cmdBuf.setImageLayout(get(), aspectMask, oldImageLayout, newImageLayout);
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

    view_ = VK_NULL_HANDLE;
    mem_ = VK_NULL_HANDLE;
    set(VK_NULL_HANDLE, false);
  }

  VkDeviceMemory mem() const { return mem_; }
  VkImageView view() const { return view_; }

  void *map() {
    void *dest = nullptr;
    VkResult err = vkMapMemory(dev(), mem_, 0, size(), 0, &dest);
    if (err) throw error(err, __FILE__, __LINE__);
    return dest;
  }

  void unmap() {
    vkUnmapMemory(dev(), mem_);
  }

  size_t size() const {
    return size_;
  }

  image(image &rhs) = default;

  image &operator=(image &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    format_ = rhs.format_;
    mem_ = rhs.mem_;
    view_ = rhs.view_;
    size_ = rhs.size_;
    return *this;
  }
public:
  /// allocate device memory
  void allocate(const vku::device &device, imageLayoutHelper &layout) {
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, get(), &memReqs);

    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.allocationSize = memReqs.size;
    mem_alloc.memoryTypeIndex = device.getMemoryType(memReqs.memoryTypeBits, layout.memoryPropertyFlag());
    if (mem_alloc.memoryTypeIndex == ~(uint32_t)0) {
      throw std::runtime_error("image: can't find correct memory properties");
    }
    VkResult err = vkAllocateMemory(device, &mem_alloc, nullptr, &mem_);
    if (err) throw error(err, __FILE__, __LINE__);

    size_ = (size_t)memReqs.size;
  }

  /// bind device memory to the image object
  void bindMemoryToImage() {
    VkResult err = vkBindImageMemory(dev(), get(), mem(), 0);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  /// todo: generalise
  void createView(imageLayoutHelper &layout) {
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format_;
    viewCreateInfo.flags = 0;
    viewCreateInfo.subresourceRange = {};
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange.aspectMask = layout.usage() & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.image = get();
    VkResult err = vkCreateImageView(dev(), &viewCreateInfo, nullptr, &view_);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  bool isDepthFormat() const {
    switch (format_) {
      case VK_FORMAT_D16_UNORM:
      case VK_FORMAT_X8_D24_UNORM_PACK32:
      case VK_FORMAT_D32_SFLOAT:
      case VK_FORMAT_D16_UNORM_S8_UINT:
      case VK_FORMAT_D24_UNORM_S8_UINT:
      case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
      default:
        return false;
    }
  }

  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkDeviceMemory mem_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;
  size_t size_ = 0;
};

} // vku

#endif
