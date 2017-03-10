
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vku/vku.hpp>

class glfw_example {
public:
  glfw_example() {
    vku::instance &instance = vku::instance::singleton();

    queue_ = vku::queue(instance.queue(), device_);
    uint32_t queueFamilyIndex = instance.graphicsQueueIndex();
    cmdPool_ = vku::commandPool(device_, queueFamilyIndex);
  }
private:
  vku::queue queue_;
  vku::cmdPool cmdPool_;
};

int main() {
  glfw_example example;
}

