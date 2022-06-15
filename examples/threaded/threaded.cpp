////////////////////////////////////////////////////////////////////////////////
//
// Vookoo threaded example (C) 2017 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//
// In this sample we demonstrate threaded building of a secondary command buffer.  
//

// Include the demo framework, vookoo (vku) for building objects and glm for maths.
// The demo framework uses GLFW to create windows.
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext.hpp> // for rotate, scale, translate
#include <thread>

int main() {
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  const char *title = "threaded";
  auto glfwwindow = glfwCreateWindow(800, 800, title,  nullptr, nullptr);

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

  // Create a window to draw into
  vku::Window window{
    fw.instance(),
    fw.device(),
    fw.physicalDevice(),
    fw.graphicsQueueFamilyIndex(),
    glfwwindow
  };
  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }
  window.clearColorValue() = {0.55f, 0.65f, 0.75f, 1.0f};

  // Create two shaders, vertex and fragment.
  vku::ShaderModule vert{fw.device(), BINARY_DIR "threaded.vert.spv"};
  vku::ShaderModule frag{fw.device(), BINARY_DIR "threaded.frag.spv"};

  // These are the parameters we are passing to the shaders
  // Note! be very careful when using vec3, vec2, float and vec4 together
  // as there are alignment rules you must follow.
  struct PushConstant {
    glm::vec4 colour;
    glm::mat4 transform;
  };

  // Define N distinct objects by their colour and transform (using push constants).
  const int N = 105;
  std::vector<PushConstant> P;
  for(int n=0; n<N; ++n) {
    P.emplace_back(
      PushConstant{
        .colour = glm::vec4{1}, 
        .transform = glm::translate(glm::vec3(0.0f,0.0f,1.0f-n/float(N))) * glm::scale(glm::vec3(1.0f-n/float(N)))
      }
    );
  }

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
  vku::HostVertexBuffer buffer(fw.device(), fw.memprops(), vertices);

  // Make a pipeline to use the vertex format and shaders.
  vku::PipelineMaker pm{window.width(), window.height()};
  pm.shader(vk::ShaderStageFlagBits::eVertex, vert)
    .shader(vk::ShaderStageFlagBits::eFragment, frag)
    .vertexBinding(0, sizeof(Vertex))
    .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos))
    .vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, colour))
    .depthTestEnable(true);

  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  // 
  vku::PipelineLayoutMaker plm{};
  plm.pushConstantRange(vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstant));
  auto pipelineLayout = plm.createUnique(fw.device());

  // Create a pipeline using a renderPass built for our window.
  auto pipeline = pm.createUnique(fw.device(), fw.pipelineCache(), *pipelineLayout, window.renderPass());

  // define number of threads
  const uint32_t Nthreads = std::thread::hardware_concurrency();
  std::cout << "Nthreads = " << Nthreads << std::endl;

  // allocate array of threads to be reused from frame to frame
  std::vector<std::thread> v( Nthreads );
  
  // allocate command pools and secondary buffers, required to be separate pools per thread
  std::vector<vk::UniqueCommandPool> scp;
  std::vector<std::vector<vk::UniqueCommandBuffer>> scb;
  for (int i=0; i<Nthreads; ++i) {
    // add pool
    scp.emplace_back( 
      fw.device().createCommandPoolUnique(
        vk::CommandPoolCreateInfo{ 
          vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
          fw.graphicsQueueFamilyIndex()
        }
      )
    );
    // add secondary command buffer(s) per pool
    scb.emplace_back( 
      std::move(
        fw.device().allocateCommandBuffersUnique( 
          vk::CommandBufferAllocateInfo{
            *scp.back(), // per thread pool 
            vk::CommandBufferLevel::eSecondary,
            (uint32_t)window.numImageIndices() // a secondary command buffer per each swap image, access via *scb[i_th thread][j_th imageindex]
          }
        )
      )
    );
  }

  // begin rendering frames
  int frame = 0;

  // Loop waiting for the window to close.
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    // we generate the different fractions of the command buffer dynamically
    // on different threads and join them for final render with single call of executeCommands.
    window.draw(
      fw.device(), fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {

        // shared state used by each kernel thread
        vk::CommandBufferInheritanceInfo inheritanceInfo;
        inheritanceInfo.setRenderPass( rpbi.renderPass );
        inheritanceInfo.setFramebuffer( rpbi.framebuffer );
        vk::CommandBufferBeginInfo commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eRenderPassContinue, &inheritanceInfo);

        // Multi-threaded command buffer generation aka multi-threaded rendering:
        //
        //     Have one command pool per thread.
        //     You populate different secondary-level (VK_COMMAND_BUFFER_LEVEL_SECONDARY) command buffers per thread.
        //     In your main thread you have a primary command buffer.
        //     After all threads are done building you call vkCmdExecuteCommands with your secondary-level command buffers.
        //     You submit the primary command buffer and get a fence back.
        //     A few frames down the line you check if the fence is triggered and if it is you free, or even better, re-use the command buffers.
        //     To re-use the command buffers you reset them. When some thread asks for a command buffer you give back one of the reset ones.
        //
        // For example, consider Nthreads=3 and N objects
        // 1234567890AB  N
        // 012           i=0; i<Nthreads; ++i
        // 0^^3  6  9    on Thread i=0: j=0;j<N;j+=Nthreads
        //  1^ 4  7  A   on Thread i=1: j=1;j<N;j+=Nthreads
        //   2  5  8  B  on Thread i=2: j=2;j<N;j+=Nthreads

        // build (multi-threaded) list of secondary command buffers to be executed later
        std::vector<vk::CommandBuffer> commandBuffers;
        { // threadRenderCode
          auto kernel = [&](const int i)->void{  
            vk::CommandBuffer cmdBuffer = *scb[i][imageIndex];
            cmdBuffer.begin(commandBufferBeginInfo);
            cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
            cmdBuffer.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));
            for (int j=i; j<N; j+=Nthreads) {
              // update p here
              PushConstant *p = &P[j];
              p->transform *= glm::rotate(glm::radians(1.0f*(1.0f-j/float(N))), glm::vec3(0, 0, 1));
              p->colour.r = (std::sin(frame * 0.01f) + 1.0f) / 2.0f;
              p->colour.g = (std::cos(frame * 0.01f) + 1.0f) / 2.0f;
    
              // Use pushConstants to set individual object's values.
              // vulkan guaranteed a pushConstant size of 128 bytes or less always possible.
              // This takes a copy of the push constant at the time 
              // we create this command buffer.
              cmdBuffer.pushConstants(
                *pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, sizeof(PushConstant), p
              );
              cmdBuffer.draw(vertices.size(), 1, 0, 0); // since secondary buffer, not drawn yet, just recorded for later
            };
            cmdBuffer.end();
          };

          // *** spawn separate rendering kernel threads to build separate command buffers in parallel *** 
          for (int i=0; i<Nthreads; ++i) {
            v[i] = std::thread( kernel, i );
          }

          // wait for all of the kernel threads to finish
          for (int i=0; i<Nthreads; ++i) {
            v[i].join();
          }
          
          // accumulate secondary command buffers built by kernel threads
          for (int i=0; i<Nthreads; ++i) {
            commandBuffers.push_back( *scb[i][imageIndex] );
          }
        } 

        // execute accumlated commandBuffers
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);
        cb.beginRenderPass(rpbi, vk::SubpassContents::eSecondaryCommandBuffers);
        cb.executeCommands( commandBuffers );
        cb.endRenderPass();
        cb.end();
      }
    );

    // Very crude method to prevent your GPU from overheating.
    //std::this_thread::sleep_for(std::chrono::milliseconds(16));

    ++frame;
  }

  // Wait until all drawing is done and then kill the window.
  fw.device().waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  // The Framework and Window objects will be destroyed here.

  return 0;
}
