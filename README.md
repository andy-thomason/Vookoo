Vulkan Utititles Library
========================

A simple utilities library for the Vulkan graphics API that
has sensible defaults and an easy programming interface.

Introduction
------------

VKU (Vookoo) is a set of C++ classes that loosely wrap
Vulkan objects.

VKU handles the creation of Vulkan instances, devices, pipelines,
buffers and images.

For example, the commandBuffer class wraps a VkCommandBuffer object
and provides some simple interfaces.

```C++
cmdbuf.begin(swapChain().renderPass(), swapChain().frameBuffer(i), width(), height());
cmdbuf.bindPipeline(pipe);
cmdbuf.bindVertexBuffer(vertex_buffer, VERTEX_BUFFER_BIND_ID);
cmdbuf.bindIndexBuffer(index_buffer);
cmdbuf.drawIndexed((uint32_t)num_indices, 1, 0, 0, 1);
cmdbuf.end(swapChain().image(i));
```

Creating pipelines is very easy with the pipelineCreateHelper class.

```C++
vku::pipelineCreateHelper pipeHelper;
pipeHelper.binding(VERTEX_BUFFER_BIND_ID, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
pipeHelper.attrib(0, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, 0);
pipeHelper.attrib(1, VERTEX_BUFFER_BIND_ID, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);
pipeHelper.uniformBuffers(1, VK_SHADER_STAGE_VERTEX_BIT);
pipeHelper.shader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
pipeHelper.shader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);
pipe = vku::pipeline(device(), swapChain().renderPass(), pipelineCache(), pipeHelper);
```
