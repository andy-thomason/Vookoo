////////////////////////////////////////////////////////////////////////////////
//
// Vookoo high level C++ Vulkan interface.
//
// (C) Andy Thomason 2017 MIT License
//
// This is a utility set alongside the vkcpp C++ interface to Vulkan which makes
// constructing Vulkan pipelines and resources very easy for beginners.
//
// It is expected that once familar with the Vulkan C++ interface you may wish
// to "go it alone" but we hope that this will make the learning experience a joyful one.
//
// You can use it with the demo framework, stand alone and mixed with C or C++ Vulkan objects.
// It should integrate with game engines nicely.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef VKU_HPP
#define VKU_HPP

#include <array>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <thread>
#include <chrono>
#include <functional>
#include <cstddef>

#include <vulkan/spirv.hpp11>
#include <vulkan/vulkan.hpp>

namespace vku {

/// Printf-style formatting function.
template <class ... Args>
std::string format(const char *fmt, Args... args) {
  int n = snprintf(nullptr, 0, fmt, args...);
  std::string result(n, '\0');
  snprintf(&*result.begin(), n+1, fmt, args...);
  return std::move(result);
}

/// Utility function for finding memory types for uniforms and images.
inline int findMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties &memprops, uint32_t memoryTypeBits, vk::MemoryPropertyFlags search) {
  for (int i = 0; i != memprops.memoryTypeCount; ++i, memoryTypeBits >>= 1) {
    if (memoryTypeBits & 1) {
      if ((memprops.memoryTypes[i].propertyFlags & search) == search) {
        return i;
      }
    }
  }
  return -1;
}

// Execute commands immediately and wait for the device to finish.
inline void executeImmediately(vk::Device device, vk::CommandPool commandPool, vk::Queue queue, const std::function<void (vk::CommandBuffer cb)> &func) {
  vk::CommandBufferAllocateInfo cbai{ commandPool, vk::CommandBufferLevel::ePrimary, 1 };

  auto cbs = device.allocateCommandBuffers(cbai);
  cbs[0].begin(vk::CommandBufferBeginInfo{});
  func(cbs[0]);
  cbs[0].end();

  vk::SubmitInfo submit;
  submit.commandBufferCount = (uint32_t)cbs.size();
  submit.pCommandBuffers = cbs.data();
  queue.submit(submit, vk::Fence{});
  device.waitIdle();

  device.freeCommandBuffers(commandPool, cbs);
}

// See https://vulkan-tutorial.com for details of many operations here.

/// Factory for renderpasses.
/// example:
/// RenderpassMaker rpm;
/// rpm.beginSubpass(vk::PipelineBindPoint::eGraphics);
/// rpm.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal);
///
/// rpm.attachmentDescription(attachmentDesc);
/// rpm.subpassDependency(dependency);
/// s.renderPass_ = rpm.createUnique(device);
class RenderpassMaker {
public:
  RenderpassMaker() {
  }

  /// Add an attachment description to the renderpass.
  /// This describes the format and operations on a colour or depth buffer.
  void attachmentDescription(vk::AttachmentDescription &desc) {
    s.attachmentDescriptions.push_back(desc);
  }

  /// Start a subpass description.
  /// After this you can can call subpassColorAttachment many times
  /// and subpassDepthStencilAttachment once.
  void beginSubpass(vk::PipelineBindPoint bp) {
    vk::SubpassDescription desc{};
    desc.pipelineBindPoint = bp;
    s.subpassDescriptions.push_back(desc);
  }

  void subpassDependency(const vk::SubpassDependency &desc) {
    s.subpassDependencies.push_back(desc);
  }


  void subpassColorAttachment(vk::ImageLayout layout, uint32_t attachment) {
    vk::SubpassDescription &subpass = s.subpassDescriptions.back();
    auto *p = getAttachmentReference();
    p->layout = layout;
    p->attachment = attachment;
    if (subpass.colorAttachmentCount == 0) {
      subpass.pColorAttachments = p;
    }
    subpass.colorAttachmentCount++;
  }

  void subpassDepthStencilAttachment(vk::ImageLayout layout, uint32_t attachment) {
    vk::SubpassDescription &subpass = s.subpassDescriptions.back();
    auto *p = getAttachmentReference();
    p->layout = layout;
    p->attachment = attachment;
    subpass.pDepthStencilAttachment = p;
  }

  vk::UniqueRenderPass createUnique(const vk::Device &device) const {
    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = (uint32_t)s.attachmentDescriptions.size();
    renderPassInfo.pAttachments = s.attachmentDescriptions.data();
    renderPassInfo.subpassCount = (uint32_t)s.subpassDescriptions.size();
    renderPassInfo.pSubpasses = s.subpassDescriptions.data();
    renderPassInfo.dependencyCount = (uint32_t)s.subpassDependencies.size();
    renderPassInfo.pDependencies = s.subpassDependencies.data();
    return device.createRenderPassUnique(renderPassInfo);
  }
private:
  constexpr static int max_refs = 64;

  vk::AttachmentReference *getAttachmentReference() {
    return (s.num_refs < max_refs) ? &s.attachmentReferences[s.num_refs++] : nullptr;
  }
  
  struct State {
    std::vector<vk::AttachmentDescription> attachmentDescriptions;
    std::vector<vk::SubpassDescription> subpassDescriptions;
    std::vector<vk::SubpassDependency> subpassDependencies;
    std::array<vk::AttachmentReference, max_refs> attachmentReferences;
    int num_refs = 0;
    bool ok_ = false;
  };

  State s;
};

// Class for building shader modules and extracting metadata from shaders.
class ShaderModule {
public:
  ShaderModule() {
  }

  // Construct a shader module from a file
  ShaderModule(const vk::Device &device, const std::string &filename) {
    auto file = std::ifstream(filename, std::ios::binary);
    if (file.bad()) {
      return;
    }

    file.seekg(0, std::ios::end);
    int length = (int)file.tellg();

    s.opcodes_.resize((size_t)(length / 4));
    file.seekg(0, std::ios::beg);
    file.read((char *)s.opcodes_.data(), s.opcodes_.size() * 4);

    vk::ShaderModuleCreateInfo ci;
    ci.codeSize = s.opcodes_.size() * 4;
    ci.pCode = s.opcodes_.data();
    s.module_ = device.createShaderModuleUnique(ci);

    s.ok_ = true;
  }

