#define VKU_GLFW
#include <vku/vku_framework.hpp>
#include <glm/glm.hpp>

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  auto *title = "swirltexture";
  auto glfwwindow = glfwCreateWindow(800, 800, title, nullptr, nullptr);

  {
  vku::Framework fw{title};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }

  auto device = fw.device();

  vku::Window window(
    fw.instance(),
    device,
    fw.physicalDevice(),
    fw.graphicsQueueFamilyIndex(),
    glfwwindow
  );
  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }

  ////////////////////////////////////////
  //
  // Define the shader modules

  vku::ShaderModule vert{device, BINARY_DIR "swirltexture.vert.spv"};
  vku::ShaderModule frag{device, BINARY_DIR "swirltexture.frag.spv"};

  ////////////////////////////////////////
  //
  // Create a Uniform Buffer Object

  struct Uniform { 
    float t = 0.0;
    float pad[3];
  } uniform;

  auto ubo = vku::UniformBuffer{device, fw.memprops(), sizeof(Uniform)};
  ubo.upload(device, fw.memprops(), window.commandPool(), fw.graphicsQueue(), uniform);

  ////////////////////////////////////////////////////////
  //
  // Create a 2x2 multi-color checkerboard texture

  std::vector<uint8_t> pixels = { 
  // R     G     B     A
    0xff, 0xff, 0xff, 0xff, // White
    0x00, 0xff, 0x00, 0xff, // Green  
    0x00, 0x00, 0xff, 0xff, // Blue
    0xff, 0x00, 0x00, 0xff, // Red
  };
  auto texture = vku::TextureImage2D{device, fw.memprops(), 2, 2, 1, vk::Format::eR8G8B8A8Unorm};
  texture.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());

  ////////////////////////////////////////
  //
  // Create a mesh

  struct Vertex { 
    glm::vec3 pos; 
    glm::vec2 uv;
  };

  const std::vector<Vertex> vertices = {
    {.pos={-1.0f,-1.0f, 0.0f}, .uv={-1.0f,-1.0f}},
    {.pos={ 1.0f,-1.0f, 0.0f}, .uv={ 1.0f,-1.0f}},
    {.pos={ 1.0f, 1.0f, 0.0f}, .uv={ 1.0f, 1.0f}},
    {.pos={-1.0f, 1.0f, 0.0f}, .uv={-1.0f, 1.0f}},
  };
  vku::HostVertexBuffer vbo(device, fw.memprops(), vertices);

  ////////////////////////////////////////
  //
  // Create mesh indices

  std::vector<uint32_t> indices = {
    0, 1, 2, 
    2, 3, 0
  };
  vku::HostIndexBuffer ibo(device, fw.memprops(), indices);

  ////////////////////////////////////////
  //
  // Create samplers
 
  auto sampler = vku::SamplerMaker{}
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Build the descriptor sets

  auto layout = vku::DescriptorSetLayoutMaker{}
    .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1)
    .image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .createUnique(device);

  auto descriptorSets = vku::DescriptorSetMaker{}
    .layout(*layout)
    .create(device, fw.descriptorPool());

  ////////////////////////////////////////
  //
  // Update the descriptor sets for the shader uniforms.

  vku::DescriptorSetUpdater{}
    .beginDescriptorSet(descriptorSets[0])
    // Set initial uniform buffer value
    .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
    .buffer(ubo.buffer(), 0, sizeof(Uniform))
    // Set initial sampler value
    .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
    .image(*sampler, texture.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
    .update(device);

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  //
  auto pipelineLayout = vku::PipelineLayoutMaker{}
    .descriptorSetLayout(*layout)
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Build the pipeline

  auto buildPipeline = [&]() {
    return vku::PipelineMaker{ window.width(), window.height() }
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .vertexBinding(0, sizeof(Vertex))
      .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv))
      .createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
  };
  auto pipeline = buildPipeline();

  const float dt = 1./16.;
  while (!glfwWindowShouldClose(glfwwindow) && glfwGetKey(glfwwindow, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
    glfwPollEvents();

    int width, height;
    glfwGetWindowSize(glfwwindow, &width, &height);
    if (width==0 || height==0) continue;

    window.draw(device, fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        // deal with resizing of window
        static auto ww = window.width();
        static auto wh = window.height();
        if (ww != window.width() || wh != window.height()) {
          ww = window.width();
          wh = window.height();
          pipeline = buildPipeline();
        }

        cb.begin(vk::CommandBufferBeginInfo{});
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
        cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets[0], nullptr);
        // Barrier since updateBuffer is treated as a transfer operation
        ubo.barrier(
          cb,
          vk::PipelineStageFlagBits::eFragmentShader, //srcStageMask
          vk::PipelineStageFlagBits::eTransfer, //dstStageMask
          vk::DependencyFlagBits::eByRegion, //dependencyFlags
          vk::AccessFlagBits::eShaderRead, //srcAccessMask
          vk::AccessFlagBits::eTransferWrite, //dstAccessMask
          fw.graphicsQueueFamilyIndex(), //srcQueueFamilyIndex
          fw.graphicsQueueFamilyIndex() //dstQueueFamilyIndex
        );
        cb.updateBuffer(
          ubo.buffer(), 0, sizeof(Uniform), (const void*)&uniform
        );
        ubo.barrier(
          cb,
          vk::PipelineStageFlagBits::eTransfer, //srcStageMask
          vk::PipelineStageFlagBits::eFragmentShader, //dstStageMask
          vk::DependencyFlagBits::eByRegion, //dependencyFlags
          vk::AccessFlagBits::eTransferWrite, //srcAccessMask
          vk::AccessFlagBits::eShaderRead, //dstAccessMask
          fw.graphicsQueueFamilyIndex(), //srcQueueFamilyIndex
          fw.graphicsQueueFamilyIndex() //dstQueueFamilyIndex
        );
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.drawIndexed(indices.size(), 1, 0, 0, 0);
        cb.endRenderPass();
        cb.end();
      }    
    );

    uniform.t += dt;
  }

  device.waitIdle();
  } // all Vulkan objects destroyed here, before GLFW teardown
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
