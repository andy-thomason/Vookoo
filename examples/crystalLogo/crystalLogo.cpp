////////////////////////////////////////////////////////////////////////////////
//
// Vookoo example (C) 2017 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//

// Include the demo framework, vookoo (vku) for building objects and glm for maths.
// The demo framework uses GLFW to create windows.
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <cmath> // M_PI

// images converted from png to c files via GIMP exporter
#include "imag_logo.c"
#include "imag_text1.c"
#include "imag_text2.c"

namespace CubeTypes { enum { DISPLACEMENT = 1, MASK = 2, FINAL = 3 }; }
namespace CubeFaces { enum { BACK = 1, FRONT = -1 }; }
namespace CubeMasks { enum { M1 = 1, M2 = 2, M3 = 3, M4 = 4, M5 = 5 }; }
namespace ContentTypes { enum { RAINBOW = 1, BLUE = 2, RED = 3 }; }

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
    auto mouse_angle = glm::length(mouse_axis) / 250.f; // 250.[degrees/pixel] arbitrary scale
    mouse_rotation = glm::rotate(mouse_angle, mouse_axis);
  } else {
    mouse_rotation = glm::mat4(1.0f);
  }
  previous_position = position;
}

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  auto *title = "crystalLogo";
  auto glfwwindow = glfwCreateWindow(720, 720, title, nullptr, nullptr);

  glfwSetCursorPosCallback(glfwwindow, mouse_callback);

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

  auto device = fw.device();

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
  window.clearColorValue() = {0.0f, 0.0f, 0.0f, 1.0f};
  window.dumpCaps(std::cout, fw.physicalDevice());

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
  // Create Uniform Buffer (fragment shader)

  struct Uniform_frag {
    glm::vec2 resolution;
    float tick;
    float borderWidth;
    float displacementLength;
    float reflectionOpacity;
    int scene;
  };
  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer ubo_frag(device, fw.memprops(), sizeof(Uniform_frag));

  ////////////////////////////////////////
  //
  // Pushconstant, unlike uniform buffer, can be updated inside beginRenderPass ... end block 
  // Note! be very careful when using vec3, vec2, float and vec4 together
  // as there are alignment rules you must follow.
  struct PushConstant_frag {
    int face;
    int typeId;
  };

  ////////////////////////////////////////
  //
  // Create Uniform Buffer (Content fragment shader)

  struct UniformContent_frag {
    glm::vec2 resolution;
    float tick;
  };
  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer uboContent_frag(device, fw.memprops(), sizeof(UniformContent_frag));

  ////////////////////////////////////////
  //
  // Pushconstant, unlike uniform buffer, can be updated inside beginRenderPass ... end block 
  // Note! be very careful when using vec3, vec2, float and vec4 together
  // as there are alignment rules you must follow.
  struct PushConstantContent_frag {
    int maskId;
    int typeId;
  };

  ////////////////////////////////////////
  //
  // Create Uniform Buffer (Reflection Reflector fragment shader)

  struct UniformReflectionReflector_frag {
    float depthOpacity;
  };
  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer uboReflectionReflector_frag(device, fw.memprops(), sizeof(UniformReflectionReflector_frag));

