////////////////////////////////////////////////////////////////////////////////
//
// Minimalistic Vulkan Triangle sample
//
// 

#define USE_GLSL
#define VERTEX_BUFFER_BIND_ID 0


// vulkan utilities.
#include "vku.hpp"

#include "../glm/glm.hpp"
#include "../glm/gtc/matrix_transform.hpp"
#include "../base/vulkanexamplebase.h"
#include "../base/vulkanexamplebase.cpp"
#include "../base/vulkantools.cpp"

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

	void draw()
	{
    vku::semaphore sema(device);
    vku::queue theQueue(queue);

		// Get next image in the swap chain (back/front buffer)
		VkResult err = swapChain.acquireNextImage(sema, &currentBuffer);
		assert(!err);

    theQueue.submit(sema, drawCmdBuffers[currentBuffer]);

		// Present the current buffer to the swap chain
		// This will display the image
		err = swapChain.queuePresent(queue, currentBuffer);
		assert(!err);

    vku::cmdBuffer postPresent(postPresentCmdBuffer);
    postPresent.beginCommandBuffer();
    postPresent.pipelineBarrier(swapChain.buffers[currentBuffer].image);
    postPresent.endCommandBuffer();

    theQueue.submit(nullptr, postPresentCmdBuffer);

    theQueue.waitIdle();
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
    vku::device dev(device, physicalDevice);

		VulkanExampleBase::prepare();

    // Vertices
		struct Vertex { float pos[3]; float col[3]; };

		static const Vertex vertex_data[] = {
			{ { 1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f} }
		};

    vertex_buffer = vku::buffer(dev, (void*)vertex_data, sizeof(vertex_data), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		// Indices
		static const uint32_t index_data[] = { 0, 1, 2 };
    index_buffer = vku::buffer(dev, (void*)index_data, sizeof(index_data), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		num_indices = 3;

    // Binding state
    vertexInputState.binding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    vertexInputState.attrib(0, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, 0);
    vertexInputState.attrib(1, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);

    uniform_buffer = vku::buffer(dev, (void*)nullptr, sizeof(uniform_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		
		updateUniformBuffers();

    pipe = vku::pipeline(device, renderPass, vertexInputState.get(), pipelineCache);

    descPool = vku::descriptorPool(device);

    pipe.allocateDescriptorSets(descPool);
    pipe.updateDescriptorSets(uniform_buffer);

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

		prepared = true;
	}

	virtual void render()
	{
    vku::device dev(device, physicalDevice);

		if (!prepared)
			return;

		dev.waitIdle();

		draw();

		dev.waitIdle();
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
