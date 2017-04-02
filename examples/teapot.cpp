
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>

#include <gilgamesh/mesh.hpp>
#include <gilgamesh/scene.hpp>
#include <gilgamesh/decoders/fbx_decoder.hpp>
#include <gilgamesh/encoders/fbx_encoder.hpp>

int main() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "teapot";
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

  vku::ShaderModule vert_{device, BINARY_DIR "teapot.vert.spv"};
  vku::ShaderModule frag_{device, BINARY_DIR "teapot.frag.spv"};

  ////////////////////////////////////////
  //
  // Build the descriptor sets

  vku::DescriptorSetLayoutMaker dslm{};
  dslm.buffer(0U, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 1);
  auto layout = dslm.createUnique(device);

  vku::DescriptorSetMaker dsm{};
  dsm.layout(*layout);
  auto descriptorSets = dsm.create(device, fw.descriptorPool());

  vku::PipelineLayoutMaker plm{};
  plm.descriptorSetLayout(*layout);
  auto pipelineLayout = plm.createUnique(device);

  gilgamesh::fbx_decoder decoder;
  gilgamesh::scene scene;
  auto filename = SOURCE_DIR "teapot.fbx";
  if (!decoder.loadScene<gilgamesh::color_mesh>(scene, filename)) {
    std::cerr << "unable to open file " << filename << "\n";
    return 1;
  }

  struct Vertex { glm::vec3 pos; glm::vec3 normal; };
  std::vector<Vertex> vertices;

  const gilgamesh::mesh &mesh = *scene.meshes()[0];
  auto meshpos = mesh.pos();
  auto meshnormal = mesh.normal();
  for (size_t i = 0; i != meshpos.size(); ++i) {
    glm::vec3 pos = meshpos[i];
    glm::vec3 normal = meshnormal[i];
    vertices.emplace_back(Vertex{pos, normal});
  }
  std::vector<uint32_t> indices = scene.meshes()[0]->indices32();

  ////////////////////////////////////////
  //
  // Build the pipeline

  vku::PipelineMaker pm{window.width(), window.height()};
  pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
  pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
  pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
  pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, pos));
  pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));

  vku::VertexBuffer vbo(fw.device(), fw.memprops(), vertices);
  vku::IndexBuffer ibo(fw.device(), fw.memprops(), indices);
  uint32_t indexCount = (uint32_t)indices.size();

  struct Uniform {
    glm::mat4 modelToPerspective;
    glm::mat4 modelToWorld;
    glm::mat3 normalToWorld;
    glm::vec4 colour;
  };

  glm::mat4 cameraToPerspective = glm::perspective(glm::radians(45.0f), (float)window.width()/window.height(), 0.1f, 100.0f);
  glm::mat4 modelToWorld = glm::translate(glm::mat4{}, glm::vec3(0, 0, -4));
  
  Uniform uniform;
  uniform.modelToPerspective = cameraToPerspective * modelToWorld;

  vku::UniformBuffer ubo(fw.device(), fw.memprops(), uniform);

  auto renderPass = window.renderPass();
  auto &cache = fw.pipelineCache();
  auto pipeline = pm.createUnique(device, cache, *pipelineLayout, renderPass);

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

  update.update(device);

  // Set the static render commands for the main renderpass.
  window.setRenderCommands(
    [&](vk::CommandBuffer cb, int imageIndex) {
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
      cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
      cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets, nullptr);
      cb.drawIndexed(indexCount, 1, 0, 0, 0);
    }
  );

  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }

  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    window.draw(fw.device(), fw.graphicsQueue(),
      [&](vk::CommandBuffer pscb, int imageIndex) {
        modelToWorld = glm::rotate(modelToWorld, glm::radians(1.0f), glm::vec3(0, 0, 1));
        uniform.modelToPerspective = cameraToPerspective * modelToWorld;
        vk::CommandBufferBeginInfo bi{};
        pscb.begin(bi);
        pscb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform), (void*)&uniform);
        pscb.end();
        //std::cout << vku::format("i%d %f\n", imageIndex, uniform.modelToPerspective[0].x );
      }
    );

    //std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  device.waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