  // Construct a shader module from a memory
  template<class InIter>
  ShaderModule(const vk::Device &device, InIter begin, InIter end) {
    s.opcodes_.assign(begin, end);
    vk::ShaderModuleCreateInfo ci;
    ci.codeSize = s.opcodes_.size() * 4;
    ci.pCode = s.opcodes_.data();
    s.module_ = device.createShaderModuleUnique(ci);

    s.ok_ = true;
  }

  // A variable in a shader.
  struct Variable {
    // The name of the variable from the GLSL/HLSL
    std::string debugName;

    // The internal name (integer) of the variable
    int name;

    // The location in the binding.
    int location;

    // The binding in the descriptor set or I/O channel.
    int binding;

    // The descriptor set (for uniforms)
    int set;
    int instruction;

    // Storage class of the variable, eg. spv::StorageClass::Uniform
    spv::StorageClass storageClass;
  };

  // Get a list of variables from the shader.
  // 
  // This exposes the Uniforms, inputs, outputs, push constants.
  // See spv::StorageClass for more details.
  std::vector<Variable> getVariables() const {
    auto bound = s.opcodes_[3];

    std::unordered_map<int, int> bindings;
    std::unordered_map<int, int> locations;
    std::unordered_map<int, int> sets;
    std::unordered_map<int, std::string> debugNames;

    for (int i = 5; i != s.opcodes_.size(); i += s.opcodes_[i] >> 16) {
      spv::Op op = spv::Op(s.opcodes_[i] & 0xffff);
      if (op == spv::Op::OpDecorate) {
        int name = s.opcodes_[i + 1];
        auto decoration = spv::Decoration(s.opcodes_[i + 2]);
        if (decoration == spv::Decoration::Binding) {
          bindings[name] = s.opcodes_[i + 3];
        } else if (decoration == spv::Decoration::Location) {
          locations[name] = s.opcodes_[i + 3];
        } else if (decoration == spv::Decoration::DescriptorSet) {
          sets[name] = s.opcodes_[i + 3];
        }
      } else if (op == spv::Op::OpName) {
        int name = s.opcodes_[i + 1];
        debugNames[name] = (const char *)&s.opcodes_[i + 2];
      }
    }

    std::vector<Variable> result;
    for (int i = 5; i != s.opcodes_.size(); i += s.opcodes_[i] >> 16) {
      spv::Op op = spv::Op(s.opcodes_[i] & 0xffff);
      if (op == spv::Op::OpVariable) {
        int name = s.opcodes_[i + 1];
        auto sc = spv::StorageClass(s.opcodes_[i + 3]);
        Variable b;
        b.debugName = debugNames[name];
        b.name = name;
        b.location = locations[name];
        b.set = sets[name];
        b.instruction = i;
        b.storageClass = sc;
        result.push_back(b);
      }
    }
    return std::move(result);
  }

  bool ok() const { return s.ok_; }
  VkShaderModule module() { return *s.module_; }

  // Write a C++ consumable dump of the shader.
  // Todo: make this more idiomatic.
  std::ostream &write(std::ostream &os) {
    os << "static const uint32_t shader[] = {\n";
    char tmp[256];
    auto p = s.opcodes_.begin();
    snprintf(
      tmp, sizeof(tmp), "  0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,\n", p[0], p[1], p[2], p[3], p[4]);
    os << tmp;
    for (int i = 5; i != s.opcodes_.size(); i += s.opcodes_[i] >> 16) {
      char *p = tmp + 2, *e = tmp + sizeof(tmp) - 2;
      for (int j = i; j != i + (s.opcodes_[i] >> 16); ++j) {
        p += snprintf(p, e-p, "0x%08x,", s.opcodes_[j]);
        if (p > e-16) { *p++ = '\n'; *p = 0; os << tmp; p = tmp + 2; }
      }
      *p++ = '\n';
      *p = 0;
      os << tmp;
    }
    os << "};\n\n";
    return os;
  }

private:
  struct State {
    std::vector<uint32_t> opcodes_;
    vk::UniqueShaderModule module_;
    bool ok_;
  };

  State s;
};

// A class for building pipeline layouts.
// Pipeline layouts describe the descriptor sets and push constants used by the shaders.
class PipelineLayoutMaker {
public:
  PipelineLayoutMaker() {}

  // Create a self-deleting pipeline layout object.
  vk::UniquePipelineLayout createUnique(const vk::Device &device) const {
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        {}, (uint32_t)setLayouts_.size(),
        setLayouts_.data(), (uint32_t)pushConstantRanges_.size(),
        pushConstantRanges_.data()};
    return std::move(device.createPipelineLayoutUnique(pipelineLayoutInfo));
  }

  // Add a descriptor set layout to the pipeline.
  void descriptorSetLayout(vk::DescriptorSetLayout layout) {
    setLayouts_.push_back(layout);
  }

  // Add a push constant range to the pipeline.
  // These describe the size and location of variables in the push constant area.
  void pushConstantRange(vk::PushConstantRange range) {
    pushConstantRanges_.push_back(range);
  }

private:
  std::vector<vk::DescriptorSetLayout> setLayouts_;
  std::vector<vk::PushConstantRange> pushConstantRanges_;
};

