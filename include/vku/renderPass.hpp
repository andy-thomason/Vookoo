////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_RENDERPASS_INCLUDED
#define VKU_RENDERPASS_INCLUDED

#include <vku/resource.hpp>
#include <vku/renderPassLayout.hpp>

namespace vku {

class renderPass : public resource<VkRenderPass, renderPass> {
public:
  renderPass() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// Render pass that does not own its pointer
  renderPass(VkRenderPass value, VkDevice dev) : resource(value, dev) {
  }

  /// Render pass that does own (and creates) its pointer
  renderPass(vku::device &dev, renderPassLayout &layout) : resource(dev) {
    VkRenderPass value = layout.createRenderPass(dev);
    set(value, true);
  }

  void destroy() {
    if (get()) vkDestroyRenderPass(dev(), get(), nullptr);
  }

  renderPass &operator=(renderPass &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    return *this;
  }
};


} // vku

#endif
