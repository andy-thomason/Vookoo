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
  /// queue that does not own its pointer (queues are obtained from devices)
  queue(VkQueue value = VK_NULL_HANDLE, VkDevice dev = VK_NULL_HANDLE) : resource(value, dev) {
  }

  void submit(VkSemaphore sema, VkCommandBuffer buffer) const {
    // The submit infor strcuture contains a list of
    // command buffers and semaphores to be submitted to a queue
    // If you want to submit multiple command buffers, pass an array
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = sema ? 1 : 0;
    submitInfo.pWaitSemaphores = &sema;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    // Submit to the graphics queue
    VkResult err = vkQueueSubmit(get(), 1, &submitInfo, VK_NULL_HANDLE);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  void waitIdle() const {  
    VkResult err = vkQueueWaitIdle(get());
    if (err) throw error(err, __FILE__, __LINE__);
  }

  VkQueue create(VkDevice dev) {
    return VK_NULL_HANDLE;
  }

  void destroy() {
  }

};


} // vku

#endif
