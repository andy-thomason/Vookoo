////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016, 2017
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_RESOURCE_INCLUDED
#define VKU_RESOURCE_INCLUDED

#ifdef _WIN32
  #define VK_USE_PLATFORM_WIN32_KHR 1
  #pragma comment(lib, "vulkan-1.lib")
  #define _CRT_SECURE_NO_WARNINGS
#else
  #define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>

#ifdef _WIN32
  // Fix windows #define badness
  #undef min
  #undef max
#endif

#include <stdexcept>

namespace vku {

class error : public std::runtime_error {
  const char *error_name(VkResult err) {
    switch (err) {
      case VK_SUCCESS: return "VK_SUCCESS";
      case VK_NOT_READY: return "VK_NOT_READY";
      case VK_TIMEOUT: return "VK_TIMEOUT";
      case VK_EVENT_SET: return "VK_EVENT_SET";
      case VK_EVENT_RESET: return "VK_EVENT_RESET";
      case VK_INCOMPLETE: return "VK_INCOMPLETE";
      case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
      case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
      case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
      case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
      case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
      case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
      case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
      case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
      case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
      case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
      case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
      case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
      case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
      case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
      case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
      case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
      case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
      default: return "UNKNOWN ERROR";
    }
  }

  const char *what(VkResult err, const char *file, int line) {
    snprintf(text_, sizeof(text_), "error: %s at %s:%d", error_name(err), file, line);
    return text_;
  }

  char text_[256];
public:
  error(VkResult err, const char *file, int line) : std::runtime_error(what(err, file, line)) {
  }
};

struct resource_aux_data {
};

/// This resource class is the base class of most of the vku wrappers.
/// It wraps a single Vulkan interface plus related interfaces.
/// The primary Vulkan interface is castable to a Vulkan handle of various kinds.
///
/// Exactly one resource object "owns" the resource. I considered using reference counting,
/// but the overhead is pretty rotten, I may return to this later.
/// A downside of this is that you cannot put these objects in vectors.
///
template <class VulkanType, class ParentClass, class AuxType=resource_aux_data>
class resource {
public:
  resource() : value_(VK_NULL_HANDLE), ownsResource_(false), dev_(VK_NULL_HANDLE) {
  }

  resource(VulkanType value, VkDevice dev = VK_NULL_HANDLE) : value_(value), ownsResource_(false), dev_(dev) {
  }

  resource(VkDevice dev) : dev_(dev), ownsResource_(false) {
  }

  resource(const resource &rhs) {
    value_ = rhs.value_;
    dev_ = rhs.dev_;
    ownsResource_ = false;
  }

  resource(resource &&rhs) {
    value_ = rhs.value_;
    dev_ = rhs.dev_;
    ownsResource_ = rhs.ownsResource_;
    rhs.value_ = VK_NULL_HANDLE;
    rhs.ownsResource_ = false;
  }

  // when a resource is copied in the normal way, the ownership is not transfered.
  void operator=(const resource &rhs) {
    clear();
    value_ = rhs.value_;
    dev_ = rhs.dev_;
    ownsResource_ = false;
  }

  // every resource is moveable transfering ownership of the
  // object to the copy. The former owner loses ownership.
  void operator=(resource &&rhs) {
    clear();
    value_ = rhs.value_;
    dev_ = rhs.dev_;
    ownsResource_ = rhs.ownsResource_;
    rhs.value_ = VK_NULL_HANDLE;
    rhs.ownsResource_ = false;
  }

  ~resource() {
    clear();
  }

  operator VulkanType() const {
    return value_;
  }

  VulkanType get() const { return value_; }
  VulkanType *ref() { return &value_; }
  VkDevice dev() const { return dev_; }
  resource &set(VulkanType value, bool owns) { value_ = value; ownsResource_ = owns; return *this; }

  void clear() {
    if (value_ && ownsResource_) ((ParentClass*)this)->destroy();
    value_ = VK_NULL_HANDLE; ownsResource_ = false;
  }

  void move(resource &&rhs) {
    operator=(std::move(rhs));
    aux_ = std::move(rhs.aux_);
  }

  void copy(const resource &rhs) {
    operator=(rhs);
    aux_ = rhs.aux_;
  }

  AuxType &aux() { return aux_; }
  const AuxType &aux() const { return aux_; }
private:

  // resources have a value, a device and an ownership flag.
  VulkanType value_ = VK_NULL_HANDLE;
  VkDevice dev_ = VK_NULL_HANDLE;
  bool ownsResource_ = false;
  AuxType aux_;
};

// ghastly boilerplate as a macro

#define VKU_RESOURCE_BOILERPLATE(vktype, vkuclass) \
  vkuclass(vktype value = VK_NULL_HANDLE, VkDevice dev = VK_NULL_HANDLE) : resource(value, dev) { \
  } \
  vkuclass(vkuclass &&rhs) { \
    move(std::move(rhs)); \
  } \
  vkuclass &operator=(vkuclass &&rhs) { \
    move(std::move(rhs)); \
    return *this; \
  } \
  vkuclass(const vkuclass &rhs) { \
    copy(rhs); \
  } \
  vkuclass &operator=(const vkuclass &rhs) { \
    copy(rhs); \
    return *this; \
  }


} // vku

#endif
