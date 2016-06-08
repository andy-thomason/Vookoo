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
  VKU_RESOURCE_BOILERPLATE(VkSemaphore, semaphore)

  /// semaphore that does own (and creates) its pointer
  semaphore(VkDevice dev) : resource(dev) {
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphore res = VK_NULL_HANDLE;
    VkResult err = vkCreateSemaphore(dev, &info, nullptr, &res);
    if (err) throw error(err, __FILE__, __LINE__);
    set(res, true);
  }

  void destroy() {
    vkDestroySemaphore(dev(), get(), nullptr);
  }
private:
};


} // vku

#endif
