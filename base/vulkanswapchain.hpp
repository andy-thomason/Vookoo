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

typedef struct _SwapChainBuffers {
	VkImage image;
	VkImageView view;
} SwapChainBuffer;

class VulkanSwapChain
{
private: 
	VkInstance instance;
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;
	// Function pointers
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR; 
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
	PFN_vkQueuePresentKHR fpQueuePresentKHR;
public:
  vku::swapChain swapChain;
	VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;;
	VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

	// Index of the deteced graphics and presenting device queue
	uint32_t queueNodeIndex = UINT32_MAX;

  size_t imageCount() { return swapChain.imageCount(); }
  VkImageView view(size_t i) { return swapChain.view(i); }
  VkImage image(size_t i) { return swapChain.image(i); }

	// wip naming
	void initSwapChain(
#ifdef _WIN32
		void* platformHandle, void* platformWindow
#else
#ifdef __ANDROID__
		ANativeWindow* window
#else
		xcb_connection_t* connection, xcb_window_t window
#endif
#endif
	)
	{
		uint32_t queueCount;
		VkQueueFamilyProperties *queueProps;

		VkResult err;

		// Create surface depending on OS
#ifdef _WIN32
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.hinstance = (HINSTANCE)platformHandle;
		surfaceCreateInfo.hwnd = (HWND)platformWindow;
		err = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
#else
#ifdef __ANDROID__
		VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.window = window;
		err = vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, NULL, &surface);
#else
		VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.connection = connection;
		surfaceCreateInfo.window = window;
		err = vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
#endif
#endif

		uint32_t i;

		// Get queue properties
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);

		queueProps = (VkQueueFamilyProperties *)malloc(queueCount * sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps);
		assert(queueCount >= 1);

		// Iterate over each queue to learn whether it supports presenting:
		VkBool32* supportsPresent = (VkBool32 *)malloc(queueCount * sizeof(VkBool32));
		for (i = 0; i < queueCount; i++) 
		{
			fpGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i,
				surface,
				&supportsPresent[i]);
		}

		// Search for a graphics and a present queue in the array of queue
		// families, try to find one that supports both
		uint32_t graphicsQueueNodeIndex = UINT32_MAX;
		uint32_t presentQueueNodeIndex = UINT32_MAX;
		for (i = 0; i < queueCount; i++) 
		{
			if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) 
			{
				if (graphicsQueueNodeIndex == UINT32_MAX) 
				{
					graphicsQueueNodeIndex = i;
				}

				if (supportsPresent[i] == VK_TRUE) 
				{
					graphicsQueueNodeIndex = i;
					presentQueueNodeIndex = i;
					break;
				}
			}
		}
		if (presentQueueNodeIndex == UINT32_MAX) 
		{	
			// If there's no queue that supports both present and graphics
			// try to find a separate present queue
			for (uint32_t i = 0; i < queueCount; ++i) 
			{
				if (supportsPresent[i] == VK_TRUE) 
				{
					presentQueueNodeIndex = i;
					break;
				}
			}
		}
		free(supportsPresent);

		// Generate error if could not find both a graphics and a present queue
		if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX) 
		{
			// todo : error message
		}

		if (graphicsQueueNodeIndex != presentQueueNodeIndex) 
		{
			// todo : error message
		}

		queueNodeIndex = graphicsQueueNodeIndex;

		// Get list of supported formats
		/*uint32_t formatCount;
		err = fpGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL);
		assert(!err);

		VkSurfaceFormatKHR *surfFormats = (VkSurfaceFormatKHR *)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
		err = fpGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfFormats);
		assert(!err);

		// If the format list includes just one entry of VK_FORMAT_UNDEFINED,
		// the surface has no preferred format.  Otherwise, at least one
		// supported format will be returned.
		if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
		}
		else
		{
			assert(formatCount >= 1);
			colorFormat = surfFormats[0].format;
		}
		colorSpace = surfFormats[0].colorSpace;*/
	}

	void init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		this->instance = instance;
		this->physicalDevice = physicalDevice;
		this->device = device;
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceSupportKHR);
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceFormatsKHR);
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfacePresentModesKHR);
		GET_DEVICE_PROC_ADDR(device, CreateSwapchainKHR);
		GET_DEVICE_PROC_ADDR(device, DestroySwapchainKHR);
		GET_DEVICE_PROC_ADDR(device, GetSwapchainImagesKHR);
		GET_DEVICE_PROC_ADDR(device, AcquireNextImageKHR);
		GET_DEVICE_PROC_ADDR(device, QueuePresentKHR);
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
		return fpAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, currentBuffer);
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
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}

};
