
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

  RaytracePipeline(vk::Device device, vk::DescriptorPool descriptorPool, vk::PipelineCache cache, vk::PhysicalDeviceMemoryProperties memprops, vk::RenderPass renderPass, uint32_t width, uint32_t height, int numImageIndices) {
    ////////////////////////////////////////
    //
    // Build the shader modules

    vert_ = vku::ShaderModule{device, BINARY_DIR "molvoo.vert.spv"};
    frag_ = vku::ShaderModule{device, BINARY_DIR "molvoo.frag.spv"};
    comp_ = vku::ShaderModule{device, BINARY_DIR "molvoo.comp.spv"};

    ////////////////////////////////////////
    //
    // Build the descriptor sets

    vku::DescriptorSetLayoutMaker dslm{};
    dslm.buffer(0U, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll, 1);
    dslm.buffer(1U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1);
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
    //pm.topology(vk::PrimitiveTopology::ePointList);
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
    //pm.depthTestEnable(VK_TRUE);
    //pm.cullMode(vk::CullModeFlagBits::eBack);
    //pm.frontFace(vk::FrontFace::eClockwise);

    pipeline_ = pm.createUnique(device, cache, *pipelineLayout_, renderPass);

    vku::ComputePipelineMaker cpm{};
    cpm.shader(vk::ShaderStageFlagBits::eCompute, comp_);
    computePipeline_ = cpm.createUnique(device, cache, *pipelineLayout_);

    // Create, but do not upload the uniform buffer as a device local buffer.
    ubo_ = vku::UniformBuffer(device, memprops, sizeof(Uniform));

    size_t maxAtoms = 10000;
    size_t sbsize = maxAtoms * sizeof(Atom);
    using buf = vk::BufferUsageFlagBits;
    atoms_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer, sbsize, vk::MemoryPropertyFlagBits::eDeviceLocal);

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
    update.beginBuffers(1, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(atoms_.buffer(), 0, sbsize);

    update.update(device);
  }

  void update(vk::Device device, vk::CommandBuffer cb, uint32_t computeFamilyIndex, uint32_t graphicsFamilyIndex, const glm::mat4 &cameraToPerspective, const glm::mat4 &modelToWorld, const glm::mat4 &cameraToWorld) {
    // Generate the uniform buffer inline in the command buffer.
    // This is good for small buffers only!
    glm::mat4 worldToCamera = glm::inverse(cameraToWorld);
    Uniform uniform;
    uniform.modelToPerspective = cameraToPerspective * worldToCamera * modelToWorld;
    uniform.modelToWorld = modelToWorld;
    uniform.normalToWorld = modelToWorld;
    uniform.pointScale = 256; //(float)window.width();
    cb.updateBuffer(ubo_.buffer(), 0, sizeof(Uniform), (void*)&uniform);
  }

  using mat4 = glm::mat4;
  using vec3 = glm::vec3;
  using vec4 = glm::vec4;

  struct Atom {
    vec3 pos;
    float radius;
    vec3 colour;
  };
  
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
//private:
  vk::UniquePipeline pipeline_;
  vk::UniquePipeline computePipeline_;
  vk::UniqueDescriptorSetLayout layout_;
  std::vector<vk::DescriptorSet> descriptorSets_;
  vk::UniquePipelineLayout pipelineLayout_;
  vku::ShaderModule vert_;
  vku::ShaderModule frag_;
  vku::ShaderModule comp_;
  vku::UniformBuffer ubo_;
  vku::GenericBuffer atoms_;
};

class MoleculeModel {
public:
  MoleculeModel() {
  }

  MoleculeModel(const std::string &filename, vk::Device device, vk::PhysicalDeviceMemoryProperties memprops) {
    auto pdb_text = vku::loadFile(filename);
    gilgamesh::pdb_decoder pdb(pdb_text.data(), pdb_text.data() + pdb_text.size());

    std::string chains = pdb.chains();
    auto pdbAtoms = pdb.atoms(chains);

    glm::vec3 mean(0);
    for (auto &atom : pdbAtoms) {
      glm::vec3 pos(atom.x(), atom.y(), atom.z());
      mean += pos;
    }
    mean /= (float)pdbAtoms.size();

    std::vector<Atom> atoms_;
    for (auto &atom : pdbAtoms) {
      glm::vec3 pos(atom.x(), atom.y(), atom.z());
      glm::vec3 colour = atom.colorByElement();
      atoms_.push_back(Atom{pos - mean, 1.0f, colour});
    }

    numAtoms_ = (uint32_t)atoms_.size();
  }

