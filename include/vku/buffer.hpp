////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016, 2017, 2017
//
// Vookoo: buffer wraps VkBuffer
// 

#ifndef VKU_BUFFER_INCLUDED
#define VKU_BUFFER_INCLUDED

#include <vku/resource.hpp>
#include <vku/instance.hpp>
#include <cstring>

namespace vku {

struct buffer_aux_data {
  VkDeviceMemory mem = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
};

class buffer : public resource<VkBuffer, buffer, buffer_aux_data> {
public:
  VKU_RESOURCE_BOILERPLATE(VkBuffer, buffer)
  
  buffer(const vku::device &device, void *init, VkDeviceSize size, VkBufferUsageFlags usage) : resource(device) {
    aux().size = size;

    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = usage;
    VkBuffer buf = VK_NULL_HANDLE;
    VkResult err = vkCreateBuffer(dev(), &bufInfo, nullptr, &buf);
    if (err) throw error(err, __FILE__, __LINE__);

    set(buf, true);

    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    vkGetBufferMemoryRequirements(dev(), get(), &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = device.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    err = vkAllocateMemory(dev(), &memAlloc, VK_NULL_HANDLE, &aux().mem);
    if (err) throw error(err, __FILE__, __LINE__);

    if (init) {
      void *dest = map();
      std::memcpy(dest, init, (size_t)size);
      unmap();
    }

    err = vkBindBufferMemory(dev(), get(), aux().mem, 0);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  void destroy() {
    vkDestroyBuffer(dev(), get(), nullptr);
    if (mem()) vkFreeMemory(dev(), mem(), nullptr);
  }

  void *map() const {
    void *dest = nullptr;
    VkResult err = vkMapMemory(dev(), mem(), 0, size(), 0, &dest);
    if (err) throw error(err, __FILE__, __LINE__);
    return dest;
  }

  void unmap() const {
    vkUnmapMemory(dev(), aux().mem);
  }

  size_t size() const {
    return (size_t)aux().size;
  }

  VkDeviceMemory mem() const {
    return aux().mem;
  }
};


} // vku

#endif
