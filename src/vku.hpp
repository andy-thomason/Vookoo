
#ifndef VKU_INCLUDED
#define VKU_INCLUDED


#include "../vulkan/vulkan.h"
#include <cstring>

// derived from https://github.com/SaschaWillems/Vulkan

namespace vku {

class device;

class instance {
public:
  instance(VkInstance inst) : inst(inst) {
  }

  operator VkInstance() const { return inst; }
public:
  VkInstance inst;
};

class device {
public:
  device(VkDevice dev, VkPhysicalDevice physicalDevice) : dev(dev), physicalDevice(physicalDevice) {
  }

  uint32_t getMemoryType(uint32_t typeBits, VkFlags properties) {
    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
  	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	  for (uint32_t i = 0; i < 32; i++) {
		  if (typeBits & (1<<i)) {
			  if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				  return i;
			  }
		  }
	  }
	  return ~(uint32_t)0;
  }

  operator VkDevice() const { return dev; }
public:
  VkDevice dev;
  VkPhysicalDevice physicalDevice;
};

class buffer {
public:
  buffer(VkDevice dev = nullptr, VkBuffer buf = nullptr) : buf_(buf), dev(dev) {
  }

  buffer(VkDevice dev, VkBufferCreateInfo *bufInfo) : dev(dev), size_(bufInfo->size) {
		vkCreateBuffer(dev, bufInfo, nullptr, &buf_);
    ownsBuffer = true;
  }

  buffer(device dev, void *init, VkDeviceSize size, VkBufferUsageFlags usage) : dev(dev), size_(size) {
    VkBufferCreateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size = size;
		bufInfo.usage = usage;
		VkResult err = vkCreateBuffer(dev, &bufInfo, nullptr, &buf_);
    if (err) throw err;

    ownsBuffer = true;

		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		vkGetBufferMemoryRequirements(dev, buf_, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = dev.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

 		err = vkAllocateMemory(dev, &memAlloc, nullptr, &mem);
    if (err) throw err;

		if (init) {
		  void *dest = map();
      std::memcpy(dest, init, size);
      unmap();
    }
    bind();
  }

  buffer(VkBufferCreateInfo bufInfo, VkDevice dev = nullptr) : dev(dev) {
		vkCreateBuffer(dev, &bufInfo, nullptr, &buf_);
  }

  // RAII move operator
  buffer &operator=(buffer &&rhs) {
    dev = rhs.dev;
    buf_ = rhs.buf_;
    mem = rhs.mem;
    size_ = rhs.size_;
    rhs.dev = nullptr;
    rhs.mem = nullptr;
    rhs.buf_ = nullptr;

    rhs.ownsBuffer = false;
    return *this;
  }

  ~buffer() {
    if (buf_ && ownsBuffer) {
      vkDestroyBuffer(dev, buf_, nullptr);
      buf_ = nullptr;
    }
  }

  void *map() {
    void *dest = nullptr;
    VkResult err = vkMapMemory(dev, mem, 0, size(), 0, &dest);
    if (err) throw err;
    return dest;
  }

  void unmap() {
		vkUnmapMemory(dev, mem);
  }

  void bind() {
		VkResult err = vkBindBufferMemory(dev, buf_, mem, 0);
    if (err) throw err;
  }

  size_t size() const {
    return size_;
  }

  //operator VkBuffer() const { return buf_; }

  VkBuffer buf() const { return buf_; }

  VkDescriptorBufferInfo desc() const {
    VkDescriptorBufferInfo d = {};
    d.buffer = buf_;
    d.range = size_;
    return d;
  }

private:
  VkBuffer buf_ = nullptr;
  VkDevice dev = nullptr;
  VkDeviceMemory mem = nullptr;
  size_t size_;
  bool ownsBuffer = false;
};

class vertexInputState {
public:
  vertexInputState() {
  }

  vertexInputState &operator=(vertexInputState && rhs) {
    vi = rhs.vi;
    bindingDescriptions = std::move(rhs.bindingDescriptions);
    attributeDescriptions = std::move(rhs.attributeDescriptions);
  }

