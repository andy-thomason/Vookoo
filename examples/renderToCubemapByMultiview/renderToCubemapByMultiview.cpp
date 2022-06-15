////////////////////////////////////////////////////////////////////////////////
//
// Vookoo example (C) 2017 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//
// multiview example rendering to 6 independent layers of cubmap (6 sides to cube)
// see https://blog.anishbhobe.site/vulkan-render-to-cubemaps-using-multiview/

// Include the demo framework, vookoo (vku) for building objects and glm for maths.
// The demo framework uses GLFW to create windows.
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/ext.hpp> // for rotate()

////////////////////////////////////////
//
// Mouse rotation of scene 
glm::mat4 mouse_rotation{1.0f}; 
static void mouse_callback(GLFWwindow* window, double x, double y) {
  static glm::vec3 previous_position{0.f};
  glm::vec3 position(float(x),float(y),0.0f);

  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
  if (state == GLFW_PRESS) {
    auto movement = position - previous_position;
    // pg.127 "Visualizing Quaternions"
    auto mouse_axis = glm::vec3(+movement.y, +movement.x, 0.0f);
    auto mouse_angle = glm::length(mouse_axis) / 250.f; // 250. arbitrary scale
    mouse_rotation = glm::rotate(mouse_angle, mouse_axis);
  } else {
    mouse_rotation = glm::mat4(1.0f);
  }
  previous_position = position;
}

int main() {
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  const char *title = "renderToCubemapByMultiview";
  auto glfwwindow = glfwCreateWindow(800, 800, title,  nullptr, nullptr);

  glfwSetCursorPosCallback(glfwwindow, mouse_callback);

  // Initialize makers
  vku::InstanceMaker im{};
  im.defaultLayers();
  vku::DeviceMaker dm{};
  dm.defaultLayers();

  // Initialise the Vookoo demo framework.
  vku::Framework fw{im, dm};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }

  // Get a device from the demo framework.
  vk::Device device = fw.device();

  // Create a window to draw into
  vku::Window window{fw.instance(), device, fw.physicalDevice(), fw.graphicsQueueFamilyIndex(), glfwwindow};
  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }
  // https://learnopengl.com/Getting-started/Coordinate-Systems
  // https://www.khronos.org/opengl/wiki/Face_Culling
  // https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
  // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
  // Note above miss fact that minDepth = 0.5f also needs to be set 
  // flip viewport to match opengl ( +x > Right, +y ^ UP, +z towards viewer from screen ), instead of vulkan default
  // also requires pipeline set with cullMode:BACK and frontFace:CounterClockWise
  auto viewport = vk::Viewport{
    0.0f,                                     //Vulkan default:0
    static_cast<float>(window.height()),      //Vulkan default:0
    static_cast<float>(window.width()),   //Vulkan default:width
    -static_cast<float>(window.height()),//Vulkan default:height
    0.5f,                              //Vulkan default:0
    1.0f                               //Vulkan default:1
  };

  // Vulkan clip space has inverted Y and half Z compared to OpenGL
  // meant to post multiply perspective clip operation i.e. as invertYhalfZclipspace*perspective*view*model
