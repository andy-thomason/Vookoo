////////////////////////////////////////////////////////////////////////////////
//
// Vulkan example that renders to an image.
// This example does not use window, surface or swap chain extensions.
// Note that it does not include <vku/window.h>
// 

// vulkan utilities.
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class triangle_image
{
public:
  // This is the constructor for a window containing our example
  triangle_image(int argc, const char **argv) {
    vku::device &device_ =  vku::instance::get().device();

    static const uint32_t indices[] = { 0, 1, 2 };
    static const float vertices[] = {
      -1, -1, 0, 0, 0, 1, 0, 0,
       0,  1, 0, 0, 0, 1, 0, 0,
       1, -1, 0, 0, 0, 1, 0, 0,
    };

    vertex_buffer = vku::buffer(device_, (void*)vertices, sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Indices
    static const uint32_t index_data[] = { 0, 1, 2 };
    index_buffer = vku::buffer(device_, (void*)indices, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    num_indices = 3;

    vku::pipelineCreateHelper pipeHelper;
    pipeHelper.binding(vertex_buffer_bind_id, sizeof(float)*8, VK_VERTEX_INPUT_RATE_VERTEX);
    pipeHelper.attrib(0, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, 0);
    pipeHelper.attrib(1, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*3);
    pipeHelper.attrib(2, vertex_buffer_bind_id, VK_FORMAT_R32G32_SFLOAT, sizeof(float)*6);

    // Matrices
    uniform_buffer = vku::buffer(device_, (void*)nullptr, sizeof(uniform_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    
    // Shaders
    vertexShader = vku::shaderModule(device_, "mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    fragmentShader = vku::shaderModule(device_, "mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    // How many uniform buffers per stage
    pipeHelper.uniformBuffers(1, VK_SHADER_STAGE_VERTEX_BIT);

    // Where the shaders are used.
    pipeHelper.shader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    pipeHelper.shader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);

    vku::renderPassLayout layout;
    uint32_t color = layout.addAttachment(VK_FORMAT_B8G8R8A8_UNORM);
    uint32_t depth = layout.addAttachment(VK_FORMAT_D24_UNORM_S8_UINT);
    layout.addSubpass(color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    vku::renderPass renderPass(device_, layout);
    vku::pipelineCache pipelineCache(device_);
    pipe = vku::pipeline(device_, renderPass.get(), pipelineCache, pipeHelper);

    // construct the descriptor pool which is used at runtime to allocate descriptor sets
    uint32_t num_uniform_buffers = 1;
    descPool = vku::descriptorPool(device_, num_uniform_buffers);

    // Allocate descriptor sets for the uniform buffer
    // todo: descriptor sets need a little more work.
    pipe.allocateDescriptorSets(descPool);
    pipe.updateDescriptorSets(uniform_buffer);

    uint32_t queueFamilyIndex = 0; //device_.getGraphicsQueueNodeIndex()
    cmdPool = vku::commandPool(device_, queueFamilyIndex);
    images[0] = vku::image(device_, width, height);
    images[1] = vku::image(device_, width, height);
    images[0].allocate(device_);
    images[0].bindMemoryToImage();
    images[0].createView();
    images[1].allocate(device_);
    images[1].bindMemoryToImage();
    images[1].createView();
    commandBuffers[0] = vku::commandBuffer(device_, cmdPool);
    commandBuffers[1] = vku::commandBuffer(device_, cmdPool);
    framebuffers[0] = vku::framebuffer(device_, images.data(), 2, renderPass, width, height);
    framebuffers[1] = vku::framebuffer(device_, images.data(), 2, renderPass, width, height);

    // We have two command buffers, one for even frames and one for odd frames.
    // This allows us to update one while rendering another.
    // In this example, we only update the command buffers once at the start.
    for (int32_t i = 0; i < images.size(); ++i) {
      const vku::commandBuffer &cmdbuf = commandBuffers[i];
      cmdbuf.begin(renderPass, framebuffers[i], width, height);

      cmdbuf.bindPipeline(pipe);
      cmdbuf.bindVertexBuffer(vertex_buffer, vertex_buffer_bind_id);
      cmdbuf.bindIndexBuffer(index_buffer);
      cmdbuf.drawIndexed((uint32_t)num_indices, 1, 0, 0, 1);

      cmdbuf.end(images[i]);
    }
  }

  // Recalculate the matrices and upload to the card.
  void updateUniformBuffers()
  {
    vku::device &device_ =  vku::instance::get().device();

    printf("%p\n", (VkDevice)device_);
    uniform_data.viewToProjection = glm::perspective(60.0f * (3.14159f/180), (float)width / (float)height, 0.1f, 256.0f);
    uniform_data.modelToWorld = glm::mat4();
    uniform_data.worldToView = glm::mat4();
    uniform_data.worldToView[3] = glm::vec4(0, 0, 10, 1);
    uniform_data.normalToWorld = uniform_data.modelToWorld;
    uniform_data.lightPosition = glm::vec4(10, 10, -10, 1);

    printf("%p\n", (VkDevice)device_);
    void *dest = uniform_buffer.map();
    memcpy(dest, &uniform_data, sizeof(uniform_data));
    uniform_buffer.unmap();
    printf("%p\n", (VkDevice)device_);
  }

  // Sumbit the command buffer to draw.
  void render()
  {
    vku::device &device_ =  vku::instance::get().device();

    printf("%p\n", (VkDevice)device_);
    updateUniformBuffers();
    printf("%p\n", (VkDevice)device_);
    device_.waitIdle();

    vku::semaphore sema(device_);

    vku::queue queue(vku::instance::get().queue(), device_);
    queue.submit(sema, commandBuffers[0]);

    //present();
    device_.waitIdle();
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

  vku::commandPool cmdPool;
  std::array<vku::image, 2> images;
  std::array<vku::commandBuffer, 2> commandBuffers;
  std::array<vku::framebuffer, 2> framebuffers;

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
