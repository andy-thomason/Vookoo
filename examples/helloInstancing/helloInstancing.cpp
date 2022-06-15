////////////////////////////////////////////////////////////////////////////////
//
// Vookoo instancing example (C) 2017 Andy Thomason
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
  auto *title = "helloInstancing";
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

  // Create a window to draw into
  vku::Window window{fw.instance(), fw.device(), fw.physicalDevice(), fw.graphicsQueueFamilyIndex(), glfwwindow};
  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }
  window.clearColorValue() = {0.0f, 0.0f, 0.0f, 1.0f};

  // Create shaders; vertex and fragment.
  vku::ShaderModule vert{fw.device(), BINARY_DIR "helloInstancing.vert.spv"};
  vku::ShaderModule frag{fw.device(), BINARY_DIR "helloInstancing.frag.spv"};

  // We will use this simple vertex description.
  // It has a 2D location (x, y) and a colour (r, g, b)
  struct Vertex { 
    glm::vec2 pos; 
    glm::vec3 colour;
  };

  // This is our mesh of triangles.
  const std::vector<Vertex> vertices = {
    {.pos={ 0.5f,  0.5f}, .colour={0.0f, 1.0f, 0.0f}},
    {.pos={-0.5f,  0.5f}, .colour={0.0f, 0.0f, 1.0f}},
    {.pos={ 0.5f, -0.5f}, .colour={1.0f, 0.0f, 0.0f}},

    {.pos={ 0.5f, -0.5f}, .colour={1.0f, 0.0f, 0.0f}},
    {.pos={-0.5f,  0.5f}, .colour={0.0f, 0.0f, 1.0f}},
    {.pos={-0.5f, -0.5f}, .colour={0.0f, 0.0f, 0.0f}},
  };
  vku::HostVertexBuffer bufferVertices(fw.device(), fw.memprops(), vertices);

  // Per-instance data block
	struct Instance {
		glm::vec3 pos;
		glm::vec3 rot;
		float scale;
	};

  // This is instance data.
  const std::vector<Instance> instances = {
    {.pos={ 0.5f,  0.5f, 0.0f}, .rot={0.0f, 0.0f,-1.0f}, .scale=0.25f},
    {.pos={-0.5f, -0.5f, 0.0f}, .rot={0.0f, 0.0f, 1.0f}, .scale=0.50f},
    {.pos={ 0.0f,  0.0f, 0.0f}, .rot={0.0f, 0.0f, 0.5f}, .scale=0.10f},
    {.pos={ 0.2f,  0.1f, 0.0f}, .rot={0.0f, 0.0f, 0.2f}, .scale=0.05f},
  };
  vku::HostVertexBuffer bufferInstances(fw.device(), fw.memprops(), instances);

  // Create a pipeline using a renderPass built for our window.
  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  vku::PipelineLayoutMaker plm{};
  auto pipelineLayout = plm.createUnique(fw.device());

  auto buildPipeline = [&]() {
    // Make a pipeline to use the vertex format and shaders.
    vku::PipelineMaker pm{window.width(), window.height()};
    return pm
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      #define VERTEX_BUFFER_BIND_ID 0
      .vertexBinding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), vk::VertexInputRate::eVertex)
      .vertexAttribute(0, VERTEX_BUFFER_BIND_ID,    vk::Format::eR32G32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, colour))
      #define INSTANCE_BUFFER_BIND_ID 1
      .vertexBinding(INSTANCE_BUFFER_BIND_ID, sizeof(Instance), vk::VertexInputRate::eInstance)
      .vertexAttribute(2, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Instance, pos))
      .vertexAttribute(3, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Instance, rot))
      .vertexAttribute(4, INSTANCE_BUFFER_BIND_ID,       vk::Format::eR32Sfloat, offsetof(Instance, scale))
      //
      .createUnique(fw.device(), fw.pipelineCache(), *pipelineLayout, window.renderPass());
  };

  auto pipeline = buildPipeline(); 

  // Static command buffer, so only need to create the command buffer(s) once.
  window.setStaticCommands(
    [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
      static auto ww = window.width();
      static auto wh = window.height();
      if (ww != window.width() || wh != window.height()) {
        ww = window.width();
        wh = window.height();
        pipeline = buildPipeline();
      }
      vk::CommandBufferBeginInfo bi{};
      cb.begin(bi);
      cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
      cb.bindVertexBuffers(  VERTEX_BUFFER_BIND_ID,  bufferVertices.buffer(), vk::DeviceSize(0)); // Binding point VERTEX_BUFFER_BIND_ID : Mesh vertex buffer
      cb.bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, bufferInstances.buffer(), vk::DeviceSize(0)); // Binding point INSTANCE_BUFFER_BIND_ID : Instance data buffer
      cb.draw(vertices.size(), instances.size(), 0, 0);
      cb.endRenderPass();
      cb.end();
    }
  );

  // Loop waiting for the window to close.
  int iFrame = 0;
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    // reuse previously recorded command buffer (defined in window.setStaticCommands)
    window.draw(fw.device(), fw.graphicsQueue());

    // animate (map... change Instance ...unmap)
    Instance* objects = static_cast<Instance*>( bufferInstances.map(fw.device()) );
    objects[0].scale = 0.5*cos(iFrame*6.248*16e-3) + 0.5;
    objects[1].pos = glm::vec3{-0.5f,-0.5f, 0.0f} + 0.25f*glm::vec3{cos(iFrame*6.248*16e-3),cos(iFrame*6.248*16e-3),0.0};
    objects[2].rot += glm::vec3{0.0f, 0.0f,-6.248*16e-3};
    objects[3].pos = glm::vec3{ 0.2f,  0.1f, 0.0f} + 0.25f*glm::vec3{cos(iFrame*6.248*16e-3),sin(iFrame*2*6.248*16e-3),0.0};
    bufferInstances.unmap(fw.device());
    // better performance expected, if move map&unmap outside animation loop
    // and instead, after all objects animated/updated, do bufferInstances.flush(fw.device()) here inside animation loop.
    // [reference: http://kylehalladay.com/blog/tutorial/vulkan/2017/08/13/Vulkan-Uniform-Buffers.html]

    // Very crude method to prevent your GPU from overheating.
    std::this_thread::sleep_for(std::chrono::milliseconds(16));

    iFrame++;
  }

  // Wait until all drawing is done and then kill the window.
  fw.device().waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  // The Framework and Window objects will be destroyed here.

  return 0;
}
