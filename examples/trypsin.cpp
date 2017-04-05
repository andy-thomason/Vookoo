
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <gilgamesh/mesh.hpp>
#include <gilgamesh/decoders/pdb_decoder.hpp>

int main() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "trypsin";
  auto glfwwindow = glfwCreateWindow(1600, 1200, title, nullptr, nullptr);

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

  vku::ShaderModule vert_{device, BINARY_DIR "trypsin.vert.spv"};
  vku::ShaderModule frag_{device, BINARY_DIR "trypsin.frag.spv"};

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

  std::ifstream f(SOURCE_DIR "trypsin.pdb", std::ios::binary|std::ios::ate);
  if (f.bad()) {
    std::cout << "could not open file\n";
    return 1;
  }

  size_t size = f.tellg();
  std::vector<uint8_t> pdb_text(size);
  f.seekg(0);
  f.read((char*)pdb_text.data(), size);
  gilgamesh::pdb_decoder pdb(pdb_text.data(), pdb_text.data() + pdb_text.size());

  std::string chains = pdb.chains();
  auto atoms = pdb.atoms(chains);

  struct Vertex { glm::vec3 pos; float radius; glm::vec3 colour; };
  std::vector<Vertex> vertices;

  glm::vec3 mean(0);
  for (auto &atom : atoms) {
    glm::vec3 pos(atom.x(), atom.y(), atom.z());
    mean += pos;
  }
  mean /= (float)atoms.size();

  for (auto &atom : atoms) {
    glm::vec3 pos(atom.x(), atom.y(), atom.z());
    glm::vec3 colour = atom.colorByElement();
    vertices.emplace_back(Vertex{pos - mean, 1.0f, colour});
  }
  //std::vector<uint32_t> indices = mesh.indices32();

  ////////////////////////////////////////
  //
  // Build the pipeline including enabling the depth test

  vku::PipelineMaker pm{window.width(), window.height()};
  pm.topology(vk::PrimitiveTopology::ePointList);
  pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
  pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
  pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
  pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, pos));
  pm.vertexAttribute(1, 0, vk::Format::eR32Sfloat, (uint32_t)offsetof(Vertex, radius));
  pm.vertexAttribute(2, 0, vk::Format::eR32G32B32A32Sfloat, (uint32_t)offsetof(Vertex, colour));
  pm.depthTestEnable(VK_TRUE);
  pm.cullMode(vk::CullModeFlagBits::eBack);
  pm.frontFace(vk::FrontFace::eClockwise);

  vku::VertexBuffer vbo(fw.device(), fw.memprops(), vertices);
  //vku::IndexBuffer ibo(fw.device(), fw.memprops(), indices);
  //uint32_t indexCount = (uint32_t)indices.size();

  using mat4 = glm::mat4;
  using vec4 = glm::vec4;
  struct Uniform {
    mat4 modelToPerspective;
    mat4 modelToWorld;
    mat4 normalToWorld;
    vec4 colour;
    float pointScale;
  };

  glm::mat4 cameraToPerspective = glm::perspective(glm::radians(45.0f), (float)window.width()/window.height(), 0.1f, 1000.0f);
  glm::mat4 modelToWorld = glm::translate(glm::mat4{}, glm::vec3(0, 0, -50));
  //modelToWorld = glm::rotate(modelToWorld, glm::radians(90.0f), glm::vec3(1, 0, 0));
  

  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer ubo(fw.device(), fw.memprops(), sizeof(Uniform));

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
  window.setStaticCommands(
    [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
      vk::CommandBufferBeginInfo bi{};
      cb.begin(bi);
      cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
      cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
      //cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets, nullptr);
      cb.draw((uint32_t)vertices.size(), 1, 0, 0);
      cb.endRenderPass();
      cb.end();
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
        // Generate the uniform buffer inline in the command buffer.
        // This is good for small buffers only!
        Uniform uniform;
        //modelToWorld = glm::rotate(modelToWorld, glm::radians(1.0f), glm::vec3(0, 0, 1));
        modelToWorld = glm::rotate(modelToWorld, glm::radians(1.0f), glm::vec3(0, 1, 0));
        uniform.modelToPerspective = cameraToPerspective * modelToWorld;
        uniform.normalToWorld = modelToWorld;
        uniform.pointScale = (float)window.width();

        // Record the dynamic buffer.
        vk::CommandBufferBeginInfo bi{};
        pscb.begin(bi);
        pscb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform), (void*)&uniform);
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
