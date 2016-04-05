
#ifndef VKU_INCLUDED
#define VKU_INCLUDED


#ifdef _WIN32
  #define VK_USE_PLATFORM_WIN32_KHR
  #pragma comment(lib, "vulkan/vulkan-1.lib")
  #define _CRT_SECURE_NO_WARNINGS

#endif

#include "../vulkan/vulkan.h"

#include <cstring>
#include <vector>
#include <fstream>

// derived from https://github.com/SaschaWillems/Vulkan

namespace vku {

template <class VulkanType> VulkanType create(VkDevice dev) {}
template <class VulkanType> void destroy(VkDevice dev, VulkanType value) {}




template <class VulkanType>
class resource {
public:
  resource() : value_(nullptr), ownsResource(false), dev_(nullptr) {
  }

  resource(VulkanType value, VkDevice dev = nullptr) : value_(value), ownsResource(false), dev_(dev) {
  }

  resource(VkDevice dev = nullptr) : value_(create<VulkanType>(dev)), ownsResource(true), dev_(dev) {
  }

  void operator=(resource &&rhs) {
    if (value_ && ownsResource) destroy<VulkanType>(dev_, value_);
    value_ = rhs.value_;
    ownsResource = rhs.ownsResource;
    rhs.value_ = nullptr;
    rhs.ownsResource = false;
  }

  ~resource() {
    if (value_ && ownsResource) destroy<VulkanType>(dev_, value_);
  }

  operator VulkanType() {
    return value_;
  }

  VulkanType get() const { return value_; }
  VkDevice dev() const { return dev_; }
private:
  VulkanType value_ = nullptr;
  bool ownsResource = false;
  VkDevice dev_ = nullptr;
};

template<> VkInstance create<VkInstance>(VkDevice dev) {
  bool enableValidation = false;
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "vku";
	appInfo.pEngineName = "vku";

	// Temporary workaround for drivers not supporting SDK 1.0.3 upon launch
	// todo : Use VK_API_VERSION 
	appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 2);

	std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

  #ifdef _WIN32
    enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
  #else
    // todo : linux/android
    enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
  #endif

	// todo : check if all extensions are present

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (enabledExtensions.size() > 0)
	{
		if (enableValidation)
		{
			enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
	}
	if (enableValidation)
	{
		//instanceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount; // todo : change validation layer names!
		//instanceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
	}

  VkInstance inst = nullptr;
	vkCreateInstance(&instanceCreateInfo, nullptr, &inst);
  return inst;
}

template<> void destroy<VkInstance>(VkDevice dev, VkInstance inst) {
  vkDestroyInstance(inst, nullptr);
}

class instance : public resource<VkInstance> {
public:
  instance() : resource((VkInstance)nullptr) {
  }

  /// queue that does not own its pointer
  instance(VkInstance value) : resource(value, nullptr) {
  }

  /// queue that does owns (and creates) its pointer
  instance(const char *name) : resource((VkDevice)nullptr) {
  }
public:
  bool enableValidation = false;
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

  void waitIdle() const {
    vkDeviceWaitIdle(dev);
  }
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

class descriptorPool {
public:
  descriptorPool() {
  }

  descriptorPool(VkDevice dev) : dev_(dev) {
		// We need to tell the API the number of max. requested descriptors per type
		VkDescriptorPoolSize typeCounts[1];
		// This example only uses one descriptor type (uniform buffer) and only
		// requests one descriptor of this type
		typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		typeCounts[0].descriptorCount = 2;
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
		descriptorPoolInfo.maxSets = 2;

		VkResult err = vkCreateDescriptorPool(dev_, &descriptorPoolInfo, nullptr, &pool_);
    if (err) throw err;

    ownsResource_ = true;
  }

  // allocate a descriptor set for a buffer
  VkWriteDescriptorSet *allocateDescriptorSet(const buffer &buffer, const VkDescriptorSetLayout *layout, VkDescriptorSet *descriptorSets) {
		// Update descriptor sets determining the shader binding points
		// For every binding point used in a shader there needs to be one
		// descriptor set matching that binding point

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool_;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layout;

		VkResult err = vkAllocateDescriptorSets(dev_, &allocInfo, descriptorSets);
    if (err) throw err;

		// Binding 0 : Uniform buffer
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSets[0];
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &buffer.desc();
		// Binds this uniform buffer to binding point 0
		writeDescriptorSet.dstBinding = 0;

    return &writeDescriptorSet;
  }

  ~descriptorPool() {
    if (pool_ && ownsResource_) {
      vkDestroyDescriptorPool(dev_, pool_, nullptr);
      ownsResource_ = false;
    }
  }