/*
  ////////////////////////////////////////
  //
  // Create Uniform Buffer (Reflection Plane fragment shader)

  struct UniformReflectionPlane {
    glm::mat4 textureMatrix;
    glm::mat4 world;
  };
  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer uboReflectionPlane_vert(device, fw.memprops(), sizeof(UniformReflectionPlane));
*/

  ////////////////////////////////////////
  //
  // Create Mesh vertices
 
  struct Vertex { 
    glm::vec3 pos; 
    glm::vec3 center; 
    glm::vec2 uv; 
    glm::vec3 color; 
  };

  // cube 
  const std::vector<Vertex> vertices = {
    // front face
    {.pos={-1, +1, +1}, .center={0, 0, 1}, .uv={0, 0}, .color={0, 1, 1}},
    {.pos={+1, +1, +1}, .center={0, 0, 1}, .uv={1, 0}, .color={0, 1, 1}},
    {.pos={+1, -1, +1}, .center={0, 0, 1}, .uv={1, 1}, .color={0, 1, 1}},
    {.pos={-1, -1, +1}, .center={0, 0, 1}, .uv={0, 1}, .color={0, 1, 1}},
    // right face
    {.pos={+1, +1, +1}, .center={1, 0, 0}, .uv={0, 0}, .color={0, 0, 1}},
    {.pos={+1, +1, -1}, .center={1, 0, 0}, .uv={1, 0}, .color={0, 0, 1}},
    {.pos={+1, -1, -1}, .center={1, 0, 0}, .uv={1, 1}, .color={0, 0, 1}},
    {.pos={+1, -1, +1}, .center={1, 0, 0}, .uv={0, 1}, .color={0, 0, 1}},
    // back face
    {.pos={+1, +1, -1}, .center={0, 0, -1}, .uv={0, 0}, .color={0, 1, 0}},
    {.pos={-1, +1, -1}, .center={0, 0, -1}, .uv={1, 0}, .color={0, 1, 0}},
    {.pos={-1, -1, -1}, .center={0, 0, -1}, .uv={1, 1}, .color={0, 1, 0}},
    {.pos={+1, -1, -1}, .center={0, 0, -1}, .uv={0, 1}, .color={0, 1, 0}},
    // left face
    {.pos={-1, +1, -1}, .center={-1, 0, 0}, .uv={0, 0}, .color={0, 1, 1}},
    {.pos={-1, +1, +1}, .center={-1, 0, 0}, .uv={1, 0}, .color={0, 1, 1}},
    {.pos={-1, -1, +1}, .center={-1, 0, 0}, .uv={1, 1}, .color={0, 1, 1}},
    {.pos={-1, -1, -1}, .center={-1, 0, 0}, .uv={0, 1}, .color={0, 1, 1}},
    // top face
    {.pos={-1, +1, -1}, .center={0, 1, 0}, .uv={0, 0}, .color={1, 0, 0}},
    {.pos={+1, +1, -1}, .center={0, 1, 0}, .uv={1, 0}, .color={1, 0, 0}},
    {.pos={+1, +1, +1}, .center={0, 1, 0}, .uv={1, 1}, .color={1, 0, 0}},
    {.pos={-1, +1, +1}, .center={0, 1, 0}, .uv={0, 1}, .color={1, 0, 0}},
    // bottom face
    {.pos={-1, -1, -1}, .center={0, -1, 0}, .uv={0, 0}, .color={1, 0, 1}},
    {.pos={+1, -1, -1}, .center={0, -1, 0}, .uv={1, 0}, .color={1, 0, 1}},
    {.pos={+1, -1, +1}, .center={0, -1, 0}, .uv={1, 1}, .color={1, 0, 1}},
    {.pos={-1, -1, +1}, .center={0, -1, 0}, .uv={0, 1}, .color={1, 0, 1}},
  };
  vku::HostVertexBuffer buffer(device, fw.memprops(), vertices);

  ////////////////////////////////////////
  //
  // Create mesh indices

  std::vector<uint32_t> indices = {
     2, 1, 0,    2, 0, 3, // front face
     6, 5, 4,    6, 4, 7, // right face
    10, 9, 8,   10, 8,11, // back face
    14,13,12,   14,12,15, // left face
    18,17,16,   18,16,19, // top face
    20,21,22,   23,20,22, // bottom face
  };
  vku::HostIndexBuffer ibo(device, fw.memprops(), indices);

  ////////////////////////////////////////
  //
  // Create Mesh vertices
 
  struct VertexContent { 
    glm::vec3 pos; 
    glm::vec2 uv; 
  };

  // fullscreen quad
  const std::vector<VertexContent> verticesContent = {
    {.pos={-1, +1, -0.0}, .uv={1, 1}},
    {.pos={+1, +1, -0.0}, .uv={0, 1}},
    {.pos={+1, -1, -0.0}, .uv={0, 0}},
    {.pos={-1, -1, -0.0}, .uv={1, 0}},
  };
  vku::HostVertexBuffer bufferContent(device, fw.memprops(), verticesContent);

  ////////////////////////////////////////
  //
  // Create mesh indices

  std::vector<uint32_t> indicesContent = {
     0, 1, 2,    2, 3, 0 // rectangular face
  };
  vku::HostIndexBuffer iboContent(device, fw.memprops(), indicesContent);
