////////////////////////////////////////////////////////////////////////////////
//
// Vookoo subpass example (C) 2017 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//
// Demonstrates usage of vulkan subpasses
// original: https://github.com/lerpingfx/vulkan_subpasses
// modified to utilize vookoo

// Include the demo framework, vookoo (vku) for building objects and glm for maths.
// The demo framework uses GLFW to create windows.
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/ext.hpp> // for glm::rotate(angle,axis)
#include "icosphereGenerator.hpp"
#include <algorithm> // std::generate
#include <tuple>

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

////////////////////////////////////////
//
// 
int main() {
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  auto *title = "subpass";
  auto glfwwindow = glfwCreateWindow(1920, 1080, title, nullptr, nullptr);

  glfwSetCursorPosCallback(glfwwindow, mouse_callback);

  // Initialise the Vookoo demo framework.
  vku::Framework fw{title};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }

  // Get a device from the demo framework.
  auto device = fw.device();

  // Create a window to draw into
  vku::Window window(
    fw.instance(),
    device,
    fw.physicalDevice(),
    fw.graphicsQueueFamilyIndex(),
    glfwwindow,
    vk::Format::eB8G8R8A8Srgb // swapChainImageFormat chosen for High Dynamic Range
  );
  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }
  auto swapchainImageCount = window.framebuffers().size();

  auto viewportGLDefault = [](int window_width, int window_height) {
    // https://learnopengl.com/Getting-started/Coordinate-Systems
    // https://www.khronos.org/opengl/wiki/Face_Culling
    // https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
    // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
    // Note above miss fact that minDepth = 0.5f also needs to be set
    // flip viewport to match opengl ( +x > Right, +y ^ UP, +z towards viewer from screen ), instead of vulkan default
    // Or alternatively, use GLM_FORCE_DEPTH_ZERO_TO_ONE with minDepth = 0.0f instead.
    // Requires pipeline set with cullMode:BACK and frontFace:CounterClockWise
    // viewport affects cullMode and frontFace depending on Vulkan or OpenGL coordinate convention
    //   Vulkan default:eFront       OpenGL default:eBack 
    //   Vulkan default:eClockwise   OpenGL default:eCounterClockwise
    return vku::ViewPortMaker{}
      .x(0.0f)                                    //Vulkan default:0       OpenGL default:0
      .y(static_cast<float>(window_height))       //Vulkan default:0       OpenGL default:height
      .width(static_cast<float>(window_width))    //Vulkan default:width   OpenGL default:width
      .height(-static_cast<float>(window_height)) //Vulkan default:height  OpenGL default:-height
      .minDepth(0.5f)                             //Vulkan default:0       OpenGL default:0.5
      .maxDepth(1.0f)                             //Vulkan default:1       OpenGL default:1
      .createUnique();
  };

  ////////////////////////////////////////
  //
  // Define vertex structure  
  //
  struct Vertex { 
    glm::vec3 pos; 
    glm::vec3 color; 
    glm::vec2 UV; 
    glm::vec3 normal;
  };

  ////////////////////////////////////////
  //
  // Define unit Plane 
  //
  std::vector<Vertex> verticesPlane = {
    // pos, color, uv, normal
    {{-1.00f,-1.00f, 0.00f}, {0.00f, 0.00f, 0.00f}, {0.00f, 1.00f}, {0.00f, 0.00f, 1.00f}},
    {{ 1.00f,-1.00f, 0.00f}, {0.00f, 0.00f, 0.00f}, {1.00f, 1.00f}, {0.00f, 0.00f, 1.00f}},
    {{ 1.00f, 1.00f, 0.00f}, {0.00f, 0.00f, 0.00f}, {1.00f, 0.00f}, {0.00f, 0.00f, 1.00f}},
    {{-1.00f, 1.00f, 0.00f}, {0.00f, 0.00f, 0.00f}, {0.00f, 0.00f}, {0.00f, 0.00f, 1.00f}},
  };
  std::vector<unsigned int> indicesPlane = {
    0, 1, 2,    2, 3, 0,
  };

  // Sync mesh vertices with GPU
  vku::HostVertexBuffer bufferVerticesPlane(fw.device(), fw.memprops(), verticesPlane);

  // Sync indices with GPU
  vku::HostIndexBuffer iboPlane(fw.device(), fw.memprops(), indicesPlane);

  ////////////////////////////////////////
  //
  // Define unit cube 
  //
  std::vector<Vertex> verticesCube = {
    // pos,      color,   uv,          normal
    {{-1,-1,-1},{0,0,1},{0.375,0   },{-1, 0, 0}},
    {{-1,-1, 1},{0,0,1},{0.625,0   },{-1, 0, 0}},
    {{-1, 1, 1},{0,0,1},{0.625,0.25},{-1, 0, 0}},
    {{-1, 1,-1},{0,0,1},{0.375,0.25},{-1, 0, 0}},
    {{-1, 1,-1},{0,0,1},{0.375,0.25},{ 0, 1, 0}},
    {{-1, 1, 1},{0,0,1},{0.625,0.25},{ 0, 1, 0}},
    {{ 1, 1, 1},{0,0,1},{0.625,0.5 },{ 0, 1, 0}},
    {{ 1, 1,-1},{0,0,1},{0.375,0.5 },{ 0, 1, 0}},
    {{ 1, 1,-1},{0,0,1},{0.375,0.5 },{ 1, 0, 0}},
    {{ 1, 1, 1},{0,0,1},{0.625,0.5 },{ 1, 0, 0}},
    {{ 1,-1, 1},{0,0,1},{0.625,0.75},{ 1, 0, 0}},
    {{ 1,-1,-1},{0,0,1},{0.375,0.75},{ 1, 0, 0}},
    {{ 1,-1,-1},{0,0,1},{0.375,0.75},{ 0,-1, 0}},
    {{ 1,-1, 1},{0,0,1},{0.625,0.75},{ 0,-1, 0}},
    {{-1,-1, 1},{0,0,1},{0.625,1   },{ 0,-1, 0}},
    {{-1,-1,-1},{0,0,1},{0.375,1   },{ 0,-1, 0}},
    {{-1, 1,-1},{0,0,1},{0.125,0.5 },{ 0, 0,-1}},
    {{ 1, 1,-1},{0,0,1},{0.375,0.5 },{ 0, 0,-1}},
    {{ 1,-1,-1},{0,0,1},{0.375,0.75},{ 0, 0,-1}},
    {{-1,-1,-1},{0,0,1},{0.125,0.75},{ 0, 0,-1}},
    {{ 1, 1, 1},{0,0,1},{0.625,0.5 },{ 0, 0, 1}},
    {{-1, 1, 1},{0,0,1},{0.875,0.5 },{ 0, 0, 1}},
    {{-1,-1, 1},{0,0,1},{0.875,0.75},{ 0, 0, 1}},
    {{ 1,-1, 1},{0,0,1},{0.625,0.75},{ 0, 0, 1}}
  };
  std::vector<unsigned int> indicesCube = {
     0,  1,  2,      2,  3,  0,
     4,  5,  6,      6,  7,  4,
     8,  9, 10,     10, 11,  8,
    12, 13, 14,     14, 15, 12,
    16, 17, 18,     18, 19, 16,
    20, 21, 22,     22, 23, 20,
  };

  // Sync mesh vertices with GPU
  vku::HostVertexBuffer bufferVerticesCube(fw.device(), fw.memprops(), verticesCube);

  // Sync indices with GPU
  vku::HostIndexBuffer iboCube(fw.device(), fw.memprops(), indicesCube);

  ////////////////////////////////////////
  //
  // Define Icosphere 
  // Part 1
  //
  std::vector<float> verticesG;
  std::vector<unsigned int> indicesSphere;
  generateIcosphere(&verticesG, &indicesSphere, 4, true); // 4-->2048 Triangles, true-->Counter Clockwise Winding

  ////////////////////////////////////////
  //
  // Define Icosphere 
  // Part 2
  // (Icosphere, but transformed to be convenient/matched with struct Vertex)
  //
  // build mesh verticesSphere (also given position derive normal vector)
  std::vector<Vertex> verticesSphere( verticesG.size()/3 );
  std::generate(verticesSphere.begin(), verticesSphere.end(), [&verticesG, i=0] () mutable { 
    glm::vec3 n = {verticesG[i*3], verticesG[i*3+1], verticesG[i*3+2]}; // for sphere, normal is same as unit radial/position vector
    n = glm::normalize(n);
    i++;
    return Vertex{
      .pos    = 0.74*n, // shaders, as written, expect a sphere with radius of 0.74
      .color  = glm::vec3(0.0f,0.0f,1.0f),
      .UV     = glm::vec2(n.x, n.y), // !!! KNOWN INCORRECT, MUST FIX generateIcosphere to generate UV coords.
      .normal = n,   // when sphere position is normalized, position same as unit normal vector
    };
  });

  // Sync mesh vertices with GPU
  vku::HostVertexBuffer bufferVerticesSphere(fw.device(), fw.memprops(), verticesSphere);

  // Sync indices with GPU
  vku::HostIndexBuffer iboSphere(fw.device(), fw.memprops(), indicesSphere);

  ////////////////////////////////////////
  //
  // Define scene's background floor as multiple instances of translated and scaled cubes

  // Per-instance data block
  struct Instance {
    glm::mat4 model;
  };

  // Build multiple instances representing the scene.
  std::vector<Instance> instancesScene = {
    {.model = glm::translate(glm::vec3( 0.0f,    0.7202f,-0.5f   )) * glm::scale(glm::vec3(10.0f,10.0f,0.5f ))},
    {.model = glm::translate(glm::vec3(-4.5538f, 4.3425f,-0.4398f)) * glm::scale(glm::vec3( 4.5f, 1.0f,1.0f ))},
    {.model = glm::translate(glm::vec3(-6.1894f,-1.2283f,-0.4393f)) * glm::scale(glm::vec3( 5.4f, 0.8f,0.68f))},
    {.model = glm::translate(glm::vec3( 5.2429f,-3.7440f,-0.3f   )) * glm::scale(glm::vec3( 4.6f, 4.9f,0.7f ))},
  };

  vku::HostVertexBuffer bufferInstancesScene(fw.device(), fw.memprops(), instancesScene);

  // Build multiple instances representing the Fx (Sphere+Cube).
  std::vector<Instance> instancesFx = {
    {.model = glm::translate(glm::vec3( 0.0f,    0.0f,    0.0f   )) * glm::scale(glm::vec3( 2.0f, 2.0f,2.0f ))},
  };

  vku::HostVertexBuffer bufferInstancesFx(fw.device(), fw.memprops(), instancesFx);

  ////////////////////////////////////////
  //
  // Create shaders.
  //
  vku::ShaderModule vert_0_scene      {device, BINARY_DIR "subpass_0_scene.vert.spv"};
  vku::ShaderModule frag_0_scene      {device, BINARY_DIR "subpass_0_scene.frag.spv"};
  vku::ShaderModule vert_1_composition{device, BINARY_DIR "subpass_1_composition.vert.spv"};
  vku::ShaderModule frag_1_composition{device, BINARY_DIR "subpass_1_composition.frag.spv"};
  vku::ShaderModule vert_2_fx         {device, BINARY_DIR "subpass_2_fx.vert.spv"};
  vku::ShaderModule frag_2_fx         {device, BINARY_DIR "subpass_2_fx.frag.spv"};
  vku::ShaderModule vert_3_decal      {device, BINARY_DIR "subpass_3_decal.vert.spv"};
  vku::ShaderModule frag_3_decal      {device, BINARY_DIR "subpass_3_decal.frag.spv"};

  ////////////////////////////////////////
  //
  // Create Uniform Buffers.
  //

  // These are the parameters we are passing to the shaders
  // Note! be careful when using vec3, vec2, float and vec4 together
  // as there are alignment rules you must follow.
  struct UniformBufferObjectScene {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
  };

  struct UniformBufferObjectFx {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec2 res;
    glm::vec2 padding; // extra padding to 64byte boundary
  };

  UniformBufferObjectScene uDataScene =  {
    .model = glm::rotate(glm::radians(-90.0f), glm::vec3(1,0,0)) * glm::rotate(glm::radians(-180.0f), glm::vec3(0,0,1)) * glm::mat4(1.0f),
    .view = glm::lookAt(
      glm::vec3(0,1,4), // Camera location in World Space
      glm::vec3(0,0,1),    // Camera looks at
      glm::vec3(0,1,0)),   // Head is up
    .proj = glm::perspective(
      glm::radians(45.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90° (extra wide) and 30° (quite zoomed in)
      float(window.width())/window.height(), // Aspect Ratio. Depends on the size of your window. Notice that 4/3 == 800/600 == 1280/960, sounds familiar ?
      0.001f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
      10.0f),              // Far clipping plane. Keep as little as possible.
  };

  UniformBufferObjectFx uDataFx = {
    .model = uDataScene.model,
    .view  = uDataScene.view,
    .proj  = uDataScene.proj,
    .res   = glm::vec2(window.width(), window.height()),
  };

  // Create a uniform buffer(s).
  // We cannot update these buffers with normal memory writes
  // because reading the buffer may happen at any time.
  std::vector<vku::UniformBuffer> uboScene;
  std::vector<vku::UniformBuffer> uboFx;
  for (int i = 0; i < swapchainImageCount; i++) {
    // One uniform buffer per swap chain image
    uboScene.push_back({device, fw.memprops(), sizeof(UniformBufferObjectScene)});
    uboFx.push_back(   {device, fw.memprops(), sizeof(UniformBufferObjectFx)});
  }

  ////////////////////////////////////////////////////////
  //
  // Create color and depth attachment images
  // Note, during window creation, the swapChanImageFormat chosen to be
  // vk::Format::eB8G8R8A8Srgb for HDR rendering. The following image formats chosen to match HDR usage.

  // 32bit-per-channel floating point attachment (to store values higher than 1.0) for HDR rendering
  // (tonemapping applied in subpass 0, before output to swapchain image in subpass 1)
  auto colorAttachmentImage = vku::ColorAttachmentImage{device, fw.memprops(), window.width(), window.height(), vk::Format::eR32G32B32A32Sfloat};
  auto depthAttachmentImage = vku::DepthStencilImage{device, fw.memprops(), window.width(), window.height(), vk::Format::eD32SfloatS8Uint};

  ////////////////////////////////////////////////////////
  //
  // Create Sampler(s)
 
  auto linearSampler = vku::SamplerMaker{}
    .magFilter( vk::Filter::eLinear )
    .minFilter( vk::Filter::eLinear )
    .mipmapMode( vk::SamplerMipmapMode::eNearest )
    .addressModeU( vk::SamplerAddressMode::eRepeat )
    .addressModeV( vk::SamplerAddressMode::eRepeat )
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Helper for building render passes
  // that only differ by particular output attachments
  //
  auto RenderPass = [&]()->vk::UniqueRenderPass {
    // Build the renderpass writing to iChannelX 
    return vku::RenderpassMaker{}
      // The color attachment.
     .attachmentBegin(colorAttachmentImage.format())
     .attachmentSamples(vk::SampleCountFlagBits::e1)
     .attachmentLoadOp(vk::AttachmentLoadOp::eClear)
     .attachmentStoreOp(vk::AttachmentStoreOp::eStore)
     .attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
     .attachmentStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
     .attachmentInitialLayout(vk::ImageLayout::eUndefined)
     .attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
      // The depth attachment.
     .attachmentBegin(depthAttachmentImage.format())
     .attachmentSamples(vk::SampleCountFlagBits::e1)
     .attachmentLoadOp(vk::AttachmentLoadOp::eClear)
     .attachmentStoreOp(vk::AttachmentStoreOp::eStore)
     .attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
     .attachmentStencilStoreOp(vk::AttachmentStoreOp::eStore)
     .attachmentInitialLayout(vk::ImageLayout::eUndefined)
     .attachmentFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal)
      // The swapChain attachment.
     .attachmentBegin(window.swapchainImageFormat())
     .attachmentSamples(vk::SampleCountFlagBits::e1)
     .attachmentLoadOp(vk::AttachmentLoadOp::eClear)
     .attachmentStoreOp(vk::AttachmentStoreOp::eStore)
     .attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
     .attachmentStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
     .attachmentInitialLayout(vk::ImageLayout::eUndefined)
     .attachmentFinalLayout(vk::ImageLayout::ePresentSrcKHR)
      // subpass 0
     .subpassBegin(vk::PipelineBindPoint::eGraphics)
     .subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0)
     .subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal,1)
      // subpass 1
     .subpassBegin(vk::PipelineBindPoint::eGraphics)
     .subpassInputAttachment(vk::ImageLayout::eShaderReadOnlyOptimal, 0)
     .subpassInputAttachment(vk::ImageLayout::eDepthStencilReadOnlyOptimal, 1)
     .subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilReadOnlyOptimal,1)
     .subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 2)
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
      //  [ ]FRAGMENT_SHADER
      //  [ ]LATE_FRAGMENT_TESTS
      //  [ ]COLOR_ATTACHMENT_OUTPUT
      //  [S]BOTTOM_OF_PIPE ----------------------------------------------
      //
      //  [ ]TOP_OF_PIPE -------------------------------------------------
      //  [ ]DRAW_INDIRECT
      //  [ ]VERTEX_INPUT
      //  [ ]VERTEX_SHADER
      //  [ ]TESSELLATION_CONTROL_SHADER
      //  [ ]TESSELLATION_EVALUATION_SHADER  [ ]COMPUTE_SHADER  [ ]TRANSFER
      //  [ ]GEOMETRY_SHADER
      //  [D]EARLY_FRAGMENT_TESTS
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
     //VK_ACCESS_MEMORY_READ_BIT 
     //VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
     .dependencySrcAccessMask(vk::AccessFlagBits::eMemoryRead)
     .dependencySrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
     //VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT 
     //VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
     .dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite|vk::AccessFlagBits::eDepthStencilAttachmentWrite)
     .dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eEarlyFragmentTests)
     .dependencyDependencyFlags(vk::DependencyFlagBits::eByRegion)

     .dependencyBegin(0, 1)
     //VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT 
     //VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
     .dependencySrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite|vk::AccessFlagBits::eDepthStencilAttachmentWrite)
     .dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eEarlyFragmentTests)
     //VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
     //VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
     .dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite|vk::AccessFlagBits::eInputAttachmentRead)
     .dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eFragmentShader)
     .dependencyDependencyFlags(vk::DependencyFlagBits::eByRegion)

      // dependency: If dstSubpass is equal to VK_SUBPASS_EXTERNAL, 
      // the second synchronization scope includes commands that occur later
      // in submission order than the vkCmdEndRenderPass used to end the 
      // render pass instance.
     .dependencyBegin(1, VK_SUBPASS_EXTERNAL)
     //VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
     //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
     .dependencySrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
     .dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
     //VK_ACCESS_MEMORY_READ_BIT
     //VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
     .dependencyDstAccessMask(vk::AccessFlagBits::eMemoryRead)
     .dependencyDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
     .dependencyDependencyFlags(vk::DependencyFlagBits::eByRegion)

     // Finally use the maker method to construct this renderpass
     .createUnique(device);
  };
  auto myRenderPass = RenderPass();

  ////////////////////////////////////////
  //
  // Build UniqueFramebuffers
  //
  // Render passes operate in conjunction with framebuffers. 
  // Framebuffers represent a collection of specific memory attachments that a render pass instance uses.

  auto UniqueFramebuffers = [&](int imageIndex) -> vk::UniqueFramebuffer {
    // RenderPass writes to multiple output imageViews 
    vk::ImageView attachmentsPassX[] = {
      colorAttachmentImage.imageView(),
      depthAttachmentImage.imageView(),
      window.imageViews()[imageIndex]
    };
    vk::FramebufferCreateInfo fbciPassX{{}, *myRenderPass, sizeof(attachmentsPassX)/sizeof(vk::ImageView), attachmentsPassX, window.width(), window.height(), 1 };
    vk::UniqueFramebuffer FrameBufferPassX = device.createFramebufferUnique(fbciPassX);
    return FrameBufferPassX;
  };
  vk::UniqueFramebuffer myFrameBufferPass[] = {
    //TODO: replace with for-loop over number of swap chain images
    UniqueFramebuffers(0),
    UniqueFramebuffers(1),
    UniqueFramebuffers(2)
  };

  // Match in order of attachments to clear the imageviews.
  // 0  colorAttachmentImage.imageView()
  // 1  depthAttachmentImage.imageView()
  // 2  window.swapchain[imageIndex] 
  std::vector<vk::ClearValue> clearValues {
    vk::ClearValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}},
    vk::ClearDepthStencilValue{1.0f, 0},
    vk::ClearValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}},
  };

  ////////////////////////////////////////
  //
  // Build a RenderPassBeginInfo
  // sets the framebuffer and renderpass

  // One per swapChainImage
  vk::RenderPassBeginInfo myRpbi[]={
    //TODO: replace with for-loop over number of swap chain images
    vku::RenderPassBeginInfoMaker{}
      .renderPass( *myRenderPass )
      .framebuffer( *myFrameBufferPass[0] )
      .renderArea( vk::Rect2D{{0, 0}, {window.width(), window.height()}} )
      .clearValueCount( (uint32_t) clearValues.size() )
      .pClearValues( clearValues.data() )
      .createUnique(),
    vku::RenderPassBeginInfoMaker{}
      .renderPass( *myRenderPass )
      .framebuffer( *myFrameBufferPass[1] )
      .renderArea( vk::Rect2D{{0, 0}, {window.width(), window.height()}} )
      .clearValueCount( (uint32_t) clearValues.size() )
      .pClearValues( clearValues.data() )
      .createUnique(),
    vku::RenderPassBeginInfoMaker{}
      .renderPass( *myRenderPass )
      .framebuffer( *myFrameBufferPass[2] )
      .renderArea( vk::Rect2D{{0, 0}, {window.width(), window.height()}} )
      .clearValueCount( (uint32_t) clearValues.size() )
      .pClearValues( clearValues.data() )
      .createUnique()
    };


  ////////////////////////////////////////
  //
  // Create pipelines.
  //

  // Build a template for descriptor sets that use these shaders.
  auto descriptorSetLayoutScene = vku::DescriptorSetLayoutMaker{}
    .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll, 1)
    .createUnique(device);

  // Create descriptor set(s) to tell the shader where our buffers are.
  // One set per swap chain image.
  auto descriptorSetMakerScene = vku::DescriptorSetMaker{};
  for (int i = 0; i < swapchainImageCount; i++) {
    descriptorSetMakerScene.layout(*descriptorSetLayoutScene);
  }
  auto descriptorSetsScene = descriptorSetMakerScene.create(device, fw.descriptorPool());
  
  // Update the descriptor sets with the uniform buffer information.
  auto descriptorSetUpdaterScene = vku::DescriptorSetUpdater{};
  for (int i = 0; i < swapchainImageCount; i++) {
    descriptorSetUpdaterScene
      .beginDescriptorSet(descriptorSetsScene[i])
      .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
      .buffer(uboScene[i].buffer(), 0*sizeof(UniformBufferObjectScene), 1*sizeof(UniformBufferObjectScene));
  }
  descriptorSetUpdaterScene.update(device);

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  auto pipelineLayoutScene = vku::PipelineLayoutMaker{}
    .descriptorSetLayout(*descriptorSetLayoutScene)
    .createUnique(device);

  auto buildPipelineScene = [&]() {
    // Make a pipeline to use the vertex format and shaders.
    return vku::PipelineMaker{ window.width(), window.height() }
      .subPass(0)
      .shader(vk::ShaderStageFlagBits::eVertex, vert_0_scene)
      .shader(vk::ShaderStageFlagBits::eFragment, frag_0_scene)
      #define VERTEX_BUFFER_BIND_ID 0
      .vertexBinding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), vk::VertexInputRate::eVertex)
      .vertexAttribute(0, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))
      .vertexAttribute(2, VERTEX_BUFFER_BIND_ID,    vk::Format::eR32G32Sfloat, offsetof(Vertex, UV))
      .vertexAttribute(3, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal))
      #define INSTANCE_BUFFER_BIND_ID 1
      .vertexBinding(INSTANCE_BUFFER_BIND_ID, sizeof(Instance), vk::VertexInputRate::eInstance)
      .vertexAttribute(4, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model))
      .vertexAttribute(5, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model)+sizeof(float) * 4)
      .vertexAttribute(6, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model)+sizeof(float) * 8)
      .vertexAttribute(7, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model)+sizeof(float) * 12)
      //
      .viewport(viewportGLDefault(window.width(), window.height())) // viewport affects cullMode and frontFace depending on Vulkan or OpenGL coordinate convention
      .cullMode(vk::CullModeFlagBits::eBack)       // Vulkan default:eFront       OpenGL default:eBack 
      .frontFace(vk::FrontFace::eCounterClockwise) // Vulkan default:eClockwise   OpenGL default:eCounterClockwise
      .depthTestEnable(VK_TRUE)
      .depthWriteEnable(VK_TRUE)
      .depthCompareOp(vk::CompareOp::eLess)
      .blendBegin(false)
      .logicOpEnable(false)
      .logicOp(vk::LogicOp::eCopy)
      .blendConstants(0.0f,0.0f,0.0f,0.0f)
      //
      .createUnique(device, fw.pipelineCache(), *pipelineLayoutScene, *myRenderPass);
  };
  auto pipelineScene = buildPipelineScene();

  ////////////////////////////////////////
  //
  // Create pipelines.
  //

  // Build a template for descriptor sets that use these shaders.
  auto descriptorSetLayoutComp = vku::DescriptorSetLayoutMaker{}
    .image(0, vk::DescriptorType::eInputAttachment, vk::ShaderStageFlagBits::eFragment, 1)
    .createUnique(device);

  // Create descriptor set(s) to tell the shader where our buffers are.
  // One set per swap chain image.
  auto descriptorSetMakerComp = vku::DescriptorSetMaker{};
  for (int i = 0; i < swapchainImageCount; i++) {
    descriptorSetMakerComp.layout(*descriptorSetLayoutComp);
  }
  auto descriptorSetsComp = descriptorSetMakerComp.create(device, fw.descriptorPool());
  
  // Update the descriptor sets with the uniform buffer information.
  auto descriptorSetUpdaterComp = vku::DescriptorSetUpdater{};
  for (int i = 0; i < swapchainImageCount; i++) {
    descriptorSetUpdaterComp
      .beginDescriptorSet(descriptorSetsComp[i])
      .beginImages(0, 0, vk::DescriptorType::eInputAttachment)
      .image(*linearSampler, colorAttachmentImage.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
  }
  descriptorSetUpdaterComp.update(device);

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  auto pipelineLayoutComp = vku::PipelineLayoutMaker{}
    .descriptorSetLayout(*descriptorSetLayoutComp)
    .createUnique(device);

  auto buildPipelineComp = [&]() {
    // Make a pipeline to use the vertex format and shaders.
    return vku::PipelineMaker{ window.width(), window.height() }
      .subPass(1)
      .shader(vk::ShaderStageFlagBits::eVertex, vert_1_composition)
      .shader(vk::ShaderStageFlagBits::eFragment, frag_1_composition)
      #define VERTEX_BUFFER_BIND_ID 0
      .vertexBinding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), vk::VertexInputRate::eVertex)
      .vertexAttribute(0, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))
      .vertexAttribute(2, VERTEX_BUFFER_BIND_ID,    vk::Format::eR32G32Sfloat, offsetof(Vertex, UV))
      .vertexAttribute(3, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal))
      //
      .viewport(viewportGLDefault(window.width(), window.height())) // viewport affects cullMode and frontFace depending on Vulkan or OpenGL coordinate convention
      .cullMode(vk::CullModeFlagBits::eBack)       // Vulkan default:eFront       OpenGL default:eBack 
      .frontFace(vk::FrontFace::eCounterClockwise) // Vulkan default:eClockwise   OpenGL default:eCounterClockwise
      .depthTestEnable(VK_FALSE)
      .depthWriteEnable(VK_FALSE)
      .depthCompareOp(vk::CompareOp::eLess)
      .blendBegin(true)
      .blendSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      .blendDstAlphaBlendFactor(vk::BlendFactor::eZero)
      .logicOpEnable(false)
      .logicOp(vk::LogicOp::eCopy)
      .blendConstants(0.0f,0.0f,0.0f,0.0f)
      //
      .createUnique(device, fw.pipelineCache(), *pipelineLayoutComp, *myRenderPass);
  };
  auto pipelineComp = buildPipelineComp();

  ////////////////////////////////////////
  //
  // Create pipelines.
  //

  /////////////////////////////////
  // Descriptor set 0 (shared Fx and Decal)
  //
  // Build a template for descriptor sets that use these shaders.
  auto descriptorSetLayoutFx0 = vku::DescriptorSetLayoutMaker{}
    .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 1)
    .createUnique(device);

  // Create descriptor set(s) to tell the shader where our buffers are.
  // One set per swap chain image.
  auto descriptorSetMakerFx0 = vku::DescriptorSetMaker{};
  for (int i = 0; i < swapchainImageCount; i++) {
    descriptorSetMakerFx0.layout(*descriptorSetLayoutFx0);
  }
  auto descriptorSetsFx0 = descriptorSetMakerFx0.create(device, fw.descriptorPool());
  
  // Update the descriptor sets with the uniform buffer information.
  auto descriptorSetUpdaterFx0 = vku::DescriptorSetUpdater{};
  for (int i = 0; i < swapchainImageCount; i++) {
    descriptorSetUpdaterFx0
      .beginDescriptorSet(descriptorSetsFx0[i])
      .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
      .buffer(uboFx[i].buffer(), 0*sizeof(UniformBufferObjectFx), 1*sizeof(UniformBufferObjectFx));
  }
  descriptorSetUpdaterFx0.update(device);

  /////////////////////////////////
  // Descriptor set 1 (shared Fx and Decal)
  //
  // Build a template for descriptor sets that use these shaders.
  auto descriptorSetLayoutFx1 = vku::DescriptorSetLayoutMaker{}
    .image(0, vk::DescriptorType::eInputAttachment, vk::ShaderStageFlagBits::eFragment, 1)
    .image(1, vk::DescriptorType::eInputAttachment, vk::ShaderStageFlagBits::eFragment, 1)
    .createUnique(device);

  // Create descriptor set(s) to tell the shader where our buffers are.
  // One set per swap chain image.
  auto descriptorSetMakerFx1 = vku::DescriptorSetMaker{};
  for (int i = 0; i < swapchainImageCount; i++) {
    descriptorSetMakerFx1.layout(*descriptorSetLayoutFx1);
  }
  auto descriptorSetsFx1 = descriptorSetMakerFx1.create(device, fw.descriptorPool());
  
  // Update the descriptor sets with the uniform buffer information.
  auto descriptorSetUpdaterFx1 = vku::DescriptorSetUpdater{};
  for (int i = 0; i < swapchainImageCount; i++) {
    descriptorSetUpdaterFx1
      .beginDescriptorSet(descriptorSetsFx1[i])
      .beginImages(0, 0, vk::DescriptorType::eInputAttachment)
      .image(*linearSampler, colorAttachmentImage.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
      .beginImages(1, 0, vk::DescriptorType::eInputAttachment)
      .image(*linearSampler, depthAttachmentImage.imageView(), vk::ImageLayout::eDepthStencilReadOnlyOptimal);
  }
  descriptorSetUpdaterFx1.update(device);

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  auto pipelineLayoutFx = vku::PipelineLayoutMaker{}
    .descriptorSetLayout(*descriptorSetLayoutFx0)
    .descriptorSetLayout(*descriptorSetLayoutFx1)
    .createUnique(device);

  auto buildPipelineFx = [&]() {
    // Make a pipeline to use the vertex format and shaders.
    return vku::PipelineMaker{ window.width(), window.height() }
      .subPass(1)
      .shader(vk::ShaderStageFlagBits::eVertex, vert_2_fx)
      .shader(vk::ShaderStageFlagBits::eFragment, frag_2_fx)
      #define VERTEX_BUFFER_BIND_ID 0
      .vertexBinding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), vk::VertexInputRate::eVertex)
      .vertexAttribute(0, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))
      .vertexAttribute(2, VERTEX_BUFFER_BIND_ID,    vk::Format::eR32G32Sfloat, offsetof(Vertex, UV))
      .vertexAttribute(3, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal))
      #define INSTANCE_BUFFER_BIND_ID 1
      .vertexBinding(INSTANCE_BUFFER_BIND_ID, sizeof(Instance), vk::VertexInputRate::eInstance)
      .vertexAttribute(4, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model))
      .vertexAttribute(5, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model)+sizeof(float) * 4)
      .vertexAttribute(6, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model)+sizeof(float) * 8)
      .vertexAttribute(7, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model)+sizeof(float) * 12)
      //
      .viewport(viewportGLDefault(window.width(), window.height())) // viewport affects cullMode and frontFace depending on Vulkan or OpenGL coordinate convention
      .cullMode(vk::CullModeFlagBits::eBack)       // Vulkan default:eFront       OpenGL default:eBack 
      .frontFace(vk::FrontFace::eCounterClockwise) // Vulkan default:eClockwise   OpenGL default:eCounterClockwise
      .depthTestEnable(VK_TRUE)
      .depthWriteEnable(VK_FALSE)
      .depthCompareOp(vk::CompareOp::eLess)
      .blendBegin(true)
      .blendSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      .blendDstAlphaBlendFactor(vk::BlendFactor::eZero)
      .logicOpEnable(false)
      .logicOp(vk::LogicOp::eCopy)
      .blendConstants(0.0f,0.0f,0.0f,0.0f)
      //
      .createUnique(device, fw.pipelineCache(), *pipelineLayoutFx, *myRenderPass);
  };
  auto pipelineFx = buildPipelineFx();

  ////////////////////////////////////////
  //
  // Create pipelines.
  //

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  auto pipelineLayoutDecal = vku::PipelineLayoutMaker{}
    .descriptorSetLayout(*descriptorSetLayoutFx0)
    .descriptorSetLayout(*descriptorSetLayoutFx1)
    .createUnique(device);

  auto buildPipelineDecal = [&]() {
    // Make a pipeline to use the vertex format and shaders.
    return vku::PipelineMaker{ window.width(), window.height() }
      .subPass(1)
      .shader(vk::ShaderStageFlagBits::eVertex, vert_3_decal)
      .shader(vk::ShaderStageFlagBits::eFragment, frag_3_decal)
      #define VERTEX_BUFFER_BIND_ID 0
      .vertexBinding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), vk::VertexInputRate::eVertex)
      .vertexAttribute(0, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))
      .vertexAttribute(2, VERTEX_BUFFER_BIND_ID,    vk::Format::eR32G32Sfloat, offsetof(Vertex, UV))
      .vertexAttribute(3, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal))
      #define INSTANCE_BUFFER_BIND_ID 1
      .vertexBinding(INSTANCE_BUFFER_BIND_ID, sizeof(Instance), vk::VertexInputRate::eInstance)
      .vertexAttribute(4, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model))
      .vertexAttribute(5, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model)+sizeof(float) * 4)
      .vertexAttribute(6, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model)+sizeof(float) * 8)
      .vertexAttribute(7, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, model)+sizeof(float) * 12)
      //
      .viewport(viewportGLDefault(window.width(), window.height())) // viewport affects cullMode and frontFace depending on Vulkan or OpenGL coordinate convention
      .cullMode(vk::CullModeFlagBits::eFront)       // Vulkan default:eFront       OpenGL default:eBack 
      .frontFace(vk::FrontFace::eCounterClockwise) // Vulkan default:eClockwise   OpenGL default:eCounterClockwise
      .depthTestEnable(VK_TRUE)
      .depthWriteEnable(VK_FALSE)
      .depthCompareOp(vk::CompareOp::eGreaterOrEqual)
      .blendBegin(true)
      .blendSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      .blendDstAlphaBlendFactor(vk::BlendFactor::eZero)
      .logicOpEnable(false)
      .logicOp(vk::LogicOp::eCopy)
      .blendConstants(0.0f,0.0f,0.0f,0.0f)
      //
      .createUnique(device, fw.pipelineCache(), *pipelineLayoutDecal, *myRenderPass);
  };
  auto pipelineDecal = buildPipelineDecal();

  ////////////////////////////////////////
  //
  // Define static drawing command buffer
  //
 
  // Static scene, so only need to create the command buffer(s) once.
  window.setStaticCommands(
    [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
      static auto ww = window.width();
      static auto wh = window.height();
      if (ww != window.width() || wh != window.height()) {
        ww = window.width();
        wh = window.height();
        pipelineScene = buildPipelineScene();
        pipelineComp = buildPipelineComp();
        pipelineFx = buildPipelineFx();
        pipelineDecal = buildPipelineDecal();
        uDataScene.proj = glm::perspective(
          glm::radians(45.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90° (extra wide) and 30° (quite zoomed in)
          float(window.width())/window.height(), // Aspect Ratio. Depends on the size of your window. Notice that 4/3 == 800/600 == 1280/960, sounds familiar ?
          0.001f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
          10.0f);
        uDataFx.proj = uDataScene.proj;
        uDataFx.res  = glm::vec2(window.width(), window.height());
      }

      cb.begin(vk::CommandBufferBeginInfo{});
      cb.beginRenderPass(myRpbi[imageIndex], vk::SubpassContents::eInline);

      // subpass 0: Scene draw to offscreen color and depth attachments
      //     bind scene descriptor set
      //         layout(set = 0, binding = 0) uniform uboScene
      //
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineScene);
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutScene, 0, descriptorSetsScene[imageIndex], nullptr);
      cb.bindIndexBuffer(iboCube.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
      cb.bindVertexBuffers(  VERTEX_BUFFER_BIND_ID,   bufferVerticesCube.buffer(), vk::DeviceSize(0)); // Binding point VERTEX_BUFFER_BIND_ID : Mesh vertex buffer
      cb.bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, bufferInstancesScene.buffer(), vk::DeviceSize(0)); // Binding point INSTANCE_BUFFER_BIND_ID : Instance data buffer
      cb.drawIndexed(indicesCube.size(), instancesScene.size(), 0, 0, 0);

      //////////////////////////////////
      // increment active subpass index
      //
      cb.nextSubpass(vk::SubpassContents::eInline);
 
      // subpass 1a: Composition, fullscreen quad draw
      //     bind composition descriptor set
      //         layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColorAttachment;
      //
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineComp);
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutComp, 0, descriptorSetsComp[imageIndex], nullptr);
      cb.bindIndexBuffer(iboPlane.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
      cb.bindVertexBuffers(  VERTEX_BUFFER_BIND_ID,   bufferVerticesPlane.buffer(), vk::DeviceSize(0)); // Binding point VERTEX_BUFFER_BIND_ID : Mesh vertex buffer
      cb.drawIndexed(indicesPlane.size(), 1, 0, 0, 0);

      // subpass 1b: Fx draw, oscillating Sphere
      //     binding fx descriptor set 0
      //         layout (set = 0, binding = 0) uniform uboFX
      //     binding fx descriptor set 1
      //         layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputColorAttachment;
      //         layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inputDepthAttachment;
      //
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineFx);
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutFx, 0, descriptorSetsFx0[imageIndex], nullptr);
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutFx, 1, descriptorSetsFx1[imageIndex], nullptr);
      cb.bindIndexBuffer(iboSphere.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
      cb.bindVertexBuffers(  VERTEX_BUFFER_BIND_ID,   bufferVerticesSphere.buffer(), vk::DeviceSize(0)); // Binding point VERTEX_BUFFER_BIND_ID : Mesh vertex buffer
      cb.bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, bufferInstancesFx.buffer(), vk::DeviceSize(0)); // Binding point INSTANCE_BUFFER_BIND_ID : Instance data buffer
      cb.drawIndexed(indicesSphere.size(), instancesFx.size(), 0, 0, 0);

      // subpass 1c: Decal draw, oscillating Cube
      //     binding fx descriptor set 0
      //         layout (set = 0, binding = 0) uniform uboFx
      //     binding fx descriptor set 1
      //         layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputColorAttachment;
      //         layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inputDepthAttachment;
      //
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineDecal);
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutDecal, 0, descriptorSetsFx0[imageIndex], nullptr);
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutDecal, 1, descriptorSetsFx1[imageIndex], nullptr);
      cb.bindIndexBuffer(iboCube.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
      cb.bindVertexBuffers(  VERTEX_BUFFER_BIND_ID,   bufferVerticesCube.buffer(), vk::DeviceSize(0)); // Binding point VERTEX_BUFFER_BIND_ID : Mesh vertex buffer
      cb.bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, bufferInstancesFx.buffer(), vk::DeviceSize(0)); // Binding point INSTANCE_BUFFER_BIND_ID : Instance data buffer
      cb.drawIndexed(indicesCube.size(), instancesFx.size(), 0, 0, 0);

      cb.endRenderPass();
      cb.end();
    }
  );

  ////////////////////////////////////////
  //
  // Main animation loop
  //
 
  // Loop waiting for the window to close.
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    // Update clock
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    // Update dynamic uniform (Scene) on CPU
    uDataScene.model = mouse_rotation * uDataScene.model;
    // Update dynamic uniform (Fx) on CPU
    uDataFx.model = uDataScene.model * glm::scale(glm::vec3(1.125f + 0.125f * sin(time)));

    // dynamic draw
    window.draw(
      device, fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {

        cb.begin(vk::CommandBufferBeginInfo{});

        // Define vector of uniform buffers to be updated on GPU with requisite parameters
        #define TupleUpdateBufferInfo std::tuple<vku::UniformBuffer*, VkDeviceSize, VkDeviceSize, const void*>
        std::vector< TupleUpdateBufferInfo > tupleUpdateBuffers {
          // Scene's uniform buffer information
          {&uboScene[imageIndex], 0, sizeof(UniformBufferObjectScene), &uDataScene},
          // Fx's uniform buffer information
          {   &uboFx[imageIndex], 0, sizeof(UniformBufferObjectFx),    &uDataFx},
        };

        // Sync all local uniform values with corresponding values on GPU
        for (auto [dstBuffer, dstOffset, dataSize, pData]: tupleUpdateBuffers) {
          // We may or may not need this barrier. It is probably a good precaution.
          dstBuffer->barrier(
            cb,
            vk::PipelineStageFlagBits::eHost,           //srcStageMask
            vk::PipelineStageFlagBits::eFragmentShader, //dstStageMask
            vk::DependencyFlagBits::eByRegion,          //dependencyFlags
            vk::AccessFlagBits::eHostWrite,             //srcAccessMask
            vk::AccessFlagBits::eShaderRead,            //dstAccessMask
            fw.graphicsQueueFamilyIndex(),              //srcQueueFamilyIndex
            fw.graphicsQueueFamilyIndex()               //dstQueueFamilyIndex
          );
  
          // Instead of pushConstants() we use updateBuffer(). This has a max of 64k.
          // Like pushConstants(), this takes a copy of the uniform buffer
          // at the time we create this command buffer.
          // Unlike push constant update, uniform buffer must be updated
          // _OUTSIDE_ of the (beginRenderPass ... endRenderPass)
          cb.updateBuffer( dstBuffer->buffer(), dstOffset, dataSize, pData );
        }

        cb.end();
      }
    );

    // Very crude method to prevent your GPU from overheating.
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  // Wait until all drawing is done and then kill the window.
  device.waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  // The Framework and Window objects will be destroyed here.

  return 0;
}
