////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: queue class: wraps VkQueue
// 

#ifndef VKU_QUEUE_INCLUDED
#define VKU_QUEUE_INCLUDED

namespace vku {

class queue : public resource<VkQueue, queue> {
public:
  VKU_RESOURCE_BOILERPLATE(VkQueue, queue)

  // submit a command buffer to this queue optionally waiting for insema and signalling outsema
  void submit(VkCommandBuffer buffer, VkSemaphore insema = VK_NULL_HANDLE, VkSemaphore outsema = VK_NULL_HANDLE, VkPipelineStageFlags flags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT) const {
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = insema ? 1 : 0;
    submitInfo.pWaitSemaphores = &insema;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;
    submitInfo.pWaitDstStageMask = &flags;
    submitInfo.signalSemaphoreCount = outsema ? 1 : 0;
    submitInfo.pSignalSemaphores = &outsema;

    // Submit to the graphics queue
    VkResult err = vkQueueSubmit(get(), 1, &submitInfo, VK_NULL_HANDLE);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  // stall the CPU waiting for the queue to complete all its commands.
  void waitIdle() const {  
    VkResult err = vkQueueWaitIdle(get());
    if (err) throw error(err, __FILE__, __LINE__);
  }

private:
  // queues are owned by the instance and cannot be destroyed explicitly.
  void destroy() {
  }
};


} // vku

#endif
