////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: Forward references for swap chain
// 

#ifndef VKU_SWAPCHAIN_INL_INCLUDED
#define VKU_SWAPCHAIN_INL_INCLUDED

namespace vku {

inline void swapChain::build_images(VkCommandBuffer buf) {
  VkDevice d = dev();
  vku::commandBuffer cb(buf, d);

  uint32_t imageCount = 0;
  VkResult err = vkGetSwapchainImagesKHR(dev(), get(), &imageCount, NULL);
  if (err) throw error(err, __FILE__, __LINE__);

  swapchainImages.resize(imageCount);
  swapchainViews.resize(imageCount);
  err = vkGetSwapchainImagesKHR(dev(), *this, &imageCount, swapchainImages.data());
  if (err) throw error(err, __FILE__, __LINE__);

  for (uint32_t i = 0; i < imageCount; i++) {
    VkImageViewCreateInfo colorAttachmentView = {};
    colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorAttachmentView.pNext = NULL;
    colorAttachmentView.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorAttachmentView.components = {
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_A
    };
    colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorAttachmentView.subresourceRange.baseMipLevel = 0;
    colorAttachmentView.subresourceRange.levelCount = 1;
    colorAttachmentView.subresourceRange.baseArrayLayer = 0;
    colorAttachmentView.subresourceRange.layerCount = 1;
    colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorAttachmentView.flags = 0;

    cb.setImageLayout(
      image(i), 
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, 
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    colorAttachmentView.image = image(i);

    err = vkCreateImageView(dev(), &colorAttachmentView, VK_NULL_HANDLE, &swapchainViews[i]);
    if (err) throw error(err, __FILE__, __LINE__);
  }
}



} // vku

#endif
