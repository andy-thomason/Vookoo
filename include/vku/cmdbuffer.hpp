////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: command buffer wraps VkCommandBuffer
// 

#ifndef VKU_COMMAND_BUFFER_INCLUDED
#define VKU_COMMAND_BUFFER_INCLUDED

#include <vku/resource.hpp>

namespace vku {

class cmdBuffer : public resource<VkCommandBuffer, cmdBuffer> {
public:
  cmdBuffer() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// command buffer that does not own its pointer
  cmdBuffer(VkCommandBuffer value, VkDevice dev) : resource(value, dev) {
  }

  /// command buffer that does owns (and creates) its pointer
  cmdBuffer(VkDevice dev, VkCommandPool cmdPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) : resource(dev) {
    VkCommandBuffer res = VK_NULL_HANDLE;
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

  // Create an image memory barrier for changing the layout of
  // an image and put it into an active command buffer
  // See chapter 11.4 "Image Layout" for details
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

    // Put barrier on top
    VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    switch (oldImageLayout) {
      case VK_IMAGE_LAYOUT_UNDEFINED: {
        // Undefined layout
        // Only allowed as initial layout!
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      } break;
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
        // Old layout is color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      } break;
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
        // Old layout is transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      } break;
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
        // Old layout is shader read (sampler, input attachment)
        // Make sure any shader reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      } break;
      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
        // change the layout back from the surface format to the rendering format.
        srcStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      } break;
      default: {
        throw(std::runtime_error("setImageLayout: unsupported source layout"));
      }
    }

    // Target layouts (new)
    switch (newImageLayout) {
      // New layout is transfer destination (copy, blit)
      // Make sure any copyies to the image have been finished
      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      } break;

      // New layout is transfer source (copy, blit)
      // Make sure any reads from and writes to the image have been finished
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
        imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      } break;

      // New layout is color attachment
      // Make sure any writes to the color buffer hav been finished
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      } break;

      // New layout is depth attachment
      // Make sure any writes to depth/stencil buffer have been finished
      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      } break;

      // New layout is shader read (sampler, input attachment)
      // Make sure any writes to the image have been finished
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      } break;

      // special case for converting to surface presentation
      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
        // wait for all stages to finish
        imageMemoryBarrier.dstAccessMask = 0;
        srcStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      } break;

      default: {
        throw(std::runtime_error("setImageLayout: unsupported destination layout"));
      }
    }

    // Put barrier inside setup command buffer
    vkCmdPipelineBarrier(get(), srcStageFlags, destStageFlags, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &imageMemoryBarrier);
  }

  /// change the layout of an image
  void addPrePresentationBarrier(VkImage image) const {
    setImageLayout(image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    return;
  }

  void endCommandBuffer() const {
    vkEndCommandBuffer(get());
  }

  void addPostPresentBariier(VkImage image) const {
    // Add a post present image memory barrier
    // This will transform the frame buffer color attachment back
    // to it's initial layout after it has been presented to the
    // windowing system
    // See buildCommandBuffers for the pre present barrier that 
    // does the opposite transformation 
    setImageLayout(image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    return;
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
  VkCommandPool pool_ = VK_NULL_HANDLE;
};


} // vku

#endif