// A class for building pipelines.
// All the state of the pipeline is exposed through individual calls.
// The pipeline encapsulates all the OpenGL state in a single object.
// This includes vertex buffer layouts, blend operations, shaders, line width etc.
// This class exposes all the values as individuals so a pipeline can be customised.
// The default is to generate a working pipeline.
class PipelineMaker {
public:
  PipelineMaker(uint32_t width, uint32_t height) {
    s.inputAssemblyState_.topology = vk::PrimitiveTopology::eTriangleList;
    s.viewport_ = vk::Viewport{0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
    s.scissor_ = vk::Rect2D{{0, 0}, {width, height}};
    s.rasterizationState_.lineWidth = 1.0f;

    // Set up depth test, but do not enable it.
    s.depthStencilState_.depthTestEnable = VK_FALSE;
    s.depthStencilState_.depthWriteEnable = VK_TRUE;
    s.depthStencilState_.depthCompareOp = vk::CompareOp::eLessOrEqual;
    s.depthStencilState_.depthBoundsTestEnable = VK_FALSE;
    s.depthStencilState_.back.failOp = vk::StencilOp::eKeep;
    s.depthStencilState_.back.passOp = vk::StencilOp::eKeep;
    s.depthStencilState_.back.compareOp = vk::CompareOp::eAlways;
    s.depthStencilState_.stencilTestEnable = VK_FALSE;
    s.depthStencilState_.front = s.depthStencilState_.back;
  }

  vk::UniquePipeline createUnique(const vk::Device &device,
                            const vk::PipelineCache &pipelineCache,
                            const vk::PipelineLayout &pipelineLayout,
                            const vk::RenderPass &renderPass) {

    // Add default colour blend attachment if necessary.
    if (s.colorBlendAttachments_.empty()) {
      vk::PipelineColorBlendAttachmentState blend{};
      blend.blendEnable = 0;
      blend.srcColorBlendFactor = vk::BlendFactor::eOne;
      blend.dstColorBlendFactor = vk::BlendFactor::eZero;
      blend.colorBlendOp = vk::BlendOp::eAdd;
      blend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
      blend.dstAlphaBlendFactor = vk::BlendFactor::eZero;
      blend.alphaBlendOp = vk::BlendOp::eAdd;
      typedef vk::ColorComponentFlagBits ccbf;
      blend.colorWriteMask = ccbf::eR|ccbf::eG|ccbf::eB|ccbf::eA;
      s.colorBlendAttachments_.push_back(blend);
    }
    s.colorBlendState_.attachmentCount = (uint32_t)s.colorBlendAttachments_.size();
    s.colorBlendState_.pAttachments = s.colorBlendAttachments_.data();

    vk::PipelineViewportStateCreateInfo viewportState{
        {}, 1, &s.viewport_, 1, &s.scissor_};

    vk::PipelineVertexInputStateCreateInfo vertexInputState;
    vertexInputState.vertexAttributeDescriptionCount = (uint32_t)s.vertexAttributeDescriptions_.size();
    vertexInputState.pVertexAttributeDescriptions = s.vertexAttributeDescriptions_.data();
    vertexInputState.vertexBindingDescriptionCount = (uint32_t)s.vertexBindingDescriptions_.size();
    vertexInputState.pVertexBindingDescriptions = s.vertexBindingDescriptions_.data();

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.stageCount = (uint32_t)s.modules_.size();
    pipelineInfo.pStages = s.modules_.data();
    pipelineInfo.pInputAssemblyState = &s.inputAssemblyState_;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &s.rasterizationState_;
    pipelineInfo.pMultisampleState = &s.multisampleState_;
    pipelineInfo.pColorBlendState = &s.colorBlendState_;
    pipelineInfo.pDepthStencilState = &s.depthStencilState_;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;

    return device.createGraphicsPipelineUnique(pipelineCache, pipelineInfo);
  }

  // Add a shader module to the pipeline.
  void shader(vk::ShaderStageFlagBits stage, vku::ShaderModule &shader,
                 const char *entryPoint = "main") {
    vk::PipelineShaderStageCreateInfo info{};
    info.module = shader.module();
    info.pName = entryPoint;
    info.stage = stage;
    s.modules_.emplace_back(info);
  }

  // Add a blend state to the pipeline for one colour attachment.
  // If you don't do this, a default is used.
  void colorBlend(const vk::PipelineColorBlendAttachmentState &state) {
    s.colorBlendAttachments_.push_back(state);
  }

  // Add a vertex attribute to the pipeline.
  void vertexAttribute(uint32_t location_, uint32_t binding_, vk::Format format_, uint32_t offset_) {
    s.vertexAttributeDescriptions_.push_back({location_, binding_, format_, offset_});
  }

  // Add a vertex attribute to the pipeline.
  void vertexAttribute(const vk::VertexInputAttributeDescription &desc) {
    s.vertexAttributeDescriptions_.push_back(desc);
  }

  // Add a vertex binding to the pipeline.
  // Usually only one of these is needed to specify the stride.
  // Vertices can also be delivered one per instance.
  void vertexBinding(uint32_t binding_, uint32_t stride_, vk::VertexInputRate inputRate_ = vk::VertexInputRate::eVertex) {
    s.vertexBindingDescriptions_.push_back({binding_, stride_, inputRate_});
  }

  // Add a vertex binding to the pipeline.
  // Usually only one of these is needed to specify the stride.
  // Vertices can also be delivered one per instance.
  void vertexBinding(const vk::VertexInputBindingDescription &desc) {
    s.vertexBindingDescriptions_.push_back(desc);
  }

  // Specify the topology of the pipeline.
  // Usually this is a triangle list, but points and lines are possible too.
  PipelineMaker &topology( vk::PrimitiveTopology topology ) { s.inputAssemblyState_.topology = topology; return *this; }

  // Enable or disable primitive restart.
  // If using triangle strips, for example, this allows a special index value (0xffff or 0xffffffff) to start a new strip.
  PipelineMaker &primitiveRestartEnable( vk::Bool32 primitiveRestartEnable ) { s.inputAssemblyState_.primitiveRestartEnable = primitiveRestartEnable; return *this; }

  // Set a whole new input assembly state.
  // Note you can set individual values with their own calls.
  PipelineMaker &inputAssemblyState(const vk::PipelineInputAssemblyStateCreateInfo &value) { s.inputAssemblyState_ = value; return *this; }

  // Set the viewport value.
  // Usually there is only one viewport, but you can have multiple viewports active for rendering cubemaps or VR stereo pairs.
  PipelineMaker &viewport(const vk::Viewport &value) { s.viewport_ = value; return *this; }

  // Set the scissor value.
  // This defines the area that the fragment shaders can write to. For example, if you are rendering a portal or a mirror.
  PipelineMaker &scissor(const vk::Rect2D &value) { s.scissor_ = value; return *this; }

  // Set a whole rasterization state.
  // Note you can set individual values with their own calls.
  PipelineMaker &rasterizationState(const vk::PipelineRasterizationStateCreateInfo &value) { s.rasterizationState_ = value; return *this; }
  PipelineMaker &depthClampEnable(vk::Bool32 value) { s.rasterizationState_.depthClampEnable = value; return *this; }
  PipelineMaker &rasterizerDiscardEnable(vk::Bool32 value) { s.rasterizationState_.rasterizerDiscardEnable = value; return *this; }
  PipelineMaker &polygonMode(vk::PolygonMode value) { s.rasterizationState_.polygonMode = value; return *this; }
  PipelineMaker &cullMode(vk::CullModeFlags value) { s.rasterizationState_.cullMode = value; return *this; }
  PipelineMaker &frontFace(vk::FrontFace value) { s.rasterizationState_.frontFace = value; return *this; }
  PipelineMaker &depthBiasEnable(vk::Bool32 value) { s.rasterizationState_.depthBiasEnable = value; return *this; }
  PipelineMaker &depthBiasConstantFactor(float value) { s.rasterizationState_.depthBiasConstantFactor = value; return *this; }
  PipelineMaker &depthBiasClamp(float value) { s.rasterizationState_.depthBiasClamp = value; return *this; }
  PipelineMaker &depthBiasSlopeFactor(float value) { s.rasterizationState_.depthBiasSlopeFactor = value; return *this; }
  PipelineMaker &lineWidth(float value) { s.rasterizationState_.lineWidth = value; return *this; }


  // Set a whole multi sample state.
  // Note you can set individual values with their own calls.
  PipelineMaker &multisampleState(const vk::PipelineMultisampleStateCreateInfo &value) { s.multisampleState_ = value; return *this; }
  PipelineMaker &rasterizationSamples(vk::SampleCountFlagBits value) { s.multisampleState_.rasterizationSamples = value; return *this; }
  PipelineMaker &sampleShadingEnable(vk::Bool32 value) { s.multisampleState_.sampleShadingEnable = value; return *this; }
  PipelineMaker &minSampleShading(float value) { s.multisampleState_.minSampleShading = value; return *this; }
  PipelineMaker &pSampleMask(const vk::SampleMask* value) { s.multisampleState_.pSampleMask = value; return *this; }
  PipelineMaker &alphaToCoverageEnable(vk::Bool32 value) { s.multisampleState_.alphaToCoverageEnable = value; return *this; }
  PipelineMaker &alphaToOneEnable(vk::Bool32 value) { s.multisampleState_.alphaToOneEnable = value; return *this; }

  // Set a whole depth stencil state.
  // Note you can set individual values with their own calls.
  PipelineMaker &depthStencilState(const vk::PipelineDepthStencilStateCreateInfo &value) { s.depthStencilState_ = value; return *this; }
  PipelineMaker &depthTestEnable(vk::Bool32 value) { s.depthStencilState_.depthTestEnable = value; return *this; }
  PipelineMaker &depthWriteEnable(vk::Bool32 value) { s.depthStencilState_.depthWriteEnable = value; return *this; }
  PipelineMaker &depthCompareOp(vk::CompareOp value) { s.depthStencilState_.depthCompareOp = value; return *this; }
  PipelineMaker &depthBoundsTestEnable(vk::Bool32 value) { s.depthStencilState_.depthBoundsTestEnable = value; return *this; }
  PipelineMaker &stencilTestEnable(vk::Bool32 value) { s.depthStencilState_.stencilTestEnable = value; return *this; }
  PipelineMaker &front(vk::StencilOpState value) { s.depthStencilState_.front = value; return *this; }
  PipelineMaker &back(vk::StencilOpState value) { s.depthStencilState_.back = value; return *this; }
  PipelineMaker &minDepthBounds(float value) { s.depthStencilState_.minDepthBounds = value; return *this; }
  PipelineMaker &maxDepthBounds(float value) { s.depthStencilState_.maxDepthBounds = value; return *this; }

  // Set a whole colour blend state.
  // Note you can set individual values with their own calls.
  PipelineMaker &colorBlendState(const vk::PipelineColorBlendStateCreateInfo &value) { s.colorBlendState_ = value; return *this; }
  PipelineMaker &logicOpEnable(vk::Bool32 value) { s.colorBlendState_.logicOpEnable = value; return *this; }
  PipelineMaker &logicOp(vk::LogicOp value) { s.colorBlendState_.logicOp = value; return *this; }
  PipelineMaker &blendConstants(float r, float g, float b, float a) { float *bc = s.colorBlendState_.blendConstants; bc[0] = r; bc[1] = g; bc[2] = b; bc[3] = a; return *this; }
private:
  struct State {
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState_;
    vk::Viewport viewport_;
    vk::Rect2D scissor_;
    vk::PipelineRasterizationStateCreateInfo rasterizationState_;
    vk::PipelineMultisampleStateCreateInfo multisampleState_;
    vk::PipelineDepthStencilStateCreateInfo depthStencilState_;
    vk::PipelineColorBlendStateCreateInfo colorBlendState_;
    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments_;
    std::vector<vk::PipelineShaderStageCreateInfo> modules_;
    std::vector<vk::VertexInputAttributeDescription> vertexAttributeDescriptions_;
    std::vector<vk::VertexInputBindingDescription> vertexBindingDescriptions_;
  };

  State s;
};

// A generic buffer that may be used as a vertex buffer, uniform buffer or other kinds of memory resident data.
// Buffers require memory objects which represent GPU and CPU resources.
class GenericBuffer {
public:
  GenericBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, vk::BufferUsageFlags usage, vk::DeviceSize size, vk::MemoryPropertyFlags memflags = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible) {
    // Create the buffer object without memory.
    vk::BufferCreateInfo ci{};
    ci.size = s.size = size;
    ci.usage = usage;
    ci.sharingMode = vk::SharingMode::eExclusive;
    s.buffer = device.createBufferUnique(ci);

    // Find out how much memory and which heap to allocate from.
    auto memreq = device.getBufferMemoryRequirements(*s.buffer);
    //auto search = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;

    // Create a memory object to bind to the buffer.
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = memreq.size;
    mai.memoryTypeIndex = vku::findMemoryTypeIndex(memprops, memreq.memoryTypeBits, memflags);
    s.mem = device.allocateMemoryUnique(mai);

    device.bindBufferMemory(*s.buffer, *s.mem, 0);
  }

