
#ifndef VKU_INCLUDED
#define VKU_INCLUDED


#include "../vulkan/vulkan.h"
#include <cstring>

namespace vku {

class device;

class instance {
public:
  instance(VkInstance inst) : inst(inst) {
  }

  operator VkInstance() const { return inst; }
public:
  VkInstance inst;
};

class device {
public:
  device(VkDevice dev, VkPhysicalDevice physicalDevice) : dev(dev), physicalDevice(physicalDevice) {
  }

  uint32_t getMemoryType(uint32_t typeBits, VkFlags properties) {
    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
  	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	  for (uint32_t i = 0; i < 32; i++) {
		  if (typeBits & (1<<i)) {
			  if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				  return i;
			  }
		  }
	  }
	  return ~(uint32_t)0;
  }

  operator VkDevice() const { return dev; }
public:
  VkDevice dev;
  VkPhysicalDevice physicalDevice;
};

class buffer {
public:
  buffer(VkDevice dev = nullptr, VkBuffer buf = nullptr) : buf(buf), dev(dev) {
  }

  buffer(VkDevice dev, VkBufferCreateInfo *bufInfo) : dev(dev), size_(bufInfo->size) {
		vkCreateBuffer(dev, bufInfo, nullptr, &buf);
    ownsBuffer = true;
  }

  buffer(device dev, void *init, VkDeviceSize size, VkBufferUsageFlags usage) : dev(dev), size_(size) {
    VkBufferCreateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size = size;
		bufInfo.usage = usage;
		VkResult err = vkCreateBuffer(dev, &bufInfo, nullptr, &buf);
    if (err) throw err;

    ownsBuffer = true;

		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		vkGetBufferMemoryRequirements(dev, buf, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = dev.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

 		err = vkAllocateMemory(dev, &memAlloc, nullptr, &mem);
    if (err) throw err;

		if (init) {
		  void *dest = map();
      std::memcpy(dest, init, size);
      unmap();
    }
    bind();
  }

  buffer(VkBufferCreateInfo bufInfo, VkDevice dev = nullptr) : dev(dev) {
		vkCreateBuffer(dev, &bufInfo, nullptr, &buf);
  }

  // RAII move operator
  buffer &operator=(buffer &&rhs) {
    dev = rhs.dev;
    buf = rhs.buf;
    mem = rhs.mem;
    size_ = rhs.size_;
    rhs.dev = nullptr;
    rhs.mem = nullptr;
    rhs.buf = nullptr;

    rhs.ownsBuffer = false;
    return *this;
  }

  ~buffer() {
    if (buf && ownsBuffer) {
      vkDestroyBuffer(dev, buf, nullptr);
      buf = nullptr;
    }
  }

  void *map() {
    void *dest = nullptr;
    VkResult err = vkMapMemory(dev, mem, 0, size(), 0, &dest);
    if (err) throw err;
    return dest;
  }

  void unmap() {
		vkUnmapMemory(dev, mem);
  }

  void bind() {
		VkResult err = vkBindBufferMemory(dev, buf, mem, 0);
    if (err) throw err;
  }

  size_t size() const {
    return size_;
  }

  operator VkBuffer() const { return buf; }
private:
  VkBuffer buf = nullptr;
  VkDevice dev = nullptr;
  VkDeviceMemory mem = nullptr;
  size_t size_;
  bool ownsBuffer = false;
};

class vertexInputState {
public:
  vertexInputState() {
  }

  vertexInputState &operator=(vertexInputState && rhs) {
    vi = rhs.vi;
    bindingDescriptions = std::move(rhs.bindingDescriptions);
    attributeDescriptions = std::move(rhs.attributeDescriptions);
  }

  vertexInputState &attrib(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription desc = {};
    desc.location = location;
    desc.binding = binding;
    desc.format = format;
    desc.offset = offset;
    attributeDescriptions.push_back(desc);
    return *this;
  }

  vertexInputState &binding(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX) {
    VkVertexInputBindingDescription desc = {};
    desc.binding = binding;
    desc.stride = stride;
    desc.inputRate = inputRate;
    bindingDescriptions.push_back(desc);
    return *this;
  }

  VkPipelineVertexInputStateCreateInfo *get() {
		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vi.pNext = nullptr;
		vi.vertexBindingDescriptionCount = (uint32_t)bindingDescriptions.size();
		vi.pVertexBindingDescriptions = bindingDescriptions.data();
		vi.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
		vi.pVertexAttributeDescriptions = attributeDescriptions.data();
    return &vi;
  }

private:

	VkPipelineVertexInputStateCreateInfo vi;
	std::vector<VkVertexInputBindingDescription> bindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
};


class uniformInputState {
public:
  uniformInputState() {
  }

  uniformInputState &operator=(uniformInputState && rhs) {
  }

  VkDescriptorBufferInfo *get() {
    return &descriptor;
  }
private:
	VkDescriptorBufferInfo descriptor;
};

} // vku

#endif
