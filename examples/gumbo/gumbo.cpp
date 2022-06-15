////////////////////////////////////////////////////////////////////////////////
//
// Vookoo triangle example (C) 2017 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//
// Excercises quad tesselation and geometry shaders with Bezier Surface representation of gumbo
// original: https://prideout.net/blog/old/blog/index.html@p=49.html
// modified here to be compatible with vulkan

// Include the demo framework, vookoo (vku) for building objects and glm for maths.
// The demo framework uses GLFW to create windows.
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
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

////////////////////////////////////////
//
// 
int main() {
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  auto *title = "gumbo";
  auto glfwwindow = glfwCreateWindow(800, 800, title, nullptr, nullptr);

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
  auto device = fw.device();

  // Create a window to draw into
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
  window.clearColorValue() = {0.703f, 0.602f, 0.5f, 1.0f};

  // Create shaders.
  vku::ShaderModule vert{device, BINARY_DIR "gumbo.vert.spv"};
  vku::ShaderModule tesc{device, BINARY_DIR "gumbo.tesc.spv"};
  vku::ShaderModule tese{device, BINARY_DIR "gumbo.tese.spv"};
  vku::ShaderModule geom{device, BINARY_DIR "gumbo.geom.spv"};
  vku::ShaderModule frag{device, BINARY_DIR "gumbo.frag.spv"};

  // These are the parameters we are passing to the shaders
  // Note! be very careful when using vec3, vec2, float and vec4 together
  // as there are alignment rules you must follow.
  struct Uniform {
    glm::mat4 Projection;
    glm::mat4 View;
    glm::mat4 GeometryTransform;
    float TessLevelInner;
    float TessLevelOuter;
    float PatchID;
    float padding; // padding ensures struct size is required multiple of minUniformBufferOffsetAlignment (64bytes on my PC)
  };

  std::vector<Uniform> U = { 
    { .Projection=glm::perspective(
        glm::radians(30.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90° (extra wide) and 30° (quite zoomed in)
        float(window.width())/window.height(), // Aspect Ratio. Depends on the size of your window. Notice that 4/3 == 800/600 == 1280/960, sounds familiar ?
        0.1f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
        10.0f),              // Far clipping plane. Keep as little as possible.,
      .View=glm::lookAt(
        glm::vec3(0,0,2.5f), // Camera location in World Space
        glm::vec3(0,0,0),    // Camera looks at the origin
        glm::vec3(0,1,0)),   // Head is up (set to 0,-1,0 to look upside-down),
      .GeometryTransform=glm::mat4{0.f, 0.f, -1.f, 0.f, -1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f},
      .TessLevelInner = 3.f,
      .TessLevelOuter = 3.f,
      .PatchID = 97.f,
    },
  };
  // Read the pushConstants example first.
  // 
  // Create a uniform buffer capable of N=U.size() "struct Uniform"s.
  // We cannot update these buffers with normal memory writes
  // because reading the buffer may happen at any time.
  auto ubo = vku::UniformBuffer{device, fw.memprops(), sizeof(Uniform)*U.size()};

  #include "GumboTranslatedScaledTransposed97to108.h"
  vku::HostVertexBuffer buffer(device, fw.memprops(), vertices);

  // Build a template for descriptor sets that use these shaders.
  vku::DescriptorSetLayoutMaker dslm{};
  auto descriptorSetLayout = dslm
    .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll, 1)
    .createUnique(device);

  // We need to create a descriptor set to tell the shader where
  // our buffers are.
  vku::DescriptorSetMaker dsm{};
  auto descriptorSets = dsm
    .layout(*descriptorSetLayout) // for U[0]
    .create(device, fw.descriptorPool());

  // Next we need to update the descriptor set with the uniform buffer.
  vku::DescriptorSetUpdater update;
  update
    // U[0]
    .beginDescriptorSet(descriptorSets[0])
    .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
    .buffer(ubo.buffer(), 0*sizeof(Uniform), sizeof(Uniform))
    // 
    .update(device);

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  vku::PipelineLayoutMaker plm{};
  auto pipelineLayout = plm
    .descriptorSetLayout(*descriptorSetLayout)
    .createUnique(device);

  auto buildPipeline = [&]() {
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

    // Make a pipeline to use the vertex format and shaders.
    vku::PipelineMaker pm{ window.width(), window.height() };
    return pm
      .topology(vk::PrimitiveTopology::ePatchList)
      .setPatchControlPoints(16)
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eTessellationControl, tesc)
      .shader(vk::ShaderStageFlagBits::eTessellationEvaluation, tese)
      .shader(vk::ShaderStageFlagBits::eGeometry, geom)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      #define VERTEX_BUFFER_BIND_ID 0
      .vertexBinding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex))
      .vertexAttribute(0, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, pos))
      //
      .viewport(viewport) // viewport set to match openGL, affects cullMode and frontFace
      .depthTestEnable(VK_TRUE)
      .cullMode(vk::CullModeFlagBits::eBack) // GL default
      //.cullMode(vk::CullModeFlagBits::eFront) // VK default
      .frontFace(vk::FrontFace::eCounterClockwise) // GL default
      //.frontFace(vk::FrontFace::eClockwise) // VK default
      //
      .createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
  };
  auto pipeline = buildPipeline();

  // Static scene, so only need to create the command buffer(s) once.
  window.setStaticCommands(
    [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
      static auto ww = window.width();
      static auto wh = window.height();
      if (ww != window.width() || wh != window.height()) {
        ww = window.width();
        wh = window.height();
        pipeline = buildPipeline();
        U[0].Projection = glm::perspective(
          glm::radians(30.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90° (extra wide) and 30° (quite zoomed in)
          float(window.width())/window.height(), // Aspect Ratio. Depends on the size of your window. Notice that 4/3 == 800/600 == 1280/960, sounds familiar ?
          0.1f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
          10.0f);
      }
      vk::CommandBufferBeginInfo cbbi{};
      cb.begin(cbbi);
      cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
      cb.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets[0], nullptr);
      cb.draw(vertices.size(), 1, 0, 0);
      cb.endRenderPass();
      cb.end();
    }
  );

  // Loop waiting for the window to close.
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    // Update dynamic uniforms
    U[0].GeometryTransform = mouse_rotation * U[0].GeometryTransform;

    // draw patches.
    window.draw(
      device, fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {

        vk::CommandBufferBeginInfo cbbi{};
        cb.begin(cbbi);

        // Instead of pushConstants() we use updateBuffer(). This has a max of 64k.
        // Like pushConstants(), this takes a copy of the uniform buffer
        // at the time we create this command buffer.
        // Unlike push constant update, uniform buffer must be updated
        // _OUTSIDE_ of the (beginRenderPass ... endRenderPass)
        cb.updateBuffer(
          ubo.buffer(), 0, sizeof(Uniform)*U.size(), (const void*)&U[0]
        );

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