  // Copy memory from the host to the buffer object.
  void update(const vk::Device &device, const void *value, vk::DeviceSize size) {
    void *ptr = device.mapMemory(*s.mem, 0, s.size, vk::MemoryMapFlags{});
    memcpy(ptr, value, (size_t)size);
    device.unmapMemory(*s.mem);
  }

  vk::Buffer buffer() const { return *s.buffer; }
private:
  struct State {
    vk::UniqueBuffer buffer;
    vk::UniqueDeviceMemory mem;
    vk::DeviceSize size;
  };

  State s;
};

/// This class is a specialisation of GenericBuffer for vertex buffers.
class VertexBuffer : public GenericBuffer {
public:
  template<class Type, class Allocator>
  VertexBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, const std::vector<Type, Allocator> &value) : GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eVertexBuffer, value.size() * sizeof(Type)) {
    update(device, value);
  }

  template<class Type, class Allocator>
  void update(const vk::Device &device, const std::vector<Type, Allocator> &value) {
    GenericBuffer::update(device, (void*)value.data(), vk::DeviceSize(value.size() * sizeof(Type)));
  }
};

/// This class is a specialisation of GenericBuffer for vertex buffers.
class IndexBuffer : public GenericBuffer {
public:
  template<class Type, class Allocator>
  IndexBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, const std::vector<Type, Allocator> &value) : GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eIndexBuffer, value.size() * sizeof(Type)) {
    update(device, value);
  }

  template<class Type, class Allocator>
  void update(const vk::Device &device, const std::vector<Type, Allocator> &value) {
    GenericBuffer::update(device, (void*)value.data(), vk::DeviceSize(value.size() * sizeof(Type)));
  }
};

