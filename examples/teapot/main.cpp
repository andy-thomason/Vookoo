////////////////////////////////////////////////////////////////////////////////
//
// Minimalistic Vulkan Mesh sample
//
// 

// vulkan utilities.
#include "../../include/vku.hpp"

#if 0
class teapot_example : public vku::window
{
public:
  vku::mesh teapot;

  // The desriptor pool is used to allocate components of the pipeline
  vku::descriptorPool descPool;

  // The pipeline tells the GPU how to render the triangle
  vku::pipeline pipe;

  // The vertex shader uses the uniforms to transform the points in the triangle
  vku::shaderModule vertexShader;

  // The fragment shader decides the colours of pixels.
  vku::shaderModule fragmentShader;

  // Matrices
  vku::buffer uniform_buffer;

  // This tells the pipeline where to get the vertices from
  static const int vertex_buffer_bind_id = 0;

  // This is the constructor for a window containing our example
  teapot_example() : vku::window(false, 1280, 720, -2.5f, "triangle") {
    
    // Shaders
    vertexShader = vku::shaderModule(device(), "../data/shaders/triangle.vert", VK_SHADER_STAGE_VERTEX_BIT);
    fragmentShader = vku::shaderModule(device(), "../data/shaders/triangle.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

    vku::pipelineCreateHelper pipeHelper;

    // How many uniform buffers per stage
    pipeHelper.uniformBuffers(1, VK_SHADER_STAGE_VERTEX_BIT);

    // Where the shaders are used.
    pipeHelper.shader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    pipeHelper.shader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);

    // Use pipeHelper to construct the pipeline
    pipe = vku::pipeline(device(), swapChain().renderPass(), pipelineCache(), pipeHelper);

    // construct the descriptor pool which is used at runtime to allocate descriptor sets
    uint32_t num_uniform_buffers = 1;
    descPool = vku::descriptorPool(device(), num_uniform_buffers);

    // Allocate descriptor sets for the uniform buffer
    // todo: descriptor sets need a little more work.
    pipe.allocateDescriptorSets(descPool);
    pipe.updateDescriptorSets(uniform_buffer);

    // We have two command buffers, one for even frames and one for odd frames.
    // This allows us to update one while rendering another.
    // In this example, we only update the command buffers once at the start.
    for (int32_t i = 0; i < swapChain().imageCount(); ++i) {
      const vku::cmdBuffer &cmdbuf = drawCmdBuffer(i);
      cmdbuf.begin(swapChain().renderPass(), swapChain().frameBuffer(i), width(), height());

      cmdbuf.bindPipeline(pipe);
      cmdbuf.bindVertexBuffer(vertex_buffer, vertex_buffer_bind_id);
      cmdbuf.bindIndexBuffer(index_buffer);
      cmdbuf.drawIndexed((uint32_t)num_indices, 1, 0, 0, 1);

      cmdbuf.end(swapChain().image(i));
    }

    // upload uniform buffer data to the GPU card.
    updateUniformBuffers();
  }

  // Recalculate the matrices and upload to the card.
  void updateUniformBuffers()
  {
    uniform_data.projectionMatrix = defaultProjectionMatrix();
    uniform_data.viewMatrix = defaultViewMatrix();
    uniform_data.modelMatrix = defaultModelMatrix();

    void *dest = uniform_buffer.map();
    memcpy(dest, &uniform_data, sizeof(uniform_data));
    uniform_buffer.unmap();
  }

  // Sumbit the command buffer to draw.
  void render() override
  {
    device().waitIdle();
    present();
    device().waitIdle();
  }

  // If the view changes, we must update the uniform buffers to change
  // the aspect ratio.
  void viewChanged() override
  {
    updateUniformBuffers();
  }
};



int main(const int argc, const char *argv[]) {
  try {
    // create a window.
    teapot_example my_example;

    // poll the windows until they are all closed
    while (vku::window::poll()) {
      if (my_example.windowIsClosed()) {
        break;
      }
      my_example.render();
    }
  } catch(std::runtime_error &e) {
    // come here if something fails.
    printf("fail: %s\n", e.what());
    char x;
    std::cin >> x;
  }

  return 0;
}
#endif
