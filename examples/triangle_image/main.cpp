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
  triangle_image(int argc, const char **argv) : device_(vku::instance::singleton().device()) {
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

    vku::renderPassLayout renderPassLayout;
    uint32_t color = renderPassLayout.addAttachment(VK_FORMAT_R8G8B8A8_UNORM);
    uint32_t depth = renderPassLayout.addAttachment(VK_FORMAT_D24_UNORM_S8_UINT);
    renderPassLayout.addSubpass(color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    vku::renderPass renderPass(device_, renderPassLayout);
    vku::pipelineCache pipelineCache(device_);
    pipe = vku::pipeline(device_, renderPass.get(), pipelineCache, pipeHelper);

    // construct the descriptor pool which is used at runtime to allocate descriptor sets
    uint32_t num_uniform_buffers = 1;
    descPool = vku::descriptorPool(device_, num_uniform_buffers);

    // Allocate descriptor sets for the uniform buffer
    // todo: descriptor sets need a little more work.
    pipe.allocateDescriptorSets(descPool);
    pipe.updateDescriptorSets(uniform_buffer);

    vku::imageLayoutHelper backBufferLayout(width, height);
    backBufferLayout.format(VK_FORMAT_R8G8B8A8_UNORM);
    backBufferLayout.tiling(VK_IMAGE_TILING_OPTIMAL);
    backBufferLayout.usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    backBufferLayout.initialLayout(VK_IMAGE_LAYOUT_UNDEFINED);

    uint32_t queueFamilyIndex = vku::instance::singleton().graphicsQueueIndex();
    cmdPool = vku::commandPool(device_, queueFamilyIndex);
    backBuffers[0] = vku::image(device_, backBufferLayout);
    backBuffers[1] = vku::image(device_, backBufferLayout);
    commandBuffers[0] = vku::commandBuffer(device_, cmdPool);
    commandBuffers[1] = vku::commandBuffer(device_, cmdPool);

    vku::imageLayoutHelper depthBufferLayout(width, height);
    depthBufferLayout.format(VK_FORMAT_D24_UNORM_S8_UINT);
    depthBufferLayout.tiling(VK_IMAGE_TILING_OPTIMAL);
    depthBufferLayout.usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    depthBufferLayout.initialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
    depthBuffer = vku::image(device_, depthBufferLayout);

    vku::imageLayoutHelper readBufferLayout(width, height);
    readBufferLayout.format(VK_FORMAT_R8G8B8A8_UNORM);
    readBufferLayout.tiling(VK_IMAGE_TILING_LINEAR);
    readBufferLayout.usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    readBufferLayout.initialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
    readBufferLayout.memoryPropertyFlag((VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    readBuffer = vku::image(device_, readBufferLayout);

    framebuffers[0] = vku::framebuffer(device_, backBuffers[0], depthBuffer, renderPass, width, height);
    framebuffers[1] = vku::framebuffer(device_, backBuffers[1], depthBuffer, renderPass, width, height);

    vku::commandBuffer preRender(device_, cmdPool);
    preRender.beginCommandBuffer();
    backBuffers[0].setImageLayout(preRender, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    backBuffers[1].setImageLayout(preRender, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    depthBuffer.setImageLayout(preRender, VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    preRender.endCommandBuffer();
    vku::queue queue(vku::instance::singleton().queue(), device_);
    vku::semaphore sema(device_);
    queue.submit(sema, preRender);

    // We have two command buffers, one for even frames and one for odd frames.
    // This allows us to update one while rendering another.
    // In this example, we only update the command buffers once at the start.
    for (size_t i = 0; i < commandBuffers.size(); ++i) {
      const vku::commandBuffer &cmdbuf = commandBuffers[i];
      cmdbuf.begin(renderPass, framebuffers[i], width, height);
      backBuffers[i].setImageLayout(cmdbuf, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      cmdbuf.bindPipeline(pipe);
      cmdbuf.bindVertexBuffer(vertex_buffer, vertex_buffer_bind_id);
      cmdbuf.bindIndexBuffer(index_buffer);
      cmdbuf.drawIndexed((uint32_t)num_indices, 1, 0, 0, 1);

      cmdbuf.end(backBuffers[i]);
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
    device_.waitIdle();

    vku::queue queue(vku::instance::singleton().queue(), device_);

    {
      vku::semaphore sema(device_);
      queue.submit(sema, commandBuffers[0]);
    }

    //present();
    device_.waitIdle();

    vku::commandBuffer postRender(device_, cmdPool);
    postRender.beginCommandBuffer();
    backBuffers[0].setImageLayout(postRender, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
    readBuffer.setImageLayout(postRender, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

    VkImageCopy cpy = {};
    cpy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpy.srcSubresource.baseArrayLayer = 0;
    cpy.srcSubresource.layerCount = 1;
    cpy.srcSubresource.mipLevel = 0;
    cpy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpy.dstSubresource.baseArrayLayer = 0;
    cpy.dstSubresource.layerCount = 1;
    cpy.dstSubresource.mipLevel = 0;
    cpy.extent = { width, height, 1 };
    postRender.copyImage(backBuffers[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);
    //readBuffer.copy
    postRender.endCommandBuffer();

    {
      vku::semaphore sema(device_);
      queue.submit(sema, postRender);
    }

    uint8_t *bytes = (uint8_t *)readBuffer.map();
    std::ofstream file("test.bmp");
    auto writer = [&file](const char *data, size_t size) { file.write(data, size); };
    readBuffer.writeBMP(width, height, writer);
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
  std::array<vku::image, 2> backBuffers;
  std::array<vku::commandBuffer, 2> commandBuffers;
  std::array<vku::framebuffer, 2> framebuffers;
  vku::image depthBuffer;

  vku::image readBuffer;

  static const uint32_t width = 256;
  static const uint32_t height = 256;

  vku::device &device_;
};

triangle_image *gv;

int main(const int argc, const char *argv[]) {
  // create a window.
  triangle_image my_example(argc, argv);
  gv = &my_example;
  my_example.render();
  return 0;
}