/// This class is a specialisation of GenericBuffer for uniform buffers.
class UniformBuffer : public GenericBuffer {
public:
  template<class Type, class Allocator>
  UniformBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, const std::vector<Type, Allocator> &value) : GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eUniformBuffer|vk::BufferUsageFlagBits::eTransferDst, value.size() * sizeof(Type)) {
    update(device, value);
  }

  template<class Type>
  UniformBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, const Type &value) : GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eUniformBuffer|vk::BufferUsageFlagBits::eTransferDst, sizeof(Type)) {
    update(device, value);
  }

  // Device local buffer for high performance update.
  UniformBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, size_t size) : GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eUniformBuffer|vk::BufferUsageFlagBits::eTransferDst, (vk::DeviceSize)size, vk::MemoryPropertyFlagBits::eDeviceLocal) {
  }

  template<class Type, class Allocator>
  void update(const vk::Device &device, const std::vector<Type, Allocator> &value) {
    GenericBuffer::update(device, (void*)value.data(), vk::DeviceSize(value.size() * sizeof(Type)));
  }

  template<class Type>
  void update(const vk::Device &device, const Type &value) {
    GenericBuffer::update(device, (void*)&value, vk::DeviceSize(sizeof(Type)));
  }
};

// Convenience class for updating descriptor sets (uniforms)
class DescriptorSetUpdater {
public:
  DescriptorSetUpdater(int maxBuffers = 10, int maxImages = 10, int maxBufferViews = 0) {
    // we must pre-size these buffers as we take pointers to their members.
    s.bufferInfo.resize(maxBuffers);
    s.imageInfo.resize(maxImages);
    s.bufferViews.resize(maxBufferViews);
  }

  void beginDescriptorSet(vk::DescriptorSet dstSet) {
    s.dstSet = dstSet;
  }

  void beginImages(uint32_t dstBinding, uint32_t dstArrayElement, vk::DescriptorType descriptorType) {
    vk::WriteDescriptorSet wdesc{};
    wdesc.dstSet = s.dstSet;
    wdesc.dstBinding = dstBinding;
    wdesc.dstArrayElement = dstArrayElement;
    wdesc.descriptorCount = 0;
    wdesc.descriptorType = descriptorType;
    wdesc.pImageInfo = s.imageInfo.data() + s.numImages;
    s.descriptorWrites.push_back(wdesc);
  }

  void image(vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout) {
    if (!s.descriptorWrites.empty() && s.numImages != s.imageInfo.size() && s.descriptorWrites.back().pImageInfo) {
      s.descriptorWrites.back().descriptorCount++;
      s.imageInfo[s.numImages++] = vk::DescriptorImageInfo{sampler, imageView, imageLayout};
    } else {
      s.ok = false;
    }
  }

  void beginBuffers(uint32_t dstBinding, uint32_t dstArrayElement, vk::DescriptorType descriptorType) {
    vk::WriteDescriptorSet wdesc{};
    wdesc.dstSet = s.dstSet;
    wdesc.dstBinding = dstBinding;
    wdesc.dstArrayElement = dstArrayElement;
    wdesc.descriptorCount = 0;
    wdesc.descriptorType = descriptorType;
    wdesc.pBufferInfo = s.bufferInfo.data() + s.numBuffers;
    s.descriptorWrites.push_back(wdesc);
  }

  void buffer(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range) {
    if (!s.descriptorWrites.empty() && s.numBuffers != s.bufferInfo.size() && s.descriptorWrites.back().pBufferInfo) {
      s.descriptorWrites.back().descriptorCount++;
      s.bufferInfo[s.numBuffers++] = vk::DescriptorBufferInfo{buffer, offset, range};
    } else {
      s.ok = false;
    }
  }

