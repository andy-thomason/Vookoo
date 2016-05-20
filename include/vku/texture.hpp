////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: texture wrapper class
// 

#ifndef VKU_TEXTURE_INCLUDED
#define VKU_TEXTURE_INCLUDED

#include <vku/instance.hpp>
#include <vku/image.hpp>
#include <algorithm>
#include <type_traits>

namespace vku {

class texture {
public:
  /// empty texture
  texture() {
  }

  /// texture defined by layout
  texture(const vku::device &device, imageLayoutHelper &layout, void *pixels, size_t size) : layout_(layout) {
    layout.usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    layout.tiling(VK_IMAGE_TILING_LINEAR);
    layout.memoryPropertyFlag(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    gpuImage_ = vku::image(device, layout);

    // create a host texture with the layout
    layout.usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    layout.format(layout.format());
    layout.memoryPropertyFlag(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    hostImage_ = vku::image(device, layout);

    uint8_t *dest = (uint8_t *)hostImage_.map();
    memcpy(dest, pixels, std::min(size, hostImage_.size()));
    hostImage_.unmap();
  }

  /// move constructor
  texture(texture &&rhs) {
    move(std::move(rhs));
  }

  /// move operator
  texture &operator=(texture &&rhs) {
    move(std::move(rhs));
    return *this;
  }

  /// copy constructor
  texture(const texture &rhs) {
    copy(rhs);
  }

  /// copy operator
  texture &operator=(const texture &rhs) {
    copy(rhs);
    return *this;
  }

  // add uploading commands for this texture to a command buffer
  void upload(const vku::commandBuffer &cmdBuf) const {
    VkImageAspectFlagBits aspectMask = layout_.aspectMask();
    cmdBuf.setImageLayout(hostImage_, aspectMask, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    cmdBuf.setImageLayout(gpuImage_, aspectMask, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // todo: upload multiple mipmaps and array layers
    // todo: cope with multiple aspect masks
    VkImageCopy cpy = {};
    cpy.srcSubresource.aspectMask = aspectMask;
    cpy.srcSubresource.baseArrayLayer = 0;
    cpy.srcSubresource.layerCount = 1;
    cpy.srcSubresource.mipLevel = 0;
    cpy.dstSubresource.aspectMask = aspectMask;
    cpy.dstSubresource.baseArrayLayer = 0;
    cpy.dstSubresource.layerCount = 1;
    cpy.dstSubresource.mipLevel = 0;
    cpy.extent = { layout_.width(), layout_.height(), layout_.depth() };

    cmdBuf.copyImage(hostImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, gpuImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);
    cmdBuf.setImageLayout(gpuImage_, aspectMask, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  const vku::image &gpuImage() const { return gpuImage_; }
  const vku::image &hostImage() const { return hostImage_; }
private:
  void copy(const texture &rhs) {
    hostImage_ = rhs.hostImage_;
    gpuImage_ = rhs.gpuImage_;
    layout_ = rhs.layout_;
  }

  void move(texture &&rhs) {
    hostImage_ = std::move(rhs.hostImage_);
    gpuImage_ = std::move(rhs.gpuImage_);
    layout_ = std::move(rhs.layout_);
  }

  vku::image hostImage_;
  vku::image gpuImage_;
  imageLayoutHelper layout_;
};

} // vku

#endif
