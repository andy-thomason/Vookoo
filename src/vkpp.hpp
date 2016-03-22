

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

  buffer(VkDevice dev, VkBufferCreateInfo bufInfo) : dev(dev) {
		vkCreateBuffer(dev, &bufInfo, nullptr, &buf);
  }

  buffer(device dev, void *init, VkDeviceSize size, VkBufferUsageFlags usage) : dev(dev) {
    VkBufferCreateInfo bufInfo = {};
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size = size;
		bufInfo.usage = usage;
		VkResult err = vkCreateBuffer(dev, &bufInfo, nullptr, &buf);
    if (err) throw err;

		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		vkGetBufferMemoryRequirements(dev, buf, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = dev.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

 		err = vkAllocateMemory(dev, &memAlloc, nullptr, &mem);
    if (err) throw err;

		void *data = nullptr;
		err = vkMapMemory(dev, mem, 0, memAlloc.allocationSize, 0, &data);
    if (err) throw err;
		std::memcpy(data, init, size);
		vkUnmapMemory(dev, mem);
    if (err) throw err;
		err = vkBindBufferMemory(dev, buf, mem, 0);
    if (err) throw err;
  }

  buffer(VkBufferCreateInfo bufInfo, VkDevice dev = nullptr) : dev(dev) {
		vkCreateBuffer(dev, &bufInfo, nullptr, &buf);
  }

  // move operator
  buffer &operator=(buffer &&rhs) {
    dev = rhs.dev;
    buf = rhs.buf;
    rhs.dev = nullptr;
    rhs.buf = nullptr;
    return *this;
  }

  buffer(buffer &&rhs) {
    dev = rhs.dev;
    buf = rhs.buf;
    rhs.dev = nullptr;
    rhs.buf = nullptr;
  }

  ~buffer() {
    if (buf) {
      vkDestroyBuffer(dev, buf, nullptr);
      buf = nullptr;
    }
  }

  operator VkBuffer() const { return buf; }
private:
  VkBuffer buf;
  VkDevice dev;
  VkDeviceMemory mem;
};


} // vku
