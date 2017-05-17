#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>

#include <glm/glm.hpp>

//#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_LEFT_HANDED
#include <glm/ext.hpp>

#include <gilgamesh/mesh.hpp>
#include <gilgamesh/decoders/pdb_decoder.hpp>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#ifdef WIN32
#define FOUNT_NAME "C:/windows/fonts/arial.ttf"
#else
#define FOUNT_NAME "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#endif

namespace molvoo {

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
  vec3 prevPos;
  int pad;
  vec3 acc;
  int connections[5];
};

struct Connection {
  uint from;
  uint to;
  float naturalLength;
  float springConstant;
};
  
struct Pick {
  static constexpr uint fifoSize = 4;
  uint atom;
  uint distance;
};
  
struct PushConstants {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 cameraToWorld;

  vec3 rayStart;
  float timeStep;
  vec3 rayDir;
  uint numAtoms;
  uint numConnections;
  uint pickIndex;
  uint pass;
};

struct Glyph {
  vec2 uv0;
  vec2 uv1;
  vec2 pos0;
  vec2 pos1;
  vec3 colour;
  int pad2;
  vec3 origin;
  int pad;
};

class StandardLayout {
public:
  StandardLayout() {
  }

  StandardLayout(vk::Device device) {
    vku::DescriptorSetLayoutMaker dslm{};
    dslm.buffer(0U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1); // Atoms
    dslm.buffer(1U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1); // Fount glyphs
    dslm.buffer(2U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1); // Pick
    dslm.buffer(3U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll, 1); // Connections
    dslm.buffer(4U, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAll, 1); // Cube map
    dslm.buffer(5U, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAll, 1); // Fount map
    layout_ = dslm.createUnique(device);

    vku::PipelineLayoutMaker plm{};
    plm.descriptorSetLayout(*layout_);
    plm.pushConstantRange(vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstants));
    pipelineLayout_ = plm.createUnique(device);
  }

  /*vk::DescriptorSet createDescriptorSet(vk::Device device, vk::DescriptorSetLayout layout, vk::DescriptorPool descriptorPool, vk::Sampler cubeSampler, vk::ImageView cubeImageView, vk::Sampler fountSampler, vk::ImageView fountImageView) {
    vku::DescriptorSetMaker dsm{};
    dsm.layout(layout);
    auto standardLayout = dsm.create(device, descriptorPool);
    vk::DescriptorSet set = standardLayout[0];

    vku::DescriptorSetUpdater update;
    update.beginDescriptorSet(set);

    // Point the descriptor set at the storage buffer.
    update.beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(atoms_.buffer(), 0, numAtoms_ * sizeof(Atom));
    //update.beginBuffers(1, 0, vk::DescriptorType::eStorageBuffer);
    //update.buffer(ubo, 0, sizeof(PushConstants));
    update.beginBuffers(2, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(pick_.buffer(), 0, sizeof(Pick) * Pick::fifoSize);
    update.beginBuffers(3, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(conns_.buffer(), 0, sizeof(Connection) * numConnections_);
    update.beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler);
    update.image(cubeSampler, cubeImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
    update.beginImages(5, 0, vk::DescriptorType::eCombinedImageSampler);
    update.image(fountSampler, fountImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

    update.update(device);
    return set;
  }*/

  vk::PipelineLayout pipelineLayout() const { return *pipelineLayout_; }
  vk::DescriptorSetLayout descriptorSetLayout() const { return *layout_; }
private:
  vk::UniqueDescriptorSetLayout layout_;
  vk::UniquePipelineLayout pipelineLayout_;
};

class MoleculePipeline {
public:
  MoleculePipeline() {
  }

