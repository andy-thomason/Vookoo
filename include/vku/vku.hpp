////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: main include file
// 

// derived from https://github.com/SaschaWillems/Vulkan
//
// Many thanks to Sascha, without who this would be a challenge!

#ifndef VKU_INCLUDED
#define VKU_INCLUDED

// Vulkan classes
#include <vku/resource.hpp>
#include <vku/device.hpp>
#include <vku/instance.hpp>
#include <vku/swapChain.hpp>
#include <vku/shaderModule.hpp>
#include <vku/buffer.hpp>
#include <vku/renderPassLayout.hpp>
#include <vku/renderPass.hpp>
#include <vku/pipeline.hpp>
#include <vku/semaphore.hpp>
#include <vku/queue.hpp>
#include <vku/commandPool.hpp>
#include <vku/commandBuffer.hpp>
#include <vku/image.hpp>
#include <vku/texture.hpp>
#include <vku/sampler.hpp>
#include <vku/framebuffer.hpp>
#include <vku/descriptorSet.hpp>


// Helper classes
#include <vku/zipDecoder.hpp>
#include <vku/pngDecoder.hpp>
#include <vku/fbxFile.hpp>
#include <vku/mesh.hpp>

// forward references
#include <vku/swapChain.inl>

inline void vku::commandBuffer::bindBindDescriptorSet(vku::pipelineLayout &layout, vku::descriptorSet &set) const {
  vkCmdBindDescriptorSets(get(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout.get(), 0, 1, set.ref(), 0, NULL);
}


#endif
