
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>

int main() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "helloTriangle";
  bool fullScreen = false;
  int width = 800;
  int height = 600;
  GLFWmonitor *monitor = nullptr;
  if (fullScreen) {
    auto monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    width = mode->width;
    height = mode->height;
  }
  auto glfwwindow = glfwCreateWindow(width, height, title, monitor, nullptr);

  vku::Framework fw{title};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }

  vk::Device device = fw.device();

  vku::Window window{fw.instance(), fw.device(), fw.physicalDevice(), fw.graphicsQueueFamilyIndex(), glfwwindow};

  vku::ShaderModule vert_{device, BINARY_DIR "helloTriangle.vert.spv"};
  vku::ShaderModule frag_{device, BINARY_DIR "helloTriangle.frag.spv"};

  vku::PipelineLayoutMaker plm{};
  auto pipelineLayout_ = plm.createUnique(device);

  struct Vertex { glm::vec2 pos; glm::vec3 colour; };

  const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
  };

  vku::PipelineMaker pm{(uint32_t)width, (uint32_t)height};
  pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
  pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
  pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
  pm.vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, pos));
  pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, colour));
  vku::VertexBuffer buffer(fw.device(), fw.memprops(), vertices);

  auto renderPass = window.renderPass();
  auto &cache = fw.pipelineCache();
  auto pipeline = pm.createUnique(device, cache, *pipelineLayout_, renderPass);

  window.setRenderCommands(
    [&pipeline, &buffer](vk::CommandBuffer cb, int imageIndex) {
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
      cb.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));
      //cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout_, 0, nullptr, nullptr);
      cb.draw(3, 1, 0, 0);
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
      }
    );
  }

  device.waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
