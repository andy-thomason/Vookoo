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
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp> // for rotate, scale, translate

int main() {
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  auto *title = "dynamicUniformBuffer";
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

  ////////////////////////////////////////
  //
  // Create Uniform Buffer

  struct PER_OBJECT {
    glm::mat4 MVP;
  };

  // fill with values (local)
  std::vector<PER_OBJECT> objects = {
    { .MVP = glm::mat4(1.) },
    { .MVP = glm::rotate(glm::radians(45.f), glm::vec3(0, 0, 1)) }
  };

  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer ubo(device, fw.memprops(), objects.size()*sizeof(PER_OBJECT));

  ////////////////////////////////////////
  //
  // Create Mesh vertices

  // We will use this simple vertex description.
  // It has a 2D location (x, y) and a colour (r, g, b)
  struct Vertex { 
    glm::vec2 pos; 
    glm::vec3 colour;
  };

  const std::vector<Vertex> vertices = {
    {.pos={ 0.5f,  0.5f}, .colour={0.0f, 1.0f, 0.0f}},
    {.pos={-0.5f,  0.5f}, .colour={0.0f, 0.0f, 1.0f}},
    {.pos={ 0.5f, -0.5f}, .colour={1.0f, 0.0f, 0.0f}},

    {.pos={ 0.5f, -0.5f}, .colour={1.0f, 0.0f, 0.0f}},
    {.pos={-0.5f,  0.5f}, .colour={0.0f, 0.0f, 1.0f}},
    {.pos={-0.5f, -0.5f}, .colour={0.0f, 0.0f, 0.0f}},
  };
  vku::HostVertexBuffer buffer(device, fw.memprops(), vertices);

  ////////////////////////////////////////
  //
  // Build the descriptor sets

  vku::DescriptorSetLayoutMaker dslm{};
  auto descriptorSetLayout = dslm
    // layout (binding = 0) uniform PER_OBJECT
    .buffer(0, vk::DescriptorType::eUniformBufferDynamic, vk::ShaderStageFlagBits::eVertex, 1)
    .createUnique(device);

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  vku::PipelineLayoutMaker plm{};
  auto pipelineLayout = plm
    .descriptorSetLayout(*descriptorSetLayout)
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Define the particular descriptor sets for the shader uniforms.

  vku::DescriptorSetMaker dsm{};
  auto descriptorSets = dsm
    .layout(*descriptorSetLayout)
    .create(device, fw.descriptorPool());

  vku::DescriptorSetUpdater dsu;
  dsu
    //-- descriptorSets[0]
    .beginDescriptorSet(descriptorSets[0])
    // layout (binding = 0) uniform PER_OBJECT
    .beginBuffers(0, 0, vk::DescriptorType::eUniformBufferDynamic)
    .buffer(ubo.buffer(), 0, sizeof(PER_OBJECT))

    //-- update the descriptor sets with their pointers (but not data).
    .update(device);

  ////////////////////////////////////////
  //
  // Build the final pipeline

  // Create two shaders, vertex and fragment. See the files dynamicUniformBuffer.vert
  // and dynamicUniformBuffer.frag for details.
  vku::ShaderModule vert{device, BINARY_DIR "dynamicUniformBuffer.vert.spv"};
  vku::ShaderModule frag{device, BINARY_DIR "dynamicUniformBuffer.frag.spv"};

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

  // Loop waiting for the window to close.
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    window.draw(device, fw.graphicsQueue(),
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

        cb.updateBuffer(ubo.buffer(), 0, objects.size()*sizeof(PER_OBJECT), &objects[0]); // validation error if inside {beginRenderPass...endRenderPass}
        // We may or may not need this barrier. It is probably a good precaution.
        ubo.barrier(
          cb,
          vk::PipelineStageFlagBits::eHost, //srcStageMask
          vk::PipelineStageFlagBits::eFragmentShader, //dstStageMask
          vk::DependencyFlagBits::eByRegion, //dependencyFlags
          vk::AccessFlagBits::eHostWrite, //srcAccessMask
          vk::AccessFlagBits::eShaderRead, //dstAccessMask
          fw.graphicsQueueFamilyIndex(), //srcQueueFamilyIndex
          fw.graphicsQueueFamilyIndex() //dstQueueFamilyIndex
        );

        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cb.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));
        for(unsigned int i=0; i<objects.size(); ++i) {
          uint32_t offset = i * static_cast<uint32_t>(sizeof(PER_OBJECT)); // offset is key to demonstrating dynamicUniformBuffer 
          cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, {descriptorSets[0]}, {offset});
          cb.draw(vertices.size(), 1, 0, 0);
        }
        cb.endRenderPass();

        cb.end();
      }
    );

    // animate transforms locally (next frame, syncronize to GPU via cb.updateBuffer(ubo.buffer()...)
    objects[0].MVP *= glm::rotate(glm::radians(-0.5f), glm::vec3(0, 0, 1));
    objects[1].MVP *= glm::rotate(glm::radians( 1.0f), glm::vec3(0, 0, 1));

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
