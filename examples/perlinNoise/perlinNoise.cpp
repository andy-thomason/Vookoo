////////////////////////////////////////////////////////////////////////////////
//
// Vookoo instancing example (C) 2017 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//

// Include the demo framework, vookoo (vku) for building objects and glm for maths.
// The demo framework uses GLFW to create windows.
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/ext.hpp> // for rotate()
#include "icosphereGenerator.hpp"
#include <algorithm> // std::generate

////////////////////////////////////////
//
// Mouse rotation of scene 
glm::mat4 mouse_rotation{1.0f}; 
static void mouse_callback(GLFWwindow* window, double x, double y) {
  static glm::vec3 previous_position{0.f};
  glm::vec3 position(float(x),float(y),0.0f);

  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
  if (state == GLFW_PRESS) {
    auto movement = position - previous_position;
    // pg.127 "Visualizing Quaternions"
    auto mouse_axis = glm::vec3(+movement.y, +movement.x, 0.0f);
    auto mouse_angle = glm::length(mouse_axis) / 250.f; // 250. arbitrary scale
    mouse_rotation = glm::rotate(mouse_angle, mouse_axis);
  } else {
    mouse_rotation = glm::mat4(1.0f);
  }
  previous_position = position;
}

int main() {
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  auto *title = "perlinNoise";
  auto glfwwindow = glfwCreateWindow(1024, 1024, title, nullptr, nullptr);

  glfwSetCursorPosCallback(glfwwindow, mouse_callback);

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
  vku::Window window{fw.instance(), fw.device(), fw.physicalDevice(), fw.graphicsQueueFamilyIndex(), glfwwindow};
  if (!window.ok()) {
    std::cout << "Window creation failed" << std::endl;
    exit(1);
  }
  window.clearColorValue() = {0.0f, 0.0f, 0.0f, 1.0f};

  ////////////////////////////////////////
  //
  // Create Uniform Buffer

  struct Uniform {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 world;
    float iTime[4]; // [0] contains time, rest just filler for memeory alignment
  };
  // Create, but do not upload the uniform buffer as a device local buffer.
  vku::UniformBuffer ubo(fw.device(), fw.memprops(), sizeof(Uniform));

  ////////////////////////////////////////
  //
  // Create mesh and indices, Part 1
  // Icosphere 
  //
  std::vector<float> verticesG;
  std::vector<unsigned int> indices;
  generateIcosphere(&verticesG, &indices, 4, true); // 4-->2048 Triangles, true-->Counter Clockwise Winding

  ////////////////////////////////////////
  //
  // Create mesh and indices, Part 2
  // (Icosphere, but transformed to be convenient/matched with Vulkan buffers)
  //
  struct Vertex { 
    glm::vec3 pos; 
    glm::vec3 normal;
  };

  // build mesh vertices (also given position derive normal vector)
  std::vector<Vertex> vertices( verticesG.size()/3 );
  std::generate(vertices.begin(), vertices.end(), [&verticesG, i=0] () mutable { 
    glm::vec3 r = {verticesG[i*3], verticesG[i*3+1], verticesG[i*3+2]};
    r = glm::normalize(r);
    i++;
    return Vertex{
      .pos=20.f*r, // vertex shader, as written, expects a sphere with radius of 20
      .normal=r,   // when sphere position is normalized, position same as unit normal vector
    };
  });

  // Sync mesh verticws with GPU
  vku::HostVertexBuffer bufferVertices(fw.device(), fw.memprops(), vertices);

  // Sync indices with GPU
  vku::HostIndexBuffer ibo(fw.device(), fw.memprops(), indices);

  ////////////////////////////////////////////////////////
  //
  // explosion.png converted by gimp (using "export as" file with .c extension)
  // (colormap)

  const struct {
    unsigned int 	 width;
    unsigned int 	 height;
    unsigned int 	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */ 
    unsigned char	 pixel_data[1 * 128 * 4 + 1];
  } explosion_image = {
    1, 128, 4,
    "\000\000\000\377\000\000\000\377\000\000\001\377\000\000\000\377\000\000\001\377\000\000\001\377\001\000\000\377\001"
    "\000\000\377\001\000\000\377\002\001\000\377\003\001\000\377\005\001\000\377\006\001\000\377\006\001\000\377\010\001"
    "\000\377\013\001\000\377\015\002\000\377\023\003\000\377\034\003\000\377%\005\000\377\064\010\000\377="
    "\010\000\377G\012\000\377J\013\000\377G\013\000\377E\011\000\377E\010\000\377I\011\000\377L\011"
    "\000\377J\013\000\377G\013\000\377G\013\000\377M\013\000\377P\013\000\377U\013\000\377]\013\000"
    "\377c\015\000\377m\020\000\377w\021\000\377\203\023\000\377\213\024\000\377\226\026\000\377"
    "\236\030\000\377\255\032\000\377\267\037\000\377\276#\000\377\304%\000\377\313&\000\377"
    "\320*\001\377\332.\001\377\342\064\001\377\352>\002\377\355C\002\377\356G\002\377\370"
    "T\003\377\376d\005\377\377q\010\377\376\177\015\377\376\215\024\377\376\256\035\377"
    "\376\313!\377\376\347(\377\376\374N\377\375\377}\377\375\377\263\377\375"
    "\377\320\377\375\377\336\377\375\377\345\377\376\377\350\377\375\377\342"
    "\377\376\376\325\377\374\377\263\377\373\377\210\377\373\377a\377\374\377"
    "L\377\376\366/\377\376\340(\377\376\324$\377\376\320#\377\376\320#\377\376"
    "\313\037\377\376\314!\377\376\320$\377\376\352/\377\375\377a\377\374\377k"
    "\377\373\377\204\377\375\377\250\377\375\377\313\377\375\377\335\377\376"
    "\377\345\377\375\377\360\377\376\377\366\377\376\377\363\377\374\377\354"
    "\377\374\377\355\377\375\377\364\377\376\377\372\377\376\377\376\377\376"
    "\377\376\377\376\377\376\377\376\376\377\377\376\376\370\377\374\377\320"
    "\377\375\377\253\377\374\377\245\377\374\377\245\377\374\377\240\377\373"
    "\377\236\377\374\377\247\377\375\377\271\377\375\377\307\377\375\377\332"
    "\377\375\377\345\377\374\377\346\377\374\377\356\377\376\377\365\377\376"
    "\377\366\377\376\377\367\377\376\377\367\377\375\377\361\377\375\377\356"
    "\377\375\377\356\377\374\377\352\377\374\377\347\377\375\377\345\377\375"
    "\377\346\377\374\377\354\377",
  };
  std::vector<uint8_t> pixels(explosion_image.width * explosion_image.height * explosion_image.bytes_per_pixel);
  std::generate(pixels.begin(), pixels.end(), [&explosion_image, i=0] () mutable { return explosion_image.pixel_data[i++]; });
  vku::TextureImage2D texture{fw.device(), fw.memprops(), explosion_image.width, explosion_image.height, 1, vk::Format::eR8G8B8A8Unorm};
  texture.upload(fw.device(), pixels, window.commandPool(), fw.memprops(), fw.graphicsQueue());

  // Create linearSampler
  vku::SamplerMaker sm{};
  auto linearSampler = sm
    .magFilter( vk::Filter::eLinear )
    .minFilter( vk::Filter::eLinear )
    .mipmapMode( vk::SamplerMipmapMode::eNearest )
    .addressModeV( vk::SamplerAddressMode::eClampToEdge )
    .createUnique(fw.device());

  ////////////////////////////////////////
  //
  // Define multiple instances of Icospheres

  // Per-instance data block
	struct Instance {
		glm::vec3 pos;
		glm::vec3 rot;
		float scale;
		float t0;
	};

  // Build the multiple instance data.
  std::vector<Instance> instances = {
    {.pos={ 0.0f, 0.0f, 0.0f}, .rot={0.0f, 0.0f, 0.5f}, .scale=0.50f/40.f, .t0=0.5f}, // index [0] is central large star
  };
  for (size_t i=1; i<1024; i++) {
    float q = (float) rand()/RAND_MAX * 6.28f;
    float r = 0.4f + (float) rand()/RAND_MAX*0.35f;
    Instance v{
      .pos={ r*cos(q), r*sin(q), 0.f}, 
      .rot={3.14f*(1.f+(float) rand()/RAND_MAX),3.14f*(1.f+(float) rand()/RAND_MAX),3.14f*(1.f+(float) rand()/RAND_MAX)},
      .scale=0.016f/40.f * (1.f+(float) rand()/RAND_MAX),
      .t0=(float) rand()/RAND_MAX*2.f
    };
    instances.push_back(v);
  };
  vku::HostVertexBuffer bufferInstances(fw.device(), fw.memprops(), instances);

  ////////////////////////////////////////
  //
  // Build the descriptor sets

  vku::DescriptorSetLayoutMaker dslm{};
  auto descriptorSetLayout = dslm
    .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1)
    .image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
    .createUnique(fw.device());

  // Create a pipeline using a renderPass built for our window.
  // Make a default pipeline layout. This shows how pointers
  // to resources are layed out.
  vku::PipelineLayoutMaker plm{};
  auto pipelineLayout = plm
    .descriptorSetLayout(*descriptorSetLayout)
    .createUnique(fw.device());

  ////////////////////////////////////////
  //
  // Define the particular descriptor sets for the shader uniforms.

  vku::DescriptorSetMaker dsm{};
  auto descriptorSets = dsm
    .layout(*descriptorSetLayout)
    .create(fw.device(), fw.descriptorPool());

  vku::DescriptorSetUpdater dsu;
  dsu
    // descriptorSets[0]
    .beginDescriptorSet(descriptorSets[0])
    // -- layout (set = 0,binding = 0) uniform Uniform
    .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
    .buffer(ubo.buffer(), 0, sizeof(Uniform))
    // -- layout (set = 0,binding = 1) uniform sampler2D tExplosion
    .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
    .image(*linearSampler, texture.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
    // update the descriptor sets with their pointers (but not data).
    .update(fw.device());

  // Create shaders; vertex and fragment.
  vku::ShaderModule vert{fw.device(), BINARY_DIR "perlinNoise.vert.spv"};
  vku::ShaderModule frag{fw.device(), BINARY_DIR "perlinNoise.frag.spv"};

  // https://learnopengl.com/Getting-started/Coordinate-Systems
  // https://www.khronos.org/opengl/wiki/Face_Culling
  // https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
  // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
  // Note above miss fact that minDepth = 0.5f also needs to be set 
  // flip viewport to match opengl ( +x > Right, +y ^ UP, +z towards viewer from screen ), instead of vulkan default
  // also requires pipeline set with cullMode:BACK and frontFace:CounterClockWise
  auto viewport = vk::Viewport{
    0.0f,                                     //Vulkan default:0
    static_cast<float>(window.height()),      //Vulkan default:0
    static_cast<float>(window.width()),   //Vulkan default:width
    -static_cast<float>(window.height()),//Vulkan default:height
    0.5f,                              //Vulkan default:0
    1.0f                               //Vulkan default:1
  };

  auto buildPipeline = [&]() {
    // Make a pipeline to use the vertex format and shaders.
    vku::PipelineMaker pm{window.width(), window.height()};
    return pm
      .shader(vk::ShaderStageFlagBits::eVertex, vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      #define VERTEX_BUFFER_BIND_ID 0
      .vertexBinding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), vk::VertexInputRate::eVertex)
      .vertexAttribute(0, VERTEX_BUFFER_BIND_ID,    vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos))
      .vertexAttribute(1, VERTEX_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal))
      #define INSTANCE_BUFFER_BIND_ID 1
      .vertexBinding(INSTANCE_BUFFER_BIND_ID, sizeof(Instance), vk::VertexInputRate::eInstance)
      .vertexAttribute(2, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Instance, pos))
      .vertexAttribute(3, INSTANCE_BUFFER_BIND_ID, vk::Format::eR32G32B32Sfloat, offsetof(Instance, rot))
      .vertexAttribute(4, INSTANCE_BUFFER_BIND_ID,       vk::Format::eR32Sfloat, offsetof(Instance, scale))
      .vertexAttribute(5, INSTANCE_BUFFER_BIND_ID,       vk::Format::eR32Sfloat, offsetof(Instance, t0))
      //
      .viewport(viewport)
      .depthTestEnable(VK_TRUE)
      .cullMode(vk::CullModeFlagBits::eBack) // GL default
      //.cullMode(vk::CullModeFlagBits::eFront) // VK default
      .frontFace(vk::FrontFace::eCounterClockwise) // GL default
      //.frontFace(vk::FrontFace::eClockwise) // VK default
      //
      .createUnique(fw.device(), fw.pipelineCache(), *pipelineLayout, window.renderPass());
  };

  auto pipeline = buildPipeline(); 

  // Static command buffer, so only need to create the command buffer(s) once.
  window.setStaticCommands(
    [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
      static auto ww = window.width();
      static auto wh = window.height();
      if (ww != window.width() || wh != window.height()) {
        ww = window.width();
        wh = window.height();
        pipeline = buildPipeline();
      }
      vk::CommandBufferBeginInfo bi{};
      cb.begin(bi);
      cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
      cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
      cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
      cb.bindVertexBuffers(  VERTEX_BUFFER_BIND_ID,  bufferVertices.buffer(), vk::DeviceSize(0)); // Binding point VERTEX_BUFFER_BIND_ID : Mesh vertex buffer
      cb.bindVertexBuffers(INSTANCE_BUFFER_BIND_ID, bufferInstances.buffer(), vk::DeviceSize(0)); // Binding point INSTANCE_BUFFER_BIND_ID : Instance data buffer
      cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, {descriptorSets[0]}, {});
      cb.drawIndexed(indices.size(), instances.size(), 0, 0, 0);
      cb.endRenderPass();
      cb.end();
    }
  );

  ////////////////////////////////////////
  //
  // Main update loop 

  Uniform uniform {
    .projection = /*invertYhalfZclipspace * */glm::perspective(
      glm::radians(30.0f), // The vertical Field of View, in radians: the amount of "zoom". Think "camera lens". Usually between 90° (extra wide) and 30° (quite zoomed in)
      float(window.width())/window.height(), // Aspect Ratio. Depends on the size of your window. Notice that 4/3 == 800/600 == 1280/960, sounds familiar ?
      0.1f,                // Near clipping plane. Keep as big as possible, or you'll get precision issues.
      10.0f),              // Far clipping plane. Keep as little as possible.
    .view = glm::lookAt(
      glm::vec3(0,0,1.5f), // Camera location in World Space
      glm::vec3(0,0,0),    // Camera looks at the origin
      glm::vec3(0,1,0)),   // Head is up (set to 0,-1,0 to look upside-down)
    .world = glm::mat4(1.0),
    .iTime = {0.f/60.f, 0, 0, 0},
  };

  int iFrame = 0;

  // Loop waiting for the window to close.
  while (!glfwWindowShouldClose(glfwwindow)) {
    glfwPollEvents();

    // reuse previously recorded command buffer (defined in window.setStaticCommands) but dynamically update ubo's iTime
    window.draw(fw.device(), fw.graphicsQueue(),
      [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
        uniform.iTime[0] = iFrame/60.f;
        vk::CommandBufferBeginInfo bi{};
        cb.begin(bi);
        cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform), &uniform);
        cb.end();
      }
    );

    // animate (map... change Instance ...unmap)
    Instance* objects = static_cast<Instance*>( bufferInstances.map(fw.device()) );
    objects[0].rot += glm::vec3{0.0f, 0.0f,-6.248/10.*16e-3};
    for (auto i=1; i<instances.size(); i++) {
      glm::vec2 vr = glm::normalize(glm::vec2(objects[i].pos.x,objects[i].pos.y));
      objects[i].pos += 0.001f*glm::vec3(-vr.y,vr.x,0.0f);
      objects[i].rot += .1f*glm::normalize(objects[i].rot); // non-physical animation, a bit cartoonish, but still visually fun
    };
    bufferInstances.unmap(fw.device());
    // better performance expected, if move map&unmap outside animation loop
    // and instead, after all objects animated/updated, do bufferInstances.flush(fw.device()) here inside animation loop.
    // [reference: http://kylehalladay.com/blog/tutorial/vulkan/2017/08/13/Vulkan-Uniform-Buffers.html]

    // Very crude method to prevent your GPU from overheating.
    //std::this_thread::sleep_for(std::chrono::milliseconds(16));

    uniform.world = mouse_rotation * uniform.world;
    iFrame++;
  }

  // Wait until all drawing is done and then kill the window.
  fw.device().waitIdle();
  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  // The Framework and Window objects will be destroyed here.

  return 0;
}