  void beginBufferViews(uint32_t dstBinding, uint32_t dstArrayElement, vk::DescriptorType descriptorType) {
    vk::WriteDescriptorSet wdesc{};
    wdesc.dstSet = s.dstSet;
    wdesc.dstBinding = dstBinding;
    wdesc.dstArrayElement = dstArrayElement;
    wdesc.descriptorCount = 0;
    wdesc.descriptorType = descriptorType;
    wdesc.pTexelBufferView = s.bufferViews.data() + s.numBufferViews;
    s.descriptorWrites.push_back(wdesc);
  }

  void bufferView(vk::BufferView view) {
    if (!s.descriptorWrites.empty() && s.numBufferViews != s.bufferViews.size() && s.descriptorWrites.back().pImageInfo) {
      s.descriptorWrites.back().descriptorCount++;
      s.bufferViews[s.numBufferViews++] = view;
    } else {
      s.ok = false;
    }
  }

  void copy(vk::DescriptorSet srcSet, uint32_t srcBinding, uint32_t srcArrayElement, vk::DescriptorSet dstSet, uint32_t dstBinding, uint32_t dstArrayElement, uint32_t descriptorCount) {
    s.descriptorCopies.emplace_back(srcSet, srcBinding, srcArrayElement, dstSet, dstBinding, dstArrayElement, descriptorCount);
  }

  void update(const vk::Device &device) const {
    device.updateDescriptorSets( s.descriptorWrites, s.descriptorCopies );
  }

  bool ok() const { return s.ok; }
private:
  struct State {
    std::vector<vk::DescriptorBufferInfo> bufferInfo;
    std::vector<vk::DescriptorImageInfo> imageInfo;
    std::vector<vk::WriteDescriptorSet> descriptorWrites;
    std::vector<vk::CopyDescriptorSet> descriptorCopies;
    std::vector<vk::BufferView> bufferViews;
    vk::DescriptorSet dstSet;
    int numBuffers = 0;
    int numImages = 0;
    int numBufferViews = 0;
    bool ok = true;
  };

  State s;
};

// A factory class for descriptor set layouts. (An interface to the shaders)
class DescriptorSetLayoutMaker {
public:
  DescriptorSetLayoutMaker() {
  }

  void buffer(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, uint32_t descriptorCount) {
    s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
  }

  void image(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, uint32_t descriptorCount) {
    s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
  }

  void samplers(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, const std::vector<vk::Sampler> immutableSamplers) {
    s.samplers.push_back(immutableSamplers);
    auto pImmutableSamplers = s.samplers.back().data();
    s.bindings.emplace_back(binding, descriptorType, (uint32_t)immutableSamplers.size(), stageFlags, pImmutableSamplers);
  }

  void bufferView(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, uint32_t descriptorCount) {
    s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
  }

  vk::UniqueDescriptorSetLayout createUnique(vk::Device device) const {
    vk::DescriptorSetLayoutCreateInfo dsci{};
    dsci.bindingCount = (uint32_t)s.bindings.size();
    dsci.pBindings = s.bindings.data();
    return device.createDescriptorSetLayoutUnique(dsci);
  }

private:
  struct State {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    std::vector<std::vector<vk::Sampler> > samplers;
    int numSamplers = 0;
  };

  State s;
};

// A factory class for descriptor sets (A set of uniform bindings)
class DescriptorSetMaker {
public:
  DescriptorSetMaker() {
  }

  void layout(vk::DescriptorSetLayout layout) {
    s.layouts.push_back(layout);
  }

  // Allocate a vector of non-self-deleting descriptor sets
  // Note: descriptor sets get freed with the pool, so this is the better choice.
  std::vector<vk::DescriptorSet> create(vk::Device device, vk::DescriptorPool descriptorPool) const {
    vk::DescriptorSetAllocateInfo dsai{};
    dsai.descriptorPool = descriptorPool;
    dsai.descriptorSetCount = (uint32_t)s.layouts.size();
    dsai.pSetLayouts = s.layouts.data();
    return device.allocateDescriptorSets(dsai);
  }

  // Allocate a vector of self-deleting descriptor sets.
  std::vector<vk::UniqueDescriptorSet> createUnique(vk::Device device, vk::DescriptorPool descriptorPool) const {
    vk::DescriptorSetAllocateInfo dsai{};
    dsai.descriptorPool = descriptorPool;
    dsai.descriptorSetCount = (uint32_t)s.layouts.size();
    dsai.pSetLayouts = s.layouts.data();
    return device.allocateDescriptorSetsUnique(dsai);
  }

private:
  struct State {
    std::vector<vk::DescriptorSetLayout> layouts;
  };

  State s;
};

// Generic image with a view and memory object.
// Vulkan images need a memory object to hold the data and a view object for the GPU to access the data.
class GenericImage {
public:
  GenericImage() {
  }

  GenericImage(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, const vk::ImageCreateInfo &info, vk::ImageViewType viewType, vk::ImageAspectFlags aspectMask, bool makeHostImage) {
    create(device, memprops, info, viewType, aspectMask, makeHostImage);
  }

  vk::Image image() const { return *s.image; }
  vk::ImageView imageView() const { return *s.imageView; }
  vk::DeviceMemory mem() const { return *s.mem; }

  // Clear the colour of an image.
  void clear(vk::CommandBuffer cb, const std::array<float,4> colour = {1, 1, 1, 1}) {
    setLayout(cb, vk::ImageLayout::eTransferDstOptimal);
    vk::ClearColorValue ccv(colour);
    vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    cb.clearColorImage(*s.image, vk::ImageLayout::eTransferDstOptimal, ccv, range);
  }