  MoleculePipeline(vk::Device device, vk::PipelineCache cache, vk::RenderPass renderPass, uint32_t width, uint32_t height, vk::PipelineLayout pipelineLayout, bool isAtoms) {
    if (isAtoms) {
      vert_ = vku::ShaderModule{device, BINARY_DIR "atoms.vert.spv"};
      frag_ = vku::ShaderModule{device, BINARY_DIR "atoms.frag.spv"};
    } else {
      vert_ = vku::ShaderModule{device, BINARY_DIR "conns.vert.spv"};
      frag_ = vku::ShaderModule{device, BINARY_DIR "conns.frag.spv"};
    }

    vku::PipelineMaker pm{width, height};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
    pm.depthTestEnable(VK_TRUE);

    pipeline_ = pm.createUnique(device, cache, pipelineLayout, renderPass);
  }

  vk::Pipeline pipeline() const { return *pipeline_; }
private:
  vk::UniquePipeline pipeline_;
  vku::ShaderModule vert_;
  vku::ShaderModule frag_;
};

class SkyboxPipeline {
public:
  SkyboxPipeline() {
  }

  SkyboxPipeline(vk::Device device, vk::PipelineCache cache, vk::RenderPass renderPass, uint32_t width, uint32_t height, vk::PipelineLayout pipelineLayout) {
    vert_ = vku::ShaderModule{device, BINARY_DIR "skybox.vert.spv"};
    frag_ = vku::ShaderModule{device, BINARY_DIR "skybox.frag.spv"};

    vku::PipelineMaker pm{width, height};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);

    pipeline_ = pm.createUnique(device, cache, pipelineLayout, renderPass);
  }

  vk::Pipeline pipeline() const { return *pipeline_; }
private:
  vk::UniquePipeline pipeline_;
  vku::ShaderModule vert_;
  vku::ShaderModule frag_;
};

class FountPipeline {
public:
  FountPipeline() {
  }

  FountPipeline(vk::Device device, vk::PipelineCache cache, vk::RenderPass renderPass, uint32_t width, uint32_t height, vk::PipelineLayout pipelineLayout) {
    vert_ = vku::ShaderModule{device, BINARY_DIR "fount.vert.spv"};
    frag_ = vku::ShaderModule{device, BINARY_DIR "fount.frag.spv"};

    vku::PipelineMaker pm{width, height};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);

    // blend with premultiplied alpha
    pm.blendBegin(1);
    pm.blendSrcColorBlendFactor(vk::BlendFactor::eOne);

    pipeline_ = pm.createUnique(device, cache, pipelineLayout, renderPass);
  }

  vk::Pipeline pipeline() const { return *pipeline_; }
private:
  vk::UniquePipeline pipeline_;
  vku::ShaderModule vert_;
  vku::ShaderModule frag_;
};

class DynamicsPipeline {
public:
  DynamicsPipeline() {
  }

  DynamicsPipeline(vk::Device device, vk::PipelineCache cache, vk::RenderPass renderPass, uint32_t width, uint32_t height, vk::PipelineLayout pipelineLayout) {
  //DynamicsPipeline(vk::Device device, vk::CommandPool commandPool, vk::Queue queue, vk::DescriptorPool descriptorPool, vk::PipelineCache cache, vk::PhysicalDeviceMemoryProperties memprops, vk::RenderPass renderPass, uint32_t width, uint32_t height, int numImageIndices, vk::PipelineLayout pipelineLayout) {
    comp_ = vku::ShaderModule{device, BINARY_DIR "dynamics.comp.spv"};

    vku::ComputePipelineMaker cpm{};
    cpm.shader(vk::ShaderStageFlagBits::eCompute, comp_);
    pipeline_ = cpm.createUnique(device, cache, pipelineLayout);
  }

  vk::Pipeline pipeline() const { return *pipeline_; }
private:
  vk::UniquePipeline pipeline_;
  vku::ShaderModule comp_;
};

class TextModel {
public:
  TextModel() {
  }
  TextModel(const std::string &filename, vk::Device device, vk::PhysicalDeviceMemoryProperties memprops, vk::CommandPool commandPool, vk::Queue queue) {
    using buf = vk::BufferUsageFlagBits;
    maxGlyphs_ = 8192;
    glyphs_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer|buf::eTransferDst, maxGlyphs_ * sizeof(Glyph), vk::MemoryPropertyFlagBits::eHostVisible);

