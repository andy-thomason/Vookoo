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

  RaytracePipeline(vk::Device device, vk::CommandPool commandPool, vk::Queue queue, vk::DescriptorPool descriptorPool, vk::PipelineCache cache, vk::PhysicalDeviceMemoryProperties memprops, vk::RenderPass renderPass, uint32_t width, uint32_t height, int numImageIndices) {
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
    dslm.buffer(0U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1); // Atoms
    dslm.buffer(1U, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll, 1); // Uniform
    dslm.buffer(2U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1); // Pick
    layout_ = dslm.createUnique(device);

    vku::PipelineLayoutMaker plm{};
    plm.descriptorSetLayout(*layout_);
    plm.pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputeUniform));
    pipelineLayout_ = plm.createUnique(device);

    ////////////////////////////////////////
    //
    // Build the pipeline including enabling the depth test

    vku::PipelineMaker pm{width, height};
    //pm.topology(vk::PrimitiveTopology::ePointList);
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
    pm.depthTestEnable(VK_TRUE);
    //pm.cullMode(vk::CullModeFlagBits::eBack);
    //pm.frontFace(vk::FrontFace::eClockwise);

    pipeline_ = pm.createUnique(device, cache, *pipelineLayout_, renderPass);

    vku::ComputePipelineMaker cpm{};
    cpm.shader(vk::ShaderStageFlagBits::eCompute, comp_);
    computePipeline_ = cpm.createUnique(device, cache, *pipelineLayout_);

    ubo_ = vku::UniformBuffer{device, memprops, sizeof(RenderUniform)};
    RenderUniform uniform{};
    ubo_.upload(device, memprops, commandPool, queue, uniform);
  }

  using mat4 = glm::mat4;
  using vec2 = glm::vec2;
  using vec3 = glm::vec3;
  using vec4 = glm::vec4;
  using uint = uint32_t;

  struct Atom {
    vec3 pos;
    float radius;
    vec3 colour;
    float mass;
    vec3 velocity;
    int pad;
    vec3 acceleration;
    int pad2;
  };

  struct Connection {
    uint from;
    uint to;
    float length;
    float springConstant;
  };
  
  struct Pick {
    static constexpr uint fifoSize = 4;
    vec3 rayStart;
    uint atom;
    vec3 rayDir;
    uint found;
  };
  
  struct RenderUniform {
    mat4 worldToPerspective;
    mat4 modelToWorld;
    mat4 normalToWorld;
    mat4 cameraToWorld;
    vec4 colour;
    vec2 pointScale;
  };

  struct ComputeUniform {
    float timeStep;
    uint numAtoms;
    uint pickIndex;
  };

  vk::Pipeline pipeline() const { return *pipeline_; }
  vk::Pipeline computePipeline() const { return *computePipeline_; }
  vk::PipelineLayout pipelineLayout() const { return *pipelineLayout_; }
  vk::DescriptorSetLayout descriptorSetLayout() const { return *layout_; }
  const vku::UniformBuffer &ubo() const { return ubo_; }
private:
  vk::UniquePipeline pipeline_;
  vk::UniquePipeline computePipeline_;
  vk::UniqueDescriptorSetLayout layout_;
  vk::UniquePipelineLayout pipelineLayout_;
  vku::ShaderModule vert_;
  vku::ShaderModule frag_;
  vku::ShaderModule comp_;
  vku::UniformBuffer ubo_;
};

class MoleculeModel {
public:
  MoleculeModel() {
  }

