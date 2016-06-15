////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: render pass creation helper
// 

#ifndef VKU_RENDERPASSLAYOUT_INCLUDED
#define VKU_RENDERPASSLAYOUT_INCLUDED

#include <vulkan/vulkan.h>
#include <vector>

namespace vku {

class renderPassLayout {
public:
  uint32_t addAttachment(VkFormat format) {
    VkAttachmentDescription ad = {};
    ad.format = format;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ad.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ad.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    uint32_t res = (uint32_t)attachments.size();
    attachments.push_back(ad);
    return res;
  }

  void addSubpass(uint32_t colorAttachment, VkImageLayout colorLayout, uint32_t depthAttachment, VkImageLayout depthLayout) {
    subpass s = {};
    s.num_color = 1;
    s.color[0].attachment = colorAttachment;
    s.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    s.depth.attachment = depthAttachment;
    s.depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    s.desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses.push_back(s);
  }

  VkRenderPass createRenderPass(VkDevice device) const {
    std::vector<VkSubpassDescription> descs;
    for (size_t i = 0; i != subpasses.size(); ++i) {
      const subpass &s = subpasses[i];
      VkSubpassDescription desc = s.desc;
      desc.colorAttachmentCount = s.num_color;
      desc.pColorAttachments = s.color;
      desc.pDepthStencilAttachment = &s.depth;
      descs.push_back(desc);
    }

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = (uint32_t)attachments.size();
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = (uint32_t)descs.size();
    renderPassInfo.pSubpasses = descs.data();

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult err = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    return renderPass;
  }

private:
  struct subpass {
    uint32_t num_color;
    VkAttachmentReference color[6];
    VkAttachmentReference depth;
    VkSubpassDescription desc;
  };

  std::vector<VkAttachmentDescription> attachments;
  std::vector<subpass> subpasses;
  std::vector<VkAttachmentReference> refs;
};


} // vku

#endif
