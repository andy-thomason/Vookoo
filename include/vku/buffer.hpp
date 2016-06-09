////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_BUFFER_INCLUDED
#define VKU_BUFFER_INCLUDED

#include <vku/resource.hpp>
#include <vku/instance.hpp>
#include <cstring>

namespace vku {

class buffer {
public:
  buffer(VkDevice dev = VK_NULL_HANDLE, VkBuffer buf = VK_NULL_HANDLE) : buf_(buf), dev(dev) {
  }

  buffer(device dev, void *init, VkDeviceSize size, VkBufferUsageFlags usage) : dev(dev), size_(size) {
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = usage;
    VkResult err = vkCreateBuffer(dev, &bufInfo, nullptr, &buf_);
    if (err) throw error(err, __FILE__, __LINE__);

    ownsBuffer = true;

    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    vkGetBufferMemoryRequirements(dev, buf_, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = dev.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    err = vkAllocateMemory(dev, &memAlloc, VK_NULL_HANDLE, &mem);
    if (err) throw error(err, __FILE__, __LINE__);

    if (init) {
      void *dest = map();
      std::memcpy(dest, init, size);
      unmap();
    }
    bind();
  }

  buffer(VkBufferCreateInfo bufInfo, VkDevice dev = VK_NULL_HANDLE) : dev(dev) {
    vkCreateBuffer(dev, &bufInfo, nullptr, &buf_);
  }

  // RAII move operator
  buffer &operator=(buffer &&rhs) {
    dev = rhs.dev;
    buf_ = rhs.buf_;
    mem = rhs.mem;
    size_ = rhs.size_;
    rhs.dev = VK_NULL_HANDLE;
    rhs.mem = VK_NULL_HANDLE;
    rhs.buf_ = VK_NULL_HANDLE;

    rhs.ownsBuffer = false;
    return *this;
  }

  ~buffer() {
    if (ownsBuffer) {
      if (buf_) vkDestroyBuffer(dev, buf_, nullptr);
      if (mem) vkFreeMemory(dev, mem, nullptr);
      buf_ = VK_NULL_HANDLE;
      mem = VK_NULL_HANDLE;
    }
  }

  void *map() {
    void *dest = nullptr;
    VkResult err = vkMapMemory(dev, mem, 0, size(), 0, &dest);
    if (err) throw error(err, __FILE__, __LINE__);
    return dest;
  }

  void unmap() {
    vkUnmapMemory(dev, mem);
  }

  size_t size() const {
    return size_;
  }

  //operator VkBuffer() const { return buf_; }

  VkBuffer buf() const { return buf_; }

  VkDescriptorBufferInfo desc() const {
    VkDescriptorBufferInfo d = {};
    d.buffer = buf_;
    d.range = size_;
    return d;
  }

private:
  void bind() {
    VkResult err = vkBindBufferMemory(dev, buf_, mem, 0);
    if (err) throw error(err, __FILE__, __LINE__);
  }

  VkBuffer buf_ = VK_NULL_HANDLE;
  VkDevice dev = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  size_t size_;
  bool ownsBuffer = false;
};


} // vku

#endif