  vertexInputState &attrib(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription desc = {};
    desc.location = location;
    desc.binding = binding;
    desc.format = format;
    desc.offset = offset;
    attributeDescriptions.push_back(desc);
    return *this;
  }

  vertexInputState &binding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX) {
    VkVertexInputBindingDescription desc = {};
    desc.binding = binding;
    desc.stride = stride;
    desc.inputRate = inputRate;
    bindingDescriptions.push_back(desc);
    return *this;
  }

  VkPipelineVertexInputStateCreateInfo *get() {
		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vi.pNext = nullptr;
		vi.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
		vi.pVertexBindingDescriptions = bindingDescriptions.data();
		vi.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
		vi.pVertexAttributeDescriptions = attributeDescriptions.data();
    return &vi;
  }

private:

	VkPipelineVertexInputStateCreateInfo vi;
	std::vector<VkVertexInputBindingDescription> bindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
};

class pipeline {
public:
  pipeline() {
  }

  pipeline(VkDevice device, VkRenderPass renderPass, VkPipelineVertexInputStateCreateInfo *vertexInputState, VkPipelineCache pipelineCache) : dev_(device) {
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
		if (err) throw err;

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
		if (err) throw err;

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};

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
		pipelineCreateInfo.pVertexInputState = vertexInputState;
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
		err = vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipe_);
		if (err) throw err;

    ownsData = true;
  }

  pipeline &operator=(pipeline &&rhs) {
    pipe_ = rhs.pipe_;
	  pipelineLayout = rhs.pipelineLayout;
	  descriptorSet = rhs.descriptorSet;
	  descriptorSetLayout = rhs.descriptorSetLayout;
    dev_ = rhs.dev_;
	  shaderModules = std::move(shaderModules);
    ownsData = true;
    rhs.ownsData = false;
    return *this;
  }

  ~pipeline() {
    if (ownsData) {
		  vkDestroyPipeline(dev_, pipe_, nullptr);
		  vkDestroyPipelineLayout(dev_, pipelineLayout, nullptr);
		  vkDestroyDescriptorSetLayout(dev_, descriptorSetLayout, nullptr);
    }
  }

  VkPipeline pipe() { return pipe_; }
  VkPipelineLayout layout() const { return pipelineLayout; }
  VkDescriptorSet *descriptorSets() { return &descriptorSet; }
  VkDescriptorSetLayout *descriptorLayouts() { return &descriptorSetLayout; }

private:
  VkPipelineShaderStageCreateInfo loadShader(const char * fileName, VkShaderStageFlagBits stage)
  {
	  VkPipelineShaderStageCreateInfo shaderStage = {};
	  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	  shaderStage.stage = stage;
	  shaderStage.module = vkTools::loadShader(fileName, dev_, stage);
	  shaderStage.pName = "main"; // todo : make param
	  assert(shaderStage.module != NULL);
	  shaderModules.push_back(shaderStage.module);
	  return shaderStage;
  }

  VkPipelineShaderStageCreateInfo loadShaderGLSL(const char * fileName, VkShaderStageFlagBits stage)
  {
	  VkPipelineShaderStageCreateInfo shaderStage = {};
	  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	  shaderStage.stage = stage;
	  shaderStage.module = vkTools::loadShaderGLSL(fileName, dev_, stage);
	  shaderStage.pName = "main"; // todo : make param
	  assert(shaderStage.module != NULL);
	  shaderModules.push_back(shaderStage.module);
	  return shaderStage;
  }

  VkPipeline pipe_ = nullptr;
	VkPipelineLayout pipelineLayout = nullptr;
	VkDescriptorSet descriptorSet = nullptr;
	VkDescriptorSetLayout descriptorSetLayout = nullptr;
  VkDevice dev_ = nullptr;
	std::vector<VkShaderModule> shaderModules;
  bool ownsData = false;
};

} // vku

#endif
