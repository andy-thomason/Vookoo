////////////////////////////////////////////////////////////////////////////////
//
// Vookoo compute example (C) 2018 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//

#define VKU_NO_GLFW
#include <vku/vku.hpp>
#include <vku/vku_framework.hpp>
#include <algorithm>

int main() {

  vku::InstanceMaker im{};
  im.defaultLayers();
  vku::DeviceMaker dm{};
  dm.defaultLayers();

  vku::Framework fw{im, dm};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }

  // Get a device from the demo framework.
  auto device = fw.device();
  auto cache = fw.pipelineCache();
  auto descriptorPool = fw.descriptorPool();
  auto memprops = fw.memprops();

  ////////////////////////////////////////
  //
  // Create Push Constant Buffer
  // Up to 256 bytes of immediate data.
  struct PushConstants {
    float value;   // The shader just adds this to the buffer.
    float pad[3];  // Buffers are usually 16 byte aligned.
  };

  ////////////////////////////////////////
  //
  // Create a buffer to store the results in.
  // Note: this won't work for everyone. With some devices you
  // may need to explictly upload and download data.
  static constexpr int N = 128;
  auto mybuf = vku::GenericBuffer(
      device, 
      memprops, 
      vk::BufferUsageFlagBits::eStorageBuffer, 
      N * sizeof(float), 
      vk::MemoryPropertyFlagBits::eHostVisible
  );

  ////////////////////////////////////////
  //
  // Build the descriptor sets
  // Shader has access to a single storage buffer.
  vku::DescriptorSetLayoutMaker dsetlm{};
  dsetlm.buffer(0U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1);

  auto dsetLayout = dsetlm.createUnique(device);

  // The descriptor set itself.
  vku::DescriptorSetMaker dsm{};
  dsm.layout(*dsetLayout);

  auto dsets = dsm.create(device, descriptorPool);
  auto descriptorSet = dsets[0];

  // Pipeline layout.
  // Shader has one descriptor set and some push constants.
  vku::PipelineLayoutMaker plm{};
  plm.descriptorSetLayout(*dsetLayout)
     .pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants));

  auto pipelineLayout = plm.createUnique(device);
 
  ////////////////////////////////////////
  //
  // Build the final pipeline 
  auto shader = vku::ShaderModule{device, BINARY_DIR "helloCompute.comp.spv"};

  vku::ComputePipelineMaker cpm{};
  cpm.shader(vk::ShaderStageFlagBits::eCompute, shader);

  auto pipeline = cpm.createUnique(device, cache, *pipelineLayout);

  ////////////////////////////////////////
  //
  // Update the descriptor sets for the shader uniforms.
  vku::DescriptorSetUpdater update;
  update.beginDescriptorSet(descriptorSet)
        .beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer)
        .buffer(mybuf.buffer(), 0, N * sizeof(float))
        .update(device); // this only copies the pointer, not any data.

  ////////////////////////////////////////
  //
  // Create Command Pool
  //
  vk::CommandPoolCreateInfo cpci{ 
    vk::CommandPoolCreateFlagBits::eTransient 
           | vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 
    fw.computeQueueFamilyIndex()
  };
  auto commandPool = device.createCommandPoolUnique(cpci);

  ////////////////////////////////////////
  //
  // Run compute shader on the GPU.
  vku::executeImmediately(device, *commandPool, fw.computeQueue(), 
    [&](vk::CommandBuffer cb) {
      PushConstants cu{
        .value = 2.0f,
        .pad = {0.0f, 0.0f, 0.0f}
      };
      cb.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants), &cu);

      cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, descriptorSet, nullptr);
      cb.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
      cb.dispatch(N, 1, 1);
    }
  );

  device.waitIdle();

  ////////////////////////////////////////
  //
  // Show result of compute shader -> (2.0f + 0..127)
  float *values = static_cast<float*>( mybuf.map(device) );
  std::for_each(values, values+N, [](float &value){ 
    std::cout << value << " "; 
  });
  std::cout << std::endl;
  mybuf.unmap(device);
}
