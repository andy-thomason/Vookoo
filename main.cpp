////////////////////////////////////////////////////////////////////////////////
//
// Minimalistic Vulkan Triangle sample
//
// 

#ifdef _WIN32
  #define VK_USE_PLATFORM_WIN32_KHR
  #pragma comment(lib, "vulkan/vulkan-1.lib")
#endif

#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
#include "base/vulkanexamplebase.h"


// Please note that this is deliberately minimal to aid comprehension.
// There is no error checking and it may assume certain properties of
// hardware.
class vulkan_triangle {
public:

  vulkan_triangle() {
    // Create an instance of the Vulkan API
    const char *extensionNames[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.enabledExtensionCount = 2;
    instanceCreateInfo.ppEnabledExtensionNames = extensionNames;

    VkInstance inst;
    vkCreateInstance(&instanceCreateInfo, nullptr, &inst);

    // Find physical devices (graphics cards)
    VkPhysicalDevice phys[4] = {};
    uint32_t physCount = 4;
    vkEnumeratePhysicalDevices(inst, &physCount, phys);

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    float priorities[] = { 0.0f };
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = priorities;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;

    VkDevice dev;
    vkCreateDevice(phys[0], &deviceCreateInfo, nullptr, &dev);

    VkSurfaceKHR surf;
    uint32_t width = 768;
    uint32_t height = 768;
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    // create a window with a vulkan surface.
    #ifdef _WIN32
      HINSTANCE instance = (HINSTANCE)GetModuleHandle(0);
      HBRUSH brush = (HBRUSH) GetStockObject(NULL_BRUSH);
      HICON icon = LoadIcon(0, IDI_ASTERISK);
      HCURSOR cursor = LoadCursor(0, IDC_ARROW);

      static WNDCLASSW wndclass = {
        CS_HREDRAW | CS_VREDRAW, DefWindowProc, 0, 0, instance,
        icon, cursor, brush, 0, L"MyClass"
      };
      RegisterClassW (&wndclass);

      HWND window_handle = CreateWindowW(L"MyClass", L"Vulkan",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, instance, (LPVOID)0
      );

	    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.hinstance = instance;
      surfaceCreateInfo.hwnd = window_handle;

      vkCreateWin32SurfaceKHR(inst, &surfaceCreateInfo, nullptr, &surf);
    #endif

    // Create a swap chain
    VkSwapchainCreateInfoKHR swapCreateInfo = {};
    swapCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapCreateInfo.surface = surf;
    swapCreateInfo.minImageCount = 2;
		swapCreateInfo.imageFormat = colorFormat;
		swapCreateInfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		swapCreateInfo.imageExtent = { width, height };
		swapCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		swapCreateInfo.imageArrayLayers = 1;
		swapCreateInfo.queueFamilyIndexCount = VK_SHARING_MODE_EXCLUSIVE;
		swapCreateInfo.queueFamilyIndexCount = 0;
		swapCreateInfo.pQueueFamilyIndices = nullptr;
		swapCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		swapCreateInfo.clipped = true;
		swapCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainKHR swap;
    vkCreateSwapchainKHR(dev, &swapCreateInfo, nullptr, &swap);

    VkImage images[2]; uint32_t swapCount = 2;
    vkGetSwapchainImagesKHR(dev, swap, &swapCount, images);

	  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	  semaphoreCreateInfo.pNext = nullptr;
    VkSemaphore presentCompleteSemaphore;
    vkCreateSemaphore(dev, &semaphoreCreateInfo, nullptr, &presentCompleteSemaphore);

    uint32_t currentSwapImage = 0;
    vkAcquireNextImageKHR(dev, swap, UINT64_MAX, presentCompleteSemaphore, nullptr, &currentSwapImage);
    VkImageView views[2];

		for (uint32_t i = 0; i < swapCount; i++) {
			VkImageViewCreateInfo colorAttachmentView = {};
			colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			colorAttachmentView.format = colorFormat;
			colorAttachmentView.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorAttachmentView.subresourceRange.levelCount = 1;
			colorAttachmentView.subresourceRange.layerCount = 1;
			colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
      colorAttachmentView.image = images[i];

		  /*VkImageMemoryBarrier imageMemoryBarrier = {};
		  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		  imageMemoryBarrier.image = images[i];
		  imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		  imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		  imageMemoryBarrier.subresourceRange.levelCount = 1;
		  imageMemoryBarrier.subresourceRange.layerCount = 1;

		  VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		  VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		  // Put barrier inside setup command buffer
		  vkCmdPipelineBarrier(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);*/

			vkCreateImageView(dev, &colorAttachmentView, nullptr, &views[i]);
		}

	  VkImageCreateInfo image = {};
	  image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	  image.pNext = NULL;
	  image.imageType = VK_IMAGE_TYPE_2D;
	  image.format = depthFormat;
	  image.extent = { width, height, 1 };
	  image.mipLevels = 1;
	  image.arrayLayers = 1;
	  image.samples = VK_SAMPLE_COUNT_1_BIT;
	  image.tiling = VK_IMAGE_TILING_OPTIMAL;
	  image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	  image.flags = 0;

	  VkMemoryAllocateInfo mem_alloc = {};
	  mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	  mem_alloc.pNext = NULL;
	  mem_alloc.allocationSize = 0;
	  mem_alloc.memoryTypeIndex = 0;

	  VkImageViewCreateInfo depthStencilView = {};
	  depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	  depthStencilView.pNext = NULL;
	  depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	  depthStencilView.format = depthFormat;
	  depthStencilView.flags = 0;
	  depthStencilView.subresourceRange = {};
	  depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	  depthStencilView.subresourceRange.baseMipLevel = 0;
	  depthStencilView.subresourceRange.levelCount = 1;
	  depthStencilView.subresourceRange.baseArrayLayer = 0;
	  depthStencilView.subresourceRange.layerCount = 1;

    VkImage depthStencilImage;
	  vkCreateImage(dev, &image, nullptr, &depthStencilImage);

	  VkMemoryRequirements memReqs;
	  vkGetImageMemoryRequirements(dev, depthStencilImage, &memReqs);
	  mem_alloc.allocationSize = memReqs.size;
    mem_alloc.memoryTypeIndex = 1; // need to check this!
    VkDeviceMemory depthStencilMem;
	  vkAllocateMemory(dev, &mem_alloc, nullptr, &depthStencilMem);

	  vkBindImageMemory(dev, depthStencilImage, depthStencilMem, 0);

	  vkTools::setImageLayout(setupCmdBuffer, depthStencil.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	  depthStencilView.image = depthStencil.image;
	  err = vkCreateImageView(device, &depthStencilView, nullptr, &depthStencil.view);
	  assert(!err);

    VkQueue queue;
    vkGetDeviceQueue(dev, 0, 0, &queue);

	  VkAttachmentDescription attachments[2];
	  attachments[0].format = colorFormat;
	  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	  attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	  attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	  attachments[1].format = depthFormat;
	  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	  attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	  attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	  VkAttachmentReference colorReference = {};
	  colorReference.attachment = 0;
	  colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	  VkAttachmentReference depthReference = {};
	  depthReference.attachment = 1;
	  depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	  VkSubpassDescription subpass = {};
	  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	  subpass.flags = 0;
	  subpass.inputAttachmentCount = 0;
	  subpass.pInputAttachments = NULL;
	  subpass.colorAttachmentCount = 1;
	  subpass.pColorAttachments = &colorReference;
	  subpass.pResolveAttachments = NULL;
	  subpass.pDepthStencilAttachment = &depthReference;
	  subpass.preserveAttachmentCount = 0;
	  subpass.pPreserveAttachments = NULL;

	  VkRenderPassCreateInfo renderPassInfo = {};
	  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	  renderPassInfo.pNext = NULL;
	  renderPassInfo.attachmentCount = 2;
	  renderPassInfo.pAttachments = attachments;
	  renderPassInfo.subpassCount = 1;
	  renderPassInfo.pSubpasses = &subpass;
	  renderPassInfo.dependencyCount = 0;
	  renderPassInfo.pDependencies = NULL;

    VkRenderPass renderPass;
	  vkCreateRenderPass(dev, &renderPassInfo, nullptr, &renderPass);

	  VkImageView attachments[2];

	  // Depth/Stencil attachment is the same for all frame buffers
	  attachments[1] = depthStencil.view;

	  VkFramebufferCreateInfo frameBufferCreateInfo = {};
	  frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	  frameBufferCreateInfo.pNext = NULL;
	  frameBufferCreateInfo.renderPass = renderPass;
	  frameBufferCreateInfo.attachmentCount = 2;
	  frameBufferCreateInfo.pAttachments = attachments;
	  frameBufferCreateInfo.width = width;
	  frameBufferCreateInfo.height = height;
	  frameBufferCreateInfo.layers = 1;

	  // Create frame buffers for every swap chain image
	  frameBuffers.resize(swapChain.imageCount);
	  for (uint32_t i = 0; i < frameBuffers.size(); i++)
	  {
		  attachments[0] = swapChain.buffers[i].view;
		  VkResult err = vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]);
		  assert(!err);
	  }

    VkFramebuffer framebuffer;
    vkCreateFramebuffer(dev, &framebufferCreateInfo, nullptr, &framebuffer);

    /*VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo = {
      // whatever we want to match our shader. e.g. Binding 0 = UBO for a simple
      // case with just a vertex shader UBO with transform data.
    };

    VkDescriptorSetLayout descSetLayout;
    vkCreateDescriptorSetLayout(dev, &descSetLayoutCreateInfo, nullptr, &descSetLayout);

    VkPipelineCreateInfo pipeLayoutCreateInfo = {
      // one descriptor set, with layout descSetLayout
    };

    VkPipelineLayout pipeLayout;
    vkCreatePipelineLayout(dev, &pipeLayoutCreateInfo, nullptr, &pipeLayout);

    // upload the SPIR-V shaders
    VkShaderModule vertModule, fragModule;
    vkCreateShaderModule(dev, &vertModuleInfoWithSPIRV, nullptr, &vertModule);
    vkCreateShaderModule(dev, &fragModuleInfoWithSPIRV, nullptr, &fragModule);

    VkGraphicsPipelineCreateInfo pipeCreateInfo = {
      // there are a LOT of sub-structures under here to fully specify
      // the PSO state. It will reference vertModule, fragModule and pipeLayout
      // as well as renderpass for compatibility
    };

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(dev, nullptr, 1, &pipeCreateInfo, nullptr, &pipeline);

    VkDescriptorPoolCreateInfo descPoolCreateInfo = {
      // the creation info states how many descriptor sets are in this pool
    };

    VkDescriptorPool descPool;
    vkCreateDescriptorPool(dev, &descPoolCreateInfo, nullptr, &descPool);

    VkDescriptorSetAllocateInfo descAllocInfo = {
      // from pool descPool, with layout descSetLayout
    };

    VkDescriptorSet descSet;
    vkAllocateDescriptorSets(dev, &descAllocInfo, &descSet);

    VkBufferCreateInfo bufferCreateInfo = {
      // buffer for uniform usage, of appropriate size
    };

    VkMemoryAllocateInfo memAllocInfo = {
      // skipping querying for memory requirements. Let's assume the buffer
      // can be placed in host visible memory.
    };
    VkBuffer buffer;
    VkDeviceMemory memory;
    vkCreateBuffer(dev, &bufferCreateInfo, nullptr, &buffer);
    vkAllocateMemory(dev, &memAllocInfo, nullptr, &memory);
    vkBindBufferMemory(dev, buffer, memory, 0);

    void *data = nullptr;
    vkMapMemory(dev, memory, 0, VK_WHOLE_SIZE, 0, &data);
    // fill data pointer with lovely transform goodness
    vkUnmapMemory(dev, memory);

    VkWriteDescriptorSet descriptorWrite = {
      // write the details of our UBO buffer into binding 0
    };

    vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, nullptr);

    // finally we can render something!
    // ...
    // Almost.

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
      // nothing interesting
    };

    VkCommandPool commandPool;
    vkCreateCommandPool(dev, &commandPoolCreateInfo, nullptr, &commandPool);

    VkCommandBufferAllocateInfo commandAllocInfo = {
      // allocate from commandPool
    };
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(dev, &commandAllocInfo, &cmd);

    // Now we can render!

    vkBeginCommandBuffer(cmd, &cmdBeginInfo);
    vkCmdBeginRenderPass(cmd, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    // bind the pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    // bind the descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            descSetLayout, 1, &descSet, 0, nullptr);
    // set the viewport
    vkCmdSetViewport(cmd, 1, &viewport);
    // draw the triangle
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {
      // this contains a reference to the above cmd to submit
    };

    vkQueueSubmit(queue, 1, &submitInfo, nullptr);

    // now we can present
    VkPresentInfoKHR presentInfo = {
      // swap and currentSwapImage are used here
    };
    vkQueuePresentKHR(queue, &presentInfo);

    // Wait for everything to be done, and destroy objects
    */
  }
private:
};

int main() {
  vulkan_triangle sample;
}
