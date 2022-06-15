////////////////////////////////////////////////////////////////////////////////
//
// Vookoo triangle example (C) 2017 Andy Thomason
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
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  auto *title = "helloGeometryShader";
  auto glfwwindow = glfwCreateWindow(800, 800, title, nullptr, nullptr);

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

  // Create two shaders, vertex and fragment. See the files helloGeometryShader.vert
  // and helloGeometryShader.frag for details.
  vku::ShaderModule vert{device, BINARY_DIR "helloGeometryShader.vert.spv"};
  vku::ShaderModule geom{device, BINARY_DIR "helloGeometryShader.geom.spv"};
  vku::ShaderModule frag{device, BINARY_DIR "helloGeometryShader.frag.spv"};

  // We will use this simple vertex description.
  // It has a 2D location (x, y) and a colour (r, g, b)
  struct Vertex { 
    glm::vec2 pos; 
    glm::vec3 colour;
  };

  // This is our triangles.
  const std::vector<Vertex> vertices = {
    {.pos={ 0.5f,  0.5f}, .colour={0.0f, 1.0f, 0.0f}},
    {.pos={-0.5f,  0.5f}, .colour={0.0f, 0.0f, 1.0f}},
    {.pos={ 0.5f, -0.5f}, .colour={1.0f, 0.0f, 0.0f}},

    {.pos={ 0.5f, -0.5f}, .colour={1.0f, 0.0f, 0.0f}},
    {.pos={-0.5f,  0.5f}, .colour={0.0f, 0.0f, 1.0f}},
    {.pos={-0.5f, -0.5f}, .colour={0.0f, 0.0f, 0.0f}},
  };
  vku::HostVertexBuffer buffer(device, fw.memprops(), vertices);

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  vku::PipelineLayoutMaker plm{};
  auto pipelineLayout = plm.createUnique(device);

  auto buildPipeline = [&]() {
    // Make a pipeline to use the vertex format and shaders.
    vku::PipelineMaker pm{ window.width(), window.height() };
    return pm
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eGeometry, geom)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(Vertex))
      .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, colour))
      .createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
  };
  auto pipeline = buildPipeline();

  // Static scene, so only need to create the command buffer(s) once.
  window.setStaticCommands(
    [&pipeline, &buffer, &window, &vertices, &buildPipeline](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
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
      cb.draw(vertices.size(), 1, 0, 0);
      cb.endRenderPass();
      cb.end();
    }
  );

  // Loop waiting for the window to close.
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    // draw one triangle.
    window.draw(device, fw.graphicsQueue());

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
