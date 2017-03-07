////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016, 2017
//
// Vookoo: command buffer wraps VkCommandBuffer
// 

#ifndef VKU_COMMAND_BUFFER_INCLUDED
#define VKU_COMMAND_BUFFER_INCLUDED

#include <vku/resource.hpp>

namespace vku {

class pipelineLayout;
class descriptorSet;

class commandBuffer : public resource<VkCommandBuffer, commandBuffer> {
public:
  VKU_RESOURCE_BOILERPLATE(VkCommandBuffer, commandBuffer)

  /// command buffer that does own (and creates) its pointer
  commandBuffer(VkDevice dev, VkCommandPool cmdPool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) : resource(dev) {
    VkCommandBuffer res = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = cmdPool;
    commandBufferAllocateInfo.level = level;
    commandBufferAllocateInfo.commandBufferCount = 1;
    VkResult vkRes = vkAllocateCommandBuffers(dev, &commandBufferAllocateInfo, &res);
    set(res, true);
    pool_ = cmdPool;
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

  void reset(VkCommandBufferResetFlags flags=0) const {
    vkResetCommandBuffer(get(), flags);
  }

  void beginCommandBuffer() const {
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(get(), &cmdBufInfo);
  }

  void beginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer, int x = 0, int y = 0, int width = 256, int height = 256) const {
    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.025f, 0.025f, 0.025f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
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

  // forward reference, needs to know about pipelineLayout and descriptorSet
  void commandBuffer::bindBindDescriptorSet(vku::pipelineLayout &layout, vku::descriptorSet &set) const;

  void bindPipeline(vku::pipeline &pipe) const {
    // Bind descriptor sets describing shader binding points
    //vkCmdBindDescriptorSets(get(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout(), 0, 1, pipe.descriptorSets(), 0, NULL);

    // Bind the rendering pipeline (including the shaders)
    vkCmdBindPipeline(get(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.get());
  }

  void bindVertexBuffer(const buffer &buf, int bindId) const {
    VkDeviceSize offsets[] = { 0 };
    VkBuffer bufs[] = { buf.get() };
    if (bufs[0] != VK_NULL_HANDLE) vkCmdBindVertexBuffers(get(), bindId, 1, bufs, offsets);
  }

  void bindIndexBuffer(const buffer &buf) const {
    // Bind triangle indices
    if (buf.get() != VK_NULL_HANDLE) vkCmdBindIndexBuffer(get(), buf.get(), 0, VK_INDEX_TYPE_UINT32);
  }

  void setLineWidth(float width) const {
    vkCmdSetLineWidth(get(), width);
  }

  void setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) const {
    vkCmdSetDepthBias(get(), depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
  }

  void setBlendConstants(float blendConstants[4]) const {
    vkCmdSetBlendConstants(get(), blendConstants);
  }

  void setStencilCompareMask(VkStencilFaceFlags faceMask, uint32_t  compareMask) const {
    vkCmdSetStencilCompareMask(get(), faceMask, compareMask);
  }

  void setStencilWriteMask(VkStencilFaceFlags faceMask,uint32_t  writeMask) const {
    vkCmdSetStencilWriteMask(get(), faceMask, writeMask);
  }

  void setStencilReference(VkStencilFaceFlags faceMask, uint32_t  reference) const {
    vkCmdSetStencilReference(get(), faceMask, reference);
  }

  void draw(uint32_t vertexCount,uint32_t instanceCount,uint32_t firstVertex,uint32_t firstInstance) const {
    vkCmdDraw(get(),vertexCount,instanceCount,firstVertex,firstInstance);
  }

  void drawIndexed(uint32_t indexCount,uint32_t instanceCount,uint32_t firstIndex,int32_t vertexOffset,uint32_t firstInstance) const {
    vkCmdDrawIndexed(get(),indexCount,instanceCount,firstIndex,vertexOffset,firstInstance);
  }

  void drawIndirect(VkBuffer buffer,VkDeviceSize offset,uint32_t drawCount,uint32_t stride) const {
    vkCmdDrawIndirect(get(),buffer,offset,drawCount,stride);
  }

  void drawIndexedIndirect(VkBuffer buffer,VkDeviceSize offset,uint32_t drawCount,uint32_t stride) const {
    vkCmdDrawIndexedIndirect(get(),buffer,offset,drawCount,stride);
  }

  void dispatch(uint32_t x,uint32_t y,uint32_t z) const {
    vkCmdDispatch(get(),x,y,z);
  }

  void dispatchIndirect(VkBuffer buffer,VkDeviceSize offset) const {
    vkCmdDispatchIndirect(get(),buffer,offset);
  }

  void copyBuffer(VkBuffer srcBuffer,VkBuffer dstBuffer,uint32_t regionCount,const VkBufferCopy* pRegions) const {
    vkCmdCopyBuffer(get(),srcBuffer,dstBuffer,regionCount,pRegions);
  }

  void copyImage(VkImage srcImage,VkImageLayout srcImageLayout,VkImage dstImage,VkImageLayout dstImageLayout,uint32_t regionCount,const VkImageCopy* pRegions) const {
    vkCmdCopyImage(get(),srcImage,srcImageLayout,dstImage,dstImageLayout,regionCount,pRegions);
  }

  void blitImage(VkImage srcImage,VkImageLayout srcImageLayout,VkImage dstImage,VkImageLayout dstImageLayout,uint32_t regionCount,const VkImageBlit* pRegions,VkFilter filter) const {
    vkCmdBlitImage(get(),srcImage,srcImageLayout,dstImage,dstImageLayout,regionCount,pRegions,filter);
  }

  void copyBufferToImage(VkBuffer srcBuffer,VkImage dstImage,VkImageLayout dstImageLayout,uint32_t regionCount,const VkBufferImageCopy* pRegions) const {
    vkCmdCopyBufferToImage(get(),srcBuffer,dstImage,dstImageLayout,regionCount,pRegions);
  }

  void copyImageToBuffer(VkImage srcImage,VkImageLayout srcImageLayout,VkBuffer dstBuffer,uint32_t regionCount,const VkBufferImageCopy* pRegions) const {
    vkCmdCopyImageToBuffer(get(),srcImage,srcImageLayout,dstBuffer,regionCount,pRegions);
  }

  void updateBuffer(VkBuffer dstBuffer,VkDeviceSize dstOffset,VkDeviceSize dataSize,const uint32_t* pData) const {
    vkCmdUpdateBuffer(get(),dstBuffer,dstOffset,dataSize,pData);
  }

  void fillBuffer(VkBuffer dstBuffer,VkDeviceSize dstOffset,VkDeviceSize size,uint32_t data) const {
    vkCmdFillBuffer(get(),dstBuffer,dstOffset,size,data);
  }

  void clearColorImage(VkImage image,VkImageLayout imageLayout,const VkClearColorValue* pColor,uint32_t rangeCount,const VkImageSubresourceRange* pRanges) const {
    vkCmdClearColorImage(get(),image,imageLayout,pColor,rangeCount,pRanges);
  }

  void clearDepthStencilImage(VkImage image,VkImageLayout imageLayout,const VkClearDepthStencilValue* pDepthStencil,uint32_t rangeCount,const VkImageSubresourceRange* pRanges) const {
    vkCmdClearDepthStencilImage(get(),image,imageLayout,pDepthStencil,rangeCount,pRanges);
  }

  void clearAttachments(uint32_t attachmentCount,const VkClearAttachment* pAttachments,uint32_t rectCount,const VkClearRect* pRects) const {
    vkCmdClearAttachments(get(),attachmentCount,pAttachments,rectCount,pRects);
  }

  void resolveImage(VkImage srcImage,VkImageLayout srcImageLayout,VkImage dstImage,VkImageLayout dstImageLayout,uint32_t regionCount,const VkImageResolve* pRegions) const {
    vkCmdResolveImage(get(),srcImage,srcImageLayout,dstImage,dstImageLayout,regionCount,pRegions);
  }

  void setEvent(VkEvent event,VkPipelineStageFlags stageMask) const {
    vkCmdSetEvent(get(),event,stageMask);
  }

  void resetEvent(VkEvent event,VkPipelineStageFlags stageMask) const {
  }

  void waitEvents(uint32_t eventCount,const VkEvent* pEvents,VkPipelineStageFlags srcStageMask,VkPipelineStageFlags dstStageMask,uint32_t memoryBarrierCount,const VkMemoryBarrier* pMemoryBarriers,uint32_t bufferMemoryBarrierCount,const VkBufferMemoryBarrier* pBufferMemoryBarriers,uint32_t imageMemoryBarrierCount,const VkImageMemoryBarrier* pImageMemoryBarriers) const {
  }

  void pipelineBarrier(VkPipelineStageFlags srcStageMask,VkPipelineStageFlags dstStageMask,VkDependencyFlags dependencyFlags,uint32_t memoryBarrierCount,const VkMemoryBarrier* pMemoryBarriers,uint32_t bufferMemoryBarrierCount,const VkBufferMemoryBarrier* pBufferMemoryBarriers,uint32_t imageMemoryBarrierCount,const VkImageMemoryBarrier* pImageMemoryBarriers) const {
    vkCmdPipelineBarrier(get(),srcStageMask,dstStageMask,dependencyFlags,memoryBarrierCount,pMemoryBarriers,bufferMemoryBarrierCount,pBufferMemoryBarriers,imageMemoryBarrierCount,pImageMemoryBarriers);
  }

  void beginQuery(VkQueryPool queryPool,uint32_t query,VkQueryControlFlags flags) const {
    vkCmdBeginQuery(get(),queryPool,query,flags);
  }

  void endQuery(VkQueryPool queryPool,uint32_t query) const {
    vkCmdEndQuery(get(),queryPool,query);
  }

  void resetQueryPool(VkQueryPool queryPool,uint32_t firstQuery,uint32_t queryCount) const {
    vkCmdResetQueryPool(get(),queryPool,firstQuery,queryCount);
  }

  void writeTimestamp(VkPipelineStageFlagBits pipelineStage,VkQueryPool queryPool,uint32_t query) const {
    vkCmdWriteTimestamp(get(),pipelineStage,queryPool,query);
  }

  void copyQueryPoolResults(VkQueryPool queryPool,uint32_t firstQuery,uint32_t queryCount,VkBuffer dstBuffer,VkDeviceSize dstOffset,VkDeviceSize stride,VkQueryResultFlags flags) const {
    vkCmdCopyQueryPoolResults(get(),queryPool,firstQuery,queryCount,dstBuffer,dstOffset,stride,flags);
  }

  void pushConstants(VkPipelineLayout layout,VkShaderStageFlags stageFlags,uint32_t offset,uint32_t size,const void* pValues) const {
    vkCmdPushConstants(get(),layout,stageFlags,offset,size,pValues);
  }

  void beginRenderPass(const VkRenderPassBeginInfo* pRenderPassBegin,VkSubpassContents contents) const {
    vkCmdBeginRenderPass(get(),pRenderPassBegin,contents);
  }

  void nextSubpass(VkSubpassContents contents) const {
    vkCmdNextSubpass(get(),contents);
  }

  void endRenderPass() const {
    vkCmdEndRenderPass(get());
  }

  void executeCommands(uint32_t commandBufferCount,const VkCommandBuffer* pCommandBuffers) const {
    vkCmdExecuteCommands(get(), commandBufferCount,pCommandBuffers);
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

    // todo: this is quite clearly wrong in many places.
    switch (oldImageLayout) {
      case VK_IMAGE_LAYOUT_PREINITIALIZED: {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      } break;

      case VK_IMAGE_LAYOUT_GENERAL: {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      } break;

      case VK_IMAGE_LAYOUT_UNDEFINED: {
        // No access mask must be set
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

      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
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
        srcStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // wait for all pipeline stages to complete
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
      } break;

      default: {
        throw(std::runtime_error("setImageLayout: unsupported source layout"));
      }
    }

    // Target layouts (new)
    switch (newImageLayout) {
      case VK_IMAGE_LAYOUT_GENERAL: {
      } break;

      // New layout is transfer destination (copy, blit)
      // Make sure any copyies to the image have been finished
      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      } break;

      // New layout is transfer source (copy, blit)
      // Make sure any reads from and writes to the image have been finished
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
        imageMemoryBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      } break;

      // New layout is color attachment
      // Make sure any writes to the color buffer hav been finished
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        //imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      } break;

      // New layout is depth attachment
      // Make sure any writes to depth/stencil buffer have been finished
      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
        imageMemoryBarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      } break;

      // New layout is shader read (sampler, input attachment)
      // Make sure any writes to the image have been finished
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
        //imageMemoryBarrier.srcAccessMask |= VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      } break;

      // special case for converting to surface presentation
      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
        // wait for all stages to finish
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        srcStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      } break;

      case VK_IMAGE_LAYOUT_UNDEFINED:
      case VK_IMAGE_LAYOUT_PREINITIALIZED:
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
