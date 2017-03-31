
#ifdef WIN32
  #define _CRT_SECURE_NO_WARNINGS
  #include <windows.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vku/vku.hpp>

class glfw_example {
public:
  glfw_example() :
    device_(vku::instance::singleton().device()),
    queue_(vku::instance::singleton().queue())
  {
    vku::instance &instance = vku::instance::singleton();

    uint32_t queueFamilyIndex = instance.graphicsQueueIndex();
    cmdPool_ = vku::commandPool(device_, queueFamilyIndex);
  }
private:
  const vku::device &device_;
  const vku::queue &queue_;
  vku::commandPool cmdPool_;
};

int main() {
  glfw_example example;
}

