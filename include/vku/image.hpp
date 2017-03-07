////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: 
// 

#ifndef VKU_IMAGE_INCLUDED
#define VKU_IMAGE_INCLUDED

#include <vku/resource.hpp>
#include <stdexcept>

namespace vku {

class imageLayoutHelper {
public:
  imageLayoutHelper(uint32_t width=0, uint32_t height=0) {
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
  imageLayoutHelper &aspectMask(VkImageAspectFlagBits value) { aspectMask_ = value; return *this; }

  auto format() const { return info_.format; }
  auto width() const { return info_.extent.width; }
  auto height() const { return info_.extent.height; }
  auto depth() const { return info_.extent.depth; }
  auto mipLevels() const { return info_.mipLevels; }
  auto arrayLayers() const { return info_.arrayLayers; }
  auto samples() const { return info_.samples; }
  auto tiling() const { return info_.tiling; }
  auto flags() const { return info_.flags; }
  auto usage() const { return info_.usage; }
  auto sharingMode() const { return info_.sharingMode; }
  auto initialLayout() const { return info_.initialLayout; }
  auto memoryPropertyFlag() const { return memoryPropertyFlag_; }
  auto aspectMask() const { return aspectMask_; }

  VkImageCreateInfo *get() { return &info_; }
private:
  VkImageCreateInfo info_ = {};
  VkMemoryPropertyFlagBits memoryPropertyFlag_ = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkImageAspectFlagBits aspectMask_ = VK_IMAGE_ASPECT_COLOR_BIT;
};

class image : public resource<VkImage, image> {
public:
  VKU_RESOURCE_BOILERPLATE(VkImage, image)

  /// image that does own (and creates) its pointer
  image(const vku::device &device, imageLayoutHelper &layout) : resource(VK_NULL_HANDLE, device) {
    VkImage result = VK_NULL_HANDLE;
    VkResult err = vkCreateImage(dev(), layout.get(), nullptr, &result);
    if (err) throw error(err, __FILE__, __LINE__);

    set(result, true);
    format_ = layout.format();

    allocate(device, layout);
    bindMemoryToImage();
    uint32_t view_usage = VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (layout.usage() & view_usage) {
      //debugCallback: Invalid usage flag for image 0x1a used by vkCreateImageView(). In this case, image should have VK_IMAGE_USAGE_[SAMPLED|STORAGE|COLOR_ATTACHMENT]_BIT set during creation.
      createView(layout);
    }
  }

  void destroy() {
    if (view()) {
      vkDestroyImageView(dev(), view(), nullptr);
    }

    if (mem()) {
      vkFreeMemory(dev(), mem(), nullptr);
    }

    if (get()) {
      vkDestroyImage(dev(), get(), nullptr);
    }

    view_ = VK_NULL_HANDLE;
    mem_ = VK_NULL_HANDLE;
    set(VK_NULL_HANDLE, false);
  }

  VkDeviceMemory mem() const { return mem_; }
  VkImageView view() const { return view_; }

  void *map() const {
    void *dest = nullptr;
    VkResult err = vkMapMemory(dev(), mem_, 0, size(), 0, &dest);
    if (err) throw error(err, __FILE__, __LINE__);
    return dest;
  }

  void unmap() const {
    vkUnmapMemory(dev(), mem_);
  }

  size_t size() const {
    return size_;
  }

  // https://en.wikipedia.org/wiki/BMP_file_format
  template <class Writer>
  void writeBMP(int width, int height, Writer &wr) const {
    if (format_ != VK_FORMAT_R8G8B8A8_UNORM) {
      throw std::runtime_error("can't write this format");
    }
    VkImageSubresource sr = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(dev(), get(), &sr, &layout);

    static const uint8_t hdr_proto[] = {
      0x42,0x4d,0xaa,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7a,0x00,0x00,0x00,0x6c,0x00,0x00,0x00,
      0x04,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x01,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x30,0x00,
      0x00,0x00,0x13,0x0b,0x00,0x00,0x13,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
      0x42,0x47,0x52,0x73,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    };
    uint8_t hdr[sizeof(hdr_proto)];
    memcpy(hdr, hdr_proto, sizeof(hdr_proto));
    le4(hdr + 0x2, width * height * 3 + sizeof(hdr));
    le4(hdr + 0x12, width);
    le4(hdr + 0x16, height);
    wr((const char*)hdr, sizeof(hdr));
    uint8_t *src = (uint8_t *)map() + layout.offset;
    std::vector<char> line(width*3);
    for (int y = 0; y != height; ++y) {
      for (int x = 0; x != width; ++x) {
        line[x*3+0] = src[x*4+2];
        line[x*3+1] = src[x*4+1];
        line[x*3+2] = src[x*4+0];
      }
      wr(line.data(), width*3);
      src += layout.rowPitch;
    }
    unmap();
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

private:
  static uint8_t *le4(uint8_t *d, int value) {
    *d++ = (uint8_t)value;
    *d++ = (uint8_t)(value >> 8);
    *d++ = (uint8_t)(value >> 16);
    *d++ = (uint8_t)(value >> 24);
    return d;
  }

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

  void move(image &&rhs) {
    (resource&)*this = (resource&&)rhs;
    format_ = rhs.format_;
    size_ = rhs.size_;
    mem_ = rhs.mem_;
    view_ = rhs.view_;
  }

  void copy(const image &rhs) {
    (resource&)*this = (const resource&)rhs;
    format_ = rhs.format_;
    size_ = rhs.size_;
    mem_ = rhs.mem_;
    view_ = rhs.view_;
  }

  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkDeviceMemory mem_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;
  size_t size_ = 0;
};

} // vku

#endif
