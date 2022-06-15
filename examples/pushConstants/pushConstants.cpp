////////////////////////////////////////////////////////////////////////////////
//
// Vookoo triangle example (C) 2017 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//
// In this sample we demonstrate push constants which are an easy way to
// pass small shader parameters without synchonisation issues.
//
// Compare this file with helloTriangle.cpp to see what we have done.

// Include the demo framework, vookoo (vku) for building objects and glm for maths.
// The demo framework uses GLFW to create windows.
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp> // for rotate()

int main() {
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  const char *title = "pushConstants";
  auto glfwwindow = glfwCreateWindow(800, 800, title,  nullptr, nullptr);

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

  // Create two shaders, vertex and fragment. See the files pushConstants.vert
  // and pushConstants.frag for details.
  vku::ShaderModule vert{device, BINARY_DIR "pushConstants.vert.spv"};
  vku::ShaderModule frag{device, BINARY_DIR "pushConstants.frag.spv"};

  // These are the parameters we are passing to the shaders
  // Note! be very careful when using vec3, vec2, float and vec4 together
  // as there are alignment rules you must follow.
  struct PushConstant {
    glm::vec4 colour;
    glm::mat4 rotation;
  };

  std::vector<PushConstant> P = { 
    {.colour = glm::vec4{1, 1, 1, 1}, .rotation = glm::scale(glm::mat4{1}, glm::vec3(2.0f,2.0f,2.0f))},
    {.colour = glm::vec4{1, 1, 1, 1}, .rotation = glm::mat4{1}},
  };

  // We will use this simple vertex description.
  // It has a 2D location (x, y) and a colour (r, g, b)
  struct Vertex { 
    glm::vec2 pos;
    glm::vec3 colour;
  };

  // This is our triangle.
  const std::vector<Vertex> vertices = {
    {.pos={ 0.0f,-0.5f}, .colour={1.0f, 0.0f, 0.0f}},
    {.pos={ 0.5f, 0.5f}, .colour={0.0f, 1.0f, 0.0f}},
    {.pos={-0.5f, 0.5f}, .colour={0.0f, 0.0f, 1.0f}},
  };
  vku::HostVertexBuffer buffer(device, fw.memprops(), vertices);

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  // 
  vku::PipelineLayoutMaker plm{};
  plm.pushConstantRange(vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstant));
  auto pipelineLayout = plm.createUnique(device);

  auto buildPipeline = [&]() {
    // Make a pipeline to use the vertex format and shaders.
    vku::PipelineMaker pm{ window.width(), window.height() };
    return pm
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(Vertex))
      .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, colour))
      .createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
  };
  auto pipeline = buildPipeline();

  int frame = 0;

  // Loop waiting for the window to close.
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    P.back().rotation = glm::rotate(P.back().rotation, glm::radians(1.0f), glm::vec3(0, 0, 1));
    P.back().colour.r = (std::sin(frame * 0.01f) + 1.0f) / 2.0f;
    P.back().colour.g = (std::cos(frame * 0.01f) + 1.0f) / 2.0f;

    // Unlike helloTriangle, we generate the command buffer dynamically
    // because it will contain different values on each frame.
    window.draw(
      device, fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        static auto ww = window.width();
        static auto wh = window.height();
        if (ww != window.width() || wh != window.height()) {
          ww = window.width();
          wh = window.height();
          pipeline = buildPipeline();
        }
        vk::CommandBufferBeginInfo cbbi{};
        cb.begin(cbbi);
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cb.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));

        // draw multiple triangles with different push constants.
        for (const PushConstant &p : P) {
          // Instead of updateBuffer() we use pushConstants()
          // This has an effective max of only about 128 bytes.
          // This takes a copy of the push constant at the time 
          // we create this command buffer.
          cb.pushConstants(
            *pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstant), (const void*)&p
          );
          cb.draw(vertices.size(), 1, 0, 0);
        };

        cb.endRenderPass();
        cb.end();
      }
    );

    // Very crude method to prevent your GPU from overheating.
    std::this_thread::sleep_for(std::chrono::milliseconds(16));

    ++frame;
  }

  // Wait until all drawing is done and then kill the window.
  device.waitIdle();

  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  // The Framework and Window objects will be destroyed here.

  return 0;
}
