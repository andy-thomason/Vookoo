////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016, 2017
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
  VKU_RESOURCE_BOILERPLATE(VkRenderPass, renderPass)

  /// Render pass that does own (and creates) its pointer
  renderPass(const vku::device &dev, const renderPassLayout &layout) : resource(dev) {
    VkRenderPass value = layout.createRenderPass(dev);
    set(value, true);
  }

  void destroy() {
    if (get()) vkDestroyRenderPass(dev(), get(), nullptr);
  }
};


} // vku

#endif
