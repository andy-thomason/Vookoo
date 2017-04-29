
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>

int main() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "firstgame";
  auto glfwwindow = glfwCreateWindow(800, 600, title, nullptr, nullptr);

  vku::Framework fw{title};
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

  vku::ShaderModule vert_{device, BINARY_DIR "firstgame.vert.spv"};
  vku::ShaderModule frag_{device, BINARY_DIR "firstgame.frag.spv"};

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

  // Our sprite class.
  // The vertex shader does most of the work of drawing the sprites
  // which are all quads drawn as instances.
  struct Sprite {
    glm::vec2 pos;
    glm::vec2 size;
    int sprite = 0;
    float angle = 0;
    glm::vec4 colour;
  };

  constexpr int maxSprites = 256;
  std::vector<Sprite> sprites(maxSprites);

  ////////////////////////////////////////
  //
  // Build the pipeline

  // For this example we are using instancing and the eInstance vertex rate
  // Which gives one value per four vertices in this case.
  vku::PipelineMaker pm{window.width(), window.height()};
  pm.blendBegin(VK_TRUE);
  pm.topology(vk::PrimitiveTopology::eTriangleFan);
  pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
  pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
  pm.vertexBinding(0, (uint32_t)sizeof(Sprite), vk::VertexInputRate::eInstance);
  pm.vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Sprite, pos));
  pm.vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Sprite, size));
  pm.vertexAttribute(2, 0, vk::Format::eR32Sint, (uint32_t)offsetof(Sprite, sprite));
  pm.vertexAttribute(3, 0, vk::Format::eR32Sfloat, (uint32_t)offsetof(Sprite, angle));
  pm.vertexAttribute(4, 0, vk::Format::eR32G32B32A32Sfloat, (uint32_t)offsetof(Sprite, colour));

  std::vector<vku::GenericBuffer> vboStaging;
  for (auto &i : window.images()) {
    vboStaging.emplace_back(fw.device(), fw.memprops(), vk::BufferUsageFlagBits::eTransferSrc, sizeof(Sprite) * maxSprites);
  }

  vku::GenericBuffer vbo{
    fw.device(), fw.memprops(),
    vk::BufferUsageFlagBits::eTransferDst|vk::BufferUsageFlagBits::eVertexBuffer,
    sizeof(Sprite) * maxSprites, vk::MemoryPropertyFlagBits::eDeviceLocal
  };

  struct Uniform {
    glm::vec4 pixelsToScreen;
  };

  vku::UniformBuffer ubo(fw.device(), fw.memprops(), sizeof(Uniform));

  auto renderPass = window.renderPass();
  auto &cache = fw.pipelineCache();
  auto pipeline = pm.createUnique(device, cache, *pipelineLayout, renderPass);

  ////////////////////////////////////////
  //
  // Create a texture

  // Create an image, memory and view for the texture on the GPU.
  uint32_t pixels_width = 64;
  uint32_t pixels_height = 64;
  vku::TextureImage2D texture{device, fw.memprops(), pixels_width, pixels_height, 1, vk::Format::eR8G8B8A8Unorm};

  auto pixels = vku::loadFile(SOURCE_DIR "firstgame.data");
  vku::GenericBuffer stagingBuffer{device, fw.memprops(), vk::BufferUsageFlagBits::eTransferSrc, pixels.size()};
  stagingBuffer.updateLocal(device, pixels.data(), pixels.size());

  // Copy the staging buffer to the GPU texture and set the layout.
  vku::executeImmediately(device, window.commandPool(), fw.graphicsQueue(), [&](vk::CommandBuffer cb) {
    texture.copy(cb, stagingBuffer.buffer(), 0, 0, pixels_width, pixels_height, 1, 0);
    texture.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal);
  });

  // Free the staging buffer.
  stagingBuffer = vku::GenericBuffer{};

  ////////////////////////////////////////
  //
  // Update the descriptor sets for the shader uniforms.

  vku::SamplerMaker sm{};
  vk::UniqueSampler sampler = sm.createUnique(device);

  vku::DescriptorSetUpdater update;
  update.beginDescriptorSet(descriptorSets[0]);

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
      std::array<float, 4> clearColorValue{0.1f, 0.1f, 0.1f, 1};
      vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
      std::array<vk::ClearValue, 2> clearColours{vk::ClearValue{clearColorValue}, clearDepthValue};
      rpbi.pClearValues = clearColours.data();

      vk::CommandBufferBeginInfo bi{};
      cb.begin(bi);
      cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
      cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets, nullptr);
      cb.draw(4, maxSprites, 0, 0);
      cb.endRenderPass();
      cb.end();
    }
  );

  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }

  int frameNumber = 0;
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    //sprites[0].angle += 0.01f;
    sprites[0].size = glm::vec2(100, 100);
    if (frameNumber % 16 == 0) {
      sprites[0].sprite++;
      sprites[0].sprite &= 3;
    }

    frameNumber++;

    window.draw(fw.device(), fw.graphicsQueue(),
      [&](vk::CommandBuffer pscb, int imageIndex) {
        Uniform uniform{};
        float rw = 2.0f/window.width();
        float rh = 2.0f/window.height();
        uniform.pixelsToScreen = glm::vec4(rw, rh, 0, 0);

        vboStaging[imageIndex].updateLocal(fw.device(), sprites);

        // Record the dynamic buffer.
        vk::CommandBufferBeginInfo bi{};
        pscb.begin(bi);

        // Copy the uniform data to the buffer. (note this is done
        // inline and so we can discard "uniform" afterwards)
        pscb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform), (void*)&uniform);

        // The vertex buffer is too big to update through updateBuffer, so we'll use a staging buffer.
        // we can't just write to vbo as it may be in use by other command buffers.
        vk::BufferCopy region{0, 0, sizeof(Sprite) * maxSprites};
        pscb.copyBuffer(vboStaging[imageIndex].buffer(), vbo.buffer(), region);

        pscb.end();
      }
    );
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  device.waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
