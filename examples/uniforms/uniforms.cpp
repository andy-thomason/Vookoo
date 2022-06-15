////////////////////////////////////////////////////////////////////////////////
//
// Vookoo triangle example (C) 2017 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//
// In this sample we demonstrate uniforms which allow you to pass values
// to shaders that will stay the same throughout the whole draw call.
//
// Compare this file with pushConstants.cpp to see what we have done.

// Include the demo framework, vookoo (vku) for building objects and glm for maths.
// The demo framework uses GLFW to create windows.
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp> // for rotate()

int main() {
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  const char *title = "uniforms";
  auto glfwwindow = glfwCreateWindow(800, 800, title, nullptr, nullptr);

  {
    // Initialize makers
    vku::InstanceMaker im{};
    im.defaultLayers();
    vku::DeviceMaker dm{};
    dm.defaultLayers();

    // Initialise the Vookoo demo framework.
    vku::Framework fw{im, dm};
    if (!fw.ok()) {
      std::cout << "Framework creation failed" << std::endl;
      exit(1);
    }

    // Get a device from the demo framework.
    vk::Device device = fw.device();

    // Create a window to draw into
    vku::Window window{
      fw.instance(),
      device,
      fw.physicalDevice(),
      fw.graphicsQueueFamilyIndex(),
      glfwwindow
    };
    if (!window.ok()) {
      std::cout << "Window creation failed" << std::endl;
      exit(1);
    }

    // Create two shaders, vertex and fragment. See the files uniforms.vert
    // and uniforms.frag for details.
    vku::ShaderModule vert{device, BINARY_DIR "uniforms.vert.spv"};
    vku::ShaderModule frag{device, BINARY_DIR "uniforms.frag.spv"};

    // These are the parameters we are passing to the shaders
    // Note! be very careful when using vec3, vec2, float and vec4 together
    // as there are alignment rules you must follow.
    struct Uniform {
      glm::vec4 colour;
      glm::mat4 rotation;
      glm::vec4 filler[3]; // filler to get overall size to required multiple of minUniformBufferOffsetAlignment (64bytes on my PC)
    };

    std::vector<Uniform> U = { 
      {.colour = glm::vec4{1, 1, 1, 1}, .rotation = glm::scale(glm::mat4{1}, glm::vec3(2.0f,2.0f,2.0f))},
      {.colour = glm::vec4{1, 1, 1, 1}, .rotation = glm::mat4{1}},
    };
    // Read the pushConstants example first.
    // 
    // Create a uniform buffer capable of N=U.size() "struct Uniform"s.
    // We cannot update these buffers with normal memory writes
    // because reading the buffer may happen at any time.
    auto ubo = vku::UniformBuffer{device, fw.memprops(), sizeof(Uniform)*U.size()};
  
    // We will use this simple vertex description.
    // It has a 2D location (x, y) and a colour (r, g, b)
    struct Vertex { 
      glm::vec2 pos;
      glm::vec3 colour;
    };
  
    // This is our triangle.
    const std::vector<Vertex> vertices = {
      {.pos={ 0.0f,-0.5f}, .colour={1.0f, 0.0f, 0.0f}},
      {.pos={ 0.5f, 0.5f}, .colour={0.0f, 1.0f, 0.0f}},
      {.pos={-0.5f, 0.5f}, .colour={0.0f, 0.0f, 1.0f}},
    };
    auto buffer = vku::HostVertexBuffer{device, fw.memprops(), vertices};
  
    // Build a template for descriptor sets that use these shaders.
    vku::DescriptorSetLayoutMaker dslm{};
    auto descriptorSetLayout = dslm
      .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll, 1)
      .createUnique(device);
  
    // We need to create a descriptor set to tell the shader where
    // our buffers are.
    vku::DescriptorSetMaker dsm{};
    auto descriptorSets = dsm
      .layout(*descriptorSetLayout) // for U[0]
      .layout(*descriptorSetLayout) // for U[1]
      .create(device, fw.descriptorPool());
  
    // Next we need to update the descriptor set with the uniform buffer.
    vku::DescriptorSetUpdater update;
    update
      // U[0]
      .beginDescriptorSet(descriptorSets[0])
      .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
      .buffer(ubo.buffer(), 0*sizeof(Uniform), sizeof(Uniform))
      // U[1]
      .beginDescriptorSet(descriptorSets[1])
      .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
      .buffer(ubo.buffer(), 1*sizeof(Uniform), sizeof(Uniform))
      // 
      .update(device);
  
    // Make a default pipeline layout. This shows how pointers
    // to resources are layed out.
    // 
    vku::PipelineLayoutMaker plm{};
    auto pipelineLayout = plm
      .descriptorSetLayout(*descriptorSetLayout)
      .createUnique(device);
  
    auto buildPipeline = [&]() {
      // Make a pipeline to use the vertex format and shaders.
      vku::PipelineMaker pm{ window.width(), window.height() };
      return pm
        .shader(vk::ShaderStageFlagBits::eVertex, vert)
        .shader(vk::ShaderStageFlagBits::eFragment, frag)
        .vertexBinding(0, sizeof(Vertex))
        .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos))
        .vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, colour))
        .createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
    };
    auto pipeline = buildPipeline();
  
    int frame = 0;
  
    // Loop waiting for the window to close.
    while (!glfwWindowShouldClose(glfwwindow)) {
      glfwPollEvents();
  
      U.back().rotation = glm::rotate(U.back().rotation, glm::radians(1.0f), glm::vec3(0, 0, 1));
      U.back().colour.r = (std::sin(frame * 0.01f) + 1.0f) / 2.0f;
      U.back().colour.g = (std::cos(frame * 0.01f) + 1.0f) / 2.0f;
  
      // Unlike helloTriangle, we generate the command buffer dynamically
      // because it will contain different values on each frame.
      window.draw(
        device, fw.graphicsQueue(),
        [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
          // deal with resizing of window
          static auto ww = window.width();
          static auto wh = window.height();
          if (ww != window.width() || wh != window.height()) {
            ww = window.width();
            wh = window.height();
            pipeline = buildPipeline();
          }

          vk::CommandBufferBeginInfo cbbi{};
          cb.begin(cbbi);

          // Instead of pushConstants() we use updateBuffer(). This has a max of 64k.
          // Like pushConstants(), this takes a copy of the uniform buffer
          // at the time we create this command buffer.
          // Unlike push constant update, uniform buffer must be updated
          // _OUTSIDE_ of the (beginRenderPass ... endRenderPass)
          cb.updateBuffer(
            ubo.buffer(), 0, sizeof(Uniform)*U.size(), (const void*)&U[0]
          );

          // We may or may not need this barrier. It is probably a good precaution.
          ubo.barrier(
            cb,
            vk::PipelineStageFlagBits::eHost, //srcStageMask
            vk::PipelineStageFlagBits::eFragmentShader, //dstStageMask
            vk::DependencyFlagBits::eByRegion, //dependencyFlags
            vk::AccessFlagBits::eHostWrite, //srcAccessMask
            vk::AccessFlagBits::eShaderRead, //dstAccessMask
            fw.graphicsQueueFamilyIndex(), //srcQueueFamilyIndex
            fw.graphicsQueueFamilyIndex() //dstQueueFamilyIndex
          );

          cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
          cb.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));
  
          cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);

          // draw multiple triangles with different Uniforms.
          for (auto descriptorSet : descriptorSets) {
            // Unlike in the pushConstants example, we need to bind descriptor sets
            // to tell the shader where to find our buffer.
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSet, nullptr);
            cb.draw(vertices.size(), 1, 0, 0);
          };

          cb.endRenderPass();
  
          cb.end();
        }
      );

      // Very crude method to prevent your GPU from overheating.
      //std::this_thread::sleep_for(std::chrono::milliseconds(16));
  
      ++frame;
    }

    // Wait until all drawing is done and then kill the window.
    device.waitIdle();
    // The Framework and Window objects will be destroyed here.
  }

  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
