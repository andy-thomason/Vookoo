#define VKU_GLFW
#include <vku/vku_framework.hpp>
#include <glm/glm.hpp>

int main() {

  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "texture";
  auto glfwwindow = glfwCreateWindow(800, 800, title, nullptr, nullptr);

  {
  // Initialise the Vookoo demo framework.
  vku::Framework fw{title};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }

  // Get some convenient aliases from the framework.
  auto device = fw.device();

  // Create a window to draw into
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
  // Build the shader modules

  vku::ShaderModule vert_{device, BINARY_DIR "texture.vert.spv"};
  vku::ShaderModule frag_{device, BINARY_DIR "texture.frag.spv"};

  ////////////////////////////////////////
  //
  // Create a vertex buffer object
 
  struct Vertex { glm::vec2 pos; glm::vec2 uv; };

  const std::vector<Vertex> vertices = {
    {{ 0.0f,-0.5f}, {1.0f, 0.0f}},
    {{ 0.5f, 0.5f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f}}
  };
  vku::HostVertexBuffer vbo(fw.device(), fw.memprops(), vertices);

  ////////////////////////////////////////
  //
  // Create a texture

  std::vector<uint8_t> pixels = { 0xff,0xff,0xff,0xff,  0x00,0x00,0xff,0xff,
                                  0x00,0xff,0x00,0xff,  0xff,0x00,0x00,0xff };
  auto texture = vku::TextureImage2D{device, fw.memprops(), 2, 2};
  texture.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());

  auto sampler = vku::SamplerMaker{}
    .createUnique(device);

  ////////////////////////////////////////
  //
  // Build the descriptor sets

  auto layout = vku::DescriptorSetLayoutMaker{}
   .image(0U, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
   .createUnique(device);

  auto descriptorSets = vku::DescriptorSetMaker{}
   .layout(*layout)
   .create(device, fw.descriptorPool());

  ////////////////////////////////////////
  //
  // Update the descriptor sets for the shader uniforms.

  vku::DescriptorSetUpdater{}
    .beginDescriptorSet(descriptorSets[0])
    // Set initial sampler value
    .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
    .image(*sampler, texture.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
    .update(device);

  ////////////////////////////////////////
  //
  // Build the pipeline

  auto pipelineLayout = vku::PipelineLayoutMaker{}
   .descriptorSetLayout(*layout)
   .createUnique(device);

  auto buildPipeline = [&]() {
    return vku::PipelineMaker{ window.width(), window.height() }
      .shader(vk::ShaderStageFlagBits::eVertex, vert_)
      .shader(vk::ShaderStageFlagBits::eFragment, frag_)
      .vertexBinding(0, (uint32_t)sizeof(Vertex))
      .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, pos))
      .vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, uv))
      .createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
  };
  auto pipeline = buildPipeline();

  // Set the static render commands for the main renderpass.
  window.setStaticCommands(
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        static auto ww = window.width();
        static auto wh = window.height();
        if (window.width() != ww || window.height() != wh) {
          ww = window.width();
          wh = window.height();
          pipeline = buildPipeline();
        }
        
        cb.begin(vk::CommandBufferBeginInfo{});
        // Image memory barrier 
        texture.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal);
        // Graphics Shader
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets[0], nullptr);
        cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
        cb.draw(vertices.size(), 1, 0, 0);
        cb.endRenderPass();
        cb.end();
      });

  while (!glfwWindowShouldClose(glfwwindow) && glfwGetKey(glfwwindow, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
    glfwPollEvents();

    int width, height;
    glfwGetWindowSize(glfwwindow, &width, &height);
    if (width==0 || height==0) continue;

    window.draw(fw.device(), fw.graphicsQueue());
  }

  // Wait until all drawing is done and then kill the window.
  device.waitIdle();
  } // all Vulkan objects destroyed here, before GLFW teardown
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
