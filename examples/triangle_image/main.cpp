////////////////////////////////////////////////////////////////////////////////
//
// Vulkan example that renders to an image.
// This example does not use window, surface or swap chain extensions.
// Note that it does not include <vku/window.h>
// 

// vulkan utilities.
#include <vku/vku.hpp>
#include <vku/imageTarget.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class triangle_image : public vku::imageTarget
{
public:
  // This is the constructor for a window containing our example
  triangle_image(int argc, const char **argv) : vku::imageTarget(vku::instance::singleton().device(), 256, 256) {
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
    vertexShader = vku::shaderModule(device(), "../shaders/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    fragmentShader = vku::shaderModule(device(), "../shaders/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    // Add a uniform buffer to the layout binding
    pipeHelper.uniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0);

    // Where the shaders are used.
    pipeHelper.shader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    pipeHelper.shader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);

    pipe = vku::pipeline(device(), renderPass().get(), pipelineCache(), pipeHelper);

    vku::descriptorPoolHelper dpHelper(2);
    dpHelper.uniformBuffers(1);
    descPool = vku::descriptorPool(device(), dpHelper);

    vku::pipelineLayout pipe_layout = vku::pipelineLayout(pipe.layout(), device());
    vku::descriptorSetLayout desc_layout = vku::descriptorSetLayout(*pipe.descriptorLayouts(), device());

    // Allocate a descriptor set for the uniform buffer
    vku::descriptorSet desc_set = vku::descriptorSet(device(), descPool, desc_layout);

    // Update the descriptor set with the uniform buffer
    desc_set.update(0, uniform_buffer);

    // We have two command buffers, one for even frames and one for odd frames.
    // This allows us to update one while rendering another.
    // In this example, we only update the command buffers once at the start.
    for (size_t i = 0; i < 2; ++i) {
      const vku::commandBuffer &cmdbuf = commandBuffers(i);
      cmdbuf.begin(renderPass(), frameBuffers(i), width, height);

      cmdbuf.bindBindDescriptorSet(pipe_layout, desc_set);
      cmdbuf.bindPipeline(pipe);
      cmdbuf.bindVertexBuffer(vertex_buffer, vertex_buffer_bind_id);
      cmdbuf.bindIndexBuffer(index_buffer);
      cmdbuf.drawIndexed((uint32_t)num_indices, 1, 0, 0, 1);

      cmdbuf.end(backBuffers(i));
    }
  }

  // Recalculate the matrices and upload to the card.
  void updateUniformBuffers()
  {
    uniform_data.viewToProjection = glm::perspective(60.0f * (3.14159f/180), (float)width / (float)height, 0.1f, 256.0f);
    uniform_data.modelToWorld = glm::mat4();
    uniform_data.worldToView = glm::mat4();
    uniform_data.worldToView[3] = glm::vec4(0, 0, -10, 1);
    uniform_data.normalToWorld = uniform_data.modelToWorld;
    uniform_data.lightPosition = glm::vec4(10, 10, -10, 1);

    void *dest = uniform_buffer.map();
    memcpy(dest, &uniform_data, sizeof(uniform_data));
    uniform_buffer.unmap();
  }

  // Submit the command buffer to draw.
  void render()
  {
    updateUniformBuffers();
    device().waitIdle();

    queue().submit(commandBuffers(0));

    //present();
    device().waitIdle();

    copyToReadBuffer(0);

    std::ofstream file("test.bmp");
    auto writer = [&file](const char *data, size_t size) { file.write(data, size); };
    readBuffer().writeBMP(width, height, writer);
  }
private:
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

  static const uint32_t width = 256;
  static const uint32_t height = 256;
};

triangle_image *gv;

int main(const int argc, const char *argv[]) {
  // create a window.
  triangle_image my_example(argc, argv);
  gv = &my_example;
  my_example.render();
  return 0;
}
