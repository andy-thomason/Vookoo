/*
* Class wrapping access to the swap chain
*
* Copyright (C) 2015 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdlib.h>
#include <string>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#endif

#include "../vulkan/vulkan.h"
#include "vulkantools.h"

#ifdef __ANDROID__
#include "vulkanandroid.h"
#endif

// Macro to get a procedure address based on a vulkan instance
#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                        \
{                                                                       \
    fp##entrypoint = (PFN_vk##entrypoint) vkGetInstanceProcAddr(inst, "vk"#entrypoint); \
    if (fp##entrypoint == NULL)                                         \
	{																    \
        exit(1);                                                        \
    }                                                                   \
}

// Macro to get a procedure address based on a vulkan device
#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                           \
{                                                                       \
    fp##entrypoint = (PFN_vk##entrypoint) vkGetDeviceProcAddr(dev, "vk"#entrypoint);   \
    if (fp##entrypoint == NULL)                                         \
	{																    \
        exit(1);                                                        \
    }                                                                   \
}

/*typedef struct _SwapChainBuffers {
	VkImage image;
	VkImageView view;
} SwapChainBuffer*/

class VulkanSwapChain
{
private: 
	VkInstance instance;
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;
	// Function pointers
	/*PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR; 
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
	PFN_vkQueuePresentKHR fpQueuePresentKHR;*/
public:
  vku::swapChain swapChain;

  size_t imageCount() { return swapChain.imageCount(); }
  VkImageView view(size_t i) { return swapChain.view(i); }
  VkImage image(size_t i) { return swapChain.image(i); }

	// wip naming
	void initSwapChain(
#ifdef _WIN32
		void* connection, void* window
#else
#ifdef __ANDROID__
		void *connection, ANativeWindow* window
#else
		xcb_connection_t* connection, xcb_window_t window
#endif
#endif
	)
	{
		//uint32_t queueCount;
		//VkQueueFamilyProperties *queueProps;
    vku::device dev(device, physicalDevice);

		//VkResult err;

    vku::instance inst(instance);
    surface = inst.createSurface((void*)connection, (void*)window);
    swapChain.queueNodeIndex = dev.getGraphicsQueueNodeIndex(surface);
    if (swapChain.queueNodeIndex == ~(uint32_t)0) throw(std::runtime_error("no graphics and present queue available"));
    auto sf = dev.getSurfaceFormat(surface);
    swapChain.colorFormat = sf.first;
    swapChain.colorSpace = sf.second;
	}

	void init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		this->instance = instance;
		this->physicalDevice = physicalDevice;
		this->device = device;
		/*GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceSupportKHR);
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceFormatsKHR);
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfacePresentModesKHR);
		GET_DEVICE_PROC_ADDR(device, CreateSwapchainKHR);
		GET_DEVICE_PROC_ADDR(device, DestroySwapchainKHR);
		GET_DEVICE_PROC_ADDR(device, GetSwapchainImagesKHR);
		GET_DEVICE_PROC_ADDR(device, AcquireNextImageKHR);
		GET_DEVICE_PROC_ADDR(device, QueuePresentKHR);*/
	}

	void setup(VkCommandBuffer cmdBuffer, uint32_t *width, uint32_t *height)
	{
    swapChain = vku::swapChain(vku::device(device, physicalDevice), *width, *height, surface, cmdBuffer);
    *width = swapChain.width();
    *height = swapChain.height();
	}

	// Acquires the next image in the swap chain
	VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t *currentBuffer)
	{
    *currentBuffer = swapChain.acquireNextImage(presentCompleteSemaphore);
    return VK_SUCCESS;
	}

	// Present the current image to the queue
	VkResult queuePresent(VkQueue queue, uint32_t currentBuffer)
	{
    swapChain.present(queue, currentBuffer);
    return VK_SUCCESS;
	}

	void cleanup()
	{
    swapChain.clear();
		//vkDestroySurfaceKHR(instance, surface, nullptr);
	}

};