    uint32_t fountWidth = 512;
    uint32_t fountHeight = 512;
    fountMap_ = vku::TextureImage2D{device, memprops, fountWidth, fountHeight, 1, vk::Format::eR8Unorm};
    std::vector<uint8_t> fountBytes(fountWidth * fountHeight);
    auto fountData = vku::loadFile(filename);

    int charCount = '~'-' '+1;
    std::vector<stbtt_packedchar> charInfo(charCount);

    stbtt_pack_context context;
    int oversampleX = 4;
    int oversampleY = 4;
    float fountSize = 20;
    stbtt_PackBegin(&context, fountBytes.data(), fountWidth, fountHeight, 0, 1, nullptr);
    stbtt_PackSetOversampling(&context, oversampleX, oversampleY);
    stbtt_PackFontRange(&context, fountData.data(), 0, fountSize, ' ', charCount, charInfo.data());
    stbtt_PackEnd(&context);

    fountMap_.upload(device, fountBytes, commandPool, memprops, queue);
    Glyph *glyphs = (Glyph *)glyphs_.map(device);
    stbtt_aligned_quad quad{};
    auto str = "arg189";
    numGlyphs_ = 0;
    vec2 pos(0);
    vec2 scale(0.1f);
    for (const char *p = str; *p; ++p) {
      auto &g = glyphs[numGlyphs_++];
      vec2 offset;
      stbtt_GetPackedQuad(charInfo.data(), fountWidth, fountHeight, *p - ' ', &offset.x, &offset.y, &quad, 1);
      printf("%f %f %f %f %f %f\n", offset.x, offset.y, quad.x0, quad.y0, quad.x1, quad.y1);
      g.colour = vec3(1, 1, 1);
      g.pos0 = pos + vec2(quad.x0, -quad.y0) * scale;
      g.pos1 = pos + vec2(quad.x1, -quad.y1) * scale;
      g.uv0 = vec2(quad.s0, quad.t0);
      g.uv1 = vec2(quad.s1, quad.t1);
      g.origin = vec3(10, 0, 0);
      pos += offset * scale;
    }
    glyphs_.unmap(device);

    vku::SamplerMaker fsm{};
    fsm.magFilter(vk::Filter::eNearest);
    fsm.minFilter(vk::Filter::eNearest);
    fsm.mipmapMode(vk::SamplerMipmapMode::eNearest);
    fountSampler_ = fsm.createUnique(device);
  }

  vk::Buffer glyphs() const { return glyphs_.buffer(); }
  vk::ImageView imageView() const { return fountMap_.imageView(); }
  vk::Sampler sampler() const { return *fountSampler_; }
  int maxGlyphs() const { return maxGlyphs_; }
  int numGlyphs() const { return numGlyphs_; }
