////////////////////////////////////////////////////////////////////////////////
//
// Minimalistic Vulkan Mesh sample
//
// 

// vulkan utilities.
#include <vku/vku.hpp>
#include <vku/window.hpp>

class teapot_example : public vku::window
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

  vku::simple_mesh mesh;

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
  teapot_example(int argc, const char **argv) : vku::window(argc, argv, false, 1280, 720, -2.5f, "teapot") {
    mesh = vku::simple_mesh("../data/teapot.fbx");
    mesh.reindex(true);

    if (mesh.numVertices()) vertex_buffer = vku::buffer(device(), (void*)mesh.vertices(), mesh.numVertices()*mesh.vertexSize(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Indices
    static const uint32_t index_data[] = { 0, 1, 2 };
    if (mesh.numIndices()) index_buffer = vku::buffer(device(), (void*)mesh.indices(), mesh.numIndices()*mesh.indexSize(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    num_indices = mesh.numIndices();

    vku::pipelineCreateHelper pipeHelper;
    mesh.getVertexFormat(pipeHelper, vertex_buffer_bind_id);

    // Matrices

    uniform_buffer = vku::buffer(device(), (void*)nullptr, sizeof(uniform_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    
    // Shaders
    vertexShader = vku::shaderModule(device(), "mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    fragmentShader = vku::shaderModule(device(), "mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    // How many uniform buffers per stage
    pipeHelper.uniformBuffers(1, VK_SHADER_STAGE_VERTEX_BIT);

    // Where the shaders are used.
    pipeHelper.shader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    pipeHelper.shader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);

    // Use pipeHelper to construct the pipeline
    pipe = vku::pipeline(device(), swapChain().renderPass(), pipelineCache(), pipeHelper);

    // construct the descriptor pool which is used at runtime to allocate descriptor sets
    vku::descriptorPoolHelper dpHelper(2);
    dpHelper.uniformBuffers(1);
    descPool = vku::descriptorPool(device(), dpHelper);

    // Allocate descriptor sets for the uniform buffer
    // todo: descriptor sets need a little more work.
    pipe.allocateDescriptorSets(descPool);
    pipe.updateDescriptorSets(uniform_buffer);

    // We have two command buffers, one for even frames and one for odd frames.
    // This allows us to update one while rendering another.
    // In this example, we only update the command buffers once at the start.
    for (int32_t i = 0; i < swapChain().imageCount(); ++i) {
      const vku::commandBuffer &cmdbuf = drawCmdBuffer(i);
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
  teapot_example my_example(argc, argv);

  // poll the windows until they are all closed
  while (vku::window::poll()) {
    if (my_example.windowIsClosed()) {
      break;
    }
    my_example.render();
  }
  return 0;
}
