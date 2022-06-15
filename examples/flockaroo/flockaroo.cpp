#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/io.hpp>
#include <algorithm> // std::generate

// credit: shaders from https://www.shadertoy.com/view/MsGSRd

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "flockaroo";
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

  uint32_t advectionSize = 512;

  std::vector<uint8_t> pixels0(advectionSize*advectionSize*1*4*4); // x*y*z*4RGBA*4bytes/float32
  std::vector<uint8_t> pixels1(advectionSize*advectionSize*1*4*1); // x*y*z*4RGBA*1bytes/uint8_t

  std::generate(pixels0.begin(), pixels0.end(), [] () { return 0; });
  std::generate(pixels1.begin(), pixels1.end(), 
    [c=0, x=0, y=0, &advectionSize] () mutable { 
      uint8_t rv = 0;
      // https://lodev.org/cgtutor/randomnoise.html
      if (sin(6.2912*(sqrt(x*x + y*y))*4./advectionSize) < 0) {
        rv = std::rand() % 0xFF;
      } else {
        rv = 0x00;
      };
      // determine color index and xy-pixel coordinate
      ++c %= 4;
      if (c==0) {
        ++x %= advectionSize;
        if (x==0) ++y %= advectionSize;
      }
      return rv;
    });

  vku::ColorAttachmentImage iChannelPing{device, fw.memprops(), advectionSize, advectionSize, vk::Format::eR32G32B32A32Sfloat};
  vku::ColorAttachmentImage iChannelPong{device, fw.memprops(), advectionSize, advectionSize, vk::Format::eR32G32B32A32Sfloat};
  vku::TextureImage2D iChannel1{device, fw.memprops(), advectionSize, advectionSize, 1, vk::Format::eR8G8B8A8Unorm};

  iChannelPing.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);
  iChannelPong.upload(device, pixels0, window.commandPool(), fw.memprops(), fw.graphicsQueue(), vk::ImageLayout::eGeneral);
  iChannel1.upload(device, pixels1, window.commandPool(), fw.memprops(), fw.graphicsQueue());

  // Create linearSampler
  vku::SamplerMaker sm{};
  auto linearSampler = sm
    .magFilter( vk::Filter::eLinear )
    .minFilter( vk::Filter::eLinear )
    .mipmapMode( vk::SamplerMipmapMode::eNearest )
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Build the descriptor sets

  vku::DescriptorSetLayoutMaker dslm{};
  auto descriptorSetLayout = dslm
    .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 1)
    .image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .createUnique(device);

  ////////////////////////////////////////
  //
  // This pipeline layout is shared amongst several pipelines.

  vku::PipelineLayoutMaker plm{};
  auto pipelineLayout = plm
    .descriptorSetLayout(*descriptorSetLayout)
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Define the particular descriptor sets for the shader uniforms.

  vku::DescriptorSetMaker dsm{};
  auto descriptorSets = dsm
    .layout(*descriptorSetLayout) // ping: index=0 for input "uniform sampler2D iChannel0" being iChannelPing
    .layout(*descriptorSetLayout) // pong: index=1 for input "uniform sampler2D iChannel0" being iChannelPong
    .create(device, fw.descriptorPool());

  vku::DescriptorSetUpdater dsu;
  dsu
    //-- descriptorSets[0]
    .beginDescriptorSet(descriptorSets[0])
    // layout (binding = 0) uniform Uniform
    .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
    .buffer(ubo.buffer(), 0, sizeof(Uniform))
    // layout (binding = 1) uniform sampler2D iChannel0
    .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
    .image(*linearSampler, iChannelPing.imageView(), vk::ImageLayout::eGeneral)
    // layout (binding = 2) uniform sampler2D iChannel1
    .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
    .image(*linearSampler, iChannel1.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)

    //-- descriptorSets[1]
    .beginDescriptorSet(descriptorSets[1])
    // layout (binding = 0) uniform Uniform
    .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
    .buffer(ubo.buffer(), 0, sizeof(Uniform))
    // layout (binding = 1) uniform sampler2D iChannel0
    .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
    .image(*linearSampler, iChannelPong.imageView(), vk::ImageLayout::eGeneral)
    // layout (binding = 2) uniform sampler2D iChannel1
    .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
    .image(*linearSampler, iChannel1.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)

    //-- update the descriptor sets with their pointers (but not data).
    .update(device);

  ////////////////////////////////////////
  //
  // Build the final pipeline

  vku::ShaderModule final_vert{device, BINARY_DIR "flockaroo.vert.spv"};
  vku::ShaderModule final_frag{device, BINARY_DIR "flockaroo.frag.spv"};

  vku::PipelineMaker pm{window.width(), window.height()};
  auto finalPipeline = pm
    .shader(vk::ShaderStageFlagBits::eVertex, final_vert)
    .shader(vk::ShaderStageFlagBits::eFragment, final_frag)
    .vertexBinding(0, sizeof(Vertex))
    .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
    .depthTestEnable(VK_TRUE)
    .cullMode(vk::CullModeFlagBits::eBack)
    .frontFace(vk::FrontFace::eClockwise)
    .viewport(viewport)
    .createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());

  ////////////////////////////////////////
  //
  // Build the advection pipeline
  // (added effort required to construct ping&pong output framebuffers) 

  vku::ShaderModule advection_vert{device, BINARY_DIR "flockaroo.vert.spv"};
  vku::ShaderModule advection_frag{device, BINARY_DIR "advection.frag.spv"};

  ////////////////////////////////////////
  //
  // Helper for building shared Advection render passes
  // that only differ by particular output attachment iChannelX.format() 
  //
  auto RenderPassAdvection = [&device](vk::Format iChannelXformat)->vk::UniqueRenderPass {
    // Build the renderpass writing to iChannelX 
    vku::RenderpassMaker rpm;
    return rpm
      // The only colour attachment.
     .attachmentBegin(iChannelXformat)
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

  // Build the Advection renderpass able to write to either iChannelPing or iChannelPong (depends on image format of both being identical)
  assert(vk::Format::eR32G32B32A32Sfloat == iChannelPing.format() && iChannelPing.format()==iChannelPong.format());
  vk::UniqueRenderPass advectionRenderPass = RenderPassAdvection(vk::Format::eR32G32B32A32Sfloat); 

  // Build the shared pipeline (ping&pong) for Advection renderpass.
  vku::PipelineMaker spm{advectionSize, advectionSize};
  auto advectionPipeline = spm
    .shader(vk::ShaderStageFlagBits::eVertex, advection_vert)
    .shader(vk::ShaderStageFlagBits::eFragment, advection_frag)
    .vertexBinding(0, sizeof(Vertex))
    .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
    .cullMode( vk::CullModeFlagBits::eBack )
    .frontFace( vk::FrontFace::eClockwise )
    .createUnique(device, fw.pipelineCache(), *pipelineLayout, *advectionRenderPass);

  ////////////////////////////////////////
  //
  // Build UniqueFramebuffers for Advection's ping&pong passes
  //
  // Render passes operate in conjunction with framebuffers. 
  // Framebuffers represent a collection of specific memory attachments that a render pass instance uses.

  auto UniqueFramebuffers = [&device, &advectionSize](vk::UniqueRenderPass &RenderPassX, vku::ColorAttachmentImage &iChannelX) -> vk::UniqueFramebuffer {
    // RenderPassX writes to output iChannelX
    vk::ImageView attachmentsPassX[] = {iChannelX.imageView()};
    vk::FramebufferCreateInfo fbciPassX{{}, *RenderPassX, sizeof(attachmentsPassX)/sizeof(vk::ImageView), attachmentsPassX, advectionSize, advectionSize, 1 };
    vk::UniqueFramebuffer FrameBufferPassX = device.createFramebufferUnique(fbciPassX);
    return FrameBufferPassX;
  };
  vk::UniqueFramebuffer advectionFrameBufferPing = UniqueFramebuffers( advectionRenderPass, iChannelPing );
  vk::UniqueFramebuffer advectionFrameBufferPong = UniqueFramebuffers( advectionRenderPass, iChannelPong );

  ////////////////////////////////////////
  //
  // Build a RenderPassBeginInfo for advection
  //

  // Match in order of attachments to clear the image.
  // kludge: Is there a better way to get really what's needed; sizeof(attachmentsP?ng)/sizeof(vk::ImageView)?
  vk::ImageView attachmentsPing[] = {iChannelPong.imageView()};
  std::array<vk::ClearValue, sizeof(attachmentsPing)/sizeof(vk::ImageView)> clearColours{
    vk::ClearColorValue{}
  };

  // when index=0 write to iChannelPong, when index=1 write to iChannelPing   
  vk::RenderPassBeginInfo advectionRpbi[]={{
    *advectionRenderPass,
    *advectionFrameBufferPong,
    vk::Rect2D{{0, 0}, {advectionSize, advectionSize}},
    (uint32_t) clearColours.size(),
    clearColours.data()
  },{
    *advectionRenderPass,
    *advectionFrameBufferPing,
    vk::Rect2D{{0, 0}, {advectionSize, advectionSize}},
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
            glm::vec4(advectionSize, advectionSize, 1., 0.),
            glm::vec4(advectionSize, advectionSize, 1., 0.),
            glm::vec4(advectionSize, advectionSize, 1., 0.),
            glm::vec4(advectionSize, advectionSize, 1., 0.)
          }
        };

        // Record the dynamic buffer.
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);

        // Copy the uniform data to the buffer. (note this is done
        // inline and so we can discard "unform" afterwards)
        cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform), &uniform);

        // vertex attributes common/shared by following render passes
        cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
        cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);

        // First renderpass. Compute advection.
        cb.beginRenderPass(advectionRpbi[iFrame%2], vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *advectionPipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, {descriptorSets[iFrame%2]}, {});
        cb.drawIndexed(indices.size(), 1, 0, 0, 0);
        cb.endRenderPass();

        // Second renderpass. Draw the final image.
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *finalPipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, {descriptorSets[(iFrame+1)%2]}, {});
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
