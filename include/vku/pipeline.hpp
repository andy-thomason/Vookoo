////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_PIPELINE_INCLUDED
#define VKU_PIPELINE_INCLUDED

#include <vku/resource.hpp>
#include <vector>

namespace vku {


class descriptorPoolHelper {
public:
  descriptorPoolHelper(uint32_t maxSets) {
    info_.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info_.poolSizeCount = 0;
    info_.pPoolSizes = nullptr;
    info_.maxSets = maxSets;
  }

  descriptorPoolHelper &samplers(uint32_t descriptorCount) {
    return descriptors(VK_DESCRIPTOR_TYPE_SAMPLER, descriptorCount);
  }

  descriptorPoolHelper &combinedImageSamplers(uint32_t descriptorCount) {
    return descriptors(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount);
  }

  descriptorPoolHelper &uniformBuffers(uint32_t descriptorCount) {
    return descriptors(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount);
  }

  /*
    todo:
    VK_DESCRIPTOR_TYPE_SAMPLER = 0,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2,
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE = 3,
    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER = 4,
    VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER = 5,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9,
  */

  descriptorPoolHelper &descriptors(VkDescriptorType type, uint32_t descriptorCount) {
    VkDescriptorPoolSize dps = {};
    dps.type = type;
    dps.descriptorCount = descriptorCount;
    typeCounts_.push_back(dps);
    return *this;
  }

  VkDescriptorPool createDescriptorPool(const vku::device &dev) const {
    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = NULL;
    descriptorPoolInfo.poolSizeCount = (uint32_t)typeCounts_.size();
    descriptorPoolInfo.pPoolSizes = typeCounts_.data();
    descriptorPoolInfo.maxSets = (uint32_t)typeCounts_.size() * 2; // todo... what should this be?

    VkDescriptorPool result = VK_NULL_HANDLE;
    VkResult err = vkCreateDescriptorPool(dev, &descriptorPoolInfo, nullptr, &result);
    if (err) throw error(err, __FILE__, __LINE__);
    return result;
  }
private:
  std::vector<VkDescriptorPoolSize> typeCounts_;
  VkDescriptorPoolCreateInfo info_ = {};
};

class descriptorPool : public resource<VkDescriptorPool, descriptorPool> {
public:
  VKU_RESOURCE_BOILERPLATE(VkDescriptorPool, descriptorPool)

  descriptorPool(const vku::device &dev, vku::descriptorPoolHelper &layout) : resource(dev) {
    set(layout.createDescriptorPool(dev), true);
  }

  void destroy() {
    vkDestroyDescriptorPool(dev(), get(), nullptr);
  }
};

class pipelineCache : public resource<VkPipelineCache, pipelineCache> {
public:
  VKU_RESOURCE_BOILERPLATE(VkPipelineCache, pipelineCache)

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
    vkDestroyPipelineCache(dev(), get(), nullptr);
  }
};

class pipelineCreateHelper {
public:
  pipelineCreateHelper() {
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;

    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    // even if the viewport is dynamic, we must include this structure.
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Enable dynamic states
    // Describes the dynamic states to be used with this pipeline
    // Dynamic states can be set even after the pipeline has been created
    // So there is no need to create new pipelines just for changing
    // a viewport's dimensions or a scissor box
    // The dynamic state properties themselves are stored in the command buffer
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    // Depth and stencil state
    // Describes depth and stencil test and compare ops
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

  pipelineCreateHelper &uniformBuffers(uint32_t count, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    layoutBindings_.push_back(layoutBinding);
    return *this;
  }

  /*pipelineCreateHelper &combinedImageSampler(uint32_t count, VkShaderStageFlags stageFlags, const VkSampler *samplers) {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    layoutBinding.pImmutableSamplers = samplers;
    layoutBindings_.push_back(layoutBinding);
    return *this;
  }*/

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

  VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device) {
    VkDescriptorSetLayoutCreateInfo descriptorLayout_ = {};
    descriptorLayout_.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout_.bindingCount = (uint32_t)layoutBindings_.size();
    descriptorLayout_.pBindings = layoutBindings_.data();
    VkDescriptorSetLayout result = VK_NULL_HANDLE;
    VkResult err = vkCreateDescriptorSetLayout(device, &descriptorLayout_, NULL, &result);
    if (err) throw error(err, __FILE__, __LINE__);
    return result; 
  } 

  VkPipeline createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkPipelineCache pipelineCache) {
    // disable blend
    blendAttachmentState[0].colorWriteMask = 0xf;
    blendAttachmentState[0].blendEnable = VK_FALSE;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blendAttachmentState;
   
    // dynamicly changable viewport and scissor 
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

    // vertex format
    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
    vi.pVertexBindingDescriptions = bindingDescriptions.data();
    vi.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
    vi.pVertexAttributeDescriptions = attributeDescriptions.data();

    // all states together.
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.stageCount = (uint32_t)shaderStages_.size();
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

    VkPipeline result = VK_NULL_HANDLE;
    VkResult err = vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &result);
    if (err) throw error(err, __FILE__, __LINE__);
    return result;
  }

  VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout) {
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

    VkPipelineLayout result = VK_NULL_HANDLE; 
    VkResult err = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &result);
    if (err) throw error(err, __FILE__, __LINE__);
    return result;
  }

  pipelineCreateHelper &operator=(pipelineCreateHelper && rhs) = delete;
  pipelineCreateHelper &operator=(pipelineCreateHelper & rhs) = delete;
private:
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
};

class pipeline : public resource<VkPipeline, pipeline> {
public:
  VKU_RESOURCE_BOILERPLATE(VkPipeline, pipeline)

  pipeline(
    const vku::device &device,
    VkRenderPass renderPass,
    const vku::pipelineCache &pipelineCache,
    pipelineCreateHelper &pipelineCreateHelper
  ) : resource(device) {
    descriptorSetLayout = pipelineCreateHelper.createDescriptorSetLayout(device); 
    pipelineLayout = pipelineCreateHelper.createPipelineLayout(device, descriptorSetLayout);
    set(pipelineCreateHelper.createGraphicsPipeline(device, renderPass, pipelineLayout, pipelineCache), true);
  }

  void destroy() {
    vkDestroyPipeline(dev(), get(), nullptr);
    vkDestroyPipelineLayout(dev(), pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(dev(), descriptorSetLayout, nullptr);
  }

  void allocateDescriptorSets(descriptorPool &descPool, uint32_t count=1) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    descriptorSet = VK_NULL_HANDLE;
    VkResult err = vkAllocateDescriptorSets(dev(), &allocInfo, &descriptorSet);
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

    vkUpdateDescriptorSets(dev(), 1, &writeDescriptorSet, 0, NULL);
  }

  VkPipelineLayout layout() const { return pipelineLayout; }
  VkDescriptorSet *descriptorSets() { return &descriptorSet; }
  VkDescriptorSetLayout *descriptorLayouts() { return &descriptorSetLayout; }

private:
  void move(pipeline &&rhs) {
    (resource&)*this = (resource&&)rhs;
    pipelineLayout = rhs.pipelineLayout;
    descriptorSet = rhs.descriptorSet;
    descriptorSetLayout = rhs.descriptorSetLayout;
  }

  void copy(const pipeline &rhs) {
    (resource&)*this = (const resource&)rhs;
    pipelineLayout = rhs.pipelineLayout;
    descriptorSet = rhs.descriptorSet;
    descriptorSetLayout = rhs.descriptorSetLayout;
  }

  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
};


} // vku

#endif