  void draw(vk::CommandBuffer cb, const RaytracePipeline &raytracePipeline, int imageIndex) {
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, raytracePipeline.pipeline());
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, raytracePipeline.pipelineLayout(), 0, raytracePipeline.descriptorSets(), nullptr);
    cb.draw(6, 1, 0, 0);
  }
private:
  using Atom = RaytracePipeline::Atom;

  uint32_t numAtoms_;
};

class Molvoo {
public:
  Molvoo(int argc, char **argv) {
    init();
  }

  bool poll() { return doPoll(); }

  ~Molvoo() {
    device_.waitIdle();
    glfwDestroyWindow(glfwwindow_);
    glfwTerminate();
  }

private:
  void init() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    const char *title = "molvoo";
    glfwwindow_ = glfwCreateWindow(1600, 1200, title, nullptr, nullptr);
    glfwSetInputMode(glfwwindow_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    fw_ = vku::Framework{title};
    if (!fw_.ok()) {
      std::cout << "Framework creation failed" << std::endl;
      exit(1);
    }

    device_ = fw_.device();

    window_ = vku::Window{fw_.instance(), fw_.device(), fw_.physicalDevice(), fw_.graphicsQueueFamilyIndex(), glfwwindow_};
    if (!window_.ok()) {
      std::cout << "Window creation failed" << std::endl;
      exit(1);
    }

    //modelToWorld = glm::rotate(modelToWorld, glm::radians(90.0f), glm::vec3(1, 0, 0));
    // This matrix converts between OpenGL perspective and Vulkan perspective.
    // It flips the Y axis and shrinks the Z value to [0,1]
    glm::mat4 leftHandCorrection(
      1.0f,  0.0f, 0.0f, 0.0f,
      0.0f, -1.0f, 0.0f, 0.0f,
      0.0f,  0.0f, 0.5f, 0.0f,
      0.0f,  0.0f, 0.5f, 1.0f
    );

    moleculeState_.cameraToPerspective = leftHandCorrection * glm::perspective(glm::radians(45.0f), (float)window_.width()/window_.height(), 0.1f, 1000.0f);
    moleculeState_.modelToWorld = glm::mat4{};
    moleculeState_.cameraToWorld = glm::translate(glm::mat4{}, glm::vec3(0, 0, 50));

    raytracePipeline_ = RaytracePipeline(device_, fw_.descriptorPool(), fw_.pipelineCache(), fw_.memprops(), window_.renderPass(), window_.width(), window_.height(), window_.numImageIndices());
    moleculeModel_ = MoleculeModel(SOURCE_DIR "molvoo.pdb", device_, fw_.memprops());

    // Set the static render commands for the main renderpass.
    window_.setStaticCommands(
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        moleculeModel_.draw(cb, raytracePipeline_, imageIndex);
        cb.endRenderPass();
        cb.end();
      }
    );

    if (!window_.ok()) {
      std::cout << "Window creation failed" << std::endl;
      exit(1);
    }