private:
  vku::GenericBuffer glyphs_;
  vku::TextureImage2D fountMap_;
  vk::UniqueSampler fountSampler_;
  int maxGlyphs_;
  int numGlyphs_;
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
      a.pos = a.prevPos = pos - mean;
      float scale = 0.1f;
      if (atom.atomNameIs(" N  ") || atom.atomNameIs(" CA ") || atom.atomNameIs(" C  ") || atom.atomNameIs(" P  ")) scale = 0.4f;
      //if (atom.atomNameIs(" O5'") || atom.atomNameIs(" O3'")) scale = 0.25f;
      //if (atom.atomNameIs(" C5'") || atom.atomNameIs(" C4'") || atom.atomNameIs(" C3'")) scale = 0.25f;
      a.radius = atom.vanDerVaalsRadius() * scale;
      a.colour = colour;
      a.acc = glm::vec3(0, 0, 0);
      a.mass = 1.0f;
      std::fill(std::begin(a.connections), std::end(a.connections), -1);
      atoms.push_back(a);
    }

    std::vector<std::pair<int, int>> pairs;
    int prevC = -1;
    char prevChainID = '?';
    for (size_t bidx = 0; bidx != pdbAtoms.size(); ) {
      // At the start of every Amino Acid, connect the atoms.
      char chainID = pdbAtoms[bidx].chainID();
      char iCode = pdbAtoms[bidx].iCode();
      size_t eidx = pdb.nextResidue(pdbAtoms, bidx);
      if (prevChainID != chainID) prevC = -1;

      // iCode is 'A' etc. for alternates.
      if (iCode == ' ') {
        prevC = pdb.addImplicitConnections(pdbAtoms, pairs, bidx, eidx, prevC, false);
        prevChainID = chainID;
      }
      bidx = eidx;
    }

    for (auto &p : pairs) {
      Atom &from = atoms[p.first];
      Atom &to = atoms[p.second];
      for (auto &i : from.connections) {
        if (i == -1) {
          i = p.second;
          break;
        }
      }
      for (auto &i : to.connections) {
        if (i == -1) {
          i = p.first;
          break;
        }
      }
    }

    std::vector<Connection> conns;
    for (auto &p : pairs) {
      Connection c;
      c.from = p.first;
      c.to = p.second;
      glm::vec3 p1 = atoms[c.from].pos;
      glm::vec3 p2 = atoms[c.to].pos;
      c.naturalLength = glm::length(p2 - p1);
      c.springConstant = 100;
      conns.push_back(c);
    }

    if (0) {
      atoms.resize(0);
      conns.resize(0);

      Atom a{};
      a.colour = glm::vec3(1);
      a.mass = 1;
      a.pos = glm::vec3(-2, 0, 0);
      a.radius = 1;
      atoms.push_back(a);
      a.pos = glm::vec3( 0, 0, 0);
      atoms.push_back(a);
      a.pos = glm::vec3( 2, 0, 0);
      atoms.push_back(a);

      Connection c{};
      c.from = 0;
      c.to = 1;
      c.naturalLength = 2;
      c.springConstant = 10;
      conns.push_back(c);
      c.from = 1;
      c.to = 2;
      conns.push_back(c);
    }

    numAtoms_ = (uint32_t)atoms.size();
    numConnections_ = (uint32_t)conns.size();

    using buf = vk::BufferUsageFlagBits;
    atoms_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer|buf::eTransferDst, numAtoms_ * sizeof(Atom), vk::MemoryPropertyFlagBits::eHostVisible);
    pick_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer, sizeof(Pick) * Pick::fifoSize, vk::MemoryPropertyFlagBits::eHostVisible);
    conns_ = vku::GenericBuffer(device, memprops, buf::eStorageBuffer|buf::eTransferDst, sizeof(Connection) * numConnections_, vk::MemoryPropertyFlagBits::eHostVisible);

    atoms_.upload(device, memprops, commandPool, queue, atoms);
    conns_.upload(device, memprops, commandPool, queue, conns);
  }

  void updateDescriptorSet(vk::Device device, vk::DescriptorSetLayout layout, vk::DescriptorPool descriptorPool, vk::Sampler cubeSampler, vk::ImageView cubeImageView, vk::Sampler fountSampler, vk::ImageView fountImageView, vk::Buffer glyphs, int maxGlyphs) {
    vku::DescriptorSetMaker dsm{};
    dsm.layout(layout);
    auto StandardLayout = dsm.create(device, descriptorPool);
    descriptorSet_ = StandardLayout[0];

    vku::DescriptorSetUpdater update;
    update.beginDescriptorSet(descriptorSet_);

    // Point the descriptor set at the storage buffer.
    update.beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(atoms_.buffer(), 0, numAtoms_ * sizeof(Atom));
    update.beginBuffers(1, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(glyphs, 0, maxGlyphs * sizeof(Glyph));
    update.beginBuffers(2, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(pick_.buffer(), 0, sizeof(Pick) * Pick::fifoSize);
    update.beginBuffers(3, 0, vk::DescriptorType::eStorageBuffer);
    update.buffer(conns_.buffer(), 0, sizeof(Connection) * numConnections_);
    update.beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler);
    update.image(cubeSampler, cubeImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
    update.beginImages(5, 0, vk::DescriptorType::eCombinedImageSampler);
    update.image(fountSampler, fountImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

    update.update(device);
  }

  vk::DescriptorSet descriptorSet() const { return descriptorSet_; }
  uint32_t numAtoms() const { return numAtoms_; }
  uint32_t numConnections() const { return numConnections_; }
  const vku::GenericBuffer &atoms() const { return atoms_; }
  const vku::GenericBuffer &pick() const { return pick_; }
  const vku::GenericBuffer &conns() const { return conns_; }
private:
  uint32_t numAtoms_;
  uint32_t numConnections_;
  vku::GenericBuffer atoms_;
  vku::GenericBuffer pick_;
  vku::GenericBuffer conns_;
  vk::DescriptorSet descriptorSet_;
};

class Molvoo {
public:
  Molvoo(int argc, char **argv) {
    init(argc, argv);
  }

  bool poll() { return doPoll(); }

  ~Molvoo() {
    device_.waitIdle();
    glfwDestroyWindow(glfwwindow_);
    glfwTerminate();
  }

private:
  void usage() {
    
  }

  void init(int argc, char **argv) {
    const char *filename = nullptr;
    for (int i = 1; i != argc; ++i) {
      const char *arg = argv[i];
      if (arg[0] == '-') {
      } else {
        if (filename) {
          fprintf(stderr, "only one filename at a time please\n");
          usage();
          return;
        }
        filename = arg;
      }
    }

    if (!filename) {
      filename = SOURCE_DIR "molvoo/1hnw.pdb";
    }

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

    float fieldOfView = glm::radians(45.0f);
    cameraState_.cameraToPerspective = leftHandCorrection * glm::perspective(fieldOfView, (float)window_.width()/window_.height(), 0.1f, 1000.0f);

    auto cubeBytes = vku::loadFile(SOURCE_DIR "okretnica.ktx");
    vku::KTXFileLayout ktx(cubeBytes.data(), cubeBytes.data()+cubeBytes.size());
    if (!ktx.ok()) {
      std::cout << "Could not load KTX file" << std::endl;
      exit(1);
    }

    cubeMap_ = vku::TextureImageCube{device_, fw_.memprops(), ktx.width(0), ktx.height(0), ktx.mipLevels(), vk::Format::eR8G8B8A8Unorm};
    ktx.upload(device_, cubeMap_, cubeBytes, window_.commandPool(), fw_.memprops(), fw_.graphicsQueue());

    vku::SamplerMaker sm{};
    sm.magFilter(vk::Filter::eLinear);
    sm.minFilter(vk::Filter::eLinear);
    sm.mipmapMode(vk::SamplerMipmapMode::eNearest);
    cubeSampler_ = sm.createUnique(device_);


    standardLayout_ = StandardLayout(device_);

    fountPipeline_ = FountPipeline(device_, fw_.pipelineCache(), window_.renderPass(), window_.width(), window_.height(), standardLayout_.pipelineLayout());
    skyboxPipeline_ = SkyboxPipeline(device_, fw_.pipelineCache(), window_.renderPass(), window_.width(), window_.height(), standardLayout_.pipelineLayout());
    dynamicsPipeline_ = DynamicsPipeline(device_, fw_.pipelineCache(), window_.renderPass(), window_.width(), window_.height(), standardLayout_.pipelineLayout());
    atomPipeline_ = MoleculePipeline(device_, fw_.pipelineCache(), window_.renderPass(), window_.width(), window_.height(), standardLayout_.pipelineLayout(), true);
    connPipeline_ = MoleculePipeline(device_, fw_.pipelineCache(), window_.renderPass(), window_.width(), window_.height(), standardLayout_.pipelineLayout(), false);

    textModel_ = TextModel(FOUNT_NAME, device_, fw_.memprops(), window_.commandPool(), fw_.graphicsQueue());
    moleculeModel_ = MoleculeModel(filename, device_, fw_.memprops(), window_.commandPool(), fw_.graphicsQueue());
    moleculeModel_.updateDescriptorSet(device_, standardLayout_.descriptorSetLayout(), fw_.descriptorPool(), *cubeSampler_, cubeMap_.imageView(), textModel_.sampler(), textModel_.imageView(), textModel_.glyphs(), textModel_.maxGlyphs());

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

    float timeStep = 1.0f / 60;

    double xpos, ypos;
    glfwGetCursorPos(glfwwindow_, &xpos, &ypos);

    if (mouseState_.rotating) {
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

    vk::Event event = *pickEvents_[pickReadIndex_ & (Pick::fifoSize-1)];
    while (device.getEventStatus(event) == vk::Result::eEventSet) {
      Pick *pick = (Pick*)moleculeModel_.pick().map(device);
      Pick &p = pick[pickReadIndex_ & (Pick::fifoSize-1)];
      moleculeState_.mouseAtom = p.atom;
      moleculeState_.mouseDistance = p.distance / 10000.0f;
      moleculeModel_.pick().unmap(device);
      p.distance = ~0;
      p.atom = ~0;
      device.resetEvent(event);
      pickReadIndex_++;
    }

    glm::mat4 cameraToWorld = glm::translate(glm::mat4{}, glm::vec3(0, 0, cameraState_.cameraDistance));
    glm::mat4 modelToWorld = moleculeState_.modelToWorld;

    glm::mat4 worldToCamera = glm::inverse(cameraToWorld);
    glm::mat4 worldToModel = glm::inverse(modelToWorld);

    glm::vec3 worldCameraPos = cameraToWorld[3];
    glm::vec3 modelCameraPos = worldToModel * glm::vec4(worldCameraPos, 1);
    float xscreen = (float)xpos * 2.0f / window_.width() - 1.0f;
    float yscreen = (float)ypos * 2.0f / window_.height() - 1.0f;
    float tanfovX = 1.0f / cameraState_.cameraToPerspective[0][0];
    float tanfovY = 1.0f / cameraState_.cameraToPerspective[1][1];
    glm::vec4 cameraMouseDir = glm::vec4(xscreen * tanfovX, yscreen * tanfovY, -1, 0);
    glm::vec3 modelMouseDir = worldToModel * (cameraToWorld * cameraMouseDir);

    {
      Atom *atoms = (Atom*)moleculeModel_.atoms().map(device);
      Connection *conns = (Connection*)moleculeModel_.conns().map(device);

      if (moleculeState_.dragging) {
        if (moleculeState_.selectedAtom < moleculeModel_.numAtoms()) {
          vec3 mousePos = modelCameraPos + modelMouseDir * moleculeState_.selectedDistance;
          vec3 atomPos = atoms[moleculeState_.selectedAtom].pos;
          //printf("%s %s\n", to_string(mousePos).c_str(), to_string(atomPos).c_str());
          vec3 axis = mousePos - atomPos;
          float f = length(axis) * 10.0f;
          atoms[moleculeState_.selectedAtom].acc += normalize(axis) * (f * timeStep);
        }
      }

      moleculeModel_.atoms().unmap(device);
      moleculeModel_.conns().unmap(device);
    }


    window_.draw(device, fw_.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);

        PushConstants cu;
        cu.timeStep = timeStep;
        cu.numAtoms = moleculeModel_.numAtoms();
        cu.numConnections = moleculeModel_.numConnections();
        cu.pickIndex = (pickWriteIndex_++) & (Pick::fifoSize-1);
        //cu.forceAtom = moleculeState_.selectedAtom;

        cu.rayStart = modelCameraPos;
        cu.rayDir = glm::normalize(modelMouseDir);

        cu.worldToPerspective = cameraState_.cameraToPerspective * worldToCamera;
        cu.modelToWorld = modelToWorld;
        cu.cameraToWorld = cameraToWorld;

        using psflags = vk::PipelineStageFlagBits;
        using aflags = vk::AccessFlagBits;

        /*moleculeModel_.atoms().barrier(
          cb, psflags::eTopOfPipe, psflags::eComputeShader, {},
          aflags::eShaderRead, aflags::eShaderRead|aflags::eShaderWrite, gfi, gfi
        );

        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, standardLayout_.pipelineLayout(), 0, moleculeModel_.descriptorSet(), nullptr);
        cb.bindPipeline(vk::PipelineBindPoint::eCompute, dynamicsPipeline_.pipeline());
    
        // Do the physics acceleration update on the GPU and find the nearest picked atom
        cu.pass = 0;
        cb.pushConstants(standardLayout_.pipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants), &cu);
        cb.dispatch(std::max(cu.numAtoms, cu.numConnections), 1, 1);

        moleculeModel_.atoms().barrier(
          cb, psflags::eComputeShader, psflags::eComputeShader, {},
          aflags::eShaderRead|aflags::eShaderWrite, aflags::eShaderRead|aflags::eShaderWrite, gfi, gfi
        );

        // Do the physics velocity update on the GPU and select an atom
        cu.pass = 1;
        cb.pushConstants(standardLayout_.pipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants), &cu);
        cb.dispatch(cu.numAtoms, 1, 1);

        cb.setEvent(*pickEvents_[cu.pickIndex], vk::PipelineStageFlagBits::eComputeShader);
        
        moleculeModel_.atoms().barrier(
          cb, psflags::eComputeShader, psflags::eTopOfPipe, {},
          aflags::eShaderRead|aflags::eShaderWrite, aflags::eShaderRead, gfi, gfi
        );*/

        cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);

        cb.pushConstants(standardLayout_.pipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstants), &cu);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, standardLayout_.pipelineLayout(), 0, moleculeModel_.descriptorSet(), nullptr);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, skyboxPipeline_.pipeline());
        cb.draw(6 * 6, 1, 0, 0);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, atomPipeline_.pipeline());
        cb.draw(moleculeModel_.numAtoms() * 6, 1, 0, 0);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, connPipeline_.pipeline());
        cb.draw(moleculeModel_.numConnections() * 6, 1, 0, 0);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, fountPipeline_.pipeline());
        cb.draw(6 * 6, 1, 0, 0);

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
        if (action == GLFW_PRESS) {
          app.moleculeState_.selectedAtom = app.moleculeState_.mouseAtom;
          app.moleculeState_.dragging = true;
          app.moleculeState_.selectedDistance = app.moleculeState_.mouseDistance;
        } else {
          app.moleculeState_.selectedAtom = ~0;
          app.moleculeState_.dragging = false;
        }
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
      case GLFW_KEY_W: {
        app.cameraState_.cameraDistance -= 4;
      } break;
      case GLFW_KEY_S: {
        app.cameraState_.cameraDistance += 4;
      } break;
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

  StandardLayout standardLayout_;
  SkyboxPipeline skyboxPipeline_;
  FountPipeline fountPipeline_;

  DynamicsPipeline dynamicsPipeline_;
  MoleculePipeline atomPipeline_;
  MoleculePipeline connPipeline_;

  TextModel textModel_;
  MoleculeModel moleculeModel_;

  vku::TextureImageCube cubeMap_;
  vk::UniqueSampler cubeSampler_;

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
    uint32_t selectedAtom;
    uint32_t mouseAtom;
    float selectedDistance;
    float mouseDistance;
    bool dragging = false;
  };
  MoleculeState moleculeState_;

  struct CameraState {
    glm::mat4 cameraRotation;
    glm::mat4 cameraToPerspective;
    float cameraDistance = 190.0f;
  };
  CameraState cameraState_;

  uint32_t pickWriteIndex_ = 0;
  uint32_t pickReadIndex_ = 0;

  std::vector<vk::UniqueEvent> pickEvents_;
};

}

int main(int argc, char **argv) {
  molvoo::Molvoo viewer(argc, argv);
  while (viewer.poll()) {
  }
  return 0;
}
