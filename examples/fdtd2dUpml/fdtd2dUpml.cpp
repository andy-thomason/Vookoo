#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/io.hpp>
#include <algorithm> // std::generate

/* Example of multiple passes writing to multiple buffer attachments.
 * Also utilizes ping-pong between render frames.
------- Frame%2 == 0 ------
-- pass0 --
0'2'    <<<<< Output Attachments
^       <<<<< Fragment shader Pass0
0 1 2 3 <<<<< Input Images

-- pass1 --
1'3'
^       <<<<< Fragment shader Pass1
0'1 2'3

-- pass2 --
image
^        <<<<< Fragment shader Pass Final
0'1'2'3'

------ Frame%2 == 1 ------
-- pass0 --
0 2
^
0'1'2'3'

-- pass1 --
1 3
^
0 1'2 3'

-- pass2 --
image
^
0 1 2 3

Note X  alias for imageChannelXPing
     X' alias for imageChannelXPong
     shader transforms _inputs_ below "^" to attachments _above_ "^"
*/

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "fdtd2dUpml";
  auto glfwwindow = glfwCreateWindow(1024, 1024, title, nullptr, nullptr);

  // Initialize makers
  vku::InstanceMaker im{};
  im.defaultLayers();
  vku::DeviceMaker dm{};
  dm.defaultLayers();

  vku::Framework fw{im, dm};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }
  fw.dumpCaps(std::cout);

  vk::Device device = fw.device();

  vku::Window window{
    fw.instance(),
    device,
    fw.physicalDevice(),
    fw.graphicsQueueFamilyIndex(),
    glfwwindow
  };
  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }
  window.dumpCaps(std::cout, fw.physicalDevice());

  auto viewport = vk::Viewport{
    0.0f, 
    0.0f,
    (float)window.width(),
    (float)window.height(),
    0.0f,
    1.0f
  };

  ////////////////////////////////////////
  //
  // Create Uniform Buffer

  struct Uniform {
    glm::vec4 iResolution; // viewport resolution (in pixels)
    int iFrame[4]; // shader playback frame
    glm::vec4 iChannelResolution[4]; // channel resolution (in pixels), *.frag only uses [0] and [1]
  };
  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer ubo(device, fw.memprops(), sizeof(Uniform));

  ////////////////////////////////////////
  //
  // Create Mesh vertices

  struct Vertex { 
    glm::vec3 pos; 
  };

  const std::vector<Vertex> vertices = {
    {.pos={-1.0f,-1.0f, 0.0f}},
    {.pos={ 1.0f,-1.0f, 0.0f}},
    {.pos={ 1.0f, 1.0f, 0.0f}},
    {.pos={-1.0f, 1.0f, 0.0f}},
  };
  vku::HostVertexBuffer vbo(device, fw.memprops(), vertices);

  ////////////////////////////////////////
  //
  // Create mesh indices

  std::vector<uint32_t> indices = {
    0, 1, 2, 
    2, 3, 0
  };
  vku::HostIndexBuffer ibo(device, fw.memprops(), indices);

  ////////////////////////////////////////////////////////
  //
  // Create textures

  uint32_t fdtdDomainSize = 1024;

  std::vector<uint8_t> pixels0(fdtdDomainSize*fdtdDomainSize*1*4*4); // x*y*z*4RGBA*4bytes/float32

  std::generate(pixels0.begin(), pixels0.end(), [] () { return 0; });

  // e&h fields in fragment shaders
  vku::ColorAttachmentImage iChannel0Ping{device, fw.memprops(), fdtdDomainSize, fdtdDomainSize, vk::Format::eR32G32B32A32Sfloat};
  vku::ColorAttachmentImage iChannel0Pong{device, fw.memprops(), fdtdDomainSize, fdtdDomainSize, vk::Format::eR32G32B32A32Sfloat};
  vku::ColorAttachmentImage iChannel1Ping{device, fw.memprops(), fdtdDomainSize, fdtdDomainSize, vk::Format::eR32G32B32A32Sfloat};
  vku::ColorAttachmentImage iChannel1Pong{device, fw.memprops(), fdtdDomainSize, fdtdDomainSize, vk::Format::eR32G32B32A32Sfloat};

  iChannel0Ping.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);
  iChannel0Pong.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);
  iChannel1Ping.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);
  iChannel1Pong.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);

  // b&d fields in fragment shaders
  vku::ColorAttachmentImage iChannel2Ping{device, fw.memprops(), fdtdDomainSize, fdtdDomainSize, vk::Format::eR32G32B32A32Sfloat};
  vku::ColorAttachmentImage iChannel2Pong{device, fw.memprops(), fdtdDomainSize, fdtdDomainSize, vk::Format::eR32G32B32A32Sfloat};
  vku::ColorAttachmentImage iChannel3Ping{device, fw.memprops(), fdtdDomainSize, fdtdDomainSize, vk::Format::eR32G32B32A32Sfloat};
  vku::ColorAttachmentImage iChannel3Pong{device, fw.memprops(), fdtdDomainSize, fdtdDomainSize, vk::Format::eR32G32B32A32Sfloat};

  iChannel2Ping.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);
  iChannel2Pong.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);
  iChannel3Ping.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);
  iChannel3Pong.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);

  ////////////////////////////////////////////////////////
  //
  // Create Samplers
 
  vku::SamplerMaker sm{};
  vk::UniqueSampler linearSampler = sm
    .magFilter( vk::Filter::eLinear )
    .minFilter( vk::Filter::eLinear )
    .mipmapMode( vk::SamplerMipmapMode::eNearest )
    .addressModeU( vk::SamplerAddressMode::eRepeat )
    .addressModeV( vk::SamplerAddressMode::eRepeat )
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Build the descriptor sets

  vku::DescriptorSetLayoutMaker dslm{};
  auto descriptorSetLayout = dslm
    .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 1)
    .image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .image(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .image(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .createUnique(device);

  // This pipeline layout is shared amongst several pipelines.
  vku::PipelineLayoutMaker plm{};
  auto pipelineLayout = plm
    .descriptorSetLayout(*descriptorSetLayout)
    .createUnique(device);

  // Define render pass specific descriptor sets (ping and pong) 
  vku::DescriptorSetMaker dsm{};
  dsm.layout(*descriptorSetLayout)  // for ping|pong, [iFrame%2]==0
     .layout(*descriptorSetLayout); // for pong|ping, [iFrame%2]==1

  auto descriptorSetsPass0 = dsm.create(device, fw.descriptorPool());
  auto descriptorSetsPass1 = dsm.create(device, fw.descriptorPool());
  auto descriptorSetsFinal = dsm.create(device, fw.descriptorPool());

  ////////////////////////////////////////
  //
  // Update the descriptor sets for the shader uniforms.
  //
  // Note in following comments
  // 0  alias for imageChannel0Ping
  // 0' alias for imageChannel0Pong
  // 1  alias for imageChannel1Ping
  // 1' alias for imageChannel1Pong
  // shader transforms _inputs_ below "^" to attachment _above_ "^"
  // The descriptor set image attachments define _inputs_
  // 
  vku::DescriptorSetUpdater dsuPass0;
  dsuPass0
     // ------- Frame%2 == 0 ------
     // 0'2'    <<<<< Output Attachments
     // ^       <<<<< Fragment shader Pass0
     // 0 1 2 3 <<<<< Input Images
     .beginDescriptorSet(descriptorSetsPass0[0])
     .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
     .buffer(ubo.buffer(), 0, sizeof(Uniform))
     .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel0Ping.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel1Ping.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel2Ping.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel3Ping.imageView(), vk::ImageLayout::eGeneral)
     // ------- Frame%2 == 1 ------
     // 0 2      <<<<< Output Attachment
     // ^        <<<<< Fragment shader Pass0
     // 0'1'2'3' <<<<< Input Images
     .beginDescriptorSet(descriptorSetsPass0[1])
     .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
     .buffer(ubo.buffer(), 0, sizeof(Uniform))
     .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel0Pong.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel1Pong.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel2Pong.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel3Pong.imageView(), vk::ImageLayout::eGeneral)
     // update the descriptor sets with their pointers (but not data).
     .update(device);

  vku::DescriptorSetUpdater dsuPass1;
  dsuPass1
     // ------- Frame%2 == 0 ------
     // 1'3'    <<<<< Output Attachments
     // ^       <<<<< Fragment shader Pass1
     // 0'1 2'3 <<<<< Input Images
     .beginDescriptorSet(descriptorSetsPass1[0])
     .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
     .buffer(ubo.buffer(), 0, sizeof(Uniform))
     .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel0Pong.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel1Ping.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel2Pong.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel3Ping.imageView(), vk::ImageLayout::eGeneral)
     // ------- Frame%2 == 1 ------
     // 1 3      <<<<< Output Attachments
     // ^        <<<<< Fragment shader Pass1
     // 0 1'2 3' <<<<< Input Images     
     .beginDescriptorSet(descriptorSetsPass1[1])
     .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
     .buffer(ubo.buffer(), 0, sizeof(Uniform))
     .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel0Ping.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel1Pong.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel2Ping.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel3Pong.imageView(), vk::ImageLayout::eGeneral)
     // update the descriptor sets with their pointers (but not data).
     .update(device);

  vku::DescriptorSetUpdater dsuPassFinal;
  dsuPassFinal
     // ------- Frame%2 == 0 ------
     // image    <<<<< Output Image
     // ^        <<<<< Fragment shader Pass Final
     // 0'1'2'3' <<<<< Input Images
     .beginDescriptorSet(descriptorSetsFinal[0])
     .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
     .buffer(ubo.buffer(), 0, sizeof(Uniform))
     .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel0Pong.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel1Pong.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel2Pong.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel3Pong.imageView(), vk::ImageLayout::eGeneral)
     // ------- Frame%2 == 1 ------
     // image   <<<<< Output Image
     // ^       <<<<< Fragment shader Pass Final
     // 0 1 2 3 <<<<< Input Images
     .beginDescriptorSet(descriptorSetsFinal[1])
     .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
     .buffer(ubo.buffer(), 0, sizeof(Uniform))
     .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel0Ping.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel1Ping.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel2Ping.imageView(), vk::ImageLayout::eGeneral)
     .beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler)
     .image(*linearSampler, iChannel3Ping.imageView(), vk::ImageLayout::eGeneral)
     .update(device);

  ////////////////////////////////////////
  //
  // Helper for building similar FDTD render passes
  // that only differ by particular iChannelX and iChannelY output attachments
  //
  auto RenderPassFDTD = [&device](vku::ColorAttachmentImage& iChannelX, vku::ColorAttachmentImage& iChannelY)->vk::UniqueRenderPass {
    // Build the renderpass writing to iChannelX 
    vku::RenderpassMaker rpmPassX;
    return rpmPassX
      // The 1st colour attachment.
     .attachmentBegin(iChannelX.format())
     .attachmentSamples(vk::SampleCountFlagBits::e1)
     .attachmentLoadOp(vk::AttachmentLoadOp::eDontCare)
     .attachmentStoreOp(vk::AttachmentStoreOp::eStore)
     .attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
     .attachmentStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
     .attachmentInitialLayout(vk::ImageLayout::eUndefined)
     .attachmentFinalLayout(vk::ImageLayout::eGeneral)
     // The 2nd colour attachment. 
     .attachmentBegin(iChannelY.format())
     .attachmentSamples(vk::SampleCountFlagBits::e1)
     .attachmentLoadOp(vk::AttachmentLoadOp::eDontCare)
     .attachmentStoreOp(vk::AttachmentStoreOp::eStore)
     .attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
     .attachmentStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
     .attachmentInitialLayout(vk::ImageLayout::eUndefined)
     .attachmentFinalLayout(vk::ImageLayout::eGeneral)
      // A subpass to render using the above attachment(s).
     .subpassBegin(vk::PipelineBindPoint::eGraphics)
     .subpassColorAttachment(vk::ImageLayout::eGeneral, 0)
     .subpassColorAttachment(vk::ImageLayout::eGeneral, 1)
      // Define dependencies, https://gpuopen.com/vulkan-barriers-explained/
      //
      //  [ ]TOP_OF_PIPE -------------------------------------------------
      //  [ ]DRAW_INDIRECT
      //  [ ]VERTEX_INPUT
      //  [ ]VERTEX_SHADER
      //  [ ]TESSELLATION_CONTROL_SHADER
      //  [ ]TESSELLATION_EVALUATION_SHADER  [ ]COMPUTE_SHADER  [ ]TRANSFER
      //  [ ]GEOMETRY_SHADER
      //  [ ]EARLY_FRAGMENT_TESTS
      //  [S]FRAGMENT_SHADER
      //  [ ]LATE_FRAGMENT_TESTS
      //  [ ]COLOR_ATTACHMENT_OUTPUT
      //  [ ]BOTTOM_OF_PIPE ----------------------------------------------
      //
      //  [ ]TOP_OF_PIPE -------------------------------------------------
      //  [ ]DRAW_INDIRECT
      //  [ ]VERTEX_INPUT
      //  [ ]VERTEX_SHADER
      //  [ ]TESSELLATION_CONTROL_SHADER
      //  [ ]TESSELLATION_EVALUATION_SHADER  [ ]COMPUTE_SHADER  [ ]TRANSFER
      //  [ ]GEOMETRY_SHADER
      //  [ ]EARLY_FRAGMENT_TESTS
      //  [ ]FRAGMENT_SHADER
      //  [ ]LATE_FRAGMENT_TESTS
      //  [D]COLOR_ATTACHMENT_OUTPUT
      //  [ ]BOTTOM_OF_PIPE ----------------------------------------------
      //
      // The special value VK_SUBPASS_EXTERNAL refers to the 
      // implicit subpass before or after the render pass depending on 
      // whether it is specified in srcSubpass or dstSubpass.
      //
      // dependency: If srcSubpass is equal to VK_SUBPASS_EXTERNAL, 
      // the first synchronization scope includes commands that occur earlier
      // in submission order than the vkCmdBeginRenderPass used to begin the
      // render pass instance.` 
     .dependencyBegin(VK_SUBPASS_EXTERNAL, 0)
     //VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
     //VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
     .dependencySrcAccessMask(vk::AccessFlagBits::eInputAttachmentRead)
     .dependencySrcStageMask(vk::PipelineStageFlagBits::eFragmentShader)
     //VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
     //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
     .dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
     .dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
     .dependencyDependencyFlags(vk::DependencyFlagBits::eByRegion)
      // dependency: If dstSubpass is equal to VK_SUBPASS_EXTERNAL, 
      // the second synchronization scope includes commands that occur later
      // in submission order than the vkCmdEndRenderPass used to end the 
      // render pass instance.
     .dependencyBegin(0, VK_SUBPASS_EXTERNAL)
     //VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
     //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
     .dependencySrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
     .dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
     //VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
     //VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
     .dependencyDstAccessMask(vk::AccessFlagBits::eInputAttachmentRead)
     .dependencyDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
     .dependencyDependencyFlags(vk::DependencyFlagBits::eByRegion)
     // Finally use the maker method to construct this renderpass
     .createUnique(device);
  };

  ////////////////////////////////////////
  //
  // Build the pass0 pipeline
  //

  vku::ShaderModule pass0_vert{device, BINARY_DIR "fdtd2dUpml.vert.spv"};
  vku::ShaderModule pass0_frag{device, BINARY_DIR "fdtd2dUpmlpass0.frag.spv"};

  vku::PipelineMaker spmPass0{fdtdDomainSize, fdtdDomainSize};
  spmPass0
     .shader(vk::ShaderStageFlagBits::eVertex, pass0_vert)
     .shader(vk::ShaderStageFlagBits::eFragment, pass0_frag)
     .vertexBinding(0, sizeof(Vertex))
     .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
     .cullMode( vk::CullModeFlagBits::eBack )
     .frontFace( vk::FrontFace::eClockwise )
     .blendBegin(false) // required for output color attachment 0
     .blendBegin(false) // required for output color attachment 1
     .viewport(viewport);

  // Build the renderpass writing to iChannel0Pong, iChannel2Pong 
  vk::UniqueRenderPass RenderPass0Ping = RenderPassFDTD(iChannel0Pong, iChannel2Pong);
  // Build the renderpass writing to iChannel0Ping, iChannel2Ping 
  vk::UniqueRenderPass RenderPass0Pong = RenderPassFDTD(iChannel0Ping, iChannel2Ping);

  // Build the ping|pong pipelines based on this renderpass.
  vk::UniquePipeline pass0Pipeline[] = {
    spmPass0.createUnique(device, fw.pipelineCache(), *pipelineLayout, *RenderPass0Ping),
    spmPass0.createUnique(device, fw.pipelineCache(), *pipelineLayout, *RenderPass0Pong)};

  ////////////////////////////////////////
  //
  // Build the pass1 pipeline
  //

  vku::ShaderModule pass1_vert{device, BINARY_DIR "fdtd2dUpml.vert.spv"};
  vku::ShaderModule pass1_frag{device, BINARY_DIR "fdtd2dUpmlpass1.frag.spv"};

  vku::PipelineMaker spmPass1{fdtdDomainSize, fdtdDomainSize};
  spmPass1
     .shader(vk::ShaderStageFlagBits::eVertex, pass1_vert)
     .shader(vk::ShaderStageFlagBits::eFragment, pass1_frag)
     .vertexBinding(0, sizeof(Vertex))
     .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
     .cullMode( vk::CullModeFlagBits::eBack )
     .frontFace( vk::FrontFace::eClockwise )
     .blendBegin(false) // required for output color attachment 0
     .blendBegin(false) // required for output color attachment 1
     .viewport(viewport);

  // Build the renderpass writing to iChannel1Pong, iChannel3Pong 
  vk::UniqueRenderPass RenderPass1Ping = RenderPassFDTD(iChannel1Pong, iChannel3Pong);
  // Build the renderpass writing to iChannel1Ping, iChannel3Ping 
  vk::UniqueRenderPass RenderPass1Pong = RenderPassFDTD(iChannel1Ping, iChannel3Ping);

  // Build the ping|pong pipelines based on this renderpass.
  vk::UniquePipeline pass1Pipeline[] = {
    spmPass1.createUnique(device, fw.pipelineCache(), *pipelineLayout, *RenderPass1Ping),
    spmPass1.createUnique(device, fw.pipelineCache(), *pipelineLayout, *RenderPass1Pong)};

  ////////////////////////////////////////
  //
  // Build the final pipeline

  vku::ShaderModule final_vert{device, BINARY_DIR "fdtd2dUpml.vert.spv"};
  vku::ShaderModule final_frag{device, BINARY_DIR "fdtd2dUpmlpass2.frag.spv"};

  vku::PipelineMaker pm{window.width(), window.height()};
  pm.shader(vk::ShaderStageFlagBits::eVertex, final_vert)
    .shader(vk::ShaderStageFlagBits::eFragment, final_frag)
    .vertexBinding(0, sizeof(Vertex))
    .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
    .depthTestEnable(VK_TRUE)
    .cullMode(vk::CullModeFlagBits::eBack)
    .frontFace(vk::FrontFace::eClockwise)
    .viewport(viewport);

  auto finalPipeline = pm.createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());

  ////////////////////////////////////////
  //
  // Build UniqueFramebuffers for pass0 and pass1
  //
  // Render passes operate in conjunction with framebuffers. 
  // Framebuffers represent a collection of specific memory attachments that a render pass instance uses.

  auto UniqueFramebuffers = [&device, &fdtdDomainSize](vk::UniqueRenderPass &RenderPassX, vku::ColorAttachmentImage &iChannelX, vku::ColorAttachmentImage &iChannelY) -> vk::UniqueFramebuffer {
    // RenderPassX writes to output iChannelX
    vk::ImageView attachmentsPassX[] = {iChannelX.imageView(), iChannelY.imageView()};
    vk::FramebufferCreateInfo fbciPassX{{}, *RenderPassX, sizeof(attachmentsPassX)/sizeof(vk::ImageView), attachmentsPassX, fdtdDomainSize, fdtdDomainSize, 1 };
    vk::UniqueFramebuffer FrameBufferPassX = device.createFramebufferUnique(fbciPassX);
    return FrameBufferPassX;
  };
  vk::UniqueFramebuffer FrameBufferPass0Ping = UniqueFramebuffers( RenderPass0Ping, iChannel0Pong, iChannel2Pong );
  vk::UniqueFramebuffer FrameBufferPass0Pong = UniqueFramebuffers( RenderPass0Pong, iChannel0Ping, iChannel2Ping );
  vk::UniqueFramebuffer FrameBufferPass1Ping = UniqueFramebuffers( RenderPass1Ping, iChannel1Pong, iChannel3Pong );
  vk::UniqueFramebuffer FrameBufferPass1Pong = UniqueFramebuffers( RenderPass1Pong, iChannel1Ping, iChannel3Ping );

  // Match in order of attachments to clear the image.
  // kludge: Is there a better way to get really what's needed; sizeof(attachmentsPass?P?ng)/sizeof(vk::ImageView)?
  vk::ImageView attachmentsPass0Ping[] = {iChannel0Pong.imageView(), iChannel2Pong.imageView()}; // kludge used later for ClearValue, since hidden in UniqueFramebuffers() 
  std::array<vk::ClearValue, sizeof(attachmentsPass0Ping)/sizeof(vk::ImageView)> clearColours{
    vk::ClearColorValue{}
  };

  ////////////////////////////////////////
  //
  // Build a RenderPassBeginInfo for pass0
  // sets the framebuffer and renderpass

  // Begin rendering using the framebuffer and renderpass
  vk::RenderPassBeginInfo pass0Rpbi[]={{
    *RenderPass0Ping,
    *FrameBufferPass0Ping,
    vk::Rect2D{{0, 0}, {fdtdDomainSize, fdtdDomainSize}},
    (uint32_t) clearColours.size(),
    clearColours.data()
  },{
    *RenderPass0Pong,
    *FrameBufferPass0Pong,
    vk::Rect2D{{0, 0}, {fdtdDomainSize, fdtdDomainSize}},
    (uint32_t) clearColours.size(),
    clearColours.data()
  }};

  ////////////////////////////////////////
  //
  // Build a RenderPassBeginInfo for pass1
  // sets the framebuffer and renderpass
 
  vk::RenderPassBeginInfo pass1Rpbi[]={{
    *RenderPass1Ping,
    *FrameBufferPass1Ping,
    vk::Rect2D{{0, 0}, {fdtdDomainSize, fdtdDomainSize}},
    (uint32_t) clearColours.size(),
    clearColours.data()
  },{
    *RenderPass1Pong,
    *FrameBufferPass1Pong,
    vk::Rect2D{{0, 0}, {fdtdDomainSize, fdtdDomainSize}},
    (uint32_t) clearColours.size(),
    clearColours.data()
  }};

  int iFrame = 0;
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    window.draw(device, fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        
        Uniform uniform {
          .iResolution = glm::vec4(window.width(), window.height(), 1., 0.),
          .iFrame = {iFrame, 0, 0, 0},
          .iChannelResolution = {
            glm::vec4(fdtdDomainSize, fdtdDomainSize, 1., 0.),
            glm::vec4(fdtdDomainSize, fdtdDomainSize, 1., 0.),
            glm::vec4(fdtdDomainSize, fdtdDomainSize, 1., 0.),
            glm::vec4(fdtdDomainSize, fdtdDomainSize, 1., 0.)
          }
        };

        // Record the dynamic buffer.
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);

        // Copy the uniform data to the buffer. (note this is done
        // inline and so we can discard "uniform" afterwards)
        cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform), &uniform);

        // Shared among all following passes
        cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
        cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);

        // 1st renderpass. Compute E.
        cb.beginRenderPass(pass0Rpbi[iFrame%2], vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pass0Pipeline[iFrame%2]);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSetsPass0[iFrame%2], nullptr);
        cb.drawIndexed(indices.size(), 1, 0, 0, 0);
        cb.endRenderPass();

        // 2nd renderpass. Compute H.
        cb.beginRenderPass(pass1Rpbi[iFrame%2], vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pass1Pipeline[iFrame%2]);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSetsPass1[iFrame%2], nullptr);
        cb.drawIndexed(indices.size(), 1, 0, 0, 0);
        cb.endRenderPass();

        // Final renderpass. Draw the final image.
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *finalPipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSetsFinal[iFrame%2], nullptr);
        cb.drawIndexed(indices.size(), 1, 0, 0, 0);
        cb.endRenderPass();

        cb.end();
      }
        
    );

    //std::this_thread::sleep_for(std::chrono::milliseconds(16)); // unnecessary with swapchain present mode being "Fifo" which is V-SYNC limited.
    iFrame++;
  }

  device.waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}


