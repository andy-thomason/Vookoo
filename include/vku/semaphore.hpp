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
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphore res = VK_NULL_HANDLE;
    VkResult err = vkCreateSemaphore(dev, &info, nullptr, &res);
    if (err) throw error(err, __FILE__, __LINE__);
    set(res, true);
  }

  /// move constructor
  semaphore(semaphore &&rhs) {
    move(std::move(rhs));
  }

  /// move operator
  semaphore &operator=(semaphore &&rhs) {
    move(std::move(rhs));
    return *this;
  }

  /// copy constructor
  semaphore(const semaphore &rhs) {
    copy(rhs);
  }

  /// copy operator
  semaphore &operator=(const semaphore &rhs) {
    copy(rhs);
    return *this;
  }

  void destroy() {
    vkDestroySemaphore(dev(), get(), nullptr);
  }
private:
  void move(semaphore &&rhs) {
    (resource&)*this = (resource&&)rhs;
  }

  void copy(const semaphore &rhs) {
    (resource&)*this = (const resource&)rhs;
  }
};


} // vku

#endif
