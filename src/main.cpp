////////////////////////////////////////////////////////////////////////////////
//
// Minimalistic Vulkan Triangle sample
//
// 

#ifdef _WIN32
  #define VK_USE_PLATFORM_WIN32_KHR
  #pragma comment(lib, "vulkan/vulkan-1.lib")
#endif

#define VERTEX_BUFFER_BIND_ID 0
#define _CRT_SECURE_NO_WARNINGS


#include "../vulkan/vulkan.h"
#include "../glm/glm.hpp"
#include "../base/vulkanexamplebase.h"
#include "../base/vulkanexamplebase.cpp"
#include "../base/vulkantools.cpp"
#include "../glm/gtc/matrix_transform.hpp"

// vulkan utilities.
#include "vku.hpp"

// Please note that this is deliberately minimal to aid comprehension.
// There is no error checking and it may assume certain properties of
// hardware.
class VulkanExample : public VulkanExampleBase
{
public:
	struct {
    vku::buffer storage;
    vku::vertexInputState vertexInputState;
	} vertices;

	struct {
		size_t count;
		vku::buffer storage;
	} indices;

	struct {
    vku::buffer storage;
	} uniformDataVS;

	struct {
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
	} uboVS;

  vku::pipeline pipe;

  vku::descriptorPool descPool;

	VulkanExample() : VulkanExampleBase(false)
	{
		width = 1280;
		height = 720;
		zoom = -2.5f;
		title = "Vulkan Example - Basic indexed triangle";
		// Values not set here are initialized in the base class constructor
	}

	// Build separate command buffers for every framebuffer image
	// Unlike in OpenGL all rendering commands are recorded once
	// into command buffers that are then resubmitted to the queue
	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.pNext = NULL;

		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = NULL;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		VkResult err;
		
		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
      vku::cmdBuffer cmdbuf(drawCmdBuffers[i]);

			// Set target frame buffer
			//renderPassBeginInfo.framebuffer = frameBuffers[i];

      cmdbuf.beginCommandBuffer();

			//err = vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo);
			//assert(!err);

      cmdbuf.beginRenderPass(renderPass, frameBuffers[i], 0, 0, width, height);
			//vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			// Update dynamic viewport state
			/*VkViewport viewport = {};
			viewport.height = (float)height;
			viewport.width = (float)width;
			viewport.minDepth = (float) 0.0f;
			viewport.maxDepth = (float) 1.0f;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);*/

      cmdbuf.setViewport(0, 0, (float)width, (float)height);

			// Update dynamic scissor state
			/*VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);*/

      cmdbuf.setScissor(0, 0, width, height);

      cmdbuf.bindPipeline(pipe);

      cmdbuf.bindVertexBuffer(vertices.storage, VERTEX_BUFFER_BIND_ID);
      cmdbuf.bindIndexBuffer(indices.storage);

			// Bind descriptor sets describing shader binding points
			//vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout(), 0, 1, pipe.descriptorSets(), 0, NULL);

			// Bind the rendering pipeline (including the shaders)
			//vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipe());

			// Bind triangle vertices
			//VkDeviceSize offsets[] = { 0 };
      //VkBuffer bufs[] = { vertices.storage.buf() };
			//vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, bufs, offsets);

			// Bind triangle indices
			//vkCmdBindIndexBuffer(drawCmdBuffers[i], indices.storage.buf(), 0, VK_INDEX_TYPE_UINT32);

