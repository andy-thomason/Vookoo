Vookoo: Vulkan Utititles Library
================================

A simple utilities library in modern C++ for the Vulkan graphics
API that has sensible defaults and an easy programming interface.

Intended to be a "GLUT" for Vulkan.

The initial implementation is based on Sasch Willem's excellent
Vulkan examples.

Introduction
------------

Vookoo (VKU) is a set of C++ classes that loosely wrap
Vulkan objects.

Vookoo handles the creation of Vulkan instances, devices, pipelines,
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
Object ownership
----------------

We currently do not use reference counting to keep track of object ownership
instead we pass ownership using the C++11 move semantics. This allows us to
aggregate objects in C++ classes without using memory allocation.

This is an example:

```C++

// In the class header.
vku::buffer vertex_buffer;
...
// In the constructor
vertex_buffer = vku::buffer(device(), data, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

```

Ownership of the VkBuffer object is transferred from the temporary object by move semantics.
When the class is destroyed, the buffer will be freed.

In order for Vulkan objects to be destroyed, we need a copy of the VkDevice handle in
the object and so the vku::buffer class contains two handles.

Some VKU classes contain and own several Vulkan objects. For example the vku::image class
owns the VkImage and also the view and memory objects. This is convenient when updating
images from host data, for example.

If you want more efficient storage, or just wish to pass an object without transfering ownership
you can use non-owning versions of the vku classes.

The library
-----------

VKU is a modern C++ library like STL, Boost and GLM and does not need to be pre-built.
Distribution is source only.

Building the examples
---------------------

Although you don't need to build the library, you may want to build the examples.

On Windows make a directory called "build" somewhere else and then run

  mkdir build
  cd build
  cmake -G "Visual Studio 14 2015 Win64" ..

This builds a visual studio 2015 project Vookoo.sln

On Linux you can drop the -G for the default of unix makefiles.

  mkdir build
  cd build
  cmake ..

This makes a makefile that you can build with make


Things to do
------------

This summer we have a number of things to do. I am hoping that my army of volunteers
will help out with this:

* Make sure all the examples run and add more examples.
* Make sure the FBX importer runs on all FBX files.
* Clean up the interfaces so we have 1:1 from classes to Vulkan objects.
* Add all available attributes to vku::pipelineLayoutHelper.
* Complete the "web view" network interface.
* Build an android app to use the web view interface more efficiently.
* Test on VR devices: HTC Vive, Occulus, Gear VR and my enormous Huwawei phablet.
* Support all descriptor set elements.
* Develop compute elements for image conversion and marching cubes.
* Develop samples for shadows and deferred rendering.

I plan to rename vku::image to vku::imageMemAndView as basic wrappers such as vku::semaphore
should only wrap their Vulkan object.

Composite objects such as vku::texture and vku::window should not have names that conflict
with 

Andy Thomason