/*
  ////////////////////////////////////////
  //
  // Reflection planes
 
  struct StructPlane { 
    glm::vec3 position; 
    glm::vec3 normal; 
    float rotation;
    glm::vec3 axis; 
    float uvRotation;
  };

  const std::vector<StructPlane> planes = {
    {.position={ 1, 0, 0}, .normal={ 1, 0, 0}, .rotation=-M_PI * 0.5, .axis={0, 1, 0}, .uvRotation=M_PI},
    {.position={-1, 0, 0}, .normal={-1, 0, 0}, .rotation= M_PI * 0.5, .axis={0, 1, 0}, .uvRotation=M_PI},
    {.position={ 0, 1, 0}, .normal={ 0, 1, 0}, .rotation= M_PI * 0.5, .axis={1, 0, 0}, .uvRotation=0},
    {.position={ 0,-1, 0}, .normal={ 0,-1, 0}, .rotation=-M_PI * 0.5, .axis={1, 0, 0}, .uvRotation=0},
    {.position={ 0, 0, 1}, .normal={ 0, 0, 1}, .rotation= M_PI,       .axis={0, 1, 0}, .uvRotation=M_PI},
    {.position={ 0, 0,-1}, .normal={ 0, 0,-1}, .rotation= 0,          .axis={0, 1, 0}, .uvRotation=M_PI},
  };

  ////////////////////////////////////////
  //
  // 
  const glm::mat4 textureMatrix = glm::mat4(
    glm::vec4(0.5,   0,   0, 0),
    glm::vec4(  0, 0.5,   0, 0),
    glm::vec4(  0,   0, 0.5, 0),
    glm::vec4(0.5, 0.5, 0.5, 1)
  );
*/
  ////////////////////////////////////////////////////////
  //
  // Create a texture : imgLogo

  vku::TextureImage2D imgLogo{device, fw.memprops(), gimp_logo.width,gimp_logo.height, 1, vk::Format::eR8G8B8A8Unorm};
  {
    std::vector<uint8_t> pixels(gimp_logo.width * gimp_logo.height * gimp_logo.bytes_per_pixel, // R     G     B     A 
      0x00);
		GIMP_LOGO_RUN_LENGTH_DECODE(pixels.data(), gimp_logo.rle_pixel_data, gimp_logo.width * gimp_logo.height, gimp_logo.bytes_per_pixel);
    imgLogo.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());
  }

  ////////////////////////////////////////////////////////
  //
  // Create a texture : imgText1

  vku::TextureImage2D imgText1{device, fw.memprops(),gimp_text1.width, gimp_text1.height, 1, vk::Format::eR8G8B8A8Unorm};
  {
    std::vector<uint8_t> pixels(gimp_text1.width * gimp_text1.height * gimp_text1.bytes_per_pixel, // R     G     B     A 
      0x00);
		GIMP_TEXT1_RUN_LENGTH_DECODE(pixels.data(), gimp_text1.rle_pixel_data, gimp_text1.width * gimp_text1.height, gimp_text1.bytes_per_pixel);
    imgText1.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());
  }

  ////////////////////////////////////////////////////////
  //
  // Create a texture : imgText2

  vku::TextureImage2D imgText2{device, fw.memprops(), gimp_text2.width, gimp_text2.height, 1, vk::Format::eR8G8B8A8Unorm};
  {
    std::vector<uint8_t> pixels(gimp_text2.width * gimp_text2.height * gimp_text2.bytes_per_pixel, // R     G     B     A 
      0x00);
		GIMP_TEXT2_RUN_LENGTH_DECODE(pixels.data(), gimp_text2.rle_pixel_data, gimp_text2.width * gimp_text2.height, gimp_text2.bytes_per_pixel);
    imgText2.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());
  }

  ////////////////////////////////////////////////////////
  //
  // Create a texture : displacementFbo

  vku::ColorAttachmentImage displacementFbo{device, fw.memprops(), window.width(), window.height(), vk::Format::eR8G8B8A8Unorm};
  //displacementFbo.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());
  
  ////////////////////////////////////////////////////////
  //
  // Create a texture : maskFbo

  vku::ColorAttachmentImage maskFbo{device, fw.memprops(), window.width(), window.height(), vk::Format::eR8G8B8A8Unorm};
  //maskFbo.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());

  ////////////////////////////////////////////////////////
  //
  // Create a texture : contentFbo

  vku::ColorAttachmentImage contentFbo{device, fw.memprops(), window.width(), window.height(), vk::Format::eR8G8B8A8Unorm};
  {
    std::vector<uint8_t> pixels( contentFbo.info().extent.width * contentFbo.info().extent.height * 4, // 4bytes(RGBA)  
      0x00);
    contentFbo.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());
  }

  ////////////////////////////////////////
  //
  // Create a cubemap : reflectionFbo

  vku::TextureImageCube reflectionFbo{device, fw.memprops(), window.width(), window.height(), 1, vk::Format::eR8G8B8A8Unorm};
  assert(reflectionFbo.info().arrayLayers==6); // vertex shader: extension GL_EXT_multiview depends on cubemap i.e expects 6 layers
  {
    std::vector<uint8_t> pixels( reflectionFbo.info().extent.width * reflectionFbo.info().extent.height * 4 * reflectionFbo.info().arrayLayers, // 4bytes(RGBA)  6 Layers 
      0x00);
    reflectionFbo.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());
  }

  ////////////////////////////////////////
  //
  // Create Sampler with Linear filter and Nearest mipmap mode
  vku::SamplerMaker sm{};
  vk::UniqueSampler SamplerLinearNearest = sm
    .magFilter( vk::Filter::eLinear )
    .minFilter( vk::Filter::eLinear )
    .mipmapMode( vk::SamplerMipmapMode::eNearest )
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Helper for building common render passes
  // that only differ by particular iChannelX output attachment
  //
  auto RenderPassCommon = [&device](vku::ColorAttachmentImage& iChannelX)->vk::UniqueRenderPass {
    // Build the renderpass writing to iChannelX 
    vku::RenderpassMaker rpmPassX;
    return rpmPassX
      // The only colour attachment.
     .attachmentBegin( iChannelX.format() )
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
     .createUnique(device);
  };

  ////////////////////////////////////////
  //
  // displacment and mask shaders
  // (mostly shared parts)
  // Memorize important data for later
  std::vector<vk::DescriptorSet> descriptorSets;
  vk::UniquePipeline displacementPipeline;
  vk::UniquePipeline maskPipeline;
  vk::UniquePipeline pipelineCULLFRONT;
  vk::UniquePipeline pipelineCULLBACK;
  vk::UniquePipelineLayout pipelineLayout; 
  vk::UniqueRenderPass displacementRenderPass;
  vk::UniqueRenderPass maskRenderPass;
  vk::UniqueFramebuffer maskFrameBuffer;
  vk::UniqueFramebuffer displacementFrameBuffer;
  vk::RenderPassBeginInfo displacementRpbi;
  vk::RenderPassBeginInfo maskRpbi;
  {
    ////////////////////////////////////////
    //
    // Build the descriptor sets
  
    vku::DescriptorSetLayoutMaker dslm{};
    auto descriptorSetLayout = dslm
      .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1)
      .buffer(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1)
      .image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
      .image(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
      .createUnique(device);
  
    vku::DescriptorSetMaker dsm{};
    descriptorSets = dsm
      .layout(*descriptorSetLayout)
      .create(device, fw.descriptorPool());
  
    ////////////////////////////////////////
    //
    // Update the descriptor sets for the shader uniforms.
  
    vku::DescriptorSetUpdater dsu;
    dsu.beginDescriptorSet(descriptorSets[0])
       // Uniform_vert
       .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
       .buffer(ubo.buffer(), 0, sizeof(Uniform_vert))
       // Uniform_frag
       .beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer)
       .buffer(ubo_frag.buffer(), 0, sizeof(Uniform_frag))
       // sampler2D u_texture --> contentFbo
       .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
       .image(*SamplerLinearNearest, contentFbo.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
       // samplerCube u_reflection --> reflectionFbo
       .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
       .image(*SamplerLinearNearest, reflectionFbo.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
       // update the descriptor sets with their pointers (but not data).
       .update(device);
  
    ////////////////////////////////////////
    //
    // Build the final pipeline
    // Make a pipeline to use the vertex format and shaders.
  
    // Create two shaders, vertex and fragment.
    vku::ShaderModule vert{device, BINARY_DIR "cube.vert.spv"};
    vku::ShaderModule frag{device, BINARY_DIR "cube.frag.spv"};
  
    // Make a default pipeline layout. This shows how pointers
    // to resources are layed out.
    vku::PipelineLayoutMaker plm{};
    pipelineLayout = plm
      .descriptorSetLayout(*descriptorSetLayout)
      .pushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstant_frag))
      .createUnique(device);
  
    vku::PipelineMaker pm{window.width(), window.height()};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(Vertex))
      .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, center))
      .vertexAttribute(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv))
      .vertexAttribute(3, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))
      .viewport(viewport) // viewport set to match openGL, affects cullMode and frontFace
      .frontFace(vk::FrontFace::eClockwise) // VK is opposite openGL default, GL_CCW face is front
      .cullMode(vk::CullModeFlagBits::eBack) // openGL default, GL_BACK is the face to be culled
      .depthTestEnable( VK_TRUE )
      .depthCompareOp(vk::CompareOp::eLess)
      .blendBegin(VK_TRUE)
      .blendEnable(VK_TRUE)
      .blendSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
      .blendSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      .blendDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
      .blendDstAlphaBlendFactor(vk::BlendFactor::eOne)
      .blendColorBlendOp(vk::BlendOp::eAdd)
      .blendAlphaBlendOp(vk::BlendOp::eAdd);
  
    // Create a pipeline using a renderPass built for our window.
    pm.cullMode(vk::CullModeFlagBits::eFront);
    pipelineCULLFRONT = pm.createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
  
    pm.cullMode(vk::CullModeFlagBits::eBack);
    pipelineCULLBACK = pm.createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
  
    // Build the renderpass 
    displacementRenderPass = RenderPassCommon( displacementFbo );
  
    // Build the pipeline for this renderpass.
    pm.cullMode(vk::CullModeFlagBits::eBack);
    displacementPipeline = pm.createUnique(device, fw.pipelineCache(), *pipelineLayout, *displacementRenderPass);
  
    ////////////////////////////////////////
    //
    // Build a RenderPassBeginInfo for displacement
    //
  
    // Build the framebuffer.
    vk::ImageView attachments[] = {displacementFbo.imageView()};
    vk::FramebufferCreateInfo fbdi{{}, *displacementRenderPass, sizeof(attachments)/sizeof(vk::ImageView), attachments, window.width(), window.height(), 1 };
    displacementFrameBuffer = device.createFramebufferUnique(fbdi);
  
    // Match in order of attachments to clear; the image.
    std::array<vk::ClearValue, sizeof(attachments)/sizeof(vk::ImageView)> clearColours{
      vk::ClearColorValue{}
    };
  
    // Begin rendering using the framebuffer and renderpass
    displacementRpbi = vk::RenderPassBeginInfo {
      *displacementRenderPass,
      *displacementFrameBuffer,
      vk::Rect2D{{0, 0}, {window.width(), window.height()}},
      (uint32_t) clearColours.size(),
      clearColours.data()
    };
  
    // Build the renderpass 
    maskRenderPass = RenderPassCommon( maskFbo );
  
    // Build the pipeline for this renderpass.
    pm.cullMode(vk::CullModeFlagBits::eBack);
    maskPipeline = pm.createUnique(device, fw.pipelineCache(), *pipelineLayout, *maskRenderPass);
  
    ////////////////////////////////////////
    //
    // Build a RenderPassBeginInfo for mask
    //
  
    // Build the framebuffer.
    vk::ImageView attachmentsMask[] = {maskFbo.imageView()};
    vk::FramebufferCreateInfo fbmi{{}, *maskRenderPass, sizeof(attachmentsMask)/sizeof(vk::ImageView), attachmentsMask, window.width(), window.height(), 1 };
    maskFrameBuffer = device.createFramebufferUnique(fbmi);
  
    // Match in order of attachments to clear; the image.
    std::array<vk::ClearValue, sizeof(attachmentsMask)/sizeof(vk::ImageView)> clearColoursMask{
      vk::ClearColorValue{}
    };
  
    // Begin rendering using the framebuffer and renderpass
    maskRpbi = vk::RenderPassBeginInfo{
      *maskRenderPass,
      *maskFrameBuffer,
      vk::Rect2D{{0, 0}, {window.width(), window.height()}},
      (uint32_t) clearColoursMask.size(),
      clearColoursMask.data()
    };
  }

  ////////////////////////////////////////
  //
  // Content shader
  // Memorize important data for later
  struct ContentTexture {
    int maskId;
    int typeId;
    vk::ImageView imageView;
  };
  const std::vector<ContentTexture> ContentTextures = {
    { .maskId = CubeMasks::M1, .typeId = ContentTypes::RAINBOW, .imageView = imgLogo.imageView() },
    { .maskId = CubeMasks::M2, .typeId = ContentTypes::BLUE,    .imageView = imgLogo.imageView() },
    { .maskId = CubeMasks::M3, .typeId = ContentTypes::RED,     .imageView = imgLogo.imageView() },
    { .maskId = CubeMasks::M4, .typeId = ContentTypes::BLUE,    .imageView = imgText1.imageView() },
    { .maskId = CubeMasks::M5, .typeId = ContentTypes::RED,     .imageView = imgText2.imageView() },
  };

  std::vector<vk::DescriptorSet> descriptorSetsContent;
  vk::UniquePipeline pipelineContent;
  vk::UniquePipelineLayout pipelineLayoutContent; 
  vk::UniqueRenderPass contentRenderPass;
  vk::UniqueFramebuffer contentFrameBuffer;
  vk::RenderPassBeginInfo contentRpbi;
  {
    ////////////////////////////////////////
    //
    // Build the descriptor sets
    vku::DescriptorSetLayoutMaker dslm{};
    auto descriptorSetLayout = dslm
      .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1)
      .buffer(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1)
      .image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
      .image(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
      .image(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
      .createUnique(device);
  
    vku::DescriptorSetMaker dsm{};
    for (auto &q : ContentTextures) {
      dsm.layout(*descriptorSetLayout); // one descriptorSetsContent for each ContentTexture
    }
    descriptorSetsContent = dsm.create(device, fw.descriptorPool());

    ////////////////////////////////////////
    //
    // Update the descriptor sets for the shader uniforms.
    for(std::size_t counter = 0; counter < ContentTextures.size(); ++counter) {
      vku::DescriptorSetUpdater dsu;
      dsu
        .beginDescriptorSet(descriptorSetsContent[counter])
        // Uniform_vert
        .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
        .buffer(ubo.buffer(), 0, sizeof(Uniform_vert))
        // UniformContent_frag
        .beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer)
        .buffer(uboContent_frag.buffer(), 0, sizeof(UniformContent_frag))
        // sampler2D u_texture --> imgLogo
        .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
        .image(*SamplerLinearNearest, ContentTextures[counter].imageView, vk::ImageLayout::eShaderReadOnlyOptimal)
        // samplerCube u_displacement --> displacementFbo
        .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
        .image(*SamplerLinearNearest, displacementFbo.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
        // samplerCube u_mask --> maskFbo
        .beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler)
        .image(*SamplerLinearNearest, maskFbo.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
        // update the descriptor sets with their pointers (but not data).
        .update(device);
    }

    ////////////////////////////////////////
    //
    // Build the final pipeline
    // Make a pipeline to use the vertex format and shaders.
  
    // Create two shaders, vertex and fragment.
    vku::ShaderModule vert{device, BINARY_DIR "content.vert.spv"};
    vku::ShaderModule frag{device, BINARY_DIR "content.frag.spv"};

    // Make a default pipeline layout. This shows how pointers
    // to resources are layed out.
    vku::PipelineLayoutMaker plm{};
    pipelineLayoutContent = plm
      .pushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstantContent_frag))
      .descriptorSetLayout(*descriptorSetLayout)
      .createUnique(device);
  
    vku::PipelineMaker pm{window.width(), window.height()};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(VertexContent))
      .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(VertexContent, pos))
      .vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(VertexContent, uv))
      .viewport(viewport) // viewport set to match openGL, affects cullMode and frontFace
      .frontFace(vk::FrontFace::eClockwise) // VK is opposite openGL default, GL_CCW face is front
      .cullMode(vk::CullModeFlagBits::eBack) // openGL default, GL_BACK is the face to be culled
      //.depthTestEnable( VK_TRUE )
      //.depthCompareOp(vk::CompareOp::eLess)
      //.blendBegin(VK_TRUE)
      //.blendEnable(VK_TRUE)
      //.blendSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
      //.blendSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      //.blendDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
      //.blendDstAlphaBlendFactor(vk::BlendFactor::eOne)
      //.blendColorBlendOp(vk::BlendOp::eAdd)
      //.blendAlphaBlendOp(vk::BlendOp::eAdd)
      ;

    // Build the renderpass
    contentRenderPass = RenderPassCommon( contentFbo );
  
    // Build the pipeline for this renderpass.
    pipelineContent = pm.createUnique(device, fw.pipelineCache(), *pipelineLayoutContent, *contentRenderPass);

    ////////////////////////////////////////
    //
    // Build a RenderPassBeginInfo for content
    //
  
    // Build the framebuffer.
    vk::ImageView attachmentscontent[] = {contentFbo.imageView()};
    vk::FramebufferCreateInfo fbmi{{}, *contentRenderPass, sizeof(attachmentscontent)/sizeof(vk::ImageView), attachmentscontent, window.width(), window.height(), 1 };
    contentFrameBuffer = device.createFramebufferUnique(fbmi);
  
    // Match in order of attachments to clear; the image.
    std::array<vk::ClearValue, sizeof(attachmentscontent)/sizeof(vk::ImageView)> clearColourscontent{
      vk::ClearColorValue{}
    };
  
    // Begin rendering using the framebuffer and renderpass
    contentRpbi = vk::RenderPassBeginInfo{
      *contentRenderPass,
      *contentFrameBuffer,
      vk::Rect2D{{0, 0}, {window.width(), window.height()}},
      (uint32_t) clearColourscontent.size(),
      clearColourscontent.data()
    };
  }

  ////////////////////////////////////////
  //
  // ReflectionReflector shader
  // Memorize important data for later
  std::vector<vk::DescriptorSet> descriptorSetsReflectionReflector;
  vk::UniquePipeline pipelineReflectionReflector;
  vk::UniquePipelineLayout pipelineLayoutReflectionReflector; 
  vk::UniqueRenderPass reflectionreflectorRenderPass;
  vk::UniqueFramebuffer reflectionreflectorFrameBuffer;
  vk::RenderPassBeginInfo reflectionreflectorRpbi;
  {
    ////////////////////////////////////////
    //
    // Build the descriptor sets
    vku::DescriptorSetLayoutMaker dslm{};
    auto descriptorSetLayout = dslm
      .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1)
      .buffer(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1)
      .image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
      .createUnique(device);
  
    vku::DescriptorSetMaker dsm{};
    descriptorSetsReflectionReflector = dsm
      .layout(*descriptorSetLayout)
      .create(device, fw.descriptorPool());

    ////////////////////////////////////////
    //
    // Update the descriptor sets for the shader uniforms.
  
    vku::DescriptorSetUpdater dsu;
    dsu.beginDescriptorSet(descriptorSetsReflectionReflector[0])
       // Uniform_vert
       .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
       .buffer(ubo.buffer(), 0, sizeof(Uniform_vert))
       // UniformReflectionReflector_frag
       .beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer)
       .buffer(uboReflectionReflector_frag.buffer(), 0, sizeof(UniformReflectionReflector_frag))
       // sampler2D u_texture --> contentFbo
       .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
       .image(*SamplerLinearNearest, contentFbo.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
       // update the descriptor sets with their pointers (but not data).
       .update(device);

    ////////////////////////////////////////
    //
    // Build the final pipeline
    // Make a pipeline to use the vertex format and shaders.
  
    // Create two shaders, vertex and fragment.
    vku::ShaderModule vert{device, BINARY_DIR "reflectionReflector.vert.spv"};
    vku::ShaderModule frag{device, BINARY_DIR "reflectionReflector.frag.spv"};

    // Make a default pipeline layout. This shows how pointers
    // to resources are layed out.
    vku::PipelineLayoutMaker plm{};
    pipelineLayoutReflectionReflector = plm
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
    reflectionreflectorRenderPass = rpm
        // The only colour attachment.
       .attachmentBegin(reflectionFbo.format())
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
  
    // Build the pipeline for this renderpass.
    vku::PipelineMaker pm{window.width(), window.height()};
    pipelineReflectionReflector = pm
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(VertexContent))
      .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(VertexContent, pos))
      .vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(VertexContent, uv))
      .viewport(viewport) // viewport set to match openGL, affects cullMode and frontFace
      .frontFace(vk::FrontFace::eClockwise) // VK is opposite openGL default, GL_CCW face is front
      .cullMode(vk::CullModeFlagBits::eNone) // openGL default, GL_BACK is the face to be culled
      .depthTestEnable( VK_TRUE )
      .depthCompareOp(vk::CompareOp::eLess)
      .blendBegin(VK_TRUE)
      .blendEnable(VK_TRUE)
      .blendSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
      .blendSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      .blendDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
      .blendDstAlphaBlendFactor(vk::BlendFactor::eOne)
      .blendColorBlendOp(vk::BlendOp::eAdd)
      .blendAlphaBlendOp(vk::BlendOp::eAdd)
      .createUnique(device, fw.pipelineCache(), *pipelineLayoutReflectionReflector, *reflectionreflectorRenderPass);

    ////////////////////////////////////////
    //
    // Build a RenderPassBeginInfo for reflectionreflector
    //
  
    // Build the framebuffer.
    vk::ImageView attachmentsreflectionreflector[] = {reflectionFbo.imageView()};
    vk::FramebufferCreateInfo fbmi{{}, *reflectionreflectorRenderPass, sizeof(attachmentsreflectionreflector)/sizeof(vk::ImageView), attachmentsreflectionreflector, window.width(), window.height(), 1}; //reflectionFbo.info().arrayLayers };
    reflectionreflectorFrameBuffer = device.createFramebufferUnique(fbmi);
  
    // Match in order of attachments to clear; the image.
    std::array<vk::ClearValue, sizeof(attachmentsreflectionreflector)/sizeof(vk::ImageView)> clearColoursreflectionreflector{
      vk::ClearColorValue{}
    };
  
    // Begin rendering using the framebuffer and renderpass
    reflectionreflectorRpbi = vk::RenderPassBeginInfo{
      *reflectionreflectorRenderPass,
      *reflectionreflectorFrameBuffer,
      vk::Rect2D{{0, 0}, {window.width(), window.height()}},
      (uint32_t) clearColoursreflectionreflector.size(),
      clearColoursreflectionreflector.data()
    };
  }