  // Update the image with an array of pixels. (Currently 2D only)
  void update(vk::Device device, const void *data, vk::DeviceSize bytesPerPixel) {
    const uint8_t *src = (const uint8_t *)data;
    for (uint32_t mipLevel = 0; mipLevel != info().mipLevels; ++mipLevel) {
      // Array images are layed out horizontally. eg. [left][front][right] etc.
      for (uint32_t arrayLayer = 0; arrayLayer != info().arrayLayers; ++arrayLayer) {
        vk::ImageSubresource subresource{vk::ImageAspectFlagBits::eColor, mipLevel, arrayLayer};
        auto srlayout = device.getImageSubresourceLayout(*s.image, subresource);
        uint8_t *dest = (uint8_t *)device.mapMemory(*s.mem, 0, s.size, vk::MemoryMapFlags{}) + srlayout.offset;
        size_t bytesPerLine = s.info.extent.width * bytesPerPixel;
        size_t srcStride = bytesPerLine * info().arrayLayers;
        for (int y = 0; y != s.info.extent.height; ++y) {
          memcpy(dest, src + arrayLayer * bytesPerLine, bytesPerLine);
          src += srcStride;
          dest += srlayout.rowPitch;
        }
      }
    }
    device.unmapMemory(*s.mem);
  }

  // Copy another image to this one. This also changes the layout.
  void copy(vk::CommandBuffer cb, vku::GenericImage &srcImage) {
    srcImage.setLayout(cb, vk::ImageLayout::eTransferSrcOptimal);
    setLayout(cb, vk::ImageLayout::eTransferDstOptimal);
    for (uint32_t mipLevel = 0; mipLevel != info().mipLevels; ++mipLevel) {
      vk::ImageCopy region{};
      region.srcSubresource = {vk::ImageAspectFlagBits::eColor, mipLevel, 0, srcImage.info().arrayLayers};
      region.dstSubresource = {vk::ImageAspectFlagBits::eColor, mipLevel, 0, info().arrayLayers};
      region.extent = s.info.extent;
      cb.copyImage(srcImage.image(), vk::ImageLayout::eTransferSrcOptimal, *s.image, vk::ImageLayout::eTransferDstOptimal, region);
    }
  }

  // Change the layout of this image using a memory barrier.
  void setLayout(vk::CommandBuffer cb, vk::ImageLayout newLayout, vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) {
    vk::ImageLayout oldLayout = s.currentLayout;
    s.currentLayout = newLayout;

    vk::ImageMemoryBarrier imageMemoryBarriers = {};
    imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.oldLayout = oldLayout;
    imageMemoryBarriers.newLayout = newLayout;
    imageMemoryBarriers.image = *s.image;
    imageMemoryBarriers.subresourceRange = {aspectMask, 0, 1, 0, 1};

    // Put barrier on top
    vk::PipelineStageFlags srcStageMask{vk::PipelineStageFlagBits::eTopOfPipe};
    vk::PipelineStageFlags dstStageMask{vk::PipelineStageFlagBits::eTopOfPipe};
    vk::DependencyFlags dependencyFlags{};
    vk::AccessFlags srcMask{};
    vk::AccessFlags dstMask{};

    typedef vk::ImageLayout il;
    typedef vk::AccessFlagBits afb;

    // Is it me, or are these the same?
    switch (oldLayout) {
      case il::eUndefined: break;
      case il::eGeneral: srcMask = afb::eTransferWrite; break;
      case il::eColorAttachmentOptimal: srcMask = afb::eColorAttachmentWrite; break;
      case il::eDepthStencilAttachmentOptimal: srcMask = afb::eDepthStencilAttachmentWrite; break;
      case il::eDepthStencilReadOnlyOptimal: srcMask = afb::eDepthStencilAttachmentRead; break;
      case il::eShaderReadOnlyOptimal: srcMask = afb::eShaderRead; break;
      case il::eTransferSrcOptimal: srcMask = afb::eTransferRead; break;
      case il::eTransferDstOptimal: srcMask = afb::eTransferWrite; break;
      case il::ePreinitialized: srcMask = afb::eTransferWrite|afb::eHostWrite; break;
      case il::ePresentSrcKHR: srcMask = afb::eMemoryRead; break;
    }

    switch (newLayout) {
      case il::eUndefined: break;
      case il::eGeneral: dstMask = afb::eTransferWrite; break;
      case il::eColorAttachmentOptimal: dstMask = afb::eColorAttachmentWrite; break;
      case il::eDepthStencilAttachmentOptimal: dstMask = afb::eDepthStencilAttachmentWrite; break;
      case il::eDepthStencilReadOnlyOptimal: dstMask = afb::eDepthStencilAttachmentRead; break;
      case il::eShaderReadOnlyOptimal: dstMask = afb::eShaderRead; break;
      case il::eTransferSrcOptimal: dstMask = afb::eTransferRead; break;
      case il::eTransferDstOptimal: dstMask = afb::eTransferWrite; break;
      case il::ePreinitialized: dstMask = afb::eTransferWrite; break;
      case il::ePresentSrcKHR: dstMask = afb::eMemoryRead; break;
    }

    imageMemoryBarriers.srcAccessMask = srcMask;
    imageMemoryBarriers.dstAccessMask = dstMask;
    auto memoryBarriers = nullptr;
    auto bufferMemoryBarriers = nullptr;
    cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, memoryBarriers, bufferMemoryBarriers, imageMemoryBarriers);
  }

  /// Set what the image thinks is its current layout (ie. the old layout in an image barrier).
  void setCurrentLayout(vk::ImageLayout oldLayout) {
    s.currentLayout = oldLayout;
  }

  vk::Format format() const { return s.info.format; }
  vk::Extent3D extent() const { return s.info.extent; }
  const vk::ImageCreateInfo &info() const { return s.info; }