    using ccbits = vk::CommandPoolCreateFlagBits;
    vk::CommandPoolCreateInfo ccpci{ ccbits::eTransient|ccbits::eResetCommandBuffer, fw_.graphicsQueueFamilyIndex() };
    computeCommandPool_ = fw_.device().createCommandPoolUnique(ccpci);
  }

  bool doPoll() {
    if (glfwWindowShouldClose(glfwwindow_)) return false;

    glfwPollEvents();

    double xpos, ypos;
    glfwGetCursorPos(glfwwindow_, &xpos, &ypos);
    int dx = int(xpos) - mouseState_.prevXpos;
    int dy = int(ypos) - mouseState_.prevYpos;
    if (!mouseState_.start) {
      float xspeed = 0.1f;
      float yspeed = 0.1f;
      moleculeState_.modelToWorld = glm::rotate(moleculeState_.modelToWorld, glm::radians(dx * xspeed), glm::vec3(0, 1, 0));
      moleculeState_.modelToWorld = glm::rotate(moleculeState_.modelToWorld, glm::radians(dy * yspeed), glm::vec3(1, 0, 0));
    }
    mouseState_.start = false;
    mouseState_.prevXpos = int(xpos);
    mouseState_.prevYpos = int(ypos);

    //moleculeState_.modelToWorld = glm::rotate(moleculeState_.modelToWorld, glm::radians(1.0f), glm::vec3(0, 1, 0));

    window_.draw(fw_.device(), fw_.graphicsQueue(),
      [&](vk::CommandBuffer pscb, int imageIndex) {
        // Record the dynamic buffer.
        vk::CommandBufferBeginInfo bi{};
        pscb.begin(bi);
        raytracePipeline_.update(
          fw_.device(), pscb, fw_.graphicsQueueFamilyIndex(), fw_.graphicsQueueFamilyIndex(),
          moleculeState_.cameraToPerspective, moleculeState_.modelToWorld, moleculeState_.cameraToWorld
        );
        pscb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, raytracePipeline_.pipelineLayout(), 0, raytracePipeline_.descriptorSets(), nullptr);
        pscb.bindPipeline(vk::PipelineBindPoint::eCompute, *raytracePipeline_.computePipeline_);
    
        using psflags = vk::PipelineStageFlagBits;
        using aflags = vk::AccessFlagBits;
        raytracePipeline_.atoms_.barrier(
          pscb, psflags::eTopOfPipe, psflags::eComputeShader, {},
          aflags::eShaderRead, aflags::eShaderRead|aflags::eShaderWrite, fw_.graphicsQueueFamilyIndex(), fw_.graphicsQueueFamilyIndex()
        );

        pscb.dispatch(1, 1, 1);
        
        raytracePipeline_.atoms_.barrier(
          pscb, psflags::eComputeShader, psflags::eTopOfPipe, {},
          aflags::eShaderRead|aflags::eShaderWrite, aflags::eShaderRead, fw_.graphicsQueueFamilyIndex(), fw_.graphicsQueueFamilyIndex()
        );
        pscb.end();

        #if 0
        vku::executeImmediately(fw_.device(), *computeCommandPool_, fw_.computeQueue(), [&](vk::CommandBuffer cb){
          cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, raytracePipeline_.pipelineLayout(), 0, raytracePipeline_.descriptorSets(), nullptr);
          cb.bindPipeline(vk::PipelineBindPoint::eCompute, *raytracePipeline_.computePipeline_);
    
          cb.dispatch(1, 1, 1);
          /*using psflags = vk::PipelineStageFlagBits;
          using aflags = vk::AccessFlagBits;
          raytracePipeline_.atoms_.barrier(
            cb, psflags::eComputeShader, psflags::eAllGraphics, {},
            aflags::eShaderWrite, aflags::eShaderRead, fw_.graphicsQueueFamilyIndex(), fw_.graphicsQueueFamilyIndex()
          );*/
          //cb.fillBuffer(raytracePipeline_.atoms_.buffer(), 0, raytracePipeline_.atoms_.size(), 0x3f800000);
        });
        #endif

        using Atom = RaytracePipeline::Atom;
        //Atom *atoms = (Atom*)raytracePipeline_.atoms_.map(fw_.device());
        //Atom atom0 = atoms[0];
        //int *p = (int*)raytracePipeline_.atoms_.map(fw_.device());
        //raytracePipeline_.atoms_.unmap(fw_.device());
        //printf("%f %f %f\n", atom0.pos.x, atom0.pos.y, atom0.pos.z);
        //printf("%08x\n", *p);
      }
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    return true;
  }

  vku::Framework fw_;
  vku::Window  window_;

  RaytracePipeline raytracePipeline_;
  MoleculeModel moleculeModel_;
  struct MouseState {
    int prevXpos = 0;
    int prevYpos = 0;
    bool start = true;
  };
  MouseState mouseState_;
  GLFWwindow *glfwwindow_;
  vk::Device device_;
  vk::UniqueCommandPool computeCommandPool_;

  struct MoleculeState {
    glm::mat4 cameraToPerspective;
    glm::mat4 modelToWorld;
    glm::mat4 cameraToWorld;
  };
  MoleculeState moleculeState_;
};

int main(int argc, char **argv) {
  Molvoo viewer(argc, argv);
  while (viewer.poll()) {
  }
  return 0;
}
