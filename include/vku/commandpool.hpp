////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_COMMAND_POOL_INCLUDED
#define VKU_COMMAND_POOL_INCLUDED

#include <vku/resource.hpp>

namespace vku {

class commandPool : public resource<VkCommandPool, commandPool> {
public:
  commandPool() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// command pool that does not own its pointer
  commandPool(VkCommandPool value, VkDevice dev) : resource(value, dev) {
  }

  /// command pool that does owns (and creates) its pointer
  commandPool(VkDevice dev, uint32_t queueFamilyIndex) : resource(dev) {
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool cmdPool;
    VkResult err = vkCreateCommandPool(dev, &cmdPoolInfo, VK_NULL_HANDLE, &cmdPool);
    if (err) throw error(err, __FILE__, __LINE__);
    set(cmdPool, true);
  }

  void destroy() {
    if (get()) vkDestroyCommandPool(dev(), get(), VK_NULL_HANDLE);
  }

  commandPool &operator=(commandPool &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    return *this;
  }
};


} // vku

#endif