/*
  ////////////////////////////////////////
  //
  // ReflectionPlane shader
  // Memorize important data for later
  std::vector<vk::DescriptorSet> descriptorSetsReflectionPlane;
  vk::UniquePipeline pipelineReflectionPlane;
  vk::UniquePipelineLayout pipelineLayoutReflectionPlane; 
  vk::UniqueRenderPass reflectionplaneRenderPass;
  vk::UniqueFramebuffer reflectionplaneFrameBuffer;
  vk::RenderPassBeginInfo reflectionplaneRpbi;
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
    descriptorSetsReflectionPlane = dsm
      .layout(*descriptorSetLayout)
      .create(device, fw.descriptorPool());

    ////////////////////////////////////////
    //
    // Update the descriptor sets for the shader uniforms.
  
    vku::DescriptorSetUpdater dsu;
    dsu.beginDescriptorSet(descriptorSetsReflectionPlane[0])
       // Uniform_vert
       .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
       .buffer(uboReflectionPlane_vert.buffer(), 0, sizeof(UniformReflectionPlane))
       // sampler2D u_texture --> imgLogo
       .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
       .image(*SamplerLinearNearest, imgLogo.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
       // update the descriptor sets with their pointers (but not data).
       .update(device);

    ////////////////////////////////////////
    //
    // Build the final pipeline
    // Make a pipeline to use the vertex format and shaders.
  
    // Create two shaders, vertex and fragment.
    vku::ShaderModule vert{device, BINARY_DIR "reflectionPlane.vert.spv"};
    vku::ShaderModule frag{device, BINARY_DIR "reflectionPlane.frag.spv"};

    // Make a default pipeline layout. This shows how pointers
    // to resources are layed out.
    vku::PipelineLayoutMaker plm{};
    pipelineLayoutReflectionPlane = plm
      .descriptorSetLayout(*descriptorSetLayout)
      .createUnique(device);

    // Build the renderpass 
    reflectionplaneRenderPass = RenderPassCommon( reflectionFbo );
  
    // Build the pipeline for this renderpass.
    vku::PipelineMaker pm{window.width(), window.height()};
    pipelineReflectionPlane = pm
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(VertexContent))
      .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(VertexContent, pos))
      .viewport(viewport) // viewport set to match openGL, affects cullMode and frontFace
      .frontFace(vk::FrontFace::eClockwise) // VK is opposite openGL default, GL_CCW face is front
      .cullMode(vk::CullModeFlagBits::eBack) // openGL default, GL_BACK is the face to be culled
      .depthTestEnable( VK_TRUE )
      .depthCompareOp(vk::CompareOp::eLess)
      .blendBegin(VK_TRUE)
      .blendEnable(VK_TRUE)
      .blendSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
      .blendSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      .blendDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
      .blendDstAlphaBlendFactor(vk::BlendFactor::eOne)
      .blendColorBlendOp(vk::BlendOp::eAdd)
      .blendAlphaBlendOp(vk::BlendOp::eAdd)
      .createUnique(device, fw.pipelineCache(), *pipelineLayoutReflectionPlane, *reflectionplaneRenderPass);

    ////////////////////////////////////////
    //
    // Build a RenderPassBeginInfo for reflectionplane
    //
  
    // Build the framebuffer.
    vk::ImageView attachmentsreflectionplane[] = {reflectionFbo.imageView()};
    vk::FramebufferCreateInfo fbmi{{}, *reflectionplaneRenderPass, sizeof(attachmentsreflectionplane)/sizeof(vk::ImageView), attachmentsreflectionplane, window.width(), window.height(), reflectionFbo.info().arrayLayers };
    reflectionplaneFrameBuffer = device.createFramebufferUnique(fbmi);
  
    // Match in order of attachments to clear; the image.
    std::array<vk::ClearValue, sizeof(attachmentsreflectionplane)/sizeof(vk::ImageView)> clearColoursreflectionplane{
      vk::ClearColorValue{}
    };
  
    // Begin rendering using the framebuffer and renderpass
    reflectionplaneRpbi = vk::RenderPassBeginInfo{
      .renderPass = *reflectionplaneRenderPass,
      .framebuffer = *reflectionplaneFrameBuffer,
      .renderArea = vk::Rect2D{{0, 0}, {window.width(), window.height()}},
      .clearValueCount = (uint32_t) clearColoursreflectionplane.size(),
      .pClearValues = clearColoursreflectionplane.data()
    };
  }
*/
  ////////////////////////////////////////
  //
  // Main update loop


  // Vulkan clip space has inverted Y and half Z.
  const glm::mat4 clip(1.0f,  0.0f, 0.0f, 0.0f,
                       0.0f, -1.0f, 0.0f, 0.0f,
                       0.0f,  0.0f, 0.5f, 0.0f,
                       0.0f,  0.0f, 0.5f, 1.0f);

  Uniform_vert uniform_vert {
    .projection = clip * glm::perspective(
      glm::radians(30.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90° (extra wide) and 30° (quite zoomed in)
      float(window.width())/window.height(), // Aspect Ratio. Depends on the size of your window. Notice that 4/3 == 800/600 == 1280/960, sounds familiar ?
      0.1f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
      10.0f),              // Far clipping plane. Keep as little as possible.
    .view = glm::lookAt(
      glm::vec3(0,0,-7),  // Camera location in World Space
      glm::vec3(0,0,0),   // and looks at the origin
      glm::vec3(0,1,0)),  // Head is up (set to 0,-1,0 to look upside-down)
    .world = glm::rotate( 0.5f*0/60.0f, glm::normalize(glm::vec3(1,1,1)) ),
  };

  Uniform_vert uniformContent_vert {
    .projection = clip * glm::perspective(
      glm::radians(30.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90° (extra wide) and 30° (quite zoomed in)
      float(window.width())/window.height(), // Aspect Ratio. Depends on the size of your window. Notice that 4/3 == 800/600 == 1280/960, sounds familiar ?
      0.1f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
      10.0f),              // Far clipping plane. Keep as little as possible.
    .view = glm::lookAt(
      glm::vec3(0,0,-7),  // Camera location in World Space
      glm::vec3(0,0,0),   // and looks at the origin
      glm::vec3(0,1,0)),  // Head is up (set to 0,-1,0 to look upside-down)
    .world = glm::mat4(1.0f),
  };
 
  int iFrame = 0;
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    Uniform_frag uniform_frag {
      .resolution = glm::vec2{window.width(), window.height()},
      .tick = float(iFrame),
      .borderWidth = 0.008f,
      .displacementLength = 0.028f,
      .reflectionOpacity = 0.3,
      .scene = 3, // {1,2,3}={displacement,mask,logo}
    };

    PushConstant_frag pushConstant_frag {
      .face = CubeFaces::BACK,           // use samplerCube u_reflection if face is -1 else use sampler2D u_texture
                                         // Note, determine face = (cullFace == FRONT ? -1 : 1)
                                         // BACK cullFace pass 1: reflectionFbo, FRONT cullFace pass 2: contentFbo
      .typeId = CubeTypes::DISPLACEMENT, // {1,2,3}={displacement,mask,logo}
    };

    UniformContent_frag uniformcontent_frag {
      .resolution = glm::vec2{window.width(), window.height()},
      .tick = float(iFrame),
    };

    PushConstantContent_frag pushConstantContent_frag {
      .maskId = CubeMasks::M1,
      .typeId = ContentTypes::RAINBOW,
    };

    UniformReflectionReflector_frag uniformReflectionReflector_frag {
      .depthOpacity=0.0f };

    window.draw(device, fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {

        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);

        // Pass: Render the displacement into the displacementFbo
        cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform_vert), &uniform_vert);
        cb.updateBuffer(ubo_frag.buffer(), 0, sizeof(Uniform_frag), &uniform_frag);
        cb.beginRenderPass(displacementRpbi, vk::SubpassContents::eInline);
        cb.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));
        cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *displacementPipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets, nullptr);
        pushConstant_frag.face = CubeFaces::BACK;
        pushConstant_frag.typeId = CubeTypes::DISPLACEMENT;
        cb.pushConstants(
          *pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstant_frag), (const void*)&pushConstant_frag
        );
        cb.drawIndexed(indices.size(), 1, 0, 0, 0);
        cb.endRenderPass();

        // Pass: Render the mask into the maskFbo
        cb.beginRenderPass(maskRpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *maskPipeline);
        pushConstant_frag.face = CubeFaces::BACK;
        pushConstant_frag.typeId = CubeTypes::MASK;
        cb.pushConstants(
          *pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstant_frag), (const void*)&pushConstant_frag
        );
        cb.drawIndexed(indices.size(), 1, 0, 0, 0);
        cb.endRenderPass();

        // Pass: Render into the contentFbo
        cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform_vert), &uniformContent_vert);
        cb.updateBuffer(uboContent_frag.buffer(), 0, sizeof(UniformContent_frag), &uniformcontent_frag);
        cb.beginRenderPass(contentRpbi, vk::SubpassContents::eInline);
        cb.bindVertexBuffers(0, bufferContent.buffer(), vk::DeviceSize(0));
        cb.bindIndexBuffer(iboContent.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineContent);
        for(std::size_t counter = 0; counter < ContentTextures.size(); ++counter) {
          pushConstantContent_frag.maskId = ContentTextures[counter].maskId;
          pushConstantContent_frag.typeId = ContentTextures[counter].typeId;
          cb.pushConstants(
            *pipelineLayoutContent, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstantContent_frag), (const void*)&pushConstantContent_frag
          );
          cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutContent, 0, descriptorSetsContent[counter], nullptr);
          cb.drawIndexed(indicesContent.size(), 1, 0, 0, 0);
        }
        cb.endRenderPass();

        // Pass: Render contentFbo into each face of the cubemap of reflectionreflectorFbo
        cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform_vert), &uniform_vert);
        cb.updateBuffer(uboReflectionReflector_frag.buffer(), 0, sizeof(UniformReflectionReflector_frag), &uniformReflectionReflector_frag);
        cb.beginRenderPass(reflectionreflectorRpbi, vk::SubpassContents::eInline);
        cb.bindVertexBuffers(0, bufferContent.buffer(), vk::DeviceSize(0));
        cb.bindIndexBuffer(iboContent.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineReflectionReflector);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutReflectionReflector, 0, descriptorSetsReflectionReflector, nullptr);
        cb.drawIndexed(indicesContent.size(), 1, 0, 0, 0);
        cb.endRenderPass();

        // Pass: Cube Cull front, CubeTypes::FINAL
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));
        cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineCULLFRONT);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets, nullptr);
        pushConstant_frag.face = CubeFaces::FRONT;
        pushConstant_frag.typeId = CubeTypes::FINAL;
        cb.pushConstants(
          *pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstant_frag), (const void*)&pushConstant_frag
        );
        cb.drawIndexed(indices.size(), 1, 0, 0, 0);

        // Pass : Cube Cull Back, CubeTypes::FINAL
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineCULLBACK);
        pushConstant_frag.face = CubeFaces::BACK;
        pushConstant_frag.typeId = CubeTypes::FINAL;
        cb.pushConstants(
          *pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstant_frag), (const void*)&pushConstant_frag
        );
        cb.drawIndexed(indices.size(), 1, 0, 0, 0);
        cb.endRenderPass();

        cb.end();
      }
    );

    uniform_vert.world = glm::rotate( 0.5f*1/60.0f, glm::normalize(glm::vec3(6,2,0)) ) * uniform_vert.world;
    uniform_vert.world = mouse_rotation * uniform_vert.world;

    // Very crude method to prevent your GPU from overheating.
    //std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  // Wait until all drawing is done and then kill the window.
  device.waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  // The Framework and Window objects will be destroyed here.

  return 0;
}
