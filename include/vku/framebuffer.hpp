////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_FRAMEBUFFER_INCLUDED
#define VKU_FRAMEBUFFER_INCLUDED

#include <vku/resource.hpp>

namespace vku {

class framebuffer : public resource<VkFramebuffer, framebuffer> {
public:
  framebuffer() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// Render pass that does not own its pointer
  framebuffer(VkFramebuffer value, VkDevice dev) : resource(value, dev) {
  }

  /// Render pass that does own (and creates) its pointer
  framebuffer(vku::device &device, vku::image &backBuffer, vku::image &depthBuffer, vku::renderPass &renderPass, uint32_t width, uint32_t height) : resource(device) {
    std::vector<VkImageView> attachments;
    attachments.push_back(backBuffer.view());
    attachments.push_back(depthBuffer.view());

    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.renderPass = renderPass.get();
    frameBufferCreateInfo.attachmentCount = (uint32_t)attachments.size();
    frameBufferCreateInfo.pAttachments = attachments.data();
    frameBufferCreateInfo.width = width;
    frameBufferCreateInfo.height = height;
    frameBufferCreateInfo.layers = 1;

    VkFramebuffer value = VK_NULL_HANDLE;
    VkResult err = vkCreateFramebuffer(dev(), &frameBufferCreateInfo, nullptr, &value);
    set(value, true);
  }

  void destroy() {
    if (get()) vkDestroyFramebuffer(dev(), get(), nullptr);
  }

  framebuffer &operator=(framebuffer &rhs) = default;

  framebuffer &operator=(framebuffer &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    return *this;
  }
};


} // vku

#endif
