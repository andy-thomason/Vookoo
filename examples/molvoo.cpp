
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <gilgamesh/mesh.hpp>
#include <gilgamesh/decoders/pdb_decoder.hpp>
#include <vector>

class RaytracePipeline {
public:
  RaytracePipeline() {
  }

  RaytracePipeline(vk::Device device, vk::DescriptorPool descriptorPool, vk::PipelineCache cache, vk::PhysicalDeviceMemoryProperties memprops, vk::RenderPass renderPass, uint32_t width, uint32_t height) {
    ////////////////////////////////////////
    //
    // Build the shader modules

    vert_ = vku::ShaderModule{device, BINARY_DIR "molvoo.vert.spv"};
    frag_ = vku::ShaderModule{device, BINARY_DIR "molvoo.frag.spv"};

    ////////////////////////////////////////
    //
    // Build the descriptor sets

    vku::DescriptorSetLayoutMaker dslm{};
    dslm.buffer(0U, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 1);
    layout_ = dslm.createUnique(device);

    vku::DescriptorSetMaker dsm{};
    dsm.layout(*layout_);
    descriptorSets_ = dsm.create(device, descriptorPool);

    vku::PipelineLayoutMaker plm{};
    plm.descriptorSetLayout(*layout_);
    pipelineLayout_ = plm.createUnique(device);

    ////////////////////////////////////////
    //
    // Build the pipeline including enabling the depth test

    vku::PipelineMaker pm{width, height};
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

    pipeline_ = pm.createUnique(device, cache, *pipelineLayout_, renderPass);

    // Create, but do not upload the uniform buffer as a device local buffer.
    ubo_ = vku::UniformBuffer(device, memprops, sizeof(Uniform));

    ////////////////////////////////////////
    //
    // Update the descriptor sets for the shader uniforms.

    vku::SamplerMaker sm{};
    vk::UniqueSampler sampler = sm.createUnique(device);

    vku::DescriptorSetUpdater update;
    update.beginDescriptorSet(descriptorSets_[0]);

    // Set initial uniform buffer value
    update.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
    update.buffer(ubo_.buffer(), 0, sizeof(Uniform));

    update.update(device);
  }

  void update(vk::CommandBuffer cb, const glm::mat4 &cameraToPerspective, const glm::mat4 &modelToWorld) {
    // Generate the uniform buffer inline in the command buffer.
    // This is good for small buffers only!
    Uniform uniform;
    //modelToWorld = glm::rotate(modelToWorld, glm::radians(1.0f), glm::vec3(0, 0, 1));
    //modelToWorld = glm::rotate(modelToWorld, glm::radians(1.0f), glm::vec3(0, 1, 0));
    uniform.modelToPerspective = cameraToPerspective * modelToWorld;
    uniform.normalToWorld = modelToWorld;
    uniform.pointScale = 256; //(float)window.width();
    cb.updateBuffer(ubo_.buffer(), 0, sizeof(Uniform), (void*)&uniform);
  }

  struct Vertex {
    glm::vec3 pos; float radius; glm::vec3 colour;
  };
  
  using mat4 = glm::mat4;
  using vec4 = glm::vec4;
  struct Uniform {
    mat4 modelToPerspective;
    mat4 modelToWorld;
    mat4 normalToWorld;
    vec4 colour;
    float pointScale;
  };

  vk::Pipeline pipeline() const { return *pipeline_; }
  vk::PipelineLayout pipelineLayout() const { return *pipelineLayout_; }
  const std::vector<vk::DescriptorSet> &descriptorSets() const { return descriptorSets_; }
private:
  vk::UniquePipeline pipeline_;
  vk::UniqueDescriptorSetLayout layout_;
  std::vector<vk::DescriptorSet> descriptorSets_;
  vk::UniquePipelineLayout pipelineLayout_;
  vku::ShaderModule vert_;
  vku::ShaderModule frag_;
  vku::UniformBuffer ubo_;
};

class MoleculeModel {
public:
  MoleculeModel() {
  }

  MoleculeModel(const std::string &filename, vk::Device device, vk::PhysicalDeviceMemoryProperties memprops) {
    auto pdb_text = vku::loadFile(filename);
    gilgamesh::pdb_decoder pdb(pdb_text.data(), pdb_text.data() + pdb_text.size());

    std::string chains = pdb.chains();
    auto atoms = pdb.atoms(chains);

    glm::vec3 mean(0);
    for (auto &atom : atoms) {
      glm::vec3 pos(atom.x(), atom.y(), atom.z());
      mean += pos;
    }
    mean /= (float)atoms.size();

    typedef RaytracePipeline::Vertex Vertex;
    std::vector<Vertex> vertices;
    for (auto &atom : atoms) {
      glm::vec3 pos(atom.x(), atom.y(), atom.z());
      glm::vec3 colour = atom.colorByElement();
      vertices.emplace_back(Vertex{pos - mean, 1.0f, colour});
    }

    vbo_ = vku::VertexBuffer(device, memprops, vertices);
    numVertices_ = (uint32_t)vertices.size();
  }

  void draw(vk::CommandBuffer cb, const RaytracePipeline &raytracePipeline) {
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, raytracePipeline.pipeline());
    cb.bindVertexBuffers(0, vbo_.buffer(), vk::DeviceSize(0));
    //cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, raytracePipeline.pipelineLayout(), 0, raytracePipeline.descriptorSets(), nullptr);
    cb.draw(numVertices_, 1, 0, 0);
  }
private:
  vku::VertexBuffer vbo_;
  uint32_t numVertices_;
};

class Molvoo {
public:
  Molvoo(int argc, char **argv) {
    run();
  }
private:
  void run() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    const char *title = "molvoo";
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

    glm::mat4 cameraToPerspective = glm::perspective(glm::radians(45.0f), (float)window.width()/window.height(), 0.1f, 1000.0f);
    glm::mat4 modelToWorld = glm::translate(glm::mat4{}, glm::vec3(0, 0, -50));
    //modelToWorld = glm::rotate(modelToWorld, glm::radians(90.0f), glm::vec3(1, 0, 0));
  

    raytracePipeline_ = RaytracePipeline(device, fw.descriptorPool(), fw.pipelineCache(), fw.memprops(), window.renderPass(), window.width(), window.height());
    moleculeModel_ = MoleculeModel(SOURCE_DIR "molvoo.pdb", device, fw.memprops());

    // Set the static render commands for the main renderpass.
    window.setStaticCommands(
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        moleculeModel_.draw(cb, raytracePipeline_);
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
          // Record the dynamic buffer.
          vk::CommandBufferBeginInfo bi{};
          pscb.begin(bi);
          raytracePipeline_.update(pscb, cameraToPerspective, modelToWorld);
          pscb.end();
        }
      );

      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    device.waitIdle();
    glfwDestroyWindow(glfwwindow);
    glfwTerminate();
  }

  RaytracePipeline raytracePipeline_;
  MoleculeModel moleculeModel_;
};

int main(int argc, char **argv) {
  Molvoo viewer(argc, argv);
  return 0;
}
