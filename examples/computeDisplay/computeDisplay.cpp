////////////////////////////////////////////////////////////////////////////////
//
// Vookoo compute and graphics example (C) 2018 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//

#define VKU_GLFW
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>

int main() {

  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  const char *title = "computeDisplay";
  auto glfwwindow = glfwCreateWindow(800, 800, title,  nullptr, nullptr);
  
  // Define framework options
  vku::FrameworkOptions fo = {
    .useCompute = true
  };

  // Initialise the Vookoo demo framework.
  vku::Framework fw{"computeDisplay", fo};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }

  // Get some convenient aliases from the framework.
  auto device = fw.device();
  auto cache = fw.pipelineCache();
  auto descriptorPool = fw.descriptorPool();
  auto memprops = fw.memprops();

  // Create a window to draw into
  vku::Window window(
    fw.instance(),
    device,
    fw.physicalDevice(),
    fw.graphicsQueueFamilyIndex(),
    glfwwindow,
    { 
      .desiredPresentMode = vk::PresentModeKHR::eImmediate
    }
  );
  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }

  ////////////////////////////////////////
  //
  // Define the vertex Buffer

  struct Vertex { glm::vec2 pos; glm::vec2 uv; };

  const std::vector<Vertex> vertices = {
    {{-0.9f,-0.9f}, {0.0f, 0.0f}},
    {{ 0.9f,-0.9f}, {1.0f, 0.0f}},
    {{ 0.9f, 0.9f}, {1.0f, 1.0f}},

    {{ 0.9f, 0.9f}, {1.0f, 1.0f}},
    {{-0.9f, 0.9f}, {0.0f, 1.0f}},
    {{-0.9f,-0.9f}, {0.0f, 0.0f}},
  };

  vku::HostVertexBuffer vbo(device, memprops, vertices);

  ////////////////////////////////////////
  //
  // Create a Storage buffer (written in compute and read in fragment shaders)
  // Note: this won't work for everyone. With some devices you
  // may need to explictly upload and download data.
  const int Nx = 128, Ny = 128;
  auto mybuf = vku::GenericBuffer(
      device, 
      memprops, 
      vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc, 
      Nx*Ny * sizeof(float), 
      vk::MemoryPropertyFlagBits::eHostVisible
  );

  ////////////////////////////////////////
  //
  // Create Push Constant Buffer
  // Up to 256 bytes of immediate data.
  struct PushConstants {
    float time;
    int Nx, Ny;
    float pad[1];  // Buffers are usually 16 byte aligned.
  };

  PushConstants pushValues{
    .time = 0,
    .Nx = Nx,
    .Ny = Ny,
  };

  ////////////////////////////////////////
  //
  // Build the descriptor sets
  // Shader has access to a single storage buffer.
  auto dsetLayout = vku::DescriptorSetLayoutMaker{}
    .buffer(0U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute|vk::ShaderStageFlagBits::eFragment, 1)
    .createUnique(device);

  // The descriptor set itself.
  auto descriptorSets = vku::DescriptorSetMaker{} 
    .layout(*dsetLayout)
    .create(device, descriptorPool);

  ////////////////////////////////////////
  //
  // Update the descriptor sets for the shader uniforms.
  vku::DescriptorSetUpdater{}
    .beginDescriptorSet(descriptorSets[0])
    .beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer)
    .buffer(mybuf.buffer(), 0, Nx*Ny * sizeof(float))
    .update(device); // this only copies the pointer, not any data.

  ////////////////////////////////////////
  //
  // Pipeline layout.
  auto pipelineLayoutGraphics = vku::PipelineLayoutMaker{}
    .descriptorSetLayout(*dsetLayout)
    .pushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstants))
    .createUnique(device);

  auto pipelineLayoutCompute = vku::PipelineLayoutMaker{}
    .descriptorSetLayout(*dsetLayout)
    .pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants))
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Specialization constants.
  // Define localwork group dimensions to use in compute shader.
  int local_size_x = 32, local_size_y = 32;
  std::vector<vku::SpecConst> specializations{
    {0, local_size_x},
    {1, local_size_y} };
 
  ////////////////////////////////////////
  //
  // Build the compute pipeline 
  vku::ShaderModule comp_{device, BINARY_DIR "computeDisplay.comp.spv"};

  auto pipelineCompute = vku::ComputePipelineMaker{}
    .shader(vk::ShaderStageFlagBits::eCompute, comp_, specializations)
    .createUnique(device, cache, *pipelineLayoutCompute);

  ////////////////////////////////////////
  //
  // Build the graphics pipeline
  vku::ShaderModule vert_{device, BINARY_DIR "computeDisplay.vert.spv"};
  vku::ShaderModule frag_{device, BINARY_DIR "computeDisplay.frag.spv"};

  auto buildPipelineGraphics = [&]() {
    return vku::PipelineMaker{ window.width(), window.height() }
      .shader(vk::ShaderStageFlagBits::eVertex, vert_)
      .shader(vk::ShaderStageFlagBits::eFragment, frag_)
      .vertexBinding(0, (uint32_t)sizeof(Vertex))
      .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, pos))
      .vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv))
      .createUnique(device, cache, *pipelineLayoutGraphics, window.renderPass());
  };
  auto pipelineGraphics = buildPipelineGraphics();

  // Loop waiting for the window to close.
  while (!glfwWindowShouldClose(glfwwindow) && glfwGetKey(glfwwindow, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
    glfwPollEvents();

    int width, height;
    glfwGetWindowSize(glfwwindow, &width, &height);
    if (width==0 || height==0) continue;

    ////////////////////////////////////////
    //
    // Display full screen quad & texture on the GPU.
    window.draw(
      device, fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        static auto ww = window.width();
        static auto wh = window.height();
        if (ww != window.width() || wh != window.height()) {
          ww = window.width();
          wh = window.height();
          pipelineGraphics = buildPipelineGraphics();
        }

        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);
        // Compute Shader
        cb.bindPipeline(vk::PipelineBindPoint::eCompute, *pipelineCompute);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayoutCompute, 0, descriptorSets[0], nullptr);
        cb.pushConstants(*pipelineLayoutCompute, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants), &pushValues);
        cb.dispatch(Nx/local_size_x,Ny/local_size_y,1); // rows/local_size_x, cols/local_size_y, 1
        // Memory Barrier
        mybuf.barrier(cb, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead, 0, 0);
        // Graphics Shader
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelineGraphics);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayoutGraphics, 0, descriptorSets[0], nullptr);
        cb.pushConstants(*pipelineLayoutGraphics, vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstants), &pushValues);
        cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
        cb.draw(vertices.size(), 1, 0, 0);
        cb.endRenderPass();
        cb.end();
    });

    pushValues.time += 1/60.;
  }
  device.waitIdle();

  ////////////////////////////////////////
  //
  // Print result of compute shader
//  float *values = static_cast<float*>( mybuf.map(device) );
//  std::for_each(values, values+Nx*Ny, [](float &value){ 
//    std::cout << value << " "; 
//  });
//  std::cout << std::endl;
//  mybuf.unmap(device);
}
