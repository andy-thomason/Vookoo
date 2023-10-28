#include <vku/vku_framework.hpp>
#include <gilgamesh/mesh.hpp>
#include <gilgamesh/shapes/teapot.hpp>
#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

// Shader compilation constant values (constant_id).
VkBool32 useIntFactor = false;
float floatFactor = 1.0;
int intFactor = 1;

// Parse commandline arguments
void parse_args(int argc, char *argv[])
{
  if( argc == 3 && argv[1] == std::string{"-f"} ) {
    useIntFactor = false;
    floatFactor = static_cast<float>(std::atof(argv[2]));
  } else if ( argc == 3 && argv[1] == std::string{"-i"} ) {
    useIntFactor = true;
    intFactor = std::atoi(argv[2]);
  } else {
    std::cerr << "Usage: " << argv[0] << " [ -f <floatScale> | -i <intScale> ]" << std::endl;
    useIntFactor = false;
    floatFactor = 1.0;
  }
}

int main(int argc, char *argv[])
{
  parse_args(argc, &argv[0]);

  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "teapot";
  auto glfwwindow = glfwCreateWindow(800, 600, title, nullptr, nullptr);

  // Initialise the Vookoo demo framework.
  vku::Framework fw{title};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }

  // Create a window to draw into
  vku::Window window(
    fw.instance(),
    fw.device(),
    fw.physicalDevice(),
    fw.graphicsQueueFamilyIndex(),
    glfwwindow
  );
  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }

  ////////////////////////////////////////
  //
  // Create Uniform Buffer

  struct Uniform {
    glm::mat4 modelToPerspective;
    glm::mat4 modelToWorld;
    glm::mat4 normalToWorld;
    glm::mat4 modelToLight;
    glm::vec4 cameraPos;
    glm::vec4 lightPos;
  };
  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer ubo(fw.device(), fw.memprops(), sizeof(Uniform));

  ////////////////////////////////////////
  //
  // Create Mesh: vertices and indices

  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
  };
  std::vector<Vertex> vertices;

  gilgamesh::simple_mesh mesh;
  gilgamesh::teapot shape;
  shape.build(mesh);
  mesh.reindex(true);
  auto meshpos = mesh.pos();
  auto meshnormal = mesh.normal();
  auto meshuv = mesh.uv(0);

  for (size_t i = 0; i != meshpos.size(); ++i) {
    vertices.emplace_back(Vertex{.pos = meshpos[i], .normal = meshnormal[i], .uv = meshuv[i]});
  }
  std::vector<uint32_t> indices = mesh.indices32();
  uint32_t indexCount = indices.size();

  vku::HostVertexBuffer vbo(fw.device(), fw.memprops(), vertices);
  vku::HostIndexBuffer ibo(fw.device(), fw.memprops(), indices);

  ////////////////////////////////////////
  //
  // Create a cubemap

  // see: https://github.com/dariomanesku/cmft
  auto cubeBytes = vku::loadFile(SOURCE_DIR "examples/okretnica.ktx");
  vku::KTXFileLayout ktx(cubeBytes.data(), cubeBytes.data()+cubeBytes.size());
  if (!ktx.ok()) {
    std::cout << "Could not load KTX file" << std::endl;
    exit(1);
  }

  vku::GenericBuffer stagingBuffer(fw.device(), fw.memprops(), vk::BufferUsageFlagBits::eTransferSrc, cubeBytes.size(), vk::MemoryPropertyFlagBits::eHostVisible);
  stagingBuffer.updateLocal(fw.device(), (const void*)cubeBytes.data(), cubeBytes.size());

  cubeBytes = std::vector<uint8_t>{};

  vku::TextureImageCube cubeMap{fw.device(), fw.memprops(), ktx.width(0), ktx.height(0), ktx.mipLevels(), vk::Format::eR8G8B8A8Unorm};

  // Copy the staging buffer to the GPU texture and set the layout.
  vku::executeImmediately(fw.device(), window.commandPool(), fw.graphicsQueue(), [&](vk::CommandBuffer cb) {
    vk::Buffer buf = stagingBuffer.buffer();
    for (uint32_t mipLevel = 0; mipLevel != ktx.mipLevels(); ++mipLevel) {
      auto width = ktx.width(mipLevel);
      auto height = ktx.height(mipLevel);
      auto depth = ktx.depth(mipLevel);
      for (uint32_t face = 0; face != ktx.faces(); ++face) {
        cubeMap.copy(cb, buf, mipLevel, face, width, height, depth, ktx.offset(mipLevel, 0, face));
      }
    }
    cubeMap.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal);
  });

  // Free the staging buffer.
  stagingBuffer = vku::GenericBuffer{};

  ////////////////////////////////////////
  //
  // Create a depth buffer
  // This image is the depth buffer for the first pass 
  // and becomes the texture for the second pass

  uint32_t shadowSize = 512;
  vku::DepthStencilImage shadowImage(fw.device(), fw.memprops(), shadowSize, shadowSize);

  ////////////////////////////////////////
  //
  // model, camera world matrices
 
  // World matrices of model, camera and light
  glm::mat4 modelToWorld = glm::rotate(glm::mat4{1}, glm::radians(-90.0f), glm::vec3(1, 0, 0));
  glm::mat4 cameraToWorld = glm::translate(glm::mat4{1}, glm::vec3(0, 2, 8));
  glm::mat4 lightToWorld = glm::translate(glm::mat4{1}, glm::vec3(8, 6, 0));
  lightToWorld = glm::rotate(lightToWorld, glm::radians(90.0f), glm::vec3(0, 1, 0));
  lightToWorld = glm::rotate(lightToWorld, glm::radians(-30.0f), glm::vec3(1, 0, 0));

  // This matrix converts from OpenGL perspective to Vulkan perspective.
  // It flips the Y axis and shrinks the Z depth range to [min,max]=[0,1]
  // https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
  glm::mat4 leftHandCorrection(
    1.0f,  0.0f, 0.0f, 0.0f,
    0.0f, -1.0f, 0.0f, 0.0f,
    0.0f,  0.0f, 0.5f, 0.0f,
    0.0f,  0.0f, 0.5f, 1.0f
  );

  glm::mat4 cameraToPerspective = leftHandCorrection * glm::perspective(glm::radians(45.0f), (float)window.width()/window.height(), 1.0f, 100.0f);
  glm::mat4 lightToPerspective = leftHandCorrection * glm::perspective(glm::radians(30.0f), (float)shadowSize/shadowSize, 1.0f, 100.0f);

  bool lookFromLight = false;
  if (lookFromLight) {
    cameraToWorld = lightToWorld;
    cameraToPerspective = lightToPerspective;
  }

  glm::mat4 worldToCamera = glm::inverse(cameraToWorld);
  glm::mat4 worldToLight = glm::inverse(lightToWorld);

  ////////////////////////////////////////
  //
  // Build the descriptor sets

  // This pipeline layout is going to be shared amongst several pipelines.
  auto layout = vku::DescriptorSetLayoutMaker{}
    .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 1)
    .image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .createUnique(fw.device());

  auto descriptorSets = vku::DescriptorSetMaker{}
    .layout(*layout)
    .create(fw.device(), fw.descriptorPool());

  // This pipeline layout is shared amongst several pipelines.
  auto pipelineLayout = vku::PipelineLayoutMaker{}
    .descriptorSetLayout(*layout)
    .createUnique(fw.device());

  ////////////////////////////////////////
  //
  // Update the descriptor sets for the shader uniforms.

  // Create finalSampler
  auto finalSampler = vku::SamplerMaker{}
    .magFilter(vk::Filter::eLinear)
    .minFilter(vk::Filter::eLinear)
    .mipmapMode(vk::SamplerMipmapMode::eNearest)
    .createUnique(fw.device());

  // Create shadowSampler
  auto shadowSampler = vku::SamplerMaker{}
    .magFilter(vk::Filter::eNearest)
    .minFilter(vk::Filter::eNearest)
    .mipmapMode(vk::SamplerMipmapMode::eNearest)
    .createUnique(fw.device());

  vku::DescriptorSetUpdater dsu;
  dsu.beginDescriptorSet(descriptorSets[0])
    // Set initial uniform buffer value
    .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
    .buffer(ubo.buffer(), 0, sizeof(Uniform))
    // Set initial finalSampler value
    .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
    .image(*finalSampler, cubeMap.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
     // Set initial shadowSampler value
    .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
    .image(*shadowSampler, shadowImage.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
     // update the descriptor sets with their pointers (but not data).
    .update(fw.device());

  ////////////////////////////////////////
  //
  // Build the final pipeline including enabling the depth test

  vku::ShaderModule final_vert{fw.device(), BINARY_DIR "teapot.vert.spv"};
  vku::ShaderModule final_frag{fw.device(), BINARY_DIR "teapot.frag.spv"};

  auto buildCameraPipeline = [&]() {
    std::vector<vku::SpecConst> specList{
        {0, intFactor},
        {1, floatFactor},
        {3, useIntFactor}
    };
    vku::PipelineMaker pm{window.width(), window.height()};
    return pm
      .shader(vk::ShaderStageFlagBits::eVertex, final_vert, specList)
      .shader(vk::ShaderStageFlagBits::eFragment, final_frag)
      .vertexBinding(0, sizeof(Vertex))
      .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal))
      .vertexAttribute(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv))
      .depthTestEnable(VK_TRUE)
      .cullMode(vk::CullModeFlagBits::eBack)
      .frontFace(vk::FrontFace::eCounterClockwise)
      .createUnique(fw.device(), fw.pipelineCache(), *pipelineLayout, window.renderPass());
  };

  auto finalPipeline = buildCameraPipeline();

  ////////////////////////////////////////
  //
  // Build a pipeline for shadows

  vku::ShaderModule shadow_vert{fw.device(), BINARY_DIR "teapot.shadow.vert.spv"};
  vku::ShaderModule shadow_frag{fw.device(), BINARY_DIR "teapot.shadow.frag.spv"};

  // Build the renderpass using only depth/stencil.
  // The depth/stencil attachment.
  // Clear to 1.0f at the start (eClear)
  // Save the depth buffer at the end. (eStore)
  auto shadowRenderPass = vku::RenderpassMaker{}
  // The depth/stencil attachment.
     .attachmentBegin(shadowImage.format())
     .attachmentLoadOp(vk::AttachmentLoadOp::eClear)
     .attachmentStoreOp(vk::AttachmentStoreOp::eStore)
     .attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
     .attachmentInitialLayout(vk::ImageLayout::eUndefined)
     .attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)

  // A subpass to render using the above attachment.
     .subpassBegin(vk::PipelineBindPoint::eGraphics)
     .subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 0)

  // A dependency to reset the layout of the depth buffer to eUndefined.
  // eLateFragmentTests is the stage where the Z buffer is written.
     .dependencyBegin(VK_SUBPASS_EXTERNAL, 0)
     .dependencySrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
     .dependencyDstStageMask(vk::PipelineStageFlagBits::eLateFragmentTests)

  // A dependency to transition to eShaderReadOnlyOptimal.
     .dependencyBegin(0, VK_SUBPASS_EXTERNAL)
     .dependencySrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests)
     .dependencyDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)

  // Construct the renderpass
     .createUnique(fw.device());

  // Build the pipeline for this renderpass.
  auto shadowPipeline = vku::PipelineMaker{shadowSize, shadowSize}
     .shader(vk::ShaderStageFlagBits::eVertex, shadow_vert)
     .shader(vk::ShaderStageFlagBits::eFragment, shadow_frag)
     .vertexBinding(0, (uint32_t)sizeof(Vertex))
     .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, pos))
     .vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal))
     .vertexAttribute(2, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv))
     // Shadows render only to the depth buffer
     // Depth test is important.
     .depthTestEnable(VK_TRUE)
     .cullMode(vk::CullModeFlagBits::eBack)
     .frontFace(vk::FrontFace::eClockwise)
     // We will be using the depth bias dynamic state: cb.setDepthBias()
     .dynamicState(vk::DynamicState::eDepthBias)
     // Create the pipeline
     .createUnique(fw.device(), fw.pipelineCache(), *pipelineLayout, *shadowRenderPass, false);

  ////////////////////////////////////////
  //
  // Build a RenderPassBeginInfo for shadows
  //

  // Build the framebuffer.
  vk::ImageView attachments[] = {shadowImage.imageView()};
  vk::FramebufferCreateInfo fbci{{}, *shadowRenderPass, sizeof(attachments)/sizeof(vk::ImageView), attachments, shadowSize, shadowSize, 1 };
  vk::UniqueFramebuffer shadowFrameBuffer = fw.device().createFramebufferUnique(fbci);

  // Only one attachment to clear, the depth buffer.
  std::array<vk::ClearValue, sizeof(attachments)/sizeof(vk::ImageView)> clearColours{
    vk::ClearDepthStencilValue{1.0f, 0}
  };

  // Begin rendering using the framebuffer and renderpass
  auto shadowRpbi = vku::RenderPassBeginInfoMaker{}
    .renderPass(*shadowRenderPass)
    .framebuffer(*shadowFrameBuffer)
    .renderArea( vk::Rect2D{{0, 0}, {shadowSize, shadowSize}})
    .clearValueCount( (uint32_t) clearColours.size() )
    .pClearValues( clearColours.data() )
    .createUnique();

  // Depth bias pulls the depth buffer value away from the surface to prevent artifacts.
  // The bias changes as the triangle slopes away from the camera.
  // Values taken from Sasha Willems' shadowmapping example.
  const float depthBiasConstantFactor = 1.25f;
  const float depthBiasClamp = 0.0f;
  const float depthBiasSlopeFactor = 1.75f;

  // Specify shared bindDescriptorSets parameters
  uint32_t firstSet = 0;
  // Specify shared bindVertexBuffers parameters
  uint32_t firstBinding = 0;
  // Specify shared bindIndexBuffer parameters
  vk::DeviceSize indexOffset = 0;
  // Specify shared drawIndexed parameters
  uint32_t instanceCount = 1;
  uint32_t firstIndex = 0;
  int32_t  vertexOffset = 0;
  uint32_t firstInstance = 0;

  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    window.draw(fw.device(), fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {

        static auto ow = window.width();
        static auto oh = window.height();
        if (ow != window.width()) {
          ow = window.width();
          oh = window.height();
          cameraToPerspective =
              leftHandCorrection *
              glm::perspective(glm::radians(45.0f),
                                (float)ow / oh, 
                                1.0f, 100.0f);
          finalPipeline = buildCameraPipeline();
        }

        modelToWorld = glm::rotate(modelToWorld, glm::radians(1.0f), glm::vec3(0, 0, 1));

        Uniform uniform {
          .modelToPerspective = cameraToPerspective * worldToCamera * modelToWorld,
          .modelToWorld = modelToWorld,
          .normalToWorld = modelToWorld,
          .modelToLight = lightToPerspective * worldToLight * modelToWorld,
          .cameraPos = cameraToWorld[3],
          .lightPos = lightToWorld[3],
        };

        // Record the dynamic buffer.
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);

        // Copy the uniform data to the buffer. (note this is done
        // inline and so we can discard "unform" afterwards)
        cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform), &uniform);

        // First renderpass. Draw the shadow.
        cb.beginRenderPass(shadowRpbi, vk::SubpassContents::eInline);
        cb.setDepthBias(depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *shadowPipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, firstSet, descriptorSets, nullptr);
        cb.bindVertexBuffers(firstBinding, vbo.buffer(), vk::DeviceSize(nullptr));
        cb.bindIndexBuffer(ibo.buffer(), indexOffset, vk::IndexType::eUint32);
        cb.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        cb.endRenderPass();

        // Second renderpass. Draw the final image.
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *finalPipeline);
        // *** redundant since already set in previous render pass (identical calls) ***
        //cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, firstSet, descriptorSets, nullptr);
        //cb.bindVertexBuffers(firstBinding, vbo.buffer(), vk::DeviceSize(nullptr));
        //cb.bindIndexBuffer(ibo.buffer(), indexOffset, vk::IndexType::eUint32);
        cb.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        cb.endRenderPass();

        cb.end();
      }
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  fw.device().waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
