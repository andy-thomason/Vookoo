////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: Vulkan semaphore. Wraps VkSemaphore
// 

#ifndef VKU_SEMAPHORE_INCLUDED
#define VKU_SEMAPHORE_INCLUDED


namespace vku {

class semaphore : public resource<VkSemaphore, semaphore> {
public:
  /// semaphore that does not own its pointer
  semaphore(VkSemaphore value = VK_NULL_HANDLE, VkDevice dev = VK_NULL_HANDLE) : resource(value, dev) {
  }

  /// semaphore that does own (and creates) its pointer
  semaphore(VkDevice dev) : resource(dev) {
  }

  VkSemaphore create(VkDevice dev) {
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphore res = VK_NULL_HANDLE;
    VkResult err = vkCreateSemaphore(dev, &info, nullptr, &res);
    if (err) throw error(err, __FILE__, __LINE__);
    return res;
  }

  void destroy() {
    vkDestroySemaphore(dev(), get(), nullptr);
  }
};


} // vku

#endif
