////////////////////////////////////////////////////////////////////////////////
//
// Minimalistic Vulkan Triangle sample
//
// 


#define VERTEX_BUFFER_BIND_ID 0


// vulkan utilities.
#include "vku.hpp"

// Please note that this is deliberately minimal to aid comprehension.
// There is no error checking and it may assume certain properties of
// hardware.
class VulkanExample : public VulkanExampleBase
{
public:
	struct {
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
	} uniform_data;

  vku::buffer vertex_buffer;
  vku::buffer index_buffer;
  vku::buffer uniform_buffer;
  vku::vertexInputState vertexInputState;
  vku::descriptorPool descPool;
  vku::pipeline pipe;
  size_t num_indices;

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
		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
      vku::cmdBuffer cmdbuf(drawCmdBuffers[i]);
      cmdbuf.begin(renderPass, frameBuffers[i], width, height);

      cmdbuf.bindPipeline(pipe);
      cmdbuf.bindVertexBuffer(vertex_buffer, VERTEX_BUFFER_BIND_ID);
      cmdbuf.bindIndexBuffer(index_buffer);
      cmdbuf.drawIndexed((uint32_t)num_indices, 1, 0, 0, 1);

      cmdbuf.end(swapChain.buffers[i].image);
		}
	}

	void draw()
	{
    vku::semaphore sema(device);

		VkResult err;
		/*VkSemaphore presentCompleteSemaphore;
		VkSemaphoreCreateInfo presentCompleteSemaphoreCreateInfo = {};
		presentCompleteSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		presentCompleteSemaphoreCreateInfo.pNext = NULL;
		presentCompleteSemaphoreCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		err = vkCreateSemaphore(device, &presentCompleteSemaphoreCreateInfo, nullptr, &presentCompleteSemaphore);
		assert(!err);*/

		// Get next image in the swap chain (back/front buffer)
		err = swapChain.acquireNextImage(sema, &currentBuffer);
		assert(!err);

		// The submit infor strcuture contains a list of
		// command buffers and semaphores to be submitted to a queue
		// If you want to submit multiple command buffers, pass an array
		VkSubmitInfo submitInfo = {};
    VkSemaphore s[] = { sema };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = s;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

		// Submit to the graphics queue
		err = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(!err);

		// Present the current buffer to the swap chain
		// This will display the image
		err = swapChain.queuePresent(queue, currentBuffer);
		assert(!err);

		//vkDestroySemaphore(device, presentCompleteSemaphore, nullptr);

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
    typedef float f3[3];
		static const f3 vertex_data[] = {
			{ 1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f },
			{ -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f}
		};

		// Setup indices
		static const uint32_t index_data[] = { 0, 1, 2 };

    vku::device dev(device, physicalDevice);
    vertex_buffer = vku::buffer(dev, (void*)vertex_data, sizeof(vertex_data), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    index_buffer = vku::buffer(dev, (void*)index_data, sizeof(index_data), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		num_indices = 3;

    vertexInputState.binding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    vertexInputState.attrib(0, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, 0);
    vertexInputState.attrib(1, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);
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
    pipe.updateDescriptorSets(uniform_buffer);
	}

	void preparePipelines()
	{
    pipe = vku::pipeline(device, renderPass, vertexInputState.get(), pipelineCache);
	}

	void prepareUniformBuffers()
	{
    vku::device dev(device, physicalDevice);
    uniform_buffer = vku::buffer(dev, (void*)nullptr, sizeof(uniform_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		
		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		// Update matrices
		uniform_data.projectionMatrix = glm::perspective(deg_to_rad(60.0f), (float)width / (float)height, 0.1f, 256.0f);

		uniform_data.viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		uniform_data.modelMatrix = glm::mat4();
		uniform_data.modelMatrix = glm::rotate(uniform_data.modelMatrix, deg_to_rad(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uniform_data.modelMatrix = glm::rotate(uniform_data.modelMatrix, deg_to_rad(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uniform_data.modelMatrix = glm::rotate(uniform_data.modelMatrix, deg_to_rad(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		void *dest = uniform_buffer.map();
 		memcpy(dest, &uniform_data, sizeof(uniform_data));
    uniform_buffer.unmap();
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