  MoleculeModel(const std::string &filename, vk::Device device, vk::PhysicalDeviceMemoryProperties memprops, vk::CommandPool commandPool, vk::Queue queue) {
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

    std::vector<Atom> atoms;
    for (auto &atom : pdbAtoms) {
      glm::vec3 pos(atom.x(), atom.y(), atom.z());
      glm::vec3 colour = atom.colorByElement();
      colour.r = colour.r * 0.75f + 0.25f;
      colour.g = colour.g * 0.75f + 0.25f;
      colour.b = colour.b * 0.75f + 0.25f;
      Atom a{};
      a.pos = pos - mean;
      a.radius = 1.0f;
      a.colour = colour;
      a.velocity = glm::vec3(0, 0, 0);
      atoms.push_back(a);
    }

    atoms[0].velocity = glm::vec3(1, 0, 0);

    numAtoms_ = (uint32_t)atoms.size();

    size_t sbsize = numAtoms_ * sizeof(Atom);
    using buf = vk::BufferUsageFlagBits;
    atoms_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer|buf::eTransferDst, sbsize, vk::MemoryPropertyFlagBits::eDeviceLocal);
    pick_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer, sizeof(Pick) * Pick::fifoSize, vk::MemoryPropertyFlagBits::eHostVisible);

    atoms_.upload(device, memprops, commandPool, queue, atoms);
  }

  void buildDescriptorSets(vk::Device device, vk::DescriptorSetLayout layout, vk::DescriptorPool descriptorPool, vk::Buffer ubo) {
    vku::DescriptorSetMaker dsm{};
    dsm.layout(layout);
    auto descriptorSets = dsm.create(device, descriptorPool);
    descriptorSet_ = descriptorSets[0];

    vku::DescriptorSetUpdater update;
    update.beginDescriptorSet(descriptorSet_);

    // Point the descriptor set at the storage buffer.
    update.beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(atoms_.buffer(), 0, numAtoms_ * sizeof(Atom));
    update.beginBuffers(1, 0, vk::DescriptorType::eUniformBuffer);
    update.buffer(ubo, 0, sizeof(RenderUniform));
    update.beginBuffers(2, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(pick_.buffer(), 0, sizeof(Pick) * Pick::fifoSize);

    update.update(device);
  }

  vk::DescriptorSet descriptorSet() const { return descriptorSet_; }
  uint32_t numAtoms() const { return numAtoms_; }
  const vku::GenericBuffer &atoms() const { return atoms_; }
  const vku::GenericBuffer &pick() const { return pick_; }
private:
  using Atom = RaytracePipeline::Atom;
  using Pick = RaytracePipeline::Pick;
  using RenderUniform = RaytracePipeline::RenderUniform;

  uint32_t numAtoms_;
  vku::GenericBuffer atoms_;
  vku::GenericBuffer pick_;
  vk::DescriptorSet descriptorSet_;
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
    //glfwSetInputMode(glfwwindow_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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

    cameraState_.cameraToPerspective = leftHandCorrection * glm::perspective(glm::radians(45.0f), (float)window_.width()/window_.height(), 0.1f, 1000.0f);
    //moleculeState_.modelToWorld = glm::mat4{};

    raytracePipeline_ = RaytracePipeline(device_, window_.commandPool(), fw_.graphicsQueue(), fw_.descriptorPool(), fw_.pipelineCache(), fw_.memprops(), window_.renderPass(), window_.width(), window_.height(), window_.numImageIndices());
    moleculeModel_ = MoleculeModel(SOURCE_DIR "molvoo/molvoo.pdb", device_, fw_.memprops(), window_.commandPool(), fw_.graphicsQueue());
    moleculeModel_.buildDescriptorSets(device_, raytracePipeline_.descriptorSetLayout(), fw_.descriptorPool(), raytracePipeline_.ubo().buffer());

    using Pick = RaytracePipeline::Pick;
    for (int i = 0; i != Pick::fifoSize; ++i) {
      vk::EventCreateInfo eci{};
      pickEvents_.push_back(fw_.device().createEventUnique(eci));
    }

    if (!window_.ok()) {
      std::cout << "Window creation failed" << std::endl;
      exit(1);
    }

    using ccbits = vk::CommandPoolCreateFlagBits;
    vk::CommandPoolCreateInfo ccpci{ ccbits::eTransient|ccbits::eResetCommandBuffer, fw_.graphicsQueueFamilyIndex() };
    computeCommandPool_ = fw_.device().createCommandPoolUnique(ccpci);

    glfwSetWindowUserPointer(glfwwindow_, (void*)this);
    glfwSetScrollCallback(glfwwindow_, scrollHandler);
    glfwSetMouseButtonCallback(glfwwindow_, mouseButtonHandler);
    glfwSetKeyCallback(glfwwindow_, keyHandler);
  }

  bool doPoll() {
    vk::Device device = fw_.device();
    if (glfwWindowShouldClose(glfwwindow_)) return false;

    glfwPollEvents();

    if (mouseState_.rotating) {
      double xpos, ypos;
      glfwGetCursorPos(glfwwindow_, &xpos, &ypos);
      float dx = float(xpos - mouseState_.prevXpos);
      float dy = float(ypos - mouseState_.prevYpos);
      float xspeed = 0.1f;
      float yspeed = 0.1f;
      glm::mat4 worldToModel = glm::inverse(moleculeState_.modelToWorld);
      glm::vec3 xaxis = worldToModel[0];
      glm::vec3 yaxis = worldToModel[1];
      auto &mat = moleculeState_.modelToWorld;
      mat = glm::rotate(mat, glm::radians(dy * yspeed), xaxis);
      mat = glm::rotate(mat, glm::radians(dx * xspeed), yaxis);
      mouseState_.prevXpos = xpos;
      mouseState_.prevYpos = ypos;
    }    

    //moleculeState_.modelToWorld = glm::rotate(moleculeState_.modelToWorld, glm::radians(1.0f), glm::vec3(0, 1, 0));

    auto gfi = fw_.graphicsQueueFamilyIndex();
    using Pick = RaytracePipeline::Pick;

    vk::Event event = *pickEvents_[pickReadIndex_ & (Pick::fifoSize-1)];
    while (device.getEventStatus(event) == vk::Result::eEventSet) {
      Pick *pick = (Pick*)moleculeModel_.pick().map(device);
      printf("%d %d %d %d %d %d\n", pickWriteIndex_, pickReadIndex_, pick[0].atom, pick[1].atom, pick[2].atom, pick[3].atom);
      moleculeModel_.pick().unmap(device);
      device.resetEvent(event);
      pickReadIndex_++;
    }

    window_.draw(device, fw_.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);

        using ComputeUniform = RaytracePipeline::ComputeUniform;
        ComputeUniform cu;
        cu.timeStep = 1.0f/60;
        cu.numAtoms = moleculeModel_.numAtoms();
        cu.pickIndex = (pickWriteIndex_++) & (Pick::fifoSize-1);

        using psflags = vk::PipelineStageFlagBits;
        using aflags = vk::AccessFlagBits;

        // Note that on my Windows PC, the Nvidia driver crashes if I use a uniform buffer here.
        cb.pushConstants(raytracePipeline_.pipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputeUniform), &cu);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, raytracePipeline_.pipelineLayout(), 0, moleculeModel_.descriptorSet(), nullptr);
        cb.bindPipeline(vk::PipelineBindPoint::eCompute, raytracePipeline_.computePipeline());
    
        moleculeModel_.atoms().barrier(
          cb, psflags::eTopOfPipe, psflags::eComputeShader, {},
          aflags::eShaderRead, aflags::eShaderRead|aflags::eShaderWrite, gfi, gfi
        );

        // Do the physics update on the GPU
        cb.dispatch(moleculeModel_.numAtoms(), 1, 1);

        cb.setEvent(*pickEvents_[cu.pickIndex], vk::PipelineStageFlagBits::eComputeShader);
        
        moleculeModel_.atoms().barrier(
          cb, psflags::eComputeShader, psflags::eTopOfPipe, {},
          aflags::eShaderRead|aflags::eShaderWrite, aflags::eShaderRead, gfi, gfi
        );

        // Final renderpass.
        using RenderUniform = RaytracePipeline::RenderUniform;
        RenderUniform uniform;

        glm::mat4 cameraToWorld = glm::translate(glm::mat4{}, glm::vec3(0, 0, cameraState_.cameraDistance));
        glm::mat4 modelToWorld = moleculeState_.modelToWorld;

        glm::mat4 worldToCamera = glm::inverse(cameraToWorld);

        uniform.worldToPerspective = cameraState_.cameraToPerspective * worldToCamera;
        uniform.modelToWorld = modelToWorld;
        uniform.cameraToWorld = cameraToWorld;
        uniform.normalToWorld = modelToWorld;

        cb.updateBuffer(raytracePipeline_.ubo().buffer(), 0, sizeof(RenderUniform), &uniform);
        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, raytracePipeline_.pipeline());
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, raytracePipeline_.pipelineLayout(), 0, moleculeModel_.descriptorSet(), nullptr);
        cb.draw(moleculeModel_.numAtoms() * 6, 1, 0, 0);
        cb.endRenderPass();
        cb.end();
      }
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    return true;
  }

  static void scrollHandler(GLFWwindow *window, double dx, double dy) {
    Molvoo &app = *(Molvoo*)glfwGetWindowUserPointer(window);
    app.cameraState_.cameraDistance -= (float)dy * 4.0f;
  }

  static void mouseButtonHandler(GLFWwindow *window, int button, int action, int mods) {
    Molvoo &app = *(Molvoo*)glfwGetWindowUserPointer(window);
    auto &mouseState_ = app.mouseState_; 
    switch (button) {
      case GLFW_MOUSE_BUTTON_1: {
      } break;
 
      case GLFW_MOUSE_BUTTON_2: {
        if (action == GLFW_PRESS) {
          mouseState_.rotating = true;
          glfwGetCursorPos(window, &mouseState_.prevXpos, &mouseState_.prevYpos);
        } else {
          mouseState_.rotating = false;
        }
      } break;
 
      case GLFW_MOUSE_BUTTON_3: {
      } break;
    }
  }

  static void keyHandler(GLFWwindow *window, int key, int scancode, int action, int mods) {
    Molvoo &app = *(Molvoo*)glfwGetWindowUserPointer(window);
    auto &pos = app.moleculeState_.modelToWorld[3];
    auto &cam = app.cameraState_.cameraRotation;

    // move the molecule along the camera x and y axis.
    switch (key) {
      case GLFW_KEY_LEFT: {
        pos += cam[0] * 0.5f;
      } break;
      case GLFW_KEY_RIGHT: {
        pos -= cam[0] * 0.5f;
      } break;
      case GLFW_KEY_UP: {
        pos -= cam[1] * 0.5f;
      } break;
      case GLFW_KEY_DOWN: {
        pos += cam[1] * 0.5f;
      } break;
    }
  }

  vku::Framework fw_;
  vku::Window  window_;

  RaytracePipeline raytracePipeline_;
  MoleculeModel moleculeModel_;

  struct MouseState {
    double prevXpos = 0;
    double prevYpos = 0;
    bool rotating = false;
  };
  MouseState mouseState_;

  GLFWwindow *glfwwindow_;
  vk::Device device_;
  vk::UniqueCommandPool computeCommandPool_;

  struct MoleculeState {
    glm::mat4 modelToWorld;
  };
  MoleculeState moleculeState_;

  struct CameraState {
    glm::mat4 cameraRotation;
    glm::mat4 cameraToPerspective;
    float cameraDistance = 50.0f;
  };
  CameraState cameraState_;

  uint32_t pickWriteIndex_ = 0;
  uint32_t pickReadIndex_ = 0;

  std::vector<vk::UniqueEvent> pickEvents_;
};

int main(int argc, char **argv) {
  Molvoo viewer(argc, argv);
  while (viewer.poll()) {
  }
  return 0;
}
