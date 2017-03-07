////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: image target class: for operating vulkan on servers.
// 

#ifndef VKU_IMAGE_TARGET_INCLUDED
#define VKU_IMAGE_TARGET_INCLUDED

#include <vku/device.hpp>
#include <vku/queue.hpp>
#include <vku/commandPool.hpp>
#include <vku/commandBuffer.hpp>
#include <vku/image.hpp>

namespace vku {
class imageTarget {
public:
  imageTarget(const vku::device &device, uint32_t width, uint32_t height) : device_(device), width_(width), height_(height) {
    queue_ = vku::queue(vku::instance::singleton().queue(), device_);
    uint32_t queueFamilyIndex = vku::instance::singleton().graphicsQueueIndex();
    cmdPool_ = vku::commandPool(device_, queueFamilyIndex);
    preRenderBuffer_ = vku::commandBuffer(device_, cmdPool_);
    postRenderBuffer_ = vku::commandBuffer(device_, cmdPool_);

    vku::imageLayoutHelper backBufferLayout(width, height);
    backBufferLayout.format(VK_FORMAT_R8G8B8A8_UNORM);
    backBufferLayout.tiling(VK_IMAGE_TILING_OPTIMAL);
    backBufferLayout.usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    backBufferLayout.initialLayout(VK_IMAGE_LAYOUT_UNDEFINED);

    backBuffers_[0] = vku::image(device_, backBufferLayout);
    backBuffers_[1] = vku::image(device_, backBufferLayout);
    commandBuffers_[0] = vku::commandBuffer(device_, cmdPool_);
    commandBuffers_[1] = vku::commandBuffer(device_, cmdPool_);

    vku::imageLayoutHelper depthBufferLayout(width, height);
    depthBufferLayout.format(VK_FORMAT_D24_UNORM_S8_UINT);
    depthBufferLayout.tiling(VK_IMAGE_TILING_OPTIMAL);
    depthBufferLayout.usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    depthBufferLayout.initialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
    depthBuffer_ = vku::image(device_, depthBufferLayout);

    vku::imageLayoutHelper readBufferLayout(width, height);
    readBufferLayout.format(VK_FORMAT_R8G8B8A8_UNORM);
    readBufferLayout.tiling(VK_IMAGE_TILING_LINEAR);
    readBufferLayout.usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    readBufferLayout.initialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
    readBufferLayout.memoryPropertyFlag((VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    readBuffer_ = vku::image(device_, readBufferLayout);

    vku::renderPassLayout renderPassLayout;
    uint32_t color = renderPassLayout.addAttachment(VK_FORMAT_R8G8B8A8_UNORM);
    uint32_t depth = renderPassLayout.addAttachment(VK_FORMAT_D24_UNORM_S8_UINT);
    renderPassLayout.addSubpass(color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    renderPass_ = vku::renderPass(device_, renderPassLayout);
    vku::pipelineCache pipelineCache(device_);

    frameBuffers_[0] = vku::framebuffer(device_, backBuffers_[0], depthBuffer_, renderPass_, width, height);
    frameBuffers_[1] = vku::framebuffer(device_, backBuffers_[1], depthBuffer_, renderPass_, width, height);

    preRenderBuffer_.beginCommandBuffer();
    preRenderBuffer_.setImageLayout(backBuffers_[0], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    preRenderBuffer_.setImageLayout(backBuffers_[1], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    preRenderBuffer_.setImageLayout(depthBuffer_, VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    preRenderBuffer_.endCommandBuffer();

    queue_.submit(preRenderBuffer_);
  }

  void copyToReadBuffer(size_t buffer) {
    postRenderBuffer_.beginCommandBuffer();
    postRenderBuffer_.setImageLayout(backBuffers_[buffer], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
    postRenderBuffer_.setImageLayout(readBuffer_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

    VkImageCopy cpy = {};
    cpy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpy.srcSubresource.baseArrayLayer = 0;
    cpy.srcSubresource.layerCount = 1;
    cpy.srcSubresource.mipLevel = 0;
    cpy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpy.dstSubresource.baseArrayLayer = 0;
    cpy.dstSubresource.layerCount = 1;
    cpy.dstSubresource.mipLevel = 0;
    cpy.extent = { width_, height_, 1 };
    postRenderBuffer_.copyImage(backBuffers_[buffer], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readBuffer_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);
    //readBuffer.copy
    postRenderBuffer_.endCommandBuffer();

    queue_.submit(postRenderBuffer_);

    device().waitIdle();
  }


  const vku::device &device() const { return device_; }
  const vku::queue &queue() const { return queue_; }
  const vku::commandPool &cmdPool() const { return cmdPool_; }
  const vku::framebuffer &frameBuffers(size_t i) const { return frameBuffers_[i]; }
  const vku::commandBuffer &commandBuffers(size_t i) const { return commandBuffers_[i]; }
  const vku::image &backBuffers(size_t i) const { return backBuffers_[i]; }
  const vku::renderPass &renderPass() const { return renderPass_; }
  const vku::pipelineCache &pipelineCache() const { return pipelineCache_; }
  const vku::image &readBuffer() const { return readBuffer_; }

private:
  vku::device device_;

  vku::commandPool cmdPool_;
  vku::queue queue_;
  vku::commandBuffer preRenderBuffer_;
  vku::commandBuffer postRenderBuffer_;
  vku::image backBuffers_[2];
  std::array<vku::framebuffer, 2> frameBuffers_;
  vku::image depthBuffer_;
  vku::image readBuffer_;
  std::array<vku::commandBuffer, 2> commandBuffers_;
  vku::renderPass renderPass_;
  vku::pipelineCache pipelineCache_;

  uint32_t width_;
  uint32_t height_;
};

} // vku

#endif
