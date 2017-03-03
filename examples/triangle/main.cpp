////////////////////////////////////////////////////////////////////////////////
//
// Minimalistic Vulkan Mesh sample
//
// 

// vulkan utilities.
#include <vku/vku.hpp>
#include <vku/window.hpp>

class triangle_example : public vku::window
{
public:
  // these matrices transform rotate and position the triangle
  struct {
	  glm::mat4 viewToProjection;
	  glm::mat4 modelToWorld;
	  glm::mat4 worldToView;
	  glm::mat4 normalToWorld;
	  glm::vec4 lightPosition;
  } uniform_data;

  // These buffers represent data on the GPU card.
  vku::buffer vertex_buffer;
  vku::buffer index_buffer;
  vku::buffer uniform_buffer;

  // The desriptor pool is used to allocate components of the pipeline
  vku::descriptorPool descPool;

  // The pipeline tells the GPU how to render the triangle
  vku::pipeline pipe;

  // The vertex shader uses the uniforms to transform the points in the triangle
  vku::shaderModule vertexShader;

  // The fragment shader decides the colours of pixels.
  vku::shaderModule fragmentShader;

  // This is the number of points on the triangle (ie. 3)
  size_t num_indices;

  // This tells the pipeline where to get the vertices from
  static const int vertex_buffer_bind_id = 0;

  // This is the constructor for a window containing our example
  triangle_example(int argc, const char **argv) : vku::window(argc, argv, false, 1280, 720, -2.5f, "triangle") {
    static const uint32_t indices[] = { 0, 1, 2 };
    static const float vertices[] = {
      -1, -1, 0, 0, 0, 1, 0, 0,
       0,  1, 0, 0, 0, 1, 0, 0,
       1, -1, 0, 0, 0, 1, 0, 0,
    };

    vertex_buffer = vku::buffer(device(), (void*)vertices, sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Indices
    static const uint32_t index_data[] = { 0, 1, 2 };
    index_buffer = vku::buffer(device(), (void*)indices, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    num_indices = 3;

    vku::pipelineCreateHelper pipeHelper;
    pipeHelper.binding(vertex_buffer_bind_id, sizeof(float)*8, VK_VERTEX_INPUT_RATE_VERTEX);
    pipeHelper.attrib(0, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, 0);
    pipeHelper.attrib(1, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*3);
    pipeHelper.attrib(2, vertex_buffer_bind_id, VK_FORMAT_R32G32_SFLOAT, sizeof(float)*6);

    // Matrices

    uniform_buffer = vku::buffer(device(), (void*)nullptr, sizeof(uniform_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    
    // Shaders
    vertexShader = vku::shaderModule(device(), "../shaders/simple.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    fragmentShader = vku::shaderModule(device(), "../shaders/simple.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    // Add a uniform buffer to the layout binding
    pipeHelper.uniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0);

    // Where the shaders are used.
    pipeHelper.shader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    pipeHelper.shader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);

    // Use pipeHelper to construct the pipeline
    pipe = vku::pipeline(device(), swapChain().renderPass(), pipelineCache(), pipeHelper);

    vku::descriptorPoolHelper dpHelper(2);
    dpHelper.uniformBuffers(1);
    descPool = vku::descriptorPool(device(), dpHelper);

    vku::pipelineLayout pipe_layout = vku::pipelineLayout(pipe.layout(), device());
    vku::descriptorSetLayout layout = vku::descriptorSetLayout(*pipe.descriptorLayouts(), device());

    // Allocate descriptor sets for the uniform buffer
    // todo: descriptor sets need a little more work.
    pipe.allocateDescriptorSets(descPool);

    vku::descriptorSet set = vku::descriptorSet(*pipe.descriptorSets(), device());
    set.update(0, uniform_buffer);
    //pipe.updateDescriptorSets(uniform_buffer);

    // We have two command buffers, one for even frames and one for odd frames.
    // This allows us to update one while rendering another.
    // In this example, we only update the command buffers once at the start.
    for (size_t i = 0; i < swapChain().imageCount(); ++i) {
      const vku::commandBuffer &cmdbuf = drawCmdBuffer(i);
      cmdbuf.begin(swapChain().renderPass(), swapChain().frameBuffer(i), width(), height());

      cmdbuf.bindBindDescriptorSet(pipe_layout, set);
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
    uniform_data.viewToProjection = defaultProjectionMatrix();
    uniform_data.modelToWorld = glm::mat4();
    uniform_data.worldToView = defaultViewMatrix();
    uniform_data.normalToWorld = uniform_data.modelToWorld;
    uniform_data.lightPosition = glm::vec4(10, 10, -10, 1);

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
  // create a window.
  triangle_example my_example(argc, argv);

  // poll the windows until they are all closed
  while (vku::window::poll()) {
    if (my_example.windowIsClosed()) {
      break;
    }
    my_example.render();
  }
  return 0;
}