//  const glm::mat4 invertYhalfZclipspace(
//      1.0f,  0.0f, 0.0f, 0.0f,
//      0.0f, -1.0f, 0.0f, 0.0f,
//      0.0f,  0.0f, 0.5f, 0.0f,
//      0.0f,  0.0f, 0.5f, 1.0f);

  ////////////////////////////////////////
  //
  // Create Uniform Buffer (vertex shader)

  struct Uniform_vert {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 world;
  };
  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer ubo(device, fw.memprops(), sizeof(Uniform_vert));

  ////////////////////////////////////////
  //
  // Create Push Constant

  // Note! be very careful when using vec3, vec2, float and vec4 together
  // as there are alignment rules you must follow.
  struct PushConstant {
    glm::vec4 color[6]; // a unique color for each face (image layer) of the cubemap frame buffer object
  };

  PushConstant pushConstantTriangle {
    .color = {
      glm::vec4{1.0, 0, 0, 1}, // match vertex shader's gl_ViewIndex=0->cube face 0->(camera@+x +y-up lookat=0,0,0)
      glm::vec4{0.3, 0, 0, 1}, // match vertex shader's gl_ViewIndex=1->cube face 1->(camera@-x +y-up lookat=0,0,0)
      glm::vec4{0, 1.0, 0, 1}, // match vertex shader's gl_ViewIndex=2->cube face 2->(camera@+y -z-up lookat=0,0,0)
      glm::vec4{0, 0.3, 0, 1}, // match vertex shader's gl_ViewIndex=3->cube face 3->(camera@-y +z-up lookat=0,0,0)
      glm::vec4{0, 0, 1.0, 1}, // match vertex shader's gl_ViewIndex=4->cube face 4->(camera@+z +y-up lookat=0,0,0)
      glm::vec4{0, 0, 0.3, 1}, // match vertex shader's gl_ViewIndex=5->cube face 5->(camera@-z +y-up lookat=0,0,0)
    }
  };

  ////////////////////////////////////////
  //
  // Create Mesh Triangle

  struct VertexTriangle { 
    glm::vec4 pos;
  };

  const std::vector<VertexTriangle> verticesTriangle = {
    // counter clockwise winding (default openGL)
    {.pos={ 0.5f, 0.0f, 0.0f, 1.0f}},
    {.pos={ 0.0f, 1.0f, 0.0f, 1.0f}},
    {.pos={ 0.0f, 0.0f, 0.0f, 1.0f}},
  };
  vku::HostVertexBuffer vboTriangle(device, fw.memprops(), verticesTriangle);

  ////////////////////////////////////////
  //
  // Create Mesh Cube
 
  struct VertexCube { 
    glm::vec3 pos; 
  };

  const std::vector<VertexCube> verticesCube = {
    // front face
    {.pos={-1, +1, +1}},
    {.pos={+1, +1, +1}},
    {.pos={+1, -1, +1}},
    {.pos={-1, -1, +1}},
    // right face
    {.pos={+1, +1, +1}},
    {.pos={+1, +1, -1}},
    {.pos={+1, -1, -1}},
    {.pos={+1, -1, +1}},
    // back face
    {.pos={+1, +1, -1}},
    {.pos={-1, +1, -1}},
    {.pos={-1, -1, -1}},
    {.pos={+1, -1, -1}},
    // left face
    {.pos={-1, +1, -1}},
    {.pos={-1, +1, +1}},
    {.pos={-1, -1, +1}},
    {.pos={-1, -1, -1}},
    // top face
    {.pos={-1, +1, -1}},
    {.pos={+1, +1, -1}},
    {.pos={+1, +1, +1}},
    {.pos={-1, +1, +1}},
    // bottom face
    {.pos={-1, -1, -1}},
    {.pos={+1, -1, -1}},
    {.pos={+1, -1, +1}},
    {.pos={-1, -1, +1}},
  };
  vku::HostVertexBuffer vboCube(device, fw.memprops(), verticesCube);

  ////////////////////////////////////////
  //
  // Create mesh indices Cube

  std::vector<uint32_t> indicesCube = {
    // counter clockwise winding (default openGL)
     2, 1, 0,    2, 0, 3, // front face  +z
     6, 5, 4,    6, 4, 7, // right face  +x
    10, 9, 8,   10, 8,11, // back face   -z
    14,13,12,   14,12,15, // left face   -x
    18,17,16,   18,16,19, // top face    +y
    20,21,22,   23,20,22, // bottom face -y
  };
  vku::HostIndexBuffer iboCube(device, fw.memprops(), indicesCube);

  ////////////////////////////////////////
  //
  // Create a cubemap : cubeFbo

  vku::TextureImageCube cubeFbo{device, fw.memprops(), window.width(), window.height(), 1, vk::Format::eR8G8B8A8Unorm};
  assert(cubeFbo.info().arrayLayers==6); // renderToCubemapByMultiview.vert: extension GL_EXT_multiview depends on cubemap i.e expects 6 layers

  ////////////////////////////////////////
  //
  // Create Sampler with Linear filter and Nearest mipmap mode
 
  vku::SamplerMaker sm{};
  sm.magFilter( vk::Filter::eLinear )
    .minFilter( vk::Filter::eLinear )
    .mipmapMode( vk::SamplerMipmapMode::eNearest );
  vk::UniqueSampler SamplerLinearNearest = sm.createUnique(device);

  ////////////////////////////////////////
  //
  // Triangle shader
  // Memorize important data for later
  std::vector<vk::DescriptorSet> descriptorSetsTriangle;
  vk::UniquePipeline pipelineTriangle;
  vk::UniquePipelineLayout pipelineLayoutTriangle; 
  vk::UniqueRenderPass TriangleRenderPass;
  vk::UniqueFramebuffer TriangleFrameBuffer;
  vk::RenderPassBeginInfo TriangleRpbi;
  {
    ////////////////////////////////////////
    //
    // Build the descriptor sets
    vku::DescriptorSetLayoutMaker dslm{};
    auto descriptorSetLayout = dslm
      .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1)
      .createUnique(device);
  
    vku::DescriptorSetMaker dsm{};
    descriptorSetsTriangle = dsm
      .layout(*descriptorSetLayout)
      .create(device, fw.descriptorPool());

    ////////////////////////////////////////
    //
    // Update the descriptor sets for the shader uniforms.
  
    vku::DescriptorSetUpdater dsu;
    dsu.beginDescriptorSet(descriptorSetsTriangle[0])
       // Uniform_vert
       .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
       .buffer(ubo.buffer(), 0, sizeof(Uniform_vert))
       // update the descriptor sets with their pointers (but not data).
       .update(device);

    // Create two shaders, vertex and fragment.
    vku::ShaderModule vert{device, BINARY_DIR "renderToCubemapByMultiview.vert.spv"};
    vku::ShaderModule frag{device, BINARY_DIR "renderToCubemapByMultiview.frag.spv"};
  
    // Make a default pipeline layout.
    vku::PipelineLayoutMaker plm{};
    pipelineLayoutTriangle = plm
      .pushConstantRange(vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstant))
      .descriptorSetLayout(*descriptorSetLayout)
      .createUnique(device);

    ////////////////////////////////////////
    //
    // Enable Multi-View during this RenderPass
    // requires vertex shader has line "#extension GL_EXT_multiview : enable"
    // to be able to use gl_ViewIndex which is index 0,1...N-1 layers
    // see https://blog.anishbhobe.site/vulkan-render-to-cubemaps-using-multiview/
    //
    // Additional requirements to use multiview:
    // dm.extension(VK_KHR_MULTIVIEW_EXTENSION_NAME); must be added on line 110 of vku_framework.hpp
    // im.extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); added on line 67 of vku_framework.hpp
    // Next, need to pass vk::PhysicalDeviceMultiviewFeatures thru pNext chain of the Vk::DeviceCreateInfo
    // vku::DeviceMaker::createUnique added following near line 427 of vku.hpp 
    //   ...
    //     vk::PhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures;
    //     physicalDeviceMultiviewFeatures.setMultiview(true);
    //     dci.pNext = &physicalDeviceMultiviewFeatures;
    
    //  Bit mask that specifies which view the rendering is broadcast to
    //  e.g. 0011 = Broadcast to first and second view (layer)
    const uint32_t viewMask = 0b00111111; // 6 layers, one for each cube face
    //  Bit mask that specifices correlation between views
    //  An implementation may use this for optimizations (concurrent render)
    const uint32_t correlationMask = 0b00000000;
    
    vk::RenderPassMultiviewCreateInfo RenderPassMultiviewCreateInfo{};
    RenderPassMultiviewCreateInfo
      .setSubpassCount(1)
      .setPViewMasks(&viewMask)
      .setCorrelationMaskCount(1)
      .setPCorrelationMasks(&correlationMask);
  
    // Build the renderpass 
    vku::RenderpassMaker rpm;
    TriangleRenderPass = rpm
        // The only color attachment.
       .attachmentBegin(cubeFbo.format())
       .attachmentSamples(vk::SampleCountFlagBits::e1)
       .attachmentLoadOp(vk::AttachmentLoadOp::eClear)
       .attachmentStoreOp(vk::AttachmentStoreOp::eStore)
       .attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
       .attachmentStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
       .attachmentInitialLayout(vk::ImageLayout::eUndefined)
       .attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        // A subpass to render using the above attachment(s).
       .subpassBegin(vk::PipelineBindPoint::eGraphics)
       .subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0)
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
       .dependencySrcStageMask(vk::PipelineStageFlagBits::eFragmentShader)
       .dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
       .dependencySrcAccessMask(vk::AccessFlagBits::eShaderRead)
       .dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
       .dependencyDependencyFlags(vk::DependencyFlagBits::eByRegion)
        // dependency: If dstSubpass is equal to VK_SUBPASS_EXTERNAL, 
        // the second synchronization scope includes commands that occur later
        // in submission order than the vkCmdEndRenderPass used to end the 
        // render pass instance.
       .dependencyBegin(0, VK_SUBPASS_EXTERNAL)
       .dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
       .dependencyDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
       .dependencySrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
       .dependencyDstAccessMask(vk::AccessFlagBits::eShaderRead)
       .dependencyDependencyFlags(vk::DependencyFlagBits::eByRegion)
       // Use the maker object to construct the renderpass
       .createUnique(device, RenderPassMultiviewCreateInfo);
 
    // Make a pipeline to use the vertex format and shaders.
    vku::PipelineMaker pm{window.width(), window.height()};
    pipelineTriangle = pm
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(VertexTriangle))
      .vertexAttribute(0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(VertexTriangle, pos))
      .viewport(viewport) // viewport set to match openGL, affects cullMode and frontFace
      .frontFace(vk::FrontFace::eCounterClockwise) // VK is opposite (openGL default, GL_CCW face is front)
      .cullMode(vk::CullModeFlagBits::eBack) // openGL default, GL_BACK is the face to be culled
      .depthTestEnable( VK_TRUE ) 
      // Build the pipeline for this renderpass.
      .createUnique(device, fw.pipelineCache(), *pipelineLayoutTriangle, *TriangleRenderPass);

    ////////////////////////////////////////
    //
    // Build a RenderPassBeginInfo for Triangle
    //
  
    // Build the framebuffer.
    vk::ImageView attachments[] = {cubeFbo.imageView()};
    vk::FramebufferCreateInfo fbmi{{}, *TriangleRenderPass, sizeof(attachments)/sizeof(vk::ImageView), attachments, window.width(), window.height(), 1};
    TriangleFrameBuffer = device.createFramebufferUnique(fbmi);
  
    // Match in order of attachments to clear; the image.
    std::array<vk::ClearValue, sizeof(attachments)/sizeof(vk::ImageView)> clearColors{
      vk::ClearColorValue{std::array<float,4>{0.1, 0.2, 0.3, 1.0}}
    };
  
    // Begin rendering using the framebuffer and renderpass
    TriangleRpbi = vk::RenderPassBeginInfo{
      *TriangleRenderPass,
      *TriangleFrameBuffer,
      vk::Rect2D{{0, 0}, {window.width(), window.height()}},
      (uint32_t) clearColors.size(),
      clearColors.data()
    };

  }

  ////////////////////////////////////////
  //
  // Cube shader
  // Memorize important data for later
  std::vector<vk::DescriptorSet> descriptorSetsCube;
  vk::UniquePipeline pipelineCube;
  vk::UniquePipelineLayout pipelineLayoutCube; 
  //vk::UniqueRenderPass CubeRenderPass;
  //vk::UniqueFramebuffer CubeFrameBuffer;
  //vk::RenderPassBeginInfo CubeRpbi;
  {
    ////////////////////////////////////////
    //
    // Build the descriptor sets
    vku::DescriptorSetLayoutMaker dslm{};
    auto descriptorSetLayout = dslm
      .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1)
      .image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
      .createUnique(device);
  
    vku::DescriptorSetMaker dsm{};
    descriptorSetsCube = dsm
      .layout(*descriptorSetLayout)
      .create(device, fw.descriptorPool());

    ////////////////////////////////////////
    //
    // Update the descriptor sets for the shader uniforms.
  
    vku::DescriptorSetUpdater dsu;
    dsu.beginDescriptorSet(descriptorSetsCube[0])
       // Uniform_vert
       .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
       .buffer(ubo.buffer(), 0, sizeof(Uniform_vert))
       // samplerCube u_cubemap --> cubeFbo
       .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
       .image(*SamplerLinearNearest, cubeFbo.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
       // update the descriptor sets with their pointers (but not data).
       .update(device);

    // Create two shaders, vertex and fragment.
    vku::ShaderModule vert{device, BINARY_DIR "renderToCubemapByMultiviewPass2.vert.spv"};
    vku::ShaderModule frag{device, BINARY_DIR "renderToCubemapByMultiviewPass2.frag.spv"};
  
    // Make a default pipeline layout.
    vku::PipelineLayoutMaker plm{};
    pipelineLayoutCube = plm
      .descriptorSetLayout(*descriptorSetLayout)
      .createUnique(device);

    // Make a pipeline to use the vertex format and shaders.
    vku::PipelineMaker pm{window.width(), window.height()};
    pipelineCube = pm
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(VertexCube))
      .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(VertexCube, pos))
      .viewport(viewport) // viewport set to match openGL, affects cullMode and frontFace
      .frontFace(vk::FrontFace::eCounterClockwise) // VK is opposite (openGL default, GL_CCW face is front)
      .cullMode(vk::CullModeFlagBits::eBack) // openGL default, GL_BACK is the face to be culled
      .depthTestEnable( VK_TRUE )
      // Create a pipeline using a renderPass built for our window.
      .createUnique(device, fw.pipelineCache(), *pipelineLayoutCube, window.renderPass());
  }

  ////////////////////////////////////////
  //
  // Main update loop 

  Uniform_vert uniform_vert {
    .projection = /*invertYhalfZclipspace * */glm::perspective(
      glm::radians(30.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90° (extra wide) and 30° (quite zoomed in)
      float(window.width())/window.height(), // Aspect Ratio. Depends on the size of your window. Notice that 4/3 == 800/600 == 1280/960, sounds familiar ?
      0.1f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
      10.0f),              // Far clipping plane. Keep as little as possible.
    .view = glm::lookAt(
      glm::vec3(0,0,7),    // Camera location in World Space
      glm::vec3(0,0,0),    // Camera looks at the origin
      glm::vec3(0,1,0)),   // Head is up (set to 0,-1,0 to look upside-down)
    .world = glm::mat4(1.0),
  };

  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    window.draw(
      device, fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);

        cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform_vert), &uniform_vert);

        // Pass 1: Render to cubemap, each face is independent view determined by gl_ViewIndex in vertex shader
        cb.beginRenderPass(TriangleRpbi, vk::SubpassContents::eInline);
        
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineTriangle);
        cb.bindVertexBuffers(0, vboTriangle.buffer(), vk::DeviceSize(0));
        cb.pushConstants(
          *pipelineLayoutTriangle, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstant), &pushConstantTriangle
        );
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutTriangle, 0, descriptorSetsTriangle, nullptr);
        cb.draw(verticesTriangle.size(), 1, 0, 0);

        cb.endRenderPass();

        // Pass 2: Render cube to screen with cube map texture computed in prior pass
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);

        cb.bindVertexBuffers(0, vboCube.buffer(), vk::DeviceSize(0));
        cb.bindIndexBuffer(iboCube.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineCube);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutCube, 0, descriptorSetsCube, nullptr);
        cb.drawIndexed(indicesCube.size(), 1, 0, 0, 0);

        cb.endRenderPass();

        cb.end();
      }
    );

    uniform_vert.world = mouse_rotation * uniform_vert.world;

    // Very crude method to prevent your GPU from overheating.
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  // Wait until all drawing is done and then kill the window.
  device.waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
