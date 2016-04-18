/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else 
// todo : split linux xcb/x11 and android
#include <xcb/xcb.h>
#endif

#include <iostream>
#include <chrono>

#define GLM_FORCE_RADIANS
#include "../glm/glm.hpp"
#include <string>
#include <array>

#include "../vulkan/vulkan.h"

//#include "vulkantools.h"
#include "vulkandebug.h"

//#include "vulkanswapchain.hpp"
#include "vulkanTextureLoader.hpp"
#include "vulkanMeshLoader.hpp"

#define deg_to_rad(deg) deg * float(3.14159 / 180)

class VulkanExampleBase : public vku::window
{
private:	
	// Set to true when example is created with enabled validation layers
	bool enableValidation = false;
	// Create application wide Vulkan instance
	VkResult createInstance(bool enableValidation);
	// Create logical Vulkan device based on physical device
	VkResult createDevice(VkDeviceQueueCreateInfo requestedQueues, bool enableValidation);
protected:
public: 

	//VulkanExampleBase(bool enableValidation);
	//VulkanExampleBase() : VulkanExampleBase(false) {};
	//~VulkanExampleBase();

	// Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
	void initVulkan(bool enableValidation);

#ifdef _WIN32 
	void setupConsole(std::string title);
	HWND setupWindow(HINSTANCE hinstance, WNDPROC wndproc);
	void handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#else
	xcb_window_t setupWindow();
	void initxcbConnection();
	void handleEvent(const xcb_generic_event_t *event);
#endif
	// Pure virtual render function (override in derived class)
	virtual void render() = 0;
	// Called when view change occurs
	// Can be overriden in derived class to e.g. update uniform buffers 
	// Containing view dependant matrices
	virtual void viewChanged();

	// Get memory type for a given memory allocation (flags and bits)
	VkBool32 getMemoryType(uint32_t typeBits, VkFlags properties, uint32_t *typeIndex);

	// Creates a new (graphics) command pool object storing command buffers
	void createCommandPool();
	// Setup default depth and stencil views
	void setupDepthStencil();
	// Create framebuffers for all requested swap chain images
	void setupFrameBuffer();
	// Setup a default render pass
	void setupRenderPass();

	// Connect and prepare the swap chain
	void initSwapchain();
	// Create swap chain images
	void setupSwapChain();

	// Check if command buffers are valid (!= VK_NULL_HANDLE)
	bool checkCommandBuffers();
	// Create command buffers for drawing commands
	void createCommandBuffers();
	// Destroy all command buffers and set their handles to VK_NULL_HANDLE
	// May be necessary during runtime if options are toggled 
	void destroyCommandBuffers();
	// Create command buffer for setup commands
	//void createSetupCommandBuffer();
	// Finalize setup command bufferm submit it to the queue and remove it
	void flushSetupCommandBuffer();

	// Create a cache pool for rendering pipelines
	void createPipelineCache();

	// Prepare commonly used Vulkan functions
	//void prepare();

	// Load a SPIR-V shader
	VkPipelineShaderStageCreateInfo loadShader(const char* fileName, VkShaderStageFlagBits stage);
	// Load a GLSL shader
	// NOTE : This may not work with any IHV and requires some magic
	VkPipelineShaderStageCreateInfo loadShaderGLSL(const char* fileName, VkShaderStageFlagBits stage);

	// Create a buffer, fill it with data and bind buffer memory
	// Can be used for e.g. vertex or index buffer based on mesh data
	VkBool32 createBuffer(
		VkBufferUsageFlags usage,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory);
	// Overload that assigns buffer info to descriptor
	VkBool32 createBuffer(
		VkBufferUsageFlags usage,
		VkDeviceSize size,
		void *data,
		VkBuffer *buffer,
		VkDeviceMemory *memory,
		VkDescriptorBufferInfo *descriptor);

	// Start the main render loop
	void renderLoop();

	// Submit a post present image barrier to the queue
	// Transforms image layout back to color attachment layout
	void submitPostPresentBarrier(VkImage image);
};

