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

  void submit(VkSemaphore sema, VkCommandBuffer buffer) const {
    VkSubmitInfo submitInfo = {};
    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = sema ? 1 : 0;
    submitInfo.pWaitSemaphores = &sema;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;
    submitInfo.pWaitDstStageMask = &flags;

    // Submit to the graphics queue
    VkResult err = vkQueueSubmit(get(), 1, &submitInfo, VK_NULL_HANDLE);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  void waitIdle() const {  
    VkResult err = vkQueueWaitIdle(get());
    if (err) throw error(err, __FILE__, __LINE__);
  }

  void destroy() {
  }
};


} // vku

#endif
