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

	//VkPipelineLayout pipelineLayout;
	//VkDescriptorSet descriptorSet;
	//VkDescriptorSetLayout descriptorSetLayout;

	VulkanExample() : VulkanExampleBase(false)
	{
		width = 1280;
		height = 720;
		zoom = -2.5f;
		title = "Vulkan Example - Basic indexed triangle";
		// Values not set here are initialized in the base class constructor
	}

	~VulkanExample()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class
		//vkDestroyPipeline(device, pipelines.solid, nullptr);

		//vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		//vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		//vkDestroyBuffer(device, vertices.buf, nullptr);
		//vkFreeMemory(device, vertices.mem, nullptr);

		//vkDestroyBuffer(device, indices.buf, nullptr);
		//vkFreeMemory(device, indices.mem, nullptr);

		//vkDestroyBuffer(device, uniformDataVS.buffer, nullptr);
		//vkFreeMemory(device, uniformDataVS.memory, nullptr);
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
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			err = vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo);
			assert(!err);

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			// Update dynamic viewport state
			VkViewport viewport = {};
			viewport.height = (float)height;
			viewport.width = (float)width;
			viewport.minDepth = (float) 0.0f;
			viewport.maxDepth = (float) 1.0f;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			// Update dynamic scissor state
			VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			// Bind descriptor sets describing shader binding points
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout(), 0, 1, pipe.descriptorSets(), 0, NULL);

			// Bind the rendering pipeline (including the shaders)
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipe());

			// Bind triangle vertices
			VkDeviceSize offsets[] = { 0 };
      VkBuffer bufs[] = { vertices.storage.buf() };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, bufs, offsets);

			// Bind triangle indices
			vkCmdBindIndexBuffer(drawCmdBuffers[i], indices.storage.buf(), 0, VK_INDEX_TYPE_UINT32);

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

		/*VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAlloc.pNext = NULL;
		memAlloc.allocationSize = 0;
		memAlloc.memoryTypeIndex = 0;*/
		//VkMemoryRequirements memReqs;

		//VkResult err;
		//void *data;

    vku::device dev(device, physicalDevice);
    vertices.storage = vku::buffer(dev, vertexBuffer.data(), vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		// Generate vertex buffer
		//	Setup
		/*VkBufferCreateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.pNext = NULL;
		bufInfo.size = vertexBufferSize;
		bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufInfo.flags = 0;
		//	Copy vertex data to VRAM
		memset(&vertices, 0, sizeof(vertices));
		err = vkCreateBuffer(device, &bufInfo, nullptr, &vertices.buf);
		assert(!err);
		vkGetBufferMemoryRequirements(device, vertices.buf, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex);
 		vkAllocateMemory(device, &memAlloc, nullptr, &vertices.mem);
		assert(!err);
		err = vkMapMemory(device, vertices.mem, 0, memAlloc.allocationSize, 0, &data);
		assert(!err);
		memcpy(data, vertexBuffer.data(), vertexBufferSize);
		vkUnmapMemory(device, vertices.mem);
		assert(!err);
		err = vkBindBufferMemory(device, vertices.buf, vertices.mem, 0);
		assert(!err);*/

		// Generate index buffer
		//	Setup
		/*VkBufferCreateInfo indexbufferInfo = {};
		indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexbufferInfo.pNext = NULL;
		indexbufferInfo.size = indexBufferSize;
		indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		indexbufferInfo.flags = 0;
		// Copy index data to VRAM
		memset(&indices, 0, sizeof(indices));
		err = vkCreateBuffer(device, &indexbufferInfo, nullptr, &indices.buf);
		assert(!err);
		vkGetBufferMemoryRequirements(device, indices.buf, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex);
		err = vkAllocateMemory(device, &memAlloc, nullptr, &indices.mem);
		assert(!err);
		err = vkMapMemory(device, indices.mem, 0, indexBufferSize, 0, &data);
		assert(!err);
		memcpy(data, indexBuffer.data(), indexBufferSize);
		vkUnmapMemory(device, indices.mem);
		err = vkBindBufferMemory(device, indices.buf, indices.mem, 0);
		assert(!err);*/

    indices.storage = vku::buffer(dev, indexBuffer.data(), indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		indices.count = indexBuffer.size();

    vertices.vertexInputState.binding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    vertices.vertexInputState.attrib(0, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, 0);
    vertices.vertexInputState.attrib(1, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);

    /*
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0].binding = VERTEX_BUFFER_BIND_ID;
		vertices.bindingDescriptions[0].stride = sizeof(Vertex);
		vertices.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// Attribute descriptions
		// Describes memory layout and shader attribute locations
		vertices.attributeDescriptions.resize(2);
		// Location 0 : Position
		vertices.attributeDescriptions[0].binding = VERTEX_BUFFER_BIND_ID;
		vertices.attributeDescriptions[0].location = 0;
		vertices.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertices.attributeDescriptions[0].offset = 0;
		vertices.attributeDescriptions[0].binding = 0;
		// Location 1 : Color
		vertices.attributeDescriptions[1].binding = VERTEX_BUFFER_BIND_ID;
		vertices.attributeDescriptions[1].location = 1;
		vertices.attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertices.attributeDescriptions[1].offset = sizeof(float) * 3;
		vertices.attributeDescriptions[1].binding = 0;

		// Assign to vertex buffer
		vertices.vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertices.vi.pNext = NULL;
		vertices.vi.vertexBindingDescriptionCount = (uint32_t)vertices.bindingDescriptions.size();
		vertices.vi.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.vi.vertexAttributeDescriptionCount = (uint32_t)vertices.attributeDescriptions.size();
		vertices.vi.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();*/
	}

	void setupDescriptorPool()
	{
		// We need to tell the API the number of max. requested descriptors per type
		VkDescriptorPoolSize typeCounts[1];
		// This example only uses one descriptor type (uniform buffer) and only
		// requests one descriptor of this type
		typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		typeCounts[0].descriptorCount = 1;
		// For additional types you need to add new entries in the type count list
		// E.g. for two combined image samplers :
		// typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		// typeCounts[1].descriptorCount = 2;

		// Create the global descriptor pool
		// All descriptors used in this example are allocated from this pool
		VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.pNext = NULL;
		descriptorPoolInfo.poolSizeCount = 1;
		descriptorPoolInfo.pPoolSizes = typeCounts;
		// Set the max. number of sets that can be requested
		// Requesting descriptors beyond maxSets will result in an error
		descriptorPoolInfo.maxSets = 1;

		VkResult vkRes = vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool);
		assert(!vkRes);
	}

	void setupDescriptorSetLayout()
	{
  /*
		// Setup layout of descriptors used in this example
		// Basically connects the different shader stages to descriptors
		// for binding uniform buffers, image samplers, etc.
		// So every shader binding should map to one descriptor set layout
		// binding

		// Binding 0 : Uniform buffer (Vertex shader)
		VkDescriptorSetLayoutBinding layoutBinding = {};
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = 1;
		layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBinding.pImmutableSamplers = NULL;

		VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
		descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayout.pNext = NULL;
		descriptorLayout.bindingCount = 1;
		descriptorLayout.pBindings = &layoutBinding;

		VkResult err = vkCreateDescriptorSetLayout(device, &descriptorLayout, NULL, &descriptorSetLayout);
		assert(!err);

		// Create the pipeline layout that is used to generate the rendering pipelines that
		// are based on this descriptor set layout
		// In a more complex scenario you would have different pipeline layouts for different
		// descriptor set layouts that could be reused
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
		pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pPipelineLayoutCreateInfo.pNext = NULL;
		pPipelineLayoutCreateInfo.setLayoutCount = 1;
		pPipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

		err = vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		assert(!err);
  */
	}

	void setupDescriptorSet()
	{
		// Update descriptor sets determining the shader binding points
		// For every binding point used in a shader there needs to be one
		// descriptor set matching that binding point
		VkWriteDescriptorSet writeDescriptorSet = {};

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = pipe.descriptorLayouts();

		VkResult vkRes = vkAllocateDescriptorSets(device, &allocInfo, pipe.descriptorSets());
		assert(!vkRes);

		// Binding 0 : Uniform buffer
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = pipe.descriptorSets()[0];
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &uniformDataVS.storage.desc();
		// Binds this uniform buffer to binding point 0
		writeDescriptorSet.dstBinding = 0;

		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);
	}

	void preparePipelines()
	{
    pipe = vku::pipeline(device, renderPass, vertices.vertexInputState.get(), pipelineCache);

		// Create our rendering pipeline used in this example
		// Vulkan uses the concept of rendering pipelines to encapsulate
		// fixed states
		// This replaces OpenGL's huge (and cumbersome) state machine
		// A pipeline is then stored and hashed on the GPU making
		// pipeline changes much faster than having to set dozens of 
		// states
		// In a real world application you'd have dozens of pipelines
		// for every shader set used in a scene
		// Note that there are a few states that are not stored with
		// the pipeline. These are called dynamic states and the 
		// pipeline only stores that they are used with this pipeline,
		// but not their states

		/*VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};

		VkResult err;

		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		// The layout used for this pipeline
		pipelineCreateInfo.layout = pipelineLayout;
		// Renderpass this pipeline is attached to
		pipelineCreateInfo.renderPass = renderPass;

		// Vertex input state
		// Describes the topoloy used with this pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
		inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		// This pipeline renders vertex data as triangle lists
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// Rasterization state
		VkPipelineRasterizationStateCreateInfo rasterizationState = {};
		rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		// Solid polygon mode
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		// No culling
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationState.depthClampEnable = VK_FALSE;
		rasterizationState.rasterizerDiscardEnable = VK_FALSE;
		rasterizationState.depthBiasEnable = VK_FALSE;

		// Color blend state
		// Describes blend modes and color masks
		VkPipelineColorBlendStateCreateInfo colorBlendState = {};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		// One blend attachment state
		// Blending is not used in this example
		VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
		blendAttachmentState[0].colorWriteMask = 0xf;
		blendAttachmentState[0].blendEnable = VK_FALSE;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = blendAttachmentState;

		// Viewport state
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		// One viewport
		viewportState.viewportCount = 1;
		// One scissor rectangle
		viewportState.scissorCount = 1;

		// Enable dynamic states
		// Describes the dynamic states to be used with this pipeline
		// Dynamic states can be set even after the pipeline has been created
		// So there is no need to create new pipelines just for changing
		// a viewport's dimensions or a scissor box
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		// The dynamic state properties themselves are stored in the command buffer
		std::vector<VkDynamicState> dynamicStateEnables;
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables.data();
		dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

		// Depth and stencil state
		// Describes depth and stenctil test and compare ops
		VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
		// Basic depth compare setup with depth writes and depth test enabled
		// No stencil used 
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
		depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
		depthStencilState.stencilTestEnable = VK_FALSE;
		depthStencilState.front = depthStencilState.back;

		// Multi sampling state
		VkPipelineMultisampleStateCreateInfo multisampleState = {};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.pSampleMask = NULL;
		// No multi sampling used in this example
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Load shaders
		VkPipelineShaderStageCreateInfo shaderStages[2] = { {},{} };

#ifdef USE_GLSL
		shaderStages[0] = loadShaderGLSL("data/shaders/_test/test.vert", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShaderGLSL("data/shaders/_test/test.frag", VK_SHADER_STAGE_FRAGMENT_BIT);
#else
		shaderStages[0] = loadShader("data/shaders/triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader("data/shaders/triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
#endif

		// Assign states
		// Two shader stages
		pipelineCreateInfo.stageCount = 2;
		// Assign pipeline state create information
		pipelineCreateInfo.pVertexInputState = vertices.vertexInputState.get();
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pStages = shaderStages;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.pDynamicState = &dynamicState;

		// Create rendering pipeline
		err = vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solid);
		assert(!err);*/

	}

	void prepareUniformBuffers()
	{
    vku::device dev(device, physicalDevice);
    uniformDataVS.storage = vku::buffer(dev, (void*)nullptr, sizeof(uboVS), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		
		// Store information in the uniform's descriptor
		//uniformDataVS.descriptor.buffer = uniformDataVS.storage.buf();
		//uniformDataVS.descriptor.offset = 0;
		//uniformDataVS.descriptor.range = sizeof(uboVS);

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