protected:
  void create(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, const vk::ImageCreateInfo &info, vk::ImageViewType viewType, vk::ImageAspectFlags aspectMask, bool hostImage) {
    s.currentLayout = info.initialLayout;
    s.info = info;
    s.image = device.createImageUnique(info);

    // Find out how much memory and which heap to allocate from.
    auto memreq = device.getImageMemoryRequirements(*s.image);
    vk::MemoryPropertyFlags search{};
    if (hostImage) search = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;

    // Create a memory object to bind to the buffer.
    // Note: we don't expect to be able to map the buffer.
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = s.size = memreq.size;
    mai.memoryTypeIndex = vku::findMemoryTypeIndex(memprops, memreq.memoryTypeBits, search);
    s.mem = device.allocateMemoryUnique(mai);

    device.bindImageMemory(*s.image, *s.mem, 0);

    if (!hostImage) {
      vk::ImageViewCreateInfo viewInfo{};
      viewInfo.flags;
      viewInfo.image = *s.image;
      viewInfo.viewType = viewType;
      viewInfo.format = info.format;
      viewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
      viewInfo.subresourceRange = vk::ImageSubresourceRange{aspectMask, 0, info.mipLevels, 0, info.arrayLayers};
      s.imageView = device.createImageViewUnique(viewInfo);
    }
  }

  struct State {
    vk::UniqueImage image;
    vk::UniqueImageView imageView;
    vk::UniqueDeviceMemory mem;
    vk::DeviceSize size;
    vk::ImageLayout currentLayout;
    vk::ImageCreateInfo info;
  };

  State s;
};

// A 2D texture image living on the GPU or a staging buffer visible to the CPU.
class TextureImage2D : public GenericImage {
public:
  TextureImage2D() {
  }

  TextureImage2D(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, uint32_t width, uint32_t height, uint32_t mipLevels=1, vk::Format format = vk::Format::eR8G8B8A8Unorm, bool hostImage = false) {
    vk::ImageCreateInfo info;
    info.flags = {};
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = mipLevels;
    info.arrayLayers = 1;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eTransferDst;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
    create(device, memprops, info, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, hostImage);
  }
private:
};

// A cube map texture image living on the GPU or a staging buffer visible to the CPU.
class TextureImageCube : public GenericImage {
public:
  TextureImageCube() {
  }

  TextureImageCube(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, uint32_t width, uint32_t height, uint32_t mipLevels=1, vk::Format format = vk::Format::eR8G8B8A8Unorm, bool hostImage = false) {
    vk::ImageCreateInfo info;
    info.flags = {};
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = mipLevels;
    info.arrayLayers = 6;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eTransferDst;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
    create(device, memprops, info, vk::ImageViewType::eCube, vk::ImageAspectFlagBits::eColor, hostImage);
  }
private:
};

class DepthStencilImage : public GenericImage {
public:
  DepthStencilImage() {
  }

  DepthStencilImage(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, uint32_t width, uint32_t height, vk::Format format = vk::Format::eD24UnormS8Uint) {
    vk::ImageCreateInfo info;
    info.flags = {};

    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment|vk::ImageUsageFlagBits::eTransferSrc;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = vk::ImageLayout::eUndefined;
    typedef vk::ImageAspectFlagBits iafb;
    create(device, memprops, info, vk::ImageViewType::e2D, iafb::eDepth|iafb::eStencil, false);
  }
private:
};

// A class to help build samplers.
// Samplers tell the shader stages how to sample an image.
// They are used in combination with an image to make a combined image sampler
// used by texture() calls in shaders.
// They can also be passed to shaders directly for use on multiple image sources.
class SamplerMaker {
public:
  // Default to a very basic sampler.
  SamplerMaker() {
    s.info.magFilter = vk::Filter::eNearest;
    s.info.minFilter = vk::Filter::eNearest;
    s.info.mipmapMode = vk::SamplerMipmapMode::eNearest;
    s.info.addressModeU = vk::SamplerAddressMode::eRepeat;
    s.info.addressModeV = vk::SamplerAddressMode::eRepeat;
    s.info.addressModeW = vk::SamplerAddressMode::eRepeat;
    s.info.mipLodBias = 0.0f;
    s.info.anisotropyEnable = 0;
    s.info.maxAnisotropy = 0.0f;
    s.info.compareEnable = 0;
    s.info.compareOp = vk::CompareOp::eNever;
    s.info.minLod = 0;
    s.info.maxLod = 0;
    s.info.borderColor = vk::BorderColor{};
    s.info.unnormalizedCoordinates = 0;
  }

  ////////////////////
  //
  // Setters
  //
  SamplerMaker &flags(vk::SamplerCreateFlags value) { s.info.flags = value; return *this; }
  SamplerMaker &magFilter(vk::Filter value) { s.info.magFilter = value; return *this; }
  SamplerMaker &minFilter(vk::Filter value) { s.info.minFilter = value; return *this; }
  SamplerMaker &mipmapMode(vk::SamplerMipmapMode value) { s.info.mipmapMode = value; return *this; }
  SamplerMaker &addressModeU(vk::SamplerAddressMode value) { s.info.addressModeU = value; return *this; }
  SamplerMaker &addressModeV(vk::SamplerAddressMode value) { s.info.addressModeV = value; return *this; }
  SamplerMaker &addressModeW(vk::SamplerAddressMode value) { s.info.addressModeW = value; return *this; }
  SamplerMaker &mipLodBias(float value) { s.info.mipLodBias = value; return *this; }
  SamplerMaker &anisotropyEnable(vk::Bool32 value) { s.info.anisotropyEnable = value; return *this; }
  SamplerMaker &maxAnisotropy(float value) { s.info.maxAnisotropy = value; return *this; }
  SamplerMaker &compareEnable(vk::Bool32 value) { s.info.compareEnable = value; return *this; }
  SamplerMaker &compareOp(vk::CompareOp value) { s.info.compareOp = value; return *this; }
  SamplerMaker &minLod(float value) { s.info.minLod = value; return *this; }
  SamplerMaker &maxLod(float value) { s.info.maxLod = value; return *this; }
  SamplerMaker &borderColor(vk::BorderColor value) { s.info.borderColor = value; return *this; }
  SamplerMaker &unnormalizedCoordinates(vk::Bool32 value) { s.info.unnormalizedCoordinates = value; return *this; }

  // Allocate a self-deleting image.
  vk::UniqueSampler createUnique(vk::Device device) const {
    return device.createSamplerUnique(s.info);
  }

  // Allocate a non self-deleting Sampler.
  vk::Sampler create(vk::Device device) const {
    return device.createSampler(s.info);
  }

private:
  struct State {
    vk::SamplerCreateInfo info;
  };

  State s;
};

} // namespace vku

#endif // VKU_HPP