			// Draw indexed triangle
			vkCmdDrawIndexed(drawCmdBuffers[i], (uint32_t)indices.count, 1, 0, 0, 1);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			// Add a present memory barrier to the end of the command buffer
			// This will transform the frame buffer color attachment to a
			// new layout for presenting it to the windowing system integration 
			VkImageMemoryBarrier prePresentBarrier = {};
			prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			prePresentBarrier.pNext = NULL;
			prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			prePresentBarrier.dstAccessMask = 0;
			prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			prePresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };			
			prePresentBarrier.image = swapChain.buffers[i].image;

			VkImageMemoryBarrier *pMemoryBarrier = &prePresentBarrier;
			vkCmdPipelineBarrier(
				drawCmdBuffers[i], 
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
				VK_FLAGS_NONE,
				0, nullptr,
				0, nullptr,
				1, &prePresentBarrier);

			err = vkEndCommandBuffer(drawCmdBuffers[i]);
			assert(!err);
		}
	}

	void draw()
	{
		VkResult err;
		VkSemaphore presentCompleteSemaphore;
		VkSemaphoreCreateInfo presentCompleteSemaphoreCreateInfo = {};
		presentCompleteSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		presentCompleteSemaphoreCreateInfo.pNext = NULL;
		presentCompleteSemaphoreCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		err = vkCreateSemaphore(device, &presentCompleteSemaphoreCreateInfo, nullptr, &presentCompleteSemaphore);
		assert(!err);

		// Get next image in the swap chain (back/front buffer)
		err = swapChain.acquireNextImage(presentCompleteSemaphore, &currentBuffer);
		assert(!err);

		// The submit infor strcuture contains a list of
		// command buffers and semaphores to be submitted to a queue
		// If you want to submit multiple command buffers, pass an array
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &presentCompleteSemaphore;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

		// Submit to the graphics queue
		err = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(!err);

		// Present the current buffer to the swap chain
		// This will display the image
		err = swapChain.queuePresent(queue, currentBuffer);
		assert(!err);

		vkDestroySemaphore(device, presentCompleteSemaphore, nullptr);

		// Add a post present image memory barrier
		// This will transform the frame buffer color attachment back
		// to it's initial layout after it has been presented to the
		// windowing system
		// See buildCommandBuffers for the pre present barrier that 
		// does the opposite transformation 
		VkImageMemoryBarrier postPresentBarrier = {};
		postPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		postPresentBarrier.pNext = NULL;
		postPresentBarrier.srcAccessMask = 0;
		postPresentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		postPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		postPresentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		postPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		postPresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		postPresentBarrier.image = swapChain.buffers[currentBuffer].image;

		// Use dedicated command buffer from example base class for submitting the post present barrier
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		err = vkBeginCommandBuffer(postPresentCmdBuffer, &cmdBufInfo);
		assert(!err);

		// Put post present barrier into command buffer
		vkCmdPipelineBarrier(
			postPresentCmdBuffer,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			0, nullptr,
			1, &postPresentBarrier);

		err = vkEndCommandBuffer(postPresentCmdBuffer);
		assert(!err);

		// Submit to the queue
		submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &postPresentCmdBuffer;

		err = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(!err);
		
		err = vkQueueWaitIdle(queue);
		assert(!err);
	}

	// Setups vertex and index buffers for an indexed triangle,
	// uploads them to the VRAM and sets binding points and attribute
	// descriptions to match locations inside the shaders
	void prepareVertices()
	{
		struct Vertex {
			float pos[3];
			float col[3];
		};

		// Setup vertices
		std::vector<Vertex> vertexBuffer = {
			{ { 1.0f,  1.0f, 0.0f },{ 1.0f, 0.0f, 0.0f } },
			{ { -1.0f,  1.0f, 0.0f },{ 0.0f, 1.0f, 0.0f } },
			{ { 0.0f, -1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }
		};
		size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);

		// Setup indices
		std::vector<uint32_t> indexBuffer = { 0, 1, 2 };
		size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);

    vku::device dev(device, physicalDevice);
    vertices.storage = vku::buffer(dev, vertexBuffer.data(), vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    indices.storage = vku::buffer(dev, indexBuffer.data(), indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		indices.count = indexBuffer.size();

    vertices.vertexInputState.binding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    vertices.vertexInputState.attrib(0, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, 0);
    vertices.vertexInputState.attrib(1, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);
	}

	void setupDescriptorPool()
	{
    descPool = vku::descriptorPool(device);
	}

	void setupDescriptorSetLayout()
	{
	}

	void setupDescriptorSet()
	{
    pipe.allocateDescriptorSets(descPool);
    pipe.updateDescriptorSets(uniformDataVS.storage);
	}

	void preparePipelines()
	{
    pipe = vku::pipeline(device, renderPass, vertices.vertexInputState.get(), pipelineCache);
	}

	void prepareUniformBuffers()
	{
    vku::device dev(device, physicalDevice);
    uniformDataVS.storage = vku::buffer(dev, (void*)nullptr, sizeof(uboVS), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		
		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		// Update matrices
		uboVS.projectionMatrix = glm::perspective(deg_to_rad(60.0f), (float)width / (float)height, 0.1f, 256.0f);

		uboVS.viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		uboVS.modelMatrix = glm::mat4();
		uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, deg_to_rad(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, deg_to_rad(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, deg_to_rad(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		void *dest = uniformDataVS.storage.map();
 		memcpy(dest, &uboVS, sizeof(uboVS));
    uniformDataVS.storage.unmap();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		prepareVertices();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		vkDeviceWaitIdle(device);
		draw();
		vkDeviceWaitIdle(device);

	}

	virtual void viewChanged()
	{
		// This function is called by the base example class 
		// each time the view is changed by user input
		updateUniformBuffers();
	}
};

VulkanExample *vulkanExample;

#ifdef _WIN32

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (vulkanExample != NULL)
	{
		vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

#else 

static void handleEvent(const xcb_generic_event_t *event)
{
	if (vulkanExample != NULL)
	{
		vulkanExample->handleEvent(event);
	}
}
#endif

#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
#else
int main(const int argc, const char *argv[])
#endif
{
	vulkanExample = new VulkanExample();
#ifdef _WIN32
	vulkanExample->setupWindow(hInstance, WndProc);
#else
	vulkanExample->setupWindow();
#endif
	vulkanExample->initSwapchain();
	vulkanExample->prepare();
	vulkanExample->renderLoop();
	delete(vulkanExample);
	return 0;
}
