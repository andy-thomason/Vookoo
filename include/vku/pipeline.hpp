////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_PIPELINE_INCLUDED
#define VKU_PIPELINE_INCLUDED

#include <vku/resource.hpp>
#include <vulkan/vulkan.h>
#include <vector>

namespace vku {

class pipelineCache : public resource<VkPipelineCache, pipelineCache> {
public:
  pipelineCache() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// descriptor pool that does not own its pointer
  pipelineCache(VkPipelineCache value, VkDevice dev) : resource(value, dev) {
  }

  /// descriptor pool that does own (and creates) its pointer
  pipelineCache(VkDevice dev) : resource(dev) {
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkPipelineCache cache;
    VkResult err = vkCreatePipelineCache(dev, &pipelineCacheCreateInfo, nullptr, &cache);
    if (err) throw error(err, __FILE__, __LINE__);
    set(cache, true);
  }

  void destroy() {
    if (get()) vkDestroyPipelineCache(dev(), get(), nullptr);
  }

  pipelineCache &operator=(pipelineCache &&rhs) {
    (resource&)(*this) = (resource&&)rhs;
    return *this;
  }
};

class pipelineCreateHelper {
public:
  pipelineCreateHelper() {
    // Vertex input state
    // Describes the topoloy used with this pipeline
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // This pipeline renders vertex data as triangle lists
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Rasterization state
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // Solid polygon mode
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    // No culling
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;

    // Color blend state
    // Describes blend modes and color masks
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    // One blend attachment state
    // Blending is not used in this example
    blendAttachmentState[0].colorWriteMask = 0xf;
    blendAttachmentState[0].blendEnable = VK_FALSE;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blendAttachmentState;

    // Viewport state
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
    // The dynamic state properties themselves are stored in the command buffer
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

    // Depth and stencil state
    // Describes depth and stenctil test and compare ops
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
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.pSampleMask = NULL;
    // No multi sampling used in this example
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  }

  pipelineCreateHelper &attrib(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription desc = {};
    desc.location = location;
    desc.binding = binding;
    desc.format = format;
    desc.offset = offset;
    attributeDescriptions.push_back(desc);
    return *this;
  }

  pipelineCreateHelper &binding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX) {
    VkVertexInputBindingDescription desc = {};
    desc.binding = binding;
    desc.stride = stride;
    desc.inputRate = inputRate;
    bindingDescriptions.push_back(desc);
    return *this;
  }

  pipelineCreateHelper &descriptors(VkDescriptorType type, uint32_t count, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = type;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    layoutBindings_.push_back(layoutBinding);
    return *this;
  }

  pipelineCreateHelper &uniformBuffers(uint32_t count, VkShaderStageFlags stageFlags) {
    return descriptors(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count, stageFlags);
  }

  pipelineCreateHelper &samplers(uint32_t count, VkShaderStageFlags stageFlags) {
    return descriptors(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count, stageFlags);
  }

  pipelineCreateHelper &shader(const vku::shaderModule &module, VkShaderStageFlagBits stage, const char *entrypoint="main") {
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;
    shaderStage.module = module.get();
    shaderStage.pName = entrypoint;
    shaderStages_.push_back(shaderStage);
    return *this;
  }

  // todo: make more of these
  pipelineCreateHelper &topology(VkPrimitiveTopology value) {
    inputAssemblyState.topology = value;
    return *this;
  }

  // querying functions
  VkDescriptorSetLayoutCreateInfo *getDescriptorSetLayout() {
    descriptorLayout_.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout_.bindingCount = (uint32_t)layoutBindings_.size();
    descriptorLayout_.pBindings = layoutBindings_.data();
    return &descriptorLayout_;
  }

  VkGraphicsPipelineCreateInfo *get(VkRenderPass renderPass, VkPipelineLayout pipelineLayout) {
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.pNext = VK_NULL_HANDLE;
    vi.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
    vi.pVertexBindingDescriptions = bindingDescriptions.data();
    vi.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
    vi.pVertexAttributeDescriptions = attributeDescriptions.data();

    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    // The layout used for this pipeline
    pipelineCreateInfo.layout = pipelineLayout;
    // Renderpass this pipeline is attached to
    pipelineCreateInfo.renderPass = renderPass;

    // Assign states
    // Two shader stages
    pipelineCreateInfo.stageCount = (uint32_t)shaderStages_.size();
    // Assign pipeline state create information
    pipelineCreateInfo.pVertexInputState = &vi;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pStages = shaderStages_.data();
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.pDynamicState = &dynamicState;

    return &pipelineCreateInfo;
  }

  pipelineCreateHelper &operator=(pipelineCreateHelper && rhs) = default;
private:
  VkPipelineVertexInputStateCreateInfo vi = {};
  VkDescriptorSetLayoutCreateInfo descriptorLayout_ = {};
  std::vector<VkVertexInputBindingDescription> bindingDescriptions;
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings_;
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages_;
  std::vector<VkDynamicState> dynamicStateEnables;

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
  VkPipelineRasterizationStateCreateInfo rasterizationState = {};
  VkPipelineColorBlendStateCreateInfo colorBlendState = {};
  VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
  VkPipelineViewportStateCreateInfo viewportState = {};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
  VkPipelineMultisampleStateCreateInfo multisampleState = {};
  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
};

class pipeline {
public:
  pipeline() {
  }

  pipeline(
    const vku::device &device,
    VkRenderPass renderPass,
    const vku::pipelineCache &pipelineCache,
    pipelineCreateHelper &pipelineCreateHelper
  ) : dev_(device) {
    VkResult err = vkCreateDescriptorSetLayout(device, pipelineCreateHelper.getDescriptorSetLayout(), NULL, &descriptorSetLayout);
    if (err) throw error(err, __FILE__, __LINE__);

    // Create the pipeline layout that is used to generate the rendering pipelines that
    // are based on this descriptor set layout
    // In a more complex scenario you would have different pipeline layouts for different
    // descriptor set layouts that could be reused
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.setLayoutCount = 1;
    pPipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

    err = vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout);
    if (err) throw error(err, __FILE__, __LINE__);

    auto info = pipelineCreateHelper.get(renderPass, pipelineLayout);

    // Create rendering pipeline
    err = vkCreateGraphicsPipelines(device, pipelineCache, 1, info, nullptr, &pipe_);
    if (err) throw error(err, __FILE__, __LINE__);

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

  void allocateDescriptorSets(descriptorPool &descPool, uint32_t count=1) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    descriptorSet = VK_NULL_HANDLE;
    VkResult err = vkAllocateDescriptorSets(dev_, &allocInfo, &descriptorSet);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  void updateDescriptorSets(buffer &uniformVS) {
    VkWriteDescriptorSet writeDescriptorSet = {};

    // Binding 0 : Uniform buffer
    VkDescriptorBufferInfo desc = uniformVS.desc();
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &desc;
    // Binds this uniform buffer to binding point 0
    writeDescriptorSet.dstBinding = 0;

    vkUpdateDescriptorSets(dev_, 1, &writeDescriptorSet, 0, NULL);
  }

  VkPipeline pipe() { return pipe_; }
  VkPipelineLayout layout() const { return pipelineLayout; }
  VkDescriptorSet *descriptorSets() { return &descriptorSet; }
  VkDescriptorSetLayout *descriptorLayouts() { return &descriptorSetLayout; }

private:

  VkPipeline pipe_ = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDevice dev_ = VK_NULL_HANDLE;
  std::vector<VkShaderModule> shaderModules;
  bool ownsData = false;
};


} // vku

#endif
