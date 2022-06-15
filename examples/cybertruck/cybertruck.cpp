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

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  auto *title = "CyberTruck";
  auto glfwwindow = glfwCreateWindow(1280, 720, title, nullptr, nullptr);

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
  window.dumpCaps(std::cout, fw.physicalDevice());

  ////////////////////////////////////////
  //
  // Create Uniform Buffer

  struct Uniform {
    glm::vec4 iResolution; // viewport resolution (in pixels)
    glm::vec4 iTime;       // render time (in seconds).
    glm::ivec4 iFrame;     // shader playback frame
    glm::vec4 iMouse;      // mouse pixel coords. xy: current (if MLB down), zw: click
  };
  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer ubo(device, fw.memprops(), sizeof(Uniform));

  ////////////////////////////////////////
  //
  // Create Mesh vertices
 
  struct Vertex { 
    glm::vec2 pos; 
  };

  // fullscreen quad
  const std::vector<Vertex> vertices = {
    {.pos={ 1.0f,  1.0f}},
    {.pos={-1.0f,  1.0f}},
    {.pos={ 1.0f, -1.0f}},

    {.pos={ 1.0f, -1.0f}},
    {.pos={-1.0f,  1.0f}},
    {.pos={-1.0f, -1.0f}},
  };
  vku::HostVertexBuffer buffer(device, fw.memprops(), vertices);

  ////////////////////////////////////////
  //
  // Build the descriptor sets

  vku::DescriptorSetLayoutMaker dslm{};
  dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1);

  auto descriptorSetLayout = dslm.createUnique(device);

  vku::DescriptorSetMaker dsm{};
  dsm.layout(*descriptorSetLayout);

  auto descriptorSets = dsm.create(device, fw.descriptorPool());

  ////////////////////////////////////////
  //
  // Update the descriptor sets for the shader uniforms.

  vku::DescriptorSetUpdater dsu;
  dsu.beginDescriptorSet(descriptorSets[0])
     // Set initial uniform buffer value
     .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
     .buffer(ubo.buffer(), 0, sizeof(Uniform))
     // update the descriptor sets with their pointers (but not data).
     .update(device);

  ////////////////////////////////////////
  //
  // Build the final pipeline
  // Make a pipeline to use the vertex format and shaders.

  // Create two shaders, vertex and fragment.
  vku::ShaderModule vert{device, BINARY_DIR "cybertruck.vert.spv"};
  vku::ShaderModule frag{device, BINARY_DIR "cybertruck.frag.spv"};

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  vku::PipelineLayoutMaker plm{};
  plm.descriptorSetLayout(*descriptorSetLayout);

  auto pipelineLayout = plm.createUnique(device);

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
  
    vku::PipelineMaker pm{window.width(), window.height()};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(Vertex))
      .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos))
      .viewport(viewport) // viewport set to match openGL, affects cullMode and frontFace
      .frontFace(vk::FrontFace::eCounterClockwise) // openGL default, GL_CCW face is front
      .cullMode(vk::CullModeFlagBits::eBack); // openGL default, GL_BACK is the face to be culled
  
    // Create a pipeline using a renderPass built for our window.
    return pm.createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
  };
  auto pipeline = buildPipeline();

  ////////////////////////////////////////
  //
  // Main update loop
 
  int iFrame = 0;
  double xpos=window.width()/2.0, ypos=window.height()/2.0;
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    int state = glfwGetMouseButton(glfwwindow, GLFW_MOUSE_BUTTON_LEFT);
    if (state) {
      glfwGetCursorPos(glfwwindow, &xpos, &ypos);
    };

    static auto ww = window.width();
    static auto wh = window.height();
    if (ww != window.width() || wh != window.height()) {
      ww = window.width();
      wh = window.height();
      pipeline = buildPipeline();
    }

    Uniform uniform {
      .iResolution = glm::vec4(ww, wh, 1., 0.),
      .iTime = glm::vec4{iFrame/60.0f, 0.0, 0.0, 0.0},
      .iFrame = glm::ivec4{iFrame++, 0, 0, 0},
      .iMouse = glm::vec4{xpos, wh-ypos, state*xpos, state*ypos},
    };

    window.draw(device, fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {

        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);

        // Copy the uniform data to the buffer.
        cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform), &uniform);

        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets, nullptr);
        cb.draw(vertices.size(), 1, 0, 0);
        cb.endRenderPass();

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
