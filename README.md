Vookoo 2.0
==========

Vookoo is a set of dependency-free utilities to assist in the construction and updating of
Vulkan graphics data structres.

It is derived from the excellent projects of Sascha Willems,
Alexander Overvoorde and Khronos for Vulkan-Hpp project.

[https://github.com/SaschaWillems/Vulkan]

[https://github.com/Overv/VulkanTutorial]

[https://github.com/KhronosGroup/Vulkan-Hpp]

Vookoo adds a "vku" namespace to the "vk" namespace of the C++ interface
and provides user friendly interfacs for building pipelines and other
Vulkan data structures.

The aim of this project is to make Vulkan programming as easy as OpenGL.
Vulkan is known for immensely verbose data structures and exacting rules
but this can be mitigated but providing classes to help the construction
of Vulkan resources.

If you want to contribute to Vookoo, please send my some pull requests.
I will post some work areas that could do with improvement.

History
=======

Vookoo1.0 was an earlier project that did much the same thing but acted
as a "Layer" on top of the C interface. Because it was duplicating much
of the work of Vulkan-Hpp, we decided to replace it with the new Vookoo interface.
The old one is still around if you want to use it.

Library
=======

Currently the library consists of two header files:

    vku.hpp            The library itself
    vku_framework.hpp  An easy framework for running the examples

If you have an existing game engine then vku can be used with no dependencies.

If you are learning Vulkan, the framework library provides window services
by way of glfw3.

The repository contains all the files needed to build the examples, but
you may also use the headers stand-alone, for example with Android builds.

Examples
========

There are currently two examples:

    helloTriangle   Draw a triangle using vertex buffers
    texture         Draw a textured triangle

Building the examples on Windows:

    mkdir build
    cd build
    cmake -G "Visual Studio 14 2015 Win64" ..\examples
    VookooExamples.sln

Building the examples on Linux:

    mkdir build
    cd build
    cmake ../examples
    make