  descriptorPool &operator=(descriptorPool &&rhs) {
    ownsResource_ = true;
    pool_ = rhs.pool_;
    rhs.ownsResource_ = false;
    dev_ = rhs.dev_;
    return *this;
  }

  operator VkDescriptorPool() { return pool_; }
private:
  VkDevice dev_ = nullptr;
  VkDescriptorPool pool_ = nullptr;
  bool ownsResource_ = false;
  VkWriteDescriptorSet writeDescriptorSet = {};
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
		shaderStages[0] = loadShaderGLSL("data/shaders/triangle.vert", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShaderGLSL("data/shaders/triangle.frag", VK_SHADER_STAGE_FRAGMENT_BIT);
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

  void allocateDescriptorSets(descriptorPool &descPool) {
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

    descriptorSet = nullptr;
		VkResult err = vkAllocateDescriptorSets(dev_, &allocInfo, &descriptorSet);
		if (err) throw err;
  }

  void updateDescriptorSets(buffer &uniformVS) {
		VkWriteDescriptorSet writeDescriptorSet = {};

		// Binding 0 : Uniform buffer
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &uniformVS.desc();
		// Binds this uniform buffer to binding point 0
		writeDescriptorSet.dstBinding = 0;

		vkUpdateDescriptorSets(dev_, 1, &writeDescriptorSet, 0, NULL);
  }

  VkPipeline pipe() { return pipe_; }
  VkPipelineLayout layout() const { return pipelineLayout; }
  VkDescriptorSet *descriptorSets() { return &descriptorSet; }
  VkDescriptorSetLayout *descriptorLayouts() { return &descriptorSetLayout; }

private:
  VkPipelineShaderStageCreateInfo loadShader(const char * fileName, VkShaderStageFlagBits stage)
  {
    std::ifstream input(fileName, std::ios::binary);
    auto &b = std::istreambuf_iterator<char>(input);
    auto &e = std::istreambuf_iterator<char>();
    std::vector<char> buf(b, e);

		VkShaderModuleCreateInfo moduleCreateInfo = {};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = buf.size();
		moduleCreateInfo.pCode = (uint32_t*)buf.data();
		moduleCreateInfo.flags = 0;
		VkShaderModule shaderModule;
		VkResult err = vkCreateShaderModule(dev_, &moduleCreateInfo, NULL, &shaderModule);
    if (err) throw err;

	  VkPipelineShaderStageCreateInfo shaderStage = {};
	  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	  shaderStage.stage = stage;
	  shaderStage.module = shaderModule;
	  shaderStage.pName = "main"; // todo : make param
	  shaderModules.push_back(shaderStage.module);
	  return shaderStage;
  }

  VkPipelineShaderStageCreateInfo loadShaderGLSL(const char * fileName, VkShaderStageFlagBits stage)
  {
    std::ifstream input(fileName, std::ios::binary);
    auto &b = std::istreambuf_iterator<char>(input);
    auto &e = std::istreambuf_iterator<char>();

    std::vector<char> buf;
    buf.resize(12);
		((uint32_t *)buf.data())[0] = 0x07230203; 
		((uint32_t *)buf.data())[1] = 0;
		((uint32_t *)buf.data())[2] = stage;
    while (b != e) buf.push_back(*b++);
    buf.push_back(0);

		VkShaderModuleCreateInfo moduleCreateInfo = {};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = buf.size();
		moduleCreateInfo.pCode = (uint32_t*)buf.data();
		moduleCreateInfo.flags = 0;

		VkShaderModule shaderModule;
		VkResult err = vkCreateShaderModule(dev_, &moduleCreateInfo, NULL, &shaderModule);
    if (err) throw err;

	  VkPipelineShaderStageCreateInfo shaderStage = {};
	  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	  shaderStage.stage = stage;
	  shaderStage.module = shaderModule;
	  shaderStage.pName = "main";
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

class cmdBuffer : public resource<VkCommandBuffer> {
public:
  /// command buffer that does not own its pointer
  cmdBuffer(VkCommandBuffer value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// command buffer that does owns (and creates) its pointer
  cmdBuffer(VkDevice dev) : resource(dev) {
  }

  void begin(VkRenderPass renderPass, VkFramebuffer framebuffer, int width, int height) {
    beginCommandBuffer();
    beginRenderPass(renderPass, framebuffer, 0, 0, width, height);
    setViewport(0, 0, (float)width, (float)height);
    setScissor(0, 0, width, height);
  }

  void end(VkImage image) {
    endRenderPass();
    addPresentationBarrier(image);
    endCommandBuffer();
  }

  void beginCommandBuffer() {
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.pNext = NULL;
    vkBeginCommandBuffer(*this, &cmdBufInfo);
  }

  void beginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer, int x = 0, int y = 0, int width = 256, int height = 256) {
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.025f, 0.025f, 0.025f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = NULL;
    renderPassBeginInfo.framebuffer = framebuffer;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = x;
		renderPassBeginInfo.renderArea.offset.y = y;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(*this, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void setViewport(float x=0, float y=0, float width=256, float height=256, float minDepth=0, float maxDepth=1) {
		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.x = x;
		viewport.y = y;
		viewport.width = width;
		viewport.height = height;
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		vkCmdSetViewport(*this, 0, 1, &viewport);
  }

  void setScissor(int x=0, int y=0, int width=256, int height=256) {
		// Update dynamic scissor state
		VkRect2D scissor = {};
		scissor.offset.x = x;
		scissor.offset.y = y;
		scissor.extent.width = width;
		scissor.extent.height = height;
		vkCmdSetScissor(*this, 0, 1, &scissor);
  }

  void bindPipeline(pipeline &pipe) {
		// Bind descriptor sets describing shader binding points
		vkCmdBindDescriptorSets(*this, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout(), 0, 1, pipe.descriptorSets(), 0, NULL);

		// Bind the rendering pipeline (including the shaders)
		vkCmdBindPipeline(*this, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipe());
  }

  void bindVertexBuffer(buffer &buf, int bindId) {
		VkDeviceSize offsets[] = { 0 };
    VkBuffer bufs[] = { buf.buf() };
		vkCmdBindVertexBuffers(*this, bindId, 1, bufs, offsets);
  }

  void bindIndexBuffer(buffer &buf) {
		// Bind triangle indices
		vkCmdBindIndexBuffer(*this, buf.buf(), 0, VK_INDEX_TYPE_UINT32);
  }

  void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
		// Draw indexed triangle
		vkCmdDrawIndexed(*this, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
  }

  void endRenderPass() {
    vkCmdEndRenderPass(*this);
  }

  void addPresentationBarrier(VkImage image) {
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
		prePresentBarrier.image = image;

		VkImageMemoryBarrier *pMemoryBarrier = &prePresentBarrier;
		vkCmdPipelineBarrier(
			*this, 
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
			0,
			0, nullptr,
			0, nullptr,
			1, &prePresentBarrier);
  }

  void endCommandBuffer() {
    vkEndCommandBuffer(*this);
  }

  void pipelineBarrier(VkImage image) {
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
		postPresentBarrier.image = image;

		// Use dedicated command buffer from example base class for submitting the post present barrier
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		// Put post present barrier into command buffer
		vkCmdPipelineBarrier(
			*this,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &postPresentBarrier
    );
  }

private:
};

template<> VkSemaphore create<VkSemaphore>(VkDevice dev) {
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphore res = nullptr;
	VkResult err = vkCreateSemaphore(dev, &info, nullptr, &res);
	if (err) throw err;
  return res;
}

template<> void destroy<VkSemaphore>(VkDevice dev, VkSemaphore sema) {
  vkDestroySemaphore(dev, sema, nullptr);
}

class semaphore : public resource<VkSemaphore> {
public:
  /// semaphore that does not own its pointer
  semaphore(VkSemaphore value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// semaphore that does owns (and creates) its pointer
  semaphore(VkDevice dev) : resource(dev) {
  }
};

template<> VkQueue create<VkQueue>(VkDevice dev) { return nullptr; }
template<> void destroy<VkQueue>(VkDevice dev, VkQueue queue_) { }

class queue : public resource<VkQueue> {
public:
  /// queue that does not own its pointer
  queue(VkQueue value = nullptr, VkDevice dev = nullptr) : resource(value, dev) {
  }

  /// queue that does owns (and creates) its pointer
  queue(VkDevice dev) : resource(dev) {
  }

  void submit(VkSemaphore sema, VkCommandBuffer buffer) const {
		// The submit infor strcuture contains a list of
		// command buffers and semaphores to be submitted to a queue
		// If you want to submit multiple command buffers, pass an array
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = sema ? 1 : 0;
		submitInfo.pWaitSemaphores = &sema;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &buffer;

		// Submit to the graphics queue
		VkResult err = vkQueueSubmit(get(), 1, &submitInfo, VK_NULL_HANDLE);
    if (err) throw err;
  }

  void waitIdle() const {  
		VkResult err = vkQueueWaitIdle(get());
    if (err) throw err;
  }

};

} // vku

#endif
