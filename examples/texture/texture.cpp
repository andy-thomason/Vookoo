
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>

int main() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "texture";
  auto glfwwindow = glfwCreateWindow(800, 600, title, nullptr, nullptr);

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

  vk::Device device = fw.device();

  vku::Window window{fw.instance(), fw.device(), fw.physicalDevice(), fw.graphicsQueueFamilyIndex(), glfwwindow};
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
  // Build the descriptor sets

  vku::DescriptorSetLayoutMaker dslm{};
  dslm.buffer(0U, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 1);
  dslm.image(1U, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
  auto layout = dslm.createUnique(device);

  vku::DescriptorSetMaker dsm{};
  dsm.layout(*layout);
  auto descriptorSets = dsm.create(device, fw.descriptorPool());

  vku::PipelineLayoutMaker plm{};
  plm.descriptorSetLayout(*layout);
  auto pipelineLayout = plm.createUnique(device);

  struct Vertex { glm::vec2 pos; glm::vec3 colour; };

  const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
  };

  ////////////////////////////////////////
  //
  // Build the pipeline

  auto buildPipeline = [&]() {
    vku::PipelineMaker pm{window.width(), window.height()};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
    pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
    pm.vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, pos));
    pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, colour));

    auto renderPass = window.renderPass();
    auto cache = fw.pipelineCache();
    return  pm.createUnique(device, cache, *pipelineLayout, renderPass);
  };

  auto pipeline = buildPipeline();

  ////////////////////////////////////////
  //
  // Create a texture

  vku::TextureImage2D texture{device, fw.memprops(), 2, 2, 1, vk::Format::eR8G8B8A8Unorm};
  std::vector<uint8_t> pixels = { 0xff, 0xff, 0xff, 0xff,  0x00, 0xff, 0xff, 0xff,  0xff, 0x00, 0xff, 0xff,  0xff, 0xff, 0x00, 0xff, };
  texture.upload(device, pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());

  ////////////////////////////////////////
  //
  // Update the descriptor sets for the shader uniforms.

  vku::SamplerMaker sm{};
  vk::UniqueSampler sampler = sm.createUnique(device);

  vku::DescriptorSetUpdater update;
  update.beginDescriptorSet(descriptorSets[0]);

  struct Uniform { glm::vec4 colour; };
  Uniform uniform;
  uniform.colour = glm::vec4(0, 1, 1, 1);
  vku::UniformBuffer ubo(fw.device(), fw.memprops(), sizeof(Uniform));
  ubo.upload(device, fw.memprops(), window.commandPool(), fw.graphicsQueue(), uniform);
  vku::HostVertexBuffer vbo(fw.device(), fw.memprops(), vertices);

  // Set initial uniform buffer value
  update.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
  update.buffer(ubo.buffer(), 0, sizeof(Uniform));

  // Set initial sampler value
  update.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
  update.image(*sampler, texture.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

  update.update(device);

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
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout,
                              0, descriptorSets, nullptr);
        cb.draw(3, 1, 0, 0);
        cb.endRenderPass();
        cb.end();
      });

  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }

  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();
    window.draw(fw.device(), fw.graphicsQueue());
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  device.waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
