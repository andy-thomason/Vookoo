////////////////////////////////////////////////////////////////////////////////
//
/// Vookoo high level C++ Vulkan interface.
//
/// (C) Vookoo Contributors, MIT License
//
/// This is a utility set alongside the vkcpp C++ interface to Vulkan which makes
/// constructing Vulkan pipelines and resources very easy for beginners.
//
/// It is expected that once familar with the Vulkan C++ interface you may wish
/// to "go it alone" but we hope that this will make the learning experience a joyful one.
//
/// You can use it with the demo framework, stand alone and mixed with C or C++ Vulkan objects.
/// It should integrate with game engines nicely.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef VKU_HPP
#define VKU_HPP

#include <memory>
#include <utility>
#include <array>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstddef>

#ifdef VOOKOO_SPIRV_SUPPORT
  #include <unified1/spirv.hpp11>
#endif

#include <vulkan/vulkan.hpp>

namespace vku {

/// Printf-style formatting function.
template <class ... Args>
std::string format(const char *fmt, Args... args) {
  int n = snprintf(nullptr, 0, fmt, args...);
  std::string result(n, '\0');
  snprintf(&*result.begin(), n+1, fmt, args...);
  return result;
}

/// Utility function for finding memory types for uniforms and images.
inline int findMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties &memprops, uint32_t memoryTypeBits, vk::MemoryPropertyFlags search) {
  for (int i = 0; i != memprops.memoryTypeCount; ++i, memoryTypeBits >>= 1) {
    if (memoryTypeBits & 1) {
      if ((memprops.memoryTypes[i].propertyFlags & search) == search) {
        return i;
      }
    }
  }
  return -1;
}

/// Execute commands immediately and wait for the device to finish.
inline void executeImmediately(vk::Device device, vk::CommandPool commandPool, vk::Queue queue, const std::function<void (vk::CommandBuffer cb)> &func) {
  vk::CommandBufferAllocateInfo cbai{ commandPool, vk::CommandBufferLevel::ePrimary, 1 };

  auto cbs = device.allocateCommandBuffers(cbai);
  cbs[0].begin(vk::CommandBufferBeginInfo{});
  func(cbs[0]);
  cbs[0].end();

  vk::SubmitInfo submit;
  submit.commandBufferCount = static_cast<uint32_t>(cbs.size());
  submit.pCommandBuffers = cbs.data();
  queue.submit(submit, vk::Fence{});
  device.waitIdle();

  device.freeCommandBuffers(commandPool, cbs);
}

/// Scale a value by mip level, but do not reduce to zero.
inline uint32_t mipScale(uint32_t value, uint32_t mipLevel) {
  return std::max(value >> mipLevel, static_cast<uint32_t>(1));
}

/// Load a binary file into a vector.
/// The vector will be zero-length if this fails.
inline std::vector<uint8_t> loadFile(const std::string &filename) {
  std::ifstream is(filename, std::ios::binary|std::ios::ate);
  std::vector<uint8_t> bytes;
  if (!is.fail()) {
    auto size = is.tellg();
    is.seekg(0);
    bytes.resize(size);
    is.read(reinterpret_cast<char*>(bytes.data()), size);
  }
  return bytes;
}

/// Description of blocks for compressed formats.
struct BlockParams {
  uint8_t blockWidth;
  uint8_t blockHeight;
  uint8_t bytesPerBlock;
};

/// Get the details of vulkan texture formats.
inline BlockParams getBlockParams(vk::Format format) {
  switch (format) {
    case vk::Format::eR4G4UnormPack8: return BlockParams{1, 1, 1};
    case vk::Format::eR4G4B4A4UnormPack16:
    case vk::Format::eB4G4R4A4UnormPack16:
    case vk::Format::eR5G6B5UnormPack16:
    case vk::Format::eB5G6R5UnormPack16:
    case vk::Format::eR5G5B5A1UnormPack16:
    case vk::Format::eB5G5R5A1UnormPack16:
    case vk::Format::eA1R5G5B5UnormPack16: return BlockParams{1, 1, 2};
    case vk::Format::eR8Unorm:
    case vk::Format::eR8Snorm:
    case vk::Format::eR8Uscaled:
    case vk::Format::eR8Sscaled:
    case vk::Format::eR8Uint:
    case vk::Format::eR8Sint:
    case vk::Format::eR8Srgb: return BlockParams{1, 1, 1};
    case vk::Format::eR8G8Unorm:
    case vk::Format::eR8G8Snorm:
    case vk::Format::eR8G8Uscaled:
    case vk::Format::eR8G8Sscaled:
    case vk::Format::eR8G8Uint:
    case vk::Format::eR8G8Sint:
    case vk::Format::eR8G8Srgb: return BlockParams{1, 1, 2};
    case vk::Format::eR8G8B8Unorm:
    case vk::Format::eR8G8B8Snorm:
    case vk::Format::eR8G8B8Uscaled:
    case vk::Format::eR8G8B8Sscaled:
    case vk::Format::eR8G8B8Uint:
    case vk::Format::eR8G8B8Sint:
    case vk::Format::eR8G8B8Srgb:
    case vk::Format::eB8G8R8Unorm:
    case vk::Format::eB8G8R8Snorm:
    case vk::Format::eB8G8R8Uscaled:
    case vk::Format::eB8G8R8Sscaled:
    case vk::Format::eB8G8R8Uint:
    case vk::Format::eB8G8R8Sint:
    case vk::Format::eB8G8R8Srgb: return BlockParams{1, 1, 3};
    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eR8G8B8A8Snorm:
    case vk::Format::eR8G8B8A8Uscaled:
    case vk::Format::eR8G8B8A8Sscaled:
    case vk::Format::eR8G8B8A8Uint:
    case vk::Format::eR8G8B8A8Sint:
    case vk::Format::eR8G8B8A8Srgb:
    case vk::Format::eB8G8R8A8Unorm:
    case vk::Format::eB8G8R8A8Snorm:
    case vk::Format::eB8G8R8A8Uscaled:
    case vk::Format::eB8G8R8A8Sscaled:
    case vk::Format::eB8G8R8A8Uint:
    case vk::Format::eB8G8R8A8Sint:
    case vk::Format::eB8G8R8A8Srgb:
    case vk::Format::eA8B8G8R8UnormPack32:
    case vk::Format::eA8B8G8R8SnormPack32:
    case vk::Format::eA8B8G8R8UscaledPack32:
    case vk::Format::eA8B8G8R8SscaledPack32:
    case vk::Format::eA8B8G8R8UintPack32:
    case vk::Format::eA8B8G8R8SintPack32:
    case vk::Format::eA8B8G8R8SrgbPack32:
    case vk::Format::eA2R10G10B10UnormPack32:
    case vk::Format::eA2R10G10B10SnormPack32:
    case vk::Format::eA2R10G10B10UscaledPack32:
    case vk::Format::eA2R10G10B10SscaledPack32:
    case vk::Format::eA2R10G10B10UintPack32:
    case vk::Format::eA2R10G10B10SintPack32:
    case vk::Format::eA2B10G10R10UnormPack32:
    case vk::Format::eA2B10G10R10SnormPack32:
    case vk::Format::eA2B10G10R10UscaledPack32:
    case vk::Format::eA2B10G10R10SscaledPack32:
    case vk::Format::eA2B10G10R10UintPack32:
    case vk::Format::eA2B10G10R10SintPack32: return BlockParams{1, 1, 4};
    case vk::Format::eR16Unorm:
    case vk::Format::eR16Snorm:
    case vk::Format::eR16Uscaled:
    case vk::Format::eR16Sscaled:
    case vk::Format::eR16Uint:
    case vk::Format::eR16Sint:
    case vk::Format::eR16Sfloat: return BlockParams{1, 1, 2};
    case vk::Format::eR16G16Unorm:
    case vk::Format::eR16G16Snorm:
    case vk::Format::eR16G16Uscaled:
    case vk::Format::eR16G16Sscaled:
    case vk::Format::eR16G16Uint:
    case vk::Format::eR16G16Sint:
    case vk::Format::eR16G16Sfloat: return BlockParams{1, 1, 4};
    case vk::Format::eR16G16B16Unorm:
    case vk::Format::eR16G16B16Snorm:
    case vk::Format::eR16G16B16Uscaled:
    case vk::Format::eR16G16B16Sscaled:
    case vk::Format::eR16G16B16Uint:
    case vk::Format::eR16G16B16Sint:
    case vk::Format::eR16G16B16Sfloat: return BlockParams{1, 1, 6};
    case vk::Format::eR16G16B16A16Unorm:
    case vk::Format::eR16G16B16A16Snorm:
    case vk::Format::eR16G16B16A16Uscaled:
    case vk::Format::eR16G16B16A16Sscaled:
    case vk::Format::eR16G16B16A16Uint:
    case vk::Format::eR16G16B16A16Sint:
    case vk::Format::eR16G16B16A16Sfloat: return BlockParams{1, 1, 8};
    case vk::Format::eR32Uint:
    case vk::Format::eR32Sint:
    case vk::Format::eR32Sfloat: return BlockParams{1, 1, 4};
    case vk::Format::eR32G32Uint:
    case vk::Format::eR32G32Sint:
    case vk::Format::eR32G32Sfloat: return BlockParams{1, 1, 8};
    case vk::Format::eR32G32B32Uint:
    case vk::Format::eR32G32B32Sint:
    case vk::Format::eR32G32B32Sfloat: return BlockParams{1, 1, 12};
    case vk::Format::eR32G32B32A32Uint:
    case vk::Format::eR32G32B32A32Sint:
    case vk::Format::eR32G32B32A32Sfloat: return BlockParams{1, 1, 16};
    case vk::Format::eR64Uint:
    case vk::Format::eR64Sint:
    case vk::Format::eR64Sfloat: return BlockParams{1, 1, 8};
    case vk::Format::eR64G64Uint:
    case vk::Format::eR64G64Sint:
    case vk::Format::eR64G64Sfloat: return BlockParams{1, 1, 16};
    case vk::Format::eR64G64B64Uint:
    case vk::Format::eR64G64B64Sint:
    case vk::Format::eR64G64B64Sfloat: return BlockParams{1, 1, 24};
    case vk::Format::eR64G64B64A64Uint:
    case vk::Format::eR64G64B64A64Sint:
    case vk::Format::eR64G64B64A64Sfloat: return BlockParams{1, 1, 32};
    case vk::Format::eB10G11R11UfloatPack32:
    case vk::Format::eE5B9G9R9UfloatPack32:
    case vk::Format::eD16Unorm:
    case vk::Format::eX8D24UnormPack32:
    case vk::Format::eD32Sfloat: return BlockParams{1, 1, 4};
    case vk::Format::eS8Uint: return BlockParams{1, 1, 1};
    case vk::Format::eD16UnormS8Uint: return BlockParams{1, 1, 3};
    case vk::Format::eD24UnormS8Uint: return BlockParams{1, 1, 4};
    case vk::Format::eD32SfloatS8Uint: return BlockParams{0, 0, 0};
    case vk::Format::eBc1RgbUnormBlock:
    case vk::Format::eBc1RgbSrgbBlock:
    case vk::Format::eBc1RgbaUnormBlock:
    case vk::Format::eBc1RgbaSrgbBlock: return BlockParams{4, 4, 8};
    case vk::Format::eBc2UnormBlock:
    case vk::Format::eBc2SrgbBlock:
    case vk::Format::eBc3UnormBlock:
    case vk::Format::eBc3SrgbBlock:
    case vk::Format::eBc4UnormBlock:
    case vk::Format::eBc4SnormBlock:
    case vk::Format::eBc5UnormBlock:
    case vk::Format::eBc5SnormBlock: return BlockParams{4, 4, 16};
    case vk::Format::eBc6HUfloatBlock: // NOLINT(*-branch-clone)
    case vk::Format::eBc6HSfloatBlock:
    case vk::Format::eBc7UnormBlock:
    case vk::Format::eBc7SrgbBlock:
    case vk::Format::eEtc2R8G8B8UnormBlock:
    case vk::Format::eEtc2R8G8B8SrgbBlock:
    case vk::Format::eEtc2R8G8B8A1UnormBlock:;
    case vk::Format::eEtc2R8G8B8A1SrgbBlock:
    case vk::Format::eEtc2R8G8B8A8UnormBlock:;
    case vk::Format::eEtc2R8G8B8A8SrgbBlock:
    case vk::Format::eEacR11UnormBlock:
    case vk::Format::eEacR11SnormBlock:
    case vk::Format::eEacR11G11UnormBlock:
    case vk::Format::eEacR11G11SnormBlock:
    case vk::Format::eAstc4x4UnormBlock:
    case vk::Format::eAstc4x4SrgbBlock:
    case vk::Format::eAstc5x4UnormBlock:
    case vk::Format::eAstc5x4SrgbBlock:
    case vk::Format::eAstc5x5UnormBlock:
    case vk::Format::eAstc5x5SrgbBlock:
    case vk::Format::eAstc6x5UnormBlock:
    case vk::Format::eAstc6x5SrgbBlock:
    case vk::Format::eAstc6x6UnormBlock:
    case vk::Format::eAstc6x6SrgbBlock:
    case vk::Format::eAstc8x5UnormBlock:
    case vk::Format::eAstc8x5SrgbBlock:
    case vk::Format::eAstc8x6UnormBlock:
    case vk::Format::eAstc8x6SrgbBlock:
    case vk::Format::eAstc8x8UnormBlock:
    case vk::Format::eAstc8x8SrgbBlock:
    case vk::Format::eAstc10x5UnormBlock:
    case vk::Format::eAstc10x5SrgbBlock:
    case vk::Format::eAstc10x6UnormBlock:
    case vk::Format::eAstc10x6SrgbBlock:
    case vk::Format::eAstc10x8UnormBlock:
    case vk::Format::eAstc10x8SrgbBlock:
    case vk::Format::eAstc10x10UnormBlock:
    case vk::Format::eAstc10x10SrgbBlock:
    case vk::Format::eAstc12x10UnormBlock:
    case vk::Format::eAstc12x10SrgbBlock:
    case vk::Format::eAstc12x12UnormBlock:
    case vk::Format::eAstc12x12SrgbBlock:
    case vk::Format::ePvrtc12BppUnormBlockIMG:
    case vk::Format::ePvrtc14BppUnormBlockIMG:
    case vk::Format::ePvrtc22BppUnormBlockIMG:
    case vk::Format::ePvrtc24BppUnormBlockIMG:
    case vk::Format::ePvrtc12BppSrgbBlockIMG:
    case vk::Format::ePvrtc14BppSrgbBlockIMG:
    case vk::Format::ePvrtc22BppSrgbBlockIMG:
    case vk::Format::ePvrtc24BppSrgbBlockIMG:
    default:
      return BlockParams{0, 0, 0};
  }
}

/// Factory for instances.
class InstanceMaker {
public:
  InstanceMaker() = default;

  InstanceMaker &extensionMultiview ()
  {
	extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // added for multiview extension
	return *this;
  }

  InstanceMaker &extensionValidation ()
  {
	layers_.push_back("VK_LAYER_KHRONOS_validation");
	return *this;
  }

  /// Set the default layers and extensions.
  InstanceMaker &defaultLayers(int core=0)
  {
    instance_extensions_.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

#ifdef VKU_SURFACE
      instance_extensions_.push_back(VKU_SURFACE);
#endif

    instance_extensions_.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    instance_extensions_.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    instance_extensions_.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

#if defined( __APPLE__ ) && defined(VK_EXT_METAL_SURFACE_EXTENSION_NAME)
		instance_extensions_.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#endif //__APPLE__

	if (core > 0)
	{

	}
	
    return *this;
  }

  /// Add a layer. eg. "VK_LAYER_KHRONOS_validation"
  InstanceMaker &layer(const char *layer) {
    layers_.push_back(layer);
    return *this;
  }

  /// Add an extension. eg. VK_EXT_DEBUG_REPORT_EXTENSION_NAME
  InstanceMaker &extension(const char *layer) {
    instance_extensions_.push_back(layer);
    return *this;
  }

  /// Set the name of the application.
  InstanceMaker &applicationName( const char* pApplicationName_ )
  {
    app_info_.pApplicationName = pApplicationName_;
    return *this;
  }

  /// Set the version of the application.
  InstanceMaker &applicationVersion( uint32_t applicationVersion_ )
  {
    app_info_.applicationVersion = applicationVersion_;
    return *this;
  }

  /// Set the name of the engine.
  InstanceMaker &engineName( const char* pEngineName_ )
  {
    app_info_.pEngineName = pEngineName_;
    return *this;
  }

  /// Set the version of the engine.
  InstanceMaker &engineVersion( uint32_t engineVersion_ )
  {
    app_info_.engineVersion = engineVersion_;
    return *this;
  }

  /// Set the version of the api.
  InstanceMaker &apiVersion( uint32_t apiVersion_ )
  {
    app_info_.apiVersion = apiVersion_;
    return *this;
  }

  /// Create a self-deleting (unique) instance.
  [[nodiscard]] vk::UniqueInstance createUnique() const {
    return vk::createInstanceUnique(
      vk::InstanceCreateInfo{
        vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR, &app_info_, static_cast<uint32_t>(layers_.size()),
        layers_.data(), static_cast<uint32_t>(instance_extensions_.size()),
        instance_extensions_.data()
      }
    );
  }
private:
  std::vector<const char *> layers_;
  std::vector<const char *> instance_extensions_;
  vk::ApplicationInfo app_info_;
};

/// Factory for devices.
class DeviceMaker {
public:
  /// Make queues and a logical device for a certain physical device.
  DeviceMaker() = default;

  DeviceMaker &extensionMultiview ()
  {
      extension(VK_KHR_MULTIVIEW_EXTENSION_NAME); // added for multiview extension
      return *this;
  }
  
  DeviceMaker &extensionValidation ()
  {
	layers_.push_back("VK_LAYER_LUNARG_standard_validation");
	return *this;
  }

  /// Set the default layers and extensions.
  DeviceMaker &defaultLayers(int core=0)
  {
	device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    if (core > 0)
    {
 		// VK_KHR_MAINTENANCE1 is required for using negative viewport heights
		// Note: This is core as of Vulkan 1.1. So if you target 1.1 you don't have to explicitly enable this
		device_extensions_.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
	}
    return *this;
  }

  /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
  DeviceMaker &layer(const char *layer) {
    layers_.push_back(layer);
    return *this;
  }

  /// Add an extension. eg. VK_EXT_DEBUG_REPORT_EXTENSION_NAME
  DeviceMaker &extension(const char *layer) {
    device_extensions_.push_back(layer);
    return *this;
  }

  /// Add one or more queues to the device from a certain family.
  DeviceMaker &queue(uint32_t familyIndex, float priority=0.0f, uint32_t n=1) {
    queue_priorities_.emplace_back(n, priority);

    qci_.emplace_back(
      vk::DeviceQueueCreateFlags{},
      familyIndex, n,
      queue_priorities_.back().data()
    );

    return *this;
  }

  DeviceMaker &physicalDeviceFeatures(const vk::PhysicalDeviceFeatures &v = {})
  {
	pdfs_.push_back(v);
	return *this;
  }
  
  DeviceMaker &enableGeometryShader ()
  {
	// assert !pdfs_.empty()
	pdfs_.back().setGeometryShader(true);
	return *this;
  }
  
  DeviceMaker &enableTessellationShader ()
  {
	// assert !pdfs_.empty()
	pdfs_.back().setTessellationShader(true);
	return *this;
  }
  
  DeviceMaker &multiviewFeatures (const vk::PhysicalDeviceMultiviewFeatures &v = {})
  {
	mvfs_.push_back(v);
	return *this;
  }
  
  DeviceMaker &enableMultiview ()
  {
	// assert !mvfs_.empty()
	mvfs_.back().setMultiview(true);
	return *this;
  }

  /// Create a new logical device.
  [[nodiscard]] vk::UniqueDevice createUnique(vk::PhysicalDevice physical_device) const {
    auto dci = vk::DeviceCreateInfo{
      {},
      static_cast<uint32_t>(qci_.size()), qci_.data(),
      static_cast<uint32_t>(layers_.size()), layers_.data(),
      static_cast<uint32_t>(device_extensions_.size()), device_extensions_.data()
    };

    //
    if (!pdfs_.empty())
		dci.setPEnabledFeatures(&pdfs_.front());

    // required to enable and use multiview
    if (!mvfs_.empty())
		dci.pNext = &mvfs_.front();

    return physical_device.createDeviceUnique(dci);
  }
private:
  std::vector<const char *> layers_;
  std::vector<const char *> device_extensions_;
  std::vector<std::vector<float> > queue_priorities_;
  std::vector<vk::DeviceQueueCreateInfo> qci_;
  std::vector<vk::PhysicalDeviceFeatures> pdfs_;
  std::vector<vk::PhysicalDeviceMultiviewFeatures> mvfs_;

  vk::ApplicationInfo app_info_;
};

class DebugCallback {
public:
  DebugCallback() = default;

  explicit DebugCallback(
    vk::Instance instance,
    vk::DebugReportFlagsEXT flags =
      vk::DebugReportFlagBitsEXT::eWarning |
      vk::DebugReportFlagBitsEXT::eError
  ) : instance_(instance) {
    auto ci = vk::DebugReportCallbackCreateInfoEXT{flags, &debugCallback};

    auto vkCreateDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(instance_.getProcAddr(
            "vkCreateDebugReportCallbackEXT"));

    VkDebugReportCallbackEXT cb;
    vkCreateDebugReportCallbackEXT(
      instance_, &reinterpret_cast<const VkDebugReportCallbackCreateInfoEXT &>(ci),
      nullptr, &cb
    );
    callback_ = cb;
  }

  ~DebugCallback() { // NOLINT(*-use-equals-default)
    //reset();
  }

  void reset() {
    if (callback_) {
      auto vkDestroyDebugReportCallbackEXT =
          reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(instance_.getProcAddr(
              "vkDestroyDebugReportCallbackEXT"));
      vkDestroyDebugReportCallbackEXT(instance_, callback_, nullptr);
      callback_ = vk::DebugReportCallbackEXT{};
    }
  }
private:
  // Report any errors or warnings.
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
      uint64_t object, size_t location, int32_t messageCode,
      const char *pLayerPrefix, const char *pMessage, void *pUserData) {
    printf("%08x debugCallback: %s\n", flags, pMessage);
    return VK_FALSE;
  }
  vk::DebugReportCallbackEXT callback_;
  vk::Instance instance_;
};

/// Factory for renderpasses.
/// example:
///     RenderpassMaker rpm;
///     rpm.subpassBegin(vk::PipelineBindPoint::eGraphics);
///     rpm.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal);
///
///     rpm.attachmentDescription(attachmentDesc);
///     rpm.subpassDependency(dependency);
///     s.renderPass_ = rpm.createUnique(device);
class RenderpassMaker {
public:
  RenderpassMaker() = default;

  /// Begin an attachment description.
  /// After this you can call attachment* many times
  RenderpassMaker& attachmentBegin(vk::Format format) {
    vk::AttachmentDescription desc{{}, format};
    s.attachmentDescriptions.push_back(desc);
    return *this;
  }

  RenderpassMaker& attachmentFlags(vk::AttachmentDescriptionFlags value) { s.attachmentDescriptions.back().flags = value; return *this;};
  RenderpassMaker& attachmentFormat(vk::Format value) { s.attachmentDescriptions.back().format = value; return *this;};
  RenderpassMaker& attachmentSamples(vk::SampleCountFlagBits value) { s.attachmentDescriptions.back().samples = value; return *this;};
  RenderpassMaker& attachmentLoadOp(vk::AttachmentLoadOp value) { s.attachmentDescriptions.back().loadOp = value; return *this;};
  RenderpassMaker& attachmentStoreOp(vk::AttachmentStoreOp value) { s.attachmentDescriptions.back().storeOp = value;  return *this; };
  RenderpassMaker& attachmentStencilLoadOp(vk::AttachmentLoadOp value) { s.attachmentDescriptions.back().stencilLoadOp = value; return *this;};
  RenderpassMaker& attachmentStencilStoreOp(vk::AttachmentStoreOp value) { s.attachmentDescriptions.back().stencilStoreOp = value; return *this;};
  RenderpassMaker& attachmentInitialLayout(vk::ImageLayout value) { s.attachmentDescriptions.back().initialLayout = value; return *this;};
  RenderpassMaker& attachmentFinalLayout(vk::ImageLayout value) { s.attachmentDescriptions.back().finalLayout = value; return *this;};

  /// Start a subpass description.
  /// After this you can can call subpassColorAttachment many times
  /// and subpassDepthStencilAttachment once.
  RenderpassMaker& subpassBegin(vk::PipelineBindPoint bp) {
    vk::SubpassDescription desc{};
    desc.pipelineBindPoint = bp;
    s.subpassDescriptions.push_back(desc);
    return *this;
  }

  RenderpassMaker& subpassColorAttachment(vk::ImageLayout layout, uint32_t attachment) {
    vk::SubpassDescription &subpass = s.subpassDescriptions.back();
    auto *p = getAttachmentReference();
    p->layout = layout;
    p->attachment = attachment;
    if (subpass.colorAttachmentCount == 0) {
      subpass.pColorAttachments = p;
    }
    subpass.colorAttachmentCount++;
    return *this;
  }

  RenderpassMaker& subpassDepthStencilAttachment(vk::ImageLayout layout, uint32_t attachment) {
    vk::SubpassDescription &subpass = s.subpassDescriptions.back();
    auto *p = getAttachmentReference();
    p->layout = layout;
    p->attachment = attachment;
    subpass.pDepthStencilAttachment = p;
    return *this;
  }

  [[nodiscard]] vk::UniqueRenderPass createUnique(const vk::Device &device) const {
    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(s.attachmentDescriptions.size());
    renderPassInfo.pAttachments = s.attachmentDescriptions.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(s.subpassDescriptions.size());
    renderPassInfo.pSubpasses = s.subpassDescriptions.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(s.subpassDependencies.size());
    renderPassInfo.pDependencies = s.subpassDependencies.data();
    return device.createRenderPassUnique(renderPassInfo);
  }

  [[nodiscard]] vk::UniqueRenderPass createUnique(const vk::Device &device, const vk::RenderPassMultiviewCreateInfo &I) const {
    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(s.attachmentDescriptions.size());
    renderPassInfo.pAttachments = s.attachmentDescriptions.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(s.subpassDescriptions.size());
    renderPassInfo.pSubpasses = s.subpassDescriptions.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(s.subpassDependencies.size());
    renderPassInfo.pDependencies = s.subpassDependencies.data();
    renderPassInfo.pNext = &I; // identical to createUnique(const vk::Device &device) except set pNext to use multi-view &I
    return device.createRenderPassUnique(renderPassInfo);
  }

  RenderpassMaker& dependencyBegin(uint32_t srcSubpass, uint32_t dstSubpass) {
    vk::SubpassDependency desc{};
    desc.srcSubpass = srcSubpass;
    desc.dstSubpass = dstSubpass;
    s.subpassDependencies.push_back(desc);
    return *this;
  }

  RenderpassMaker& dependencySrcSubpass(uint32_t value) { s.subpassDependencies.back().srcSubpass = value; return *this;};
  RenderpassMaker& dependencyDstSubpass(uint32_t value) { s.subpassDependencies.back().dstSubpass = value; return *this;};
  RenderpassMaker& dependencySrcStageMask(vk::PipelineStageFlags value) { s.subpassDependencies.back().srcStageMask = value; return *this;};
  RenderpassMaker& dependencyDstStageMask(vk::PipelineStageFlags value) { s.subpassDependencies.back().dstStageMask = value; return *this;};
  RenderpassMaker& dependencySrcAccessMask(vk::AccessFlags value) { s.subpassDependencies.back().srcAccessMask = value; return *this;};
  RenderpassMaker& dependencyDstAccessMask(vk::AccessFlags value) { s.subpassDependencies.back().dstAccessMask = value; return *this;};
  RenderpassMaker& dependencyDependencyFlags(vk::DependencyFlags value) { s.subpassDependencies.back().dependencyFlags = value; return *this;};
private:
  constexpr static int max_refs = 64;

  vk::AttachmentReference *getAttachmentReference() {
    return (s.num_refs < max_refs) ? &s.attachmentReferences[s.num_refs++] : nullptr;
  }

  struct State {
    std::vector<vk::AttachmentDescription> attachmentDescriptions;
    std::vector<vk::SubpassDescription> subpassDescriptions;
    std::vector<vk::SubpassDependency> subpassDependencies;
    std::array<vk::AttachmentReference, max_refs> attachmentReferences;
    int num_refs = 0;
    bool ok_ = false;
  };

  State s;
};

/// Class for building shader modules and extracting metadata from shaders.
class ShaderModule {
public:
  ShaderModule() = default;

  /// Construct a shader module from a file
  ShaderModule(const vk::Device &device, const std::string &filename) {
    auto file = std::ifstream(filename, std::ios::binary);
    if (!file.good()) {
      return;
    }

    file.seekg(0, std::ios::end);
    int length = (int)file.tellg();

    s.opcodes_.resize(static_cast<size_t>(length / 4));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(s.opcodes_.data()), static_cast<std::streamsize>(s.opcodes_.size() * 4));

    vk::ShaderModuleCreateInfo ci;
    ci.codeSize = s.opcodes_.size() * 4;
    ci.pCode = s.opcodes_.data();
    s.module_ = device.createShaderModuleUnique(ci);

    s.ok_ = true;
  }

  /// Construct a shader module from a memory
  template<class InIter>
  ShaderModule(const vk::Device &device, InIter begin, InIter end) {
    s.opcodes_.assign(begin, end);
    vk::ShaderModuleCreateInfo ci;
    ci.codeSize = s.opcodes_.size() * 4;
    ci.pCode = s.opcodes_.data();
    s.module_ = device.createShaderModuleUnique(ci);

    s.ok_ = true;
  }

#ifdef VOOKOO_SPIRV_SUPPORT
  /// A variable in a shader.
  struct Variable {
    // The name of the variable from the GLSL/HLSL
    std::string debugName;

    // The internal name (integer) of the variable
    int name;

    // The location in the binding.
    int location;

    // The binding in the descriptor set or I/O channel.
    int binding;

    // The descriptor set (for uniforms)
    int set;
    int instruction;

    // Storage class of the variable, eg. spv::StorageClass::Uniform
    spv::StorageClass storageClass;
  };

  /// Get a list of variables from the shader.
  ///
  /// This exposes the Uniforms, inputs, outputs, push constants.
  /// See spv::StorageClass for more details.
  std::vector<Variable> getVariables() const {
    auto bound = s.opcodes_[3];

    std::unordered_map<int, int> bindings;
    std::unordered_map<int, int> locations;
    std::unordered_map<int, int> sets;
    std::unordered_map<int, std::string> debugNames;

    for (int i = 5; i != s.opcodes_.size(); i += s.opcodes_[i] >> 16) {
      spv::Op op = spv::Op(s.opcodes_[i] & 0xffff);
      if (op == spv::Op::OpDecorate) {
        int name = s.opcodes_[i + 1];
        auto decoration = spv::Decoration(s.opcodes_[i + 2]);
        if (decoration == spv::Decoration::Binding) {
          bindings[name] = s.opcodes_[i + 3];
        } else if (decoration == spv::Decoration::Location) {
          locations[name] = s.opcodes_[i + 3];
        } else if (decoration == spv::Decoration::DescriptorSet) {
          sets[name] = s.opcodes_[i + 3];
        }
      } else if (op == spv::Op::OpName) {
        int name = s.opcodes_[i + 1];
        debugNames[name] = (const char *)&s.opcodes_[i + 2];
      }
    }

    std::vector<Variable> result;
    for (int i = 5; i != s.opcodes_.size(); i += s.opcodes_[i] >> 16) {
      spv::Op op = spv::Op(s.opcodes_[i] & 0xffff);
      if (op == spv::Op::OpVariable) {
        int name = s.opcodes_[i + 1];
        auto sc = spv::StorageClass(s.opcodes_[i + 3]);
        Variable b;
        b.debugName = debugNames[name];
        b.name = name;
        b.location = locations[name];
        b.set = sets[name];
        b.instruction = i;
        b.storageClass = sc;
        result.push_back(b);
      }
    }
    return result;
  }
#endif

  [[nodiscard]] bool ok() const { return s.ok_; }
  [[nodiscard]] VkShaderModule module() const { return *s.module_; }

  /// Write a C++ consumable dump of the shader.
  /// Todo: make this more idiomatic.
  std::ostream &write(std::ostream &os) const {
    os << "static const uint32_t shader[] = {\n";
    char tmp[256];
    auto p = s.opcodes_.begin();
    snprintf(
      tmp, sizeof(tmp), "  0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,\n", p[0], p[1], p[2], p[3], p[4]);
    os << tmp;
    for (int i = 5; i != s.opcodes_.size(); i += static_cast<int>(s.opcodes_[i] >> 16)) {
      char *ptr = tmp + 2, *e = tmp + sizeof(tmp) - 2;
      for (int j = i; j != i + (s.opcodes_[i] >> 16); ++j) {
        ptr += snprintf(ptr, e-ptr, "0x%08x,", s.opcodes_[j]);
        if (ptr > e-16) { *ptr++ = '\n'; *ptr = 0; os << tmp; ptr = tmp + 2; }
      }
      *ptr++ = '\n';
      *ptr = 0;
      os << tmp;
    }
    os << "};\n\n";
    return os;
  }

private:
  struct State {
    std::vector<uint32_t> opcodes_;
    vk::UniqueShaderModule module_;
    bool ok_ = false;
  };

  State s;
};

/// A class for building pipeline layouts.
/// Pipeline layouts describe the descriptor sets and push constants used by the shaders.
class PipelineLayoutMaker {
public:
  PipelineLayoutMaker() = default;

  /// Create a self-deleting pipeline layout object.
  [[nodiscard]] vk::UniquePipelineLayout createUnique(const vk::Device &device) const {
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        {}, static_cast<uint32_t>(setLayouts_.size()),
        setLayouts_.data(), static_cast<uint32_t>(pushConstantRanges_.size()),
        pushConstantRanges_.data()};
    return device.createPipelineLayoutUnique(pipelineLayoutInfo);
  }

  /// Add a descriptor set layout to the pipeline.
  PipelineLayoutMaker& descriptorSetLayout(vk::DescriptorSetLayout layout) {
    setLayouts_.push_back(layout);
    return *this;
  }

  /// Add a push constant range to the pipeline.
  /// These describe the size and location of variables in the push constant area.
  PipelineLayoutMaker& pushConstantRange(vk::ShaderStageFlags stageFlags_, uint32_t offset_, uint32_t size_) {
    pushConstantRanges_.emplace_back(stageFlags_, offset_, size_);
    return *this;
  }

private:
  std::vector<vk::DescriptorSetLayout> setLayouts_;
  std::vector<vk::PushConstantRange> pushConstantRanges_;
};

template<typename... Ts>
class AlignedContainer {
private:
  // Equivalent to std::aligned_union_t<0, Ts...> t_buff;
  alignas(Ts...) std::byte t_buff[std::max({sizeof(Ts)...})];
};

struct SpecConst {
  uint32_t    constantID;
  alignas(8) AlignedContainer<VkBool32, uint32_t, int32_t, float, double> data;
  uint32_t alignment;
  uint32_t size;

  template <typename T>
  SpecConst(uint32_t constantID, T value)
      : constantID(constantID), alignment{alignof(T)}, size(sizeof(T)) {
    new (&data) T{value};
  }

};

/// A class for building pipelines.
/// All the state of the pipeline is exposed through individual calls.
/// The pipeline encapsulates all the OpenGL state in a single object.
/// This includes vertex buffer layouts, blend operations, shaders, line width etc.
/// This class exposes all the values as individuals so a pipeline can be customised.
/// The default is to generate a working pipeline.
class PipelineMaker {
public:
  struct SpecData {
    vk::SpecializationInfo specializationInfo_;
    std::vector<vk::SpecializationMapEntry> specializationMapEntries_;
    std::unique_ptr<char []> data_;
    size_t data_size_;

    SpecData()= default;

    template <typename iterator, typename sentinel>
    SpecData(iterator b, sentinel e);

    template <typename SCList>
    SpecData(const SCList &specConstants); // NOLINT(*-explicit-constructor)

    SpecData(std::initializer_list<SpecConst> list)
        : SpecData(list.begin(), list.end()) {}
    SpecData(SpecData &&) = default;
    SpecData(const SpecData &) = delete;
    SpecData &operator=(SpecData &&) = default;
    SpecData &operator=(const SpecData &) = delete;
  };
public:
  void init ()
  {
    inputAssemblyState_.topology = vk::PrimitiveTopology::eTriangleList;
    rasterizationState_.lineWidth = 1.0f;

     // Set up depth test, but do not enable it.
    depthStencilState_.depthTestEnable = VK_FALSE;
    depthStencilState_.depthWriteEnable = VK_TRUE;
    depthStencilState_.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depthStencilState_.depthBoundsTestEnable = VK_FALSE;
    depthStencilState_.back.failOp = vk::StencilOp::eKeep;
    depthStencilState_.back.passOp = vk::StencilOp::eKeep;
    depthStencilState_.back.compareOp = vk::CompareOp::eAlways;
    depthStencilState_.stencilTestEnable = VK_FALSE;
    depthStencilState_.front = depthStencilState_.back;
  }

  PipelineMaker() {
	init();
  }

  PipelineMaker(uint32_t width, uint32_t height) {
    viewport(vk::Viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f});
	scissor(vk::Rect2D{{0, 0}, {width, height}});
	
	init();
  }
  
  vk::UniquePipeline createUnique(const vk::Device &device,
                            const vk::PipelineCache &pipelineCache,
                            const vk::PipelineLayout &pipelineLayout,
                            const vk::RenderPass &renderPass, bool defaultBlend=true) {

    // Add default colour blend attachment if necessary.
    if (colorBlendAttachments_.empty() && defaultBlend) {
      vk::PipelineColorBlendAttachmentState blend{};
      blend.blendEnable = 0;
      blend.srcColorBlendFactor = vk::BlendFactor::eOne;
      blend.dstColorBlendFactor = vk::BlendFactor::eZero;
      blend.colorBlendOp = vk::BlendOp::eAdd;
      blend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
      blend.dstAlphaBlendFactor = vk::BlendFactor::eZero;
      blend.alphaBlendOp = vk::BlendOp::eAdd;
      typedef vk::ColorComponentFlagBits ccbf;
      blend.colorWriteMask = ccbf::eR|ccbf::eG|ccbf::eB|ccbf::eA;
      colorBlendAttachments_.push_back(blend);
    }

    auto count = static_cast<uint32_t>(colorBlendAttachments_.size());
    colorBlendState_.attachmentCount = count;
    colorBlendState_.pAttachments = count ? colorBlendAttachments_.data() : nullptr;

    vk::PipelineViewportStateCreateInfo viewportState{
        {}, static_cast<uint32_t>(viewport_.size()), viewport_.data(), static_cast<uint32_t>(scissor_.size()), scissor_.data()};

    vk::PipelineVertexInputStateCreateInfo vertexInputState;
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions_.size());
    vertexInputState.pVertexAttributeDescriptions = vertexAttributeDescriptions_.data();
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDescriptions_.size());
    vertexInputState.pVertexBindingDescriptions = vertexBindingDescriptions_.data();

    vk::PipelineDynamicStateCreateInfo dynState{{}, static_cast<uint32_t>(dynamicState_.size()), dynamicState_.data()};

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.stageCount = static_cast<uint32_t>(modules_.size());
    pipelineInfo.pStages = modules_.data();
    pipelineInfo.pInputAssemblyState = &inputAssemblyState_;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizationState_;
    pipelineInfo.pMultisampleState = &multisampleState_;
    pipelineInfo.pColorBlendState = &colorBlendState_;
    pipelineInfo.pDepthStencilState = &depthStencilState_;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.pDynamicState = dynamicState_.empty() ? nullptr : &dynState;
    pipelineInfo.subpass = subpass_;
    pipelineInfo.pTessellationState = &tessellationState_;

    auto [result, pipeline] = device.createGraphicsPipelineUnique(pipelineCache, pipelineInfo);
    // TODO check result for vk::Result::ePipelineCompileRequiredEXT
    return std::move(pipeline);
  }

  /// Add a shader module to the pipeline.
  PipelineMaker& shader(vk::ShaderStageFlagBits stage, const vku::ShaderModule &shader,
                 const char *entryPoint = "main") {
    vk::PipelineShaderStageCreateInfo info{};
    info.module = shader.module();
    info.pName = entryPoint;
    info.stage = stage;
    modules_.emplace_back(info);
    return *this;
  }

  /// Add a shader module with specialized constants to the pipeline.
  PipelineMaker& shader(vk::ShaderStageFlagBits stage, vku::ShaderModule &shader,
              SpecData specConstants,
              const char *entryPoint = "main") {
    auto data = std::make_unique<SpecData>(std::move(specConstants));
    vk::PipelineShaderStageCreateInfo info{};
    info.module = shader.module();
    info.pName = entryPoint;
    info.stage = stage;
    info.pSpecializationInfo = &data->specializationInfo_;
    modules_.emplace_back(info);
    moduleSpecializations_.emplace_back(std::move(data));
    return *this;
  }

  /// Add a blend state to the pipeline for one colour attachment.
  /// If you don't do this, a default is used.
  PipelineMaker& colorBlend(const vk::PipelineColorBlendAttachmentState &state) {
    colorBlendAttachments_.push_back(state);
    return *this;
  }

  PipelineMaker& subPass(uint32_t subpass) {
    subpass_ = subpass;
    return *this;
  }

  /// Begin setting colour blend value
  /// If you don't do this, a default is used.
  /// Follow this with blendEnable() blendSrcColorBlendFactor() etc.
  /// Default is a regular alpha blend.
  PipelineMaker& blendBegin(vk::Bool32 enable) {
    colorBlendAttachments_.emplace_back();
    auto &blend = colorBlendAttachments_.back();
    blend.blendEnable = enable;
    blend.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    blend.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blend.colorBlendOp = vk::BlendOp::eAdd;
    blend.srcAlphaBlendFactor = vk::BlendFactor::eSrcAlpha;
    blend.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blend.alphaBlendOp = vk::BlendOp::eAdd;
    typedef vk::ColorComponentFlagBits ccbf;
    blend.colorWriteMask = ccbf::eR|ccbf::eG|ccbf::eB|ccbf::eA;
    return *this;
  }

  /// Enable or disable blending (called after blendBegin())
  PipelineMaker& blendEnable(vk::Bool32 value) { colorBlendAttachments_.back().blendEnable = value; return *this;}

  /// Source colour blend factor (called after blendBegin())
  PipelineMaker& blendSrcColorBlendFactor(vk::BlendFactor value) { colorBlendAttachments_.back().srcColorBlendFactor = value; return *this;}

  /// Destination colour blend factor (called after blendBegin())
  PipelineMaker& blendDstColorBlendFactor(vk::BlendFactor value) { colorBlendAttachments_.back().dstColorBlendFactor = value; return *this;}

  /// Blend operation (called after blendBegin())
  PipelineMaker& blendColorBlendOp(vk::BlendOp value) { colorBlendAttachments_.back().colorBlendOp = value; return *this;}

  /// Source alpha blend factor (called after blendBegin())
  PipelineMaker& blendSrcAlphaBlendFactor(vk::BlendFactor value) { colorBlendAttachments_.back().srcAlphaBlendFactor = value; return *this;}

  /// Destination alpha blend factor (called after blendBegin())
  PipelineMaker& blendDstAlphaBlendFactor(vk::BlendFactor value) { colorBlendAttachments_.back().dstAlphaBlendFactor = value; return *this;}

  /// Alpha operation (called after blendBegin())
  PipelineMaker& blendAlphaBlendOp(vk::BlendOp value) { colorBlendAttachments_.back().alphaBlendOp = value; return *this;}

  /// Colour write mask (called after blendBegin())
  PipelineMaker& blendColorWriteMask(vk::ColorComponentFlags value) { colorBlendAttachments_.back().colorWriteMask = value; return *this;}

  /// Add a vertex attribute to the pipeline.
  PipelineMaker& vertexAttribute(uint32_t location_, uint32_t binding_, vk::Format format_, uint32_t offset_) {
    vertexAttributeDescriptions_.emplace_back(location_, binding_, format_, offset_);
    return *this;
  }

  /// Add a vertex attribute to the pipeline.
  PipelineMaker& vertexAttribute(const vk::VertexInputAttributeDescription &desc) {
    vertexAttributeDescriptions_.push_back(desc);
    return *this;
  }

  /// Add a vertex binding to the pipeline.
  /// Usually only one of these is needed to specify the stride.
  /// Vertices can also be delivered one per instance.
  PipelineMaker& vertexBinding(uint32_t binding_, uint32_t stride_, vk::VertexInputRate inputRate_ = vk::VertexInputRate::eVertex) {
    vertexBindingDescriptions_.emplace_back(binding_, stride_, inputRate_);
    return *this;
  }

  /// Add a vertex binding to the pipeline.
  /// Usually only one of these is needed to specify the stride.
  /// Vertices can also be delivered one per instance.
  PipelineMaker& vertexBinding(const vk::VertexInputBindingDescription &desc) {
    vertexBindingDescriptions_.push_back(desc);
    return *this;
  }

  /// Specify the topology of the pipeline.
  /// Usually this is a triangle list, but points and lines are possible too.
  PipelineMaker &topology( vk::PrimitiveTopology topology ) { inputAssemblyState_.topology = topology; return *this; }

  /// Specify patch count.
  /// Applies when (inputAssemblyState_.topology == vk::PrimitiveTopology::ePatchList).
  PipelineMaker &setPatchControlPoints( uint32_t patchControlPoints ) { tessellationState_.setPatchControlPoints(patchControlPoints); return *this; }

  /// Enable or disable primitive restart.
  /// If using triangle strips, for example, this allows a special index value (0xffff or 0xffffffff) to start a new strip.
  PipelineMaker &primitiveRestartEnable( vk::Bool32 primitiveRestartEnable ) { inputAssemblyState_.primitiveRestartEnable = primitiveRestartEnable; return *this; }

  /// Set a whole new input assembly state.
  /// Note you can set individual values with their own call
  PipelineMaker &inputAssemblyState(const vk::PipelineInputAssemblyStateCreateInfo &value) { inputAssemblyState_ = value; return *this; }

  /// Set the viewport value.
  /// Usually there is only one viewport, but you can have multiple viewports active for rendering cubemaps or VR stereo pair
  PipelineMaker &viewport(const vk::Viewport &value) { viewport_.push_back(value); return *this; }

  /// Set the scissor value.
  /// This defines the area that the fragment shaders can write to. For example, if you are rendering a portal or a mirror.
  PipelineMaker &scissor(const vk::Rect2D &value) { scissor_.push_back(value); return *this; }

  /// Set a whole rasterization state.
  /// Note you can set individual values with their own call
  PipelineMaker &rasterizationState(const vk::PipelineRasterizationStateCreateInfo &value) { rasterizationState_ = value; return *this; }
  PipelineMaker &depthClampEnable(vk::Bool32 value) { rasterizationState_.depthClampEnable = value; return *this; }
  PipelineMaker &rasterizerDiscardEnable(vk::Bool32 value) { rasterizationState_.rasterizerDiscardEnable = value; return *this; }
  PipelineMaker &polygonMode(vk::PolygonMode value) { rasterizationState_.polygonMode = value; return *this; }
  PipelineMaker &cullMode(vk::CullModeFlags value) { rasterizationState_.cullMode = value; return *this; }
  PipelineMaker &frontFace(vk::FrontFace value) { rasterizationState_.frontFace = value; return *this; }
  PipelineMaker &depthBiasEnable(vk::Bool32 value) { rasterizationState_.depthBiasEnable = value; return *this; }
  PipelineMaker &depthBiasConstantFactor(float value) { rasterizationState_.depthBiasConstantFactor = value; return *this; }
  PipelineMaker &depthBiasClamp(float value) { rasterizationState_.depthBiasClamp = value; return *this; }
  PipelineMaker &depthBiasSlopeFactor(float value) { rasterizationState_.depthBiasSlopeFactor = value; return *this; }
  PipelineMaker &lineWidth(float value) { rasterizationState_.lineWidth = value; return *this; }


  /// Set a whole multi sample state.
  /// Note you can set individual values with their own call
  PipelineMaker &multisampleState(const vk::PipelineMultisampleStateCreateInfo &value) { multisampleState_ = value; return *this; }
  PipelineMaker &rasterizationSamples(vk::SampleCountFlagBits value) { multisampleState_.rasterizationSamples = value; return *this; }
  PipelineMaker &sampleShadingEnable(vk::Bool32 value) { multisampleState_.sampleShadingEnable = value; return *this; }
  PipelineMaker &minSampleShading(float value) { multisampleState_.minSampleShading = value; return *this; }
  PipelineMaker &pSampleMask(const vk::SampleMask* value) { multisampleState_.pSampleMask = value; return *this; }
  PipelineMaker &alphaToCoverageEnable(vk::Bool32 value) { multisampleState_.alphaToCoverageEnable = value; return *this; }
  PipelineMaker &alphaToOneEnable(vk::Bool32 value) { multisampleState_.alphaToOneEnable = value; return *this; }

  /// Set a whole depth stencil state.
  /// Note you can set individual values with their own call
  PipelineMaker &depthStencilState(const vk::PipelineDepthStencilStateCreateInfo &value) { depthStencilState_ = value; return *this; }
  PipelineMaker &depthTestEnable(vk::Bool32 value) { depthStencilState_.depthTestEnable = value; return *this; }
  PipelineMaker &depthWriteEnable(vk::Bool32 value) { depthStencilState_.depthWriteEnable = value; return *this; }
  PipelineMaker &depthCompareOp(vk::CompareOp value) { depthStencilState_.depthCompareOp = value; return *this; }
  PipelineMaker &depthBoundsTestEnable(vk::Bool32 value) { depthStencilState_.depthBoundsTestEnable = value; return *this; }
  PipelineMaker &stencilTestEnable(vk::Bool32 value) { depthStencilState_.stencilTestEnable = value; return *this; }
  PipelineMaker &front(const vk::StencilOpState& value) { depthStencilState_.front = value; return *this; }
  PipelineMaker &back(const vk::StencilOpState& value) { depthStencilState_.back = value; return *this; }
  PipelineMaker &minDepthBounds(float value) { depthStencilState_.minDepthBounds = value; return *this; }
  PipelineMaker &maxDepthBounds(float value) { depthStencilState_.maxDepthBounds = value; return *this; }

  /// Set a whole colour blend state.
  /// Note you can set individual values with their own call
  PipelineMaker &colorBlendState(const vk::PipelineColorBlendStateCreateInfo &value) { colorBlendState_ = value; return *this; }
  PipelineMaker &logicOpEnable(vk::Bool32 value) { colorBlendState_.logicOpEnable = value; return *this; }
  PipelineMaker &logicOp(vk::LogicOp value) { colorBlendState_.logicOp = value; return *this; }
  PipelineMaker &blendConstants(float r, float g, float b, float a) { float *bc = colorBlendState_.blendConstants; bc[0] = r; bc[1] = g; bc[2] = b; bc[3] = a; return *this; }

  PipelineMaker &dynamicState(vk::DynamicState value) { dynamicState_.push_back(value); return *this; }
private:
  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState_;
  std::vector<vk::Viewport> viewport_;
  std::vector<vk::Rect2D> scissor_;
  vk::PipelineRasterizationStateCreateInfo rasterizationState_;
  vk::PipelineMultisampleStateCreateInfo multisampleState_;
  vk::PipelineDepthStencilStateCreateInfo depthStencilState_;
  vk::PipelineColorBlendStateCreateInfo colorBlendState_;
  vk::PipelineTessellationStateCreateInfo tessellationState_;
  std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments_;
  std::vector<vk::PipelineShaderStageCreateInfo> modules_;
  std::vector<std::unique_ptr<SpecData>> moduleSpecializations_;
  std::vector<vk::VertexInputAttributeDescription> vertexAttributeDescriptions_;
  std::vector<vk::VertexInputBindingDescription> vertexBindingDescriptions_;
  std::vector<vk::DynamicState> dynamicState_;
  uint32_t subpass_ = 0;
};

template <typename iterator, typename sentinel>
PipelineMaker::SpecData::SpecData(iterator b, sentinel e)
{
  auto round_offset = [](uint32_t offset, uint32_t alignment) {
    uint32_t unaligned = offset & (alignment-1);
    return unaligned == 0 ? offset : offset + alignment-unaligned;
  };
  uint32_t offset = 0;
  for (auto it = b; it != e; ++it) {
    auto &entry = *it;
    offset = round_offset(offset, entry.alignment) + entry.size;
  }
  data_size_ = offset;
  // We rely on the fact that new allocates with the maximum basic type alignment.
  data_ = std::unique_ptr<char[]>(new char[data_size_]);
  offset = 0;
  int specCount = 0;
  for (auto it = b; it != e; ++it) {
    auto &entry = *it;
    offset = round_offset(offset, entry.alignment);
    specializationMapEntries_.emplace_back(
        entry.constantID, offset, entry.size);
    const char *src = reinterpret_cast<const char *>(&entry.data);
    std::copy(src, src+entry.size, data_.get() + offset);
    offset += entry.size;
    ++specCount;
  }
  specializationInfo_.mapEntryCount = specCount;
  specializationInfo_.pMapEntries = specializationMapEntries_.data();
  specializationInfo_.dataSize =  data_size_;
  specializationInfo_.pData = data_.get();
}

template <typename SCList>
PipelineMaker::SpecData::SpecData(const SCList &specConstants)
{
  auto round_offset = [](uint32_t offset, uint32_t alignment) {
    uint32_t unaligned = offset & (alignment-1);
    return unaligned == 0 ? offset : offset + alignment-unaligned;
  };
  uint32_t offset = 0;
  for (auto &entry : specConstants) {
    offset = round_offset(offset, entry.alignment) + entry.size;
  }
  data_size_ = offset;
  // We rely on the fact that new allocates with the maximum basic type alignment.
  data_ = std::unique_ptr<char[]>(new char[data_size_]);
  offset = 0;
  for (auto &entry : specConstants) {
    offset = round_offset(offset, entry.alignment);
    specializationMapEntries_.emplace_back(
        entry.constantID, offset, entry.size);
    const char *src = reinterpret_cast<const char *>(&entry.data);
    std::copy(src, src+entry.size, data_.get() + offset);
    offset += entry.size;
  }
  specializationInfo_.mapEntryCount = static_cast<uint32_t>(specConstants.size());
  specializationInfo_.pMapEntries = specializationMapEntries_.data();
  specializationInfo_.dataSize =  data_size_;
  specializationInfo_.pData = data_.get();
}

/// A class for building compute pipelines.
class ComputePipelineMaker {
public:
  ComputePipelineMaker() = default;

  /// Add a shader module to the pipeline.
  ComputePipelineMaker& shader(vk::ShaderStageFlagBits stage, const vku::ShaderModule &shader,
                 const char *entryPoint = "main") {
    stage_.module = shader.module();
    stage_.pName = entryPoint;
    stage_.stage = stage;
    return *this;
  }

  /// Set the compute shader module.
  ComputePipelineMaker &module(const vk::PipelineShaderStageCreateInfo &value) {
    stage_ = value;
    return *this;
  }

  /// Create a managed handle to a compute shader.
  [[nodiscard]] vk::UniquePipeline createUnique(vk::Device device, const vk::PipelineCache &pipelineCache, const vk::PipelineLayout &pipelineLayout) const {
    vk::ComputePipelineCreateInfo pipelineInfo{};

    pipelineInfo.stage = stage_;
    pipelineInfo.layout = pipelineLayout;

    auto [ result, pipeline ] = device.createComputePipelineUnique(pipelineCache, pipelineInfo);
    // TODO check result for vk::Result::ePipelineCompileRequiredEXT
    return std::move(pipeline);
  }
private:
  vk::PipelineShaderStageCreateInfo stage_;
};

/// A generic buffer that may be used as a vertex buffer, uniform buffer or other kinds of memory resident data.
/// Buffers require memory objects which represent GPU and CPU resources.
class GenericBuffer {
public:
  GenericBuffer() = default;

  GenericBuffer(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, vk::BufferUsageFlags usage, vk::DeviceSize size, vk::MemoryPropertyFlags memflags = vk::MemoryPropertyFlagBits::eDeviceLocal) {
    // Create the buffer object without memory.
    vk::BufferCreateInfo ci{};
    ci.size = size_ = size;
    ci.usage = usage;
    ci.sharingMode = vk::SharingMode::eExclusive;
    buffer_ = device.createBufferUnique(ci);

    // Find out how much memory and which heap to allocate from.
    auto memreq = device.getBufferMemoryRequirements(*buffer_);

    // Create a memory object to bind to the buffer.
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = memreq.size;
    mai.memoryTypeIndex = vku::findMemoryTypeIndex(memprops, memreq.memoryTypeBits, memflags);
    mem_ = device.allocateMemoryUnique(mai);

    device.bindBufferMemory(*buffer_, *mem_, 0);
  }

  /// For a host visible buffer, copy memory to the buffer object.
  void updateLocal(const vk::Device &device, const void *value, vk::DeviceSize size) const {
    void *ptr = device.mapMemory(*mem_, 0, size_, vk::MemoryMapFlags{});
    memcpy(ptr, value, (size_t)size);
    flush(device);
    device.unmapMemory(*mem_);
  }

  /// For a purely device local buffer, copy memory to the buffer object immediately.
  /// Note that this will stall the pipeline!
  void upload(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, vk::CommandPool commandPool, vk::Queue queue, const void *value, vk::DeviceSize size) const {
    if (size == 0) return;
    using buf = vk::BufferUsageFlagBits;
    using pfb = vk::MemoryPropertyFlagBits;
    auto tmp = vku::GenericBuffer(device, memprops, buf::eTransferSrc, size, pfb::eHostVisible);
    tmp.updateLocal(device, value, size);

    vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {
      vk::BufferCopy bc{0, 0, size};
      cb.copyBuffer(tmp.buffer(), *buffer_, bc);
    });
  }

  template<typename T>
  void upload(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, vk::CommandPool commandPool, vk::Queue queue, const std::vector<T> &value) const {
    upload(device, memprops, commandPool, queue, value.data(), value.size() * sizeof(T));
  }

  template<typename T>
  void upload(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, vk::CommandPool commandPool, vk::Queue queue, const T &value) const {
    upload(device, memprops, commandPool, queue, &value, sizeof(value));
  }

  void barrier(vk::CommandBuffer cb, vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask, vk::DependencyFlags dependencyFlags, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) const {
    vk::BufferMemoryBarrier bmb{srcAccessMask, dstAccessMask, srcQueueFamilyIndex, dstQueueFamilyIndex, *buffer_, 0, size_};
    cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, nullptr, bmb, nullptr);
  }

  template<class Type, class Allocator>
  void updateLocal(const vk::Device &device, const std::vector<Type, Allocator> &value) const {
    updateLocal(device, static_cast<const void*>(value.data()), static_cast<vk::DeviceSize>(value.size() * sizeof(Type)));
  }

  template<class Type>
  void updateLocal(const vk::Device &device, const Type &value) const {
    updateLocal(device, static_cast<const void*>(&value), static_cast<vk::DeviceSize>(sizeof(Type)));
  }

  [[nodiscard]] void *map(const vk::Device &device) const { return device.mapMemory(*mem_, 0, size_, vk::MemoryMapFlags{}); };
  void unmap(const vk::Device &device) const { return device.unmapMemory(*mem_); };

  void flush(const vk::Device &device) const {
    vk::MappedMemoryRange mr{*mem_, 0, VK_WHOLE_SIZE};
    return device.flushMappedMemoryRanges(mr);
  }

  void invalidate(const vk::Device &device) const {
    vk::MappedMemoryRange mr{*mem_, 0, VK_WHOLE_SIZE};
    return device.invalidateMappedMemoryRanges(mr);
  }

  [[nodiscard]] vk::Buffer buffer() const { return *buffer_; }
  [[nodiscard]] vk::DeviceMemory mem() const { return *mem_; }
  [[nodiscard]] vk::DeviceSize size() const { return size_; }
private:
  vk::UniqueBuffer buffer_;
  vk::UniqueDeviceMemory mem_;
  vk::DeviceSize size_;
};

/// This class is a specialisation of GenericBuffer for high performance vertex buffers on the GPU.
/// You must upload the contents before use.
class VertexBuffer : public GenericBuffer {
public:
  VertexBuffer() = default;

  VertexBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, size_t size) : GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, size, vk::MemoryPropertyFlagBits::eDeviceLocal) {
  }
};

/// This class is a specialisation of GenericBuffer for low performance vertex buffers on the host.
class HostVertexBuffer : public GenericBuffer {
public:
  HostVertexBuffer() = default;

  template<class Type, class Allocator>
  HostVertexBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, const std::vector<Type, Allocator> &value) : GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eVertexBuffer, value.size() * sizeof(Type), vk::MemoryPropertyFlagBits::eHostVisible) {
    updateLocal(device, value);
  }
};

/// This class is a specialisation of GenericBuffer for high performance index buffers.
/// You must upload the contents before use.
class IndexBuffer : public GenericBuffer {
public:
  IndexBuffer() = default;

  IndexBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, vk::DeviceSize size) : GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, size, vk::MemoryPropertyFlagBits::eDeviceLocal) {
  }
};

/// This class is a specialisation of GenericBuffer for low performance vertex buffers in CPU memory.
class HostIndexBuffer : public GenericBuffer {
public:
  HostIndexBuffer() = default;

  template<class Type, class Allocator>
  HostIndexBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, const std::vector<Type, Allocator> &value) : GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eIndexBuffer, value.size() * sizeof(Type), vk::MemoryPropertyFlagBits::eHostVisible) {
    updateLocal(device, value);
  }
};

/// This class is a specialisation of GenericBuffer for uniform buffers.
class UniformBuffer : public GenericBuffer {
public:
  UniformBuffer() = default;

  /// Device local uniform buffer.
  UniformBuffer(const vk::Device &device, const vk::PhysicalDeviceMemoryProperties &memprops, size_t size) :
  GenericBuffer(device, memprops, vk::BufferUsageFlagBits::eUniformBuffer|vk::BufferUsageFlagBits::eTransferDst, static_cast<vk::DeviceSize>(size), vk::MemoryPropertyFlagBits::eDeviceLocal) {
  }
};

/// Convenience class for updating descriptor sets (uniforms)
class DescriptorSetUpdater {
public:
  explicit DescriptorSetUpdater(int maxBuffers = 10, int maxImages = 10, int maxBufferViews = 0) {
    // we must pre-size these buffers as we take pointers to their members.
    bufferInfo_.resize(maxBuffers);
    imageInfo_.resize(maxImages);
    bufferViews_.resize(maxBufferViews);
  }

  /// Call this to begin a new descriptor set.
  DescriptorSetUpdater& beginDescriptorSet(vk::DescriptorSet dstSet) {
    dstSet_ = dstSet;
    return *this;
  }

  /// Call this to begin a new set of images.
  DescriptorSetUpdater& beginImages(uint32_t dstBinding, uint32_t dstArrayElement, vk::DescriptorType descriptorType) {
    vk::WriteDescriptorSet wdesc{};
    wdesc.dstSet = dstSet_;
    wdesc.dstBinding = dstBinding;
    wdesc.dstArrayElement = dstArrayElement;
    wdesc.descriptorCount = 0;
    wdesc.descriptorType = descriptorType;
    wdesc.pImageInfo = imageInfo_.data() + numImages_;
    descriptorWrites_.push_back(wdesc);
    return *this;
  }

  /// Call this to add a combined image sampler.
  DescriptorSetUpdater& image(vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout) {
    if (!descriptorWrites_.empty() && numImages_ != imageInfo_.size() && descriptorWrites_.back().pImageInfo) {
      descriptorWrites_.back().descriptorCount++;
      imageInfo_[numImages_++] = vk::DescriptorImageInfo{sampler, imageView, imageLayout};
    } else {
      ok_ = false;
    }
    return *this;
  }

  /// Call this to start defining buffers.
  DescriptorSetUpdater& beginBuffers(uint32_t dstBinding, uint32_t dstArrayElement, vk::DescriptorType descriptorType) {
    vk::WriteDescriptorSet wdesc{};
    wdesc.dstSet = dstSet_;
    wdesc.dstBinding = dstBinding;
    wdesc.dstArrayElement = dstArrayElement;
    wdesc.descriptorCount = 0;
    wdesc.descriptorType = descriptorType;
    wdesc.pBufferInfo = bufferInfo_.data() + numBuffers_;
    descriptorWrites_.push_back(wdesc);
    return *this;
  }

  /// Call this to add a new buffer.
  DescriptorSetUpdater& buffer(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range) {
    if (!descriptorWrites_.empty() && numBuffers_ != bufferInfo_.size() && descriptorWrites_.back().pBufferInfo) {
      descriptorWrites_.back().descriptorCount++;
      bufferInfo_[numBuffers_++] = vk::DescriptorBufferInfo{buffer, offset, range};
    } else {
      ok_ = false;
    }
    return *this;
  }

  /// Call this to start adding buffer views. (for example, writable images).
  void beginBufferViews(uint32_t dstBinding, uint32_t dstArrayElement, vk::DescriptorType descriptorType) {
    vk::WriteDescriptorSet wdesc{};
    wdesc.dstSet = dstSet_;
    wdesc.dstBinding = dstBinding;
    wdesc.dstArrayElement = dstArrayElement;
    wdesc.descriptorCount = 0;
    wdesc.descriptorType = descriptorType;
    wdesc.pTexelBufferView = bufferViews_.data() + numBufferViews_;
    descriptorWrites_.push_back(wdesc);
  }

  /// Call this to add a buffer view. (Texel images)
  void bufferView(vk::BufferView view) {
    if (!descriptorWrites_.empty() && numBufferViews_ != bufferViews_.size() && descriptorWrites_.back().pImageInfo) {
      descriptorWrites_.back().descriptorCount++;
      bufferViews_[numBufferViews_++] = view;
    } else {
      ok_ = false;
    }
  }

  /// Copy an existing descriptor.
  void copy(vk::DescriptorSet srcSet, uint32_t srcBinding, uint32_t srcArrayElement, vk::DescriptorSet dstSet, uint32_t dstBinding, uint32_t dstArrayElement, uint32_t descriptorCount) {
    descriptorCopies_.emplace_back(srcSet, srcBinding, srcArrayElement, dstSet, dstBinding, dstArrayElement, descriptorCount);
  }

  /// Call this to update the descriptor sets with their pointers (but not data).
  void update(const vk::Device &device) const {
    device.updateDescriptorSets( descriptorWrites_, descriptorCopies_ );
  }

  /// Returns true if the updater is error free.
  [[nodiscard]] bool ok() const { return ok_; }
private:
  std::vector<vk::DescriptorBufferInfo> bufferInfo_;
  std::vector<vk::DescriptorImageInfo> imageInfo_;
  std::vector<vk::WriteDescriptorSet> descriptorWrites_;
  std::vector<vk::CopyDescriptorSet> descriptorCopies_;
  std::vector<vk::BufferView> bufferViews_;
  vk::DescriptorSet dstSet_;
  int numBuffers_ = 0;
  int numImages_ = 0;
  int numBufferViews_ = 0;
  bool ok_ = true;
};

/// A factory class for descriptor set layouts. (An interface to the shaders)
class DescriptorSetLayoutMaker {
public:
  DescriptorSetLayoutMaker() = default;

  DescriptorSetLayoutMaker& buffer(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, uint32_t descriptorCount) {
    s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
    return *this;
  }

  DescriptorSetLayoutMaker& image(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, uint32_t descriptorCount) {
    s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
    return *this;
  }

  DescriptorSetLayoutMaker& samplers(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, const std::vector<vk::Sampler>& immutableSamplers) {
    s.samplers.push_back(immutableSamplers);
    auto pImmutableSamplers = s.samplers.back().data();
    s.bindings.emplace_back(binding, descriptorType, static_cast<uint32_t>(immutableSamplers.size()), stageFlags, pImmutableSamplers);
    return *this;
  }

  DescriptorSetLayoutMaker& bufferView(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, uint32_t descriptorCount) {
    s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
    return *this;
  }

  /// Create a self-deleting descriptor set object.
  [[nodiscard]] vk::UniqueDescriptorSetLayout createUnique(vk::Device device) const {
    vk::DescriptorSetLayoutCreateInfo dsci{};
    dsci.bindingCount = static_cast<uint32_t>(s.bindings.size());
    dsci.pBindings = s.bindings.data();
    return device.createDescriptorSetLayoutUnique(dsci);
  }

private:
  struct State {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    std::vector<std::vector<vk::Sampler> > samplers;
    int numSamplers = 0;
  };

  State s;
};

/// A factory class for descriptor sets (A set of uniform bindings)
class DescriptorSetMaker {
public:
  // Construct a new, empty DescriptorSetMaker.
  DescriptorSetMaker() = default;

  /// Add another layout describing a descriptor set.
  DescriptorSetMaker &layout(vk::DescriptorSetLayout layout) {
    s.layouts.push_back(layout);
    return *this;
  }

  /// Allocate a vector of non-self-deleting descriptor sets
  /// Note: descriptor sets get freed with the pool, so this is the better choice.
  [[nodiscard]] std::vector<vk::DescriptorSet> create(vk::Device device, vk::DescriptorPool descriptorPool) const {
    vk::DescriptorSetAllocateInfo dsai{};
    dsai.descriptorPool = descriptorPool;
    dsai.descriptorSetCount = static_cast<uint32_t>(s.layouts.size());
    dsai.pSetLayouts = s.layouts.data();
    return device.allocateDescriptorSets(dsai);
  }

  /// Allocate a vector of self-deleting descriptor sets.
  [[nodiscard]] std::vector<vk::UniqueDescriptorSet> createUnique(vk::Device device, vk::DescriptorPool descriptorPool) const {
    vk::DescriptorSetAllocateInfo dsai{};
    dsai.descriptorPool = descriptorPool;
    dsai.descriptorSetCount = static_cast<uint32_t>(s.layouts.size());
    dsai.pSetLayouts = s.layouts.data();
    return device.allocateDescriptorSetsUnique(dsai);
  }

private:
  struct State {
    std::vector<vk::DescriptorSetLayout> layouts;
  };

  State s;
};

/// Generic image with a view and memory object.
/// Vulkan images need a memory object to hold the data and a view object for the GPU to access the data.
class GenericImage {
public:
  GenericImage() = default;

  GenericImage(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, const vk::ImageCreateInfo &info, vk::ImageViewType viewType, vk::ImageAspectFlags aspectMask, bool makeHostImage) {
    create(device, memprops, info, viewType, aspectMask, makeHostImage);
  }

  [[nodiscard]] vk::Image image() const { return *s.image; }
  [[nodiscard]] vk::ImageView imageView() const { return *s.imageView; }
  [[nodiscard]] vk::DeviceMemory mem() const { return *s.mem; }

  /// Clear the colour of an image.
  void clear(vk::CommandBuffer cb, const std::array<float,4> colour = {1, 1, 1, 1}) {
    setLayout(cb, vk::ImageLayout::eTransferDstOptimal);
    vk::ClearColorValue ccv(colour);
    vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    cb.clearColorImage(*s.image, vk::ImageLayout::eTransferDstOptimal, ccv, range);
  }

  /// Update the image with an array of pixels. (Currently 2D only)
  void update(vk::Device device, const void *data, vk::DeviceSize bytesPerPixel) {
    const auto *src = static_cast<const uint8_t *>(data);
    for (uint32_t mipLevel = 0; mipLevel != info().mipLevels; ++mipLevel) {
      // Array images are layed out horizontally. eg. [left][front][right] etc.
      for (uint32_t arrayLayer = 0; arrayLayer != info().arrayLayers; ++arrayLayer) {
        vk::ImageSubresource subresource{vk::ImageAspectFlagBits::eColor, mipLevel, arrayLayer};
        auto srlayout = device.getImageSubresourceLayout(*s.image, subresource);
        auto *dest = static_cast<uint8_t *>(device.mapMemory(*s.mem, 0, s.size, vk::MemoryMapFlags{})) + srlayout.offset;
        size_t bytesPerLine = s.info.extent.width * bytesPerPixel;
        size_t srcStride = bytesPerLine * info().arrayLayers;
        for (int y = 0; y != s.info.extent.height; ++y) {
          memcpy(dest, src + arrayLayer * bytesPerLine, bytesPerLine);
          src += srcStride;
          dest += srlayout.rowPitch;
        }
      }
    }
    device.unmapMemory(*s.mem);
  }

  /// Copy another image to this one. This also changes the layout.
  void copy(vk::CommandBuffer cb, vku::GenericImage &srcImage) {
    srcImage.setLayout(cb, vk::ImageLayout::eTransferSrcOptimal);
    setLayout(cb, vk::ImageLayout::eTransferDstOptimal);
    for (uint32_t mipLevel = 0; mipLevel != info().mipLevels; ++mipLevel) {
      vk::ImageCopy region{};
      region.srcSubresource = {vk::ImageAspectFlagBits::eColor, mipLevel, 0, 1};
      region.dstSubresource = {vk::ImageAspectFlagBits::eColor, mipLevel, 0, 1};
      region.extent = s.info.extent;
      cb.copyImage(srcImage.image(), vk::ImageLayout::eTransferSrcOptimal, *s.image, vk::ImageLayout::eTransferDstOptimal, region);
    }
  }

  /// Copy a subimage in a buffer to this image.
  void copy(vk::CommandBuffer cb, vk::Buffer buffer, uint32_t mipLevel, uint32_t arrayLayer, uint32_t width, uint32_t height, uint32_t depth, uint32_t offset) {
    setLayout(cb, vk::ImageLayout::eTransferDstOptimal);
    vk::BufferImageCopy region{};
    region.bufferOffset = offset;
    vk::Extent3D extent;
    extent.width = width;
    extent.height = height;
    extent.depth = depth;
    region.imageSubresource = {vk::ImageAspectFlagBits::eColor, mipLevel, arrayLayer, 1};
    region.imageExtent = extent;
    cb.copyBufferToImage(buffer, *s.image, vk::ImageLayout::eTransferDstOptimal, region);
  }

  void upload(vk::Device device, const std::vector<uint8_t> &bytes, vk::CommandPool commandPool, const vk::PhysicalDeviceMemoryProperties& memprops, vk::Queue queue, vk::ImageLayout finalLayout=vk::ImageLayout::eShaderReadOnlyOptimal) {
	return upload(device, bytes.data(), bytes.size(), commandPool, memprops, queue, finalLayout);
  }

  void upload(vk::Device device, const uint8_t *bytes, size_t bytesSize, vk::CommandPool commandPool, const vk::PhysicalDeviceMemoryProperties& memprops, vk::Queue queue, vk::ImageLayout finalLayout=vk::ImageLayout::eShaderReadOnlyOptimal) {
    vku::GenericBuffer stagingBuffer(device, memprops, static_cast<vk::BufferUsageFlags>(vk::BufferUsageFlagBits::eTransferSrc), static_cast<vk::DeviceSize>(bytesSize), vk::MemoryPropertyFlagBits::eHostVisible);
    stagingBuffer.updateLocal(device, (const void*)bytes, bytesSize);

    // Copy the staging buffer to the GPU texture and set the layout.
    vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {
      auto bp = getBlockParams(s.info.format);
      vk::Buffer buf = stagingBuffer.buffer();
      uint32_t offset = 0;
      for (uint32_t mipLevel = 0; mipLevel != s.info.mipLevels; ++mipLevel) {
        auto width = mipScale(s.info.extent.width, mipLevel);
        auto height = mipScale(s.info.extent.height, mipLevel);
        auto depth = mipScale(s.info.extent.depth, mipLevel);
        for (uint32_t face = 0; face != s.info.arrayLayers; ++face) {
          copy(cb, buf, mipLevel, face, width, height, depth, offset);
          offset += ((bp.bytesPerBlock + 3) & ~3) * (width * height);
        }
      }
      setLayout(cb, finalLayout);
    });
  }

  /// Change the layout of this image using a memory barrier.
  void setLayout(vk::CommandBuffer cb, vk::ImageLayout newLayout, vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) {
    if (newLayout == s.currentLayout) return;
    vk::ImageLayout oldLayout = s.currentLayout;
    s.currentLayout = newLayout;

    vk::ImageMemoryBarrier imageMemoryBarriers = {};
    imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.oldLayout = oldLayout;
    imageMemoryBarriers.newLayout = newLayout;
    imageMemoryBarriers.image = *s.image;
    imageMemoryBarriers.subresourceRange = {aspectMask, 0, s.info.mipLevels, 0, s.info.arrayLayers};

    // Put barrier on top
    // https://www.khronos.org/registry/vulkan/specs/1.2/html/chap7.html#synchronization-access-types-supported
    vk::PipelineStageFlags srcStageMask{vk::PipelineStageFlagBits::eTopOfPipe};
    vk::PipelineStageFlags dstStageMask{vk::PipelineStageFlagBits::eTopOfPipe};
    vk::DependencyFlags dependencyFlags{};
    vk::AccessFlags srcMask{};
    vk::AccessFlags dstMask{};

    typedef vk::ImageLayout il;
    typedef vk::AccessFlagBits afb;

    // Is it me, or are these the same?
    switch (oldLayout) {
      case il::eUndefined: break;
      case il::eGeneral: srcMask = afb::eTransferWrite; srcStageMask=vk::PipelineStageFlagBits::eTransfer; break;
      case il::eColorAttachmentOptimal: srcMask = afb::eColorAttachmentWrite; srcStageMask=vk::PipelineStageFlagBits::eColorAttachmentOutput; break;
      case il::eDepthStencilAttachmentOptimal: srcMask = afb::eDepthStencilAttachmentWrite; srcStageMask=vk::PipelineStageFlagBits::eEarlyFragmentTests; break;
      case il::eDepthStencilReadOnlyOptimal: srcMask = afb::eDepthStencilAttachmentRead; srcStageMask=vk::PipelineStageFlagBits::eEarlyFragmentTests; break;
      case il::eShaderReadOnlyOptimal: srcMask = afb::eShaderRead; srcStageMask=vk::PipelineStageFlagBits::eVertexShader; break;
      case il::eTransferSrcOptimal: srcMask = afb::eTransferRead; srcStageMask=vk::PipelineStageFlagBits::eTransfer; break;
      case il::eTransferDstOptimal: srcMask = afb::eTransferWrite; srcStageMask=vk::PipelineStageFlagBits::eTransfer; break;
      case il::ePreinitialized: srcMask = afb::eTransferWrite|afb::eHostWrite; srcStageMask=vk::PipelineStageFlagBits::eTransfer|vk::PipelineStageFlagBits::eHost; break;
      case il::ePresentSrcKHR: srcMask = afb::eMemoryRead; break;
      default: break;
    }

    switch (newLayout) {
      case il::eUndefined: break;
      case il::eGeneral: dstMask = afb::eTransferWrite; dstStageMask=vk::PipelineStageFlagBits::eTransfer; break;
      case il::eColorAttachmentOptimal: dstMask = afb::eColorAttachmentWrite; dstStageMask=vk::PipelineStageFlagBits::eColorAttachmentOutput; break;
      case il::eDepthStencilAttachmentOptimal: dstMask = afb::eDepthStencilAttachmentWrite; dstStageMask=vk::PipelineStageFlagBits::eEarlyFragmentTests; break;
      case il::eDepthStencilReadOnlyOptimal: dstMask = afb::eDepthStencilAttachmentRead; dstStageMask=vk::PipelineStageFlagBits::eEarlyFragmentTests; break;
      case il::eShaderReadOnlyOptimal: dstMask = afb::eShaderRead; dstStageMask=vk::PipelineStageFlagBits::eVertexShader; break;
      case il::eTransferSrcOptimal: dstMask = afb::eTransferRead; dstStageMask=vk::PipelineStageFlagBits::eTransfer; break;
      case il::eTransferDstOptimal: dstMask = afb::eTransferWrite; dstStageMask=vk::PipelineStageFlagBits::eTransfer; break; // NOLINT(*-branch-clone)
      case il::ePreinitialized: dstMask = afb::eTransferWrite; dstStageMask=vk::PipelineStageFlagBits::eTransfer; break;
      case il::ePresentSrcKHR: dstMask = afb::eMemoryRead; break;
      default: break;
    }
//printf("%08x %08x\n", (VkFlags)srcMask, (VkFlags)dstMask);

    imageMemoryBarriers.srcAccessMask = srcMask;
    imageMemoryBarriers.dstAccessMask = dstMask;
    auto memoryBarriers = nullptr;
    auto bufferMemoryBarriers = nullptr;
    cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, memoryBarriers, bufferMemoryBarriers, imageMemoryBarriers);
  }

  /// Set what the image thinks is its current layout (ie. the old layout in an image barrier).
  void setCurrentLayout(vk::ImageLayout oldLayout) {
    s.currentLayout = oldLayout;
  }

  [[nodiscard]] vk::Format format() const { return s.info.format; }
  [[nodiscard]] vk::Extent3D extent() const { return s.info.extent; }
  [[nodiscard]] const vk::ImageCreateInfo &info() const { return s.info; }
protected:
  void create(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, const vk::ImageCreateInfo &info, vk::ImageViewType viewType, vk::ImageAspectFlags aspectMask, bool hostImage) {
    s.currentLayout = info.initialLayout;
    s.info = info;
    s.image = device.createImageUnique(info);

    // Find out how much memory and which heap to allocate from.
    auto memreq = device.getImageMemoryRequirements(*s.image);
    vk::MemoryPropertyFlags search{};
    if (hostImage) search = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;

    // Create a memory object to bind to the buffer.
    // Note: we don't expect to be able to map the buffer.
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = s.size = memreq.size;
    mai.memoryTypeIndex = vku::findMemoryTypeIndex(memprops, memreq.memoryTypeBits, search);
    s.mem = device.allocateMemoryUnique(mai);

    device.bindImageMemory(*s.image, *s.mem, 0);

    if (!hostImage) {
      vk::ImageViewCreateInfo viewInfo{};
      viewInfo.image = *s.image;
      viewInfo.viewType = viewType;
      viewInfo.format = info.format;
      viewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
      viewInfo.subresourceRange = vk::ImageSubresourceRange{aspectMask, 0, info.mipLevels, 0, info.arrayLayers};
      s.imageView = device.createImageViewUnique(viewInfo);
    }
  }

  struct State {
    vk::UniqueImage image;
    vk::UniqueImageView imageView;
    vk::UniqueDeviceMemory mem;
    vk::DeviceSize size;
    vk::ImageLayout currentLayout;
    vk::ImageCreateInfo info;

  };

  State s;
};


/// A 2D texture image living on the GPU or a staging buffer visible to the CPU.
class TextureImage2D : public GenericImage {
public:
  TextureImage2D() = default;

  TextureImage2D(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, uint32_t width, uint32_t height, uint32_t mipLevels=1, vk::Format format = vk::Format::eR8G8B8A8Unorm, bool hostImage = false) {
    vk::ImageCreateInfo info;
    info.flags = {};
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = mipLevels;
    info.arrayLayers = 1;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eTransferDst;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
    create(device, memprops, info, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, hostImage);
  }
private:
};

/// A cube map texture image living on the GPU or a staging buffer visible to the CPU.
class TextureImageCube : public GenericImage {
public:
  TextureImageCube() = default;

  TextureImageCube(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, uint32_t width, uint32_t height, uint32_t mipLevels=1, vk::Format format = vk::Format::eR8G8B8A8Unorm, bool hostImage = false) {
    vk::ImageCreateInfo info;
    info.flags = {vk::ImageCreateFlagBits::eCubeCompatible};
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = mipLevels;
    info.arrayLayers = 6;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eColorAttachment|vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eTransferDst;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
    //info.initialLayout = vk::ImageLayout::ePreinitialized;
    create(device, memprops, info, vk::ImageViewType::eCube, vk::ImageAspectFlagBits::eColor, hostImage);
  }
private:
};

/// An image to use as a depth buffer on a renderpass.
class DepthStencilImage : public GenericImage {
public:
  DepthStencilImage() = default;

  DepthStencilImage(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, uint32_t width, uint32_t height, vk::Format format = vk::Format::eD24UnormS8Uint) {
    vk::ImageCreateInfo info;
    info.flags = {};

    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment|vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eSampled;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = vk::ImageLayout::eUndefined;
    typedef vk::ImageAspectFlagBits iafb;
    create(device, memprops, info, vk::ImageViewType::e2D, iafb::eDepth, false);
  }
private:
};

/// An image to use as a colour buffer on a renderpass.
class ColorAttachmentImage : public GenericImage {
public:
  ColorAttachmentImage() = default;

  ColorAttachmentImage(vk::Device device, const vk::PhysicalDeviceMemoryProperties &memprops, uint32_t width, uint32_t height, vk::Format format = vk::Format::eR8G8B8A8Unorm) {
    vk::ImageCreateInfo info;
    info.flags = {};

    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eColorAttachment|vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eTransferDst|vk::ImageUsageFlagBits::eSampled;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = vk::ImageLayout::eUndefined;
    typedef vk::ImageAspectFlagBits iafb;
    create(device, memprops, info, vk::ImageViewType::e2D, iafb::eColor, false);
  }
private:
};

/// A class to help build samplers.
/// Samplers tell the shader stages how to sample an image.
/// They are used in combination with an image to make a combined image sampler
/// used by texture() calls in shaders.
/// They can also be passed to shaders directly for use on multiple image sources.
class SamplerMaker {
public:
  /// Default to a very basic sampler.
  SamplerMaker() {
    s.info.magFilter = vk::Filter::eNearest;
    s.info.minFilter = vk::Filter::eNearest;
    s.info.mipmapMode = vk::SamplerMipmapMode::eNearest;
    s.info.addressModeU = vk::SamplerAddressMode::eRepeat;
    s.info.addressModeV = vk::SamplerAddressMode::eRepeat;
    s.info.addressModeW = vk::SamplerAddressMode::eRepeat;
    s.info.mipLodBias = 0.0f;
    s.info.anisotropyEnable = 0;
    s.info.maxAnisotropy = 0.0f;
    s.info.compareEnable = 0;
    s.info.compareOp = vk::CompareOp::eNever;
    s.info.minLod = 0;
    s.info.maxLod = 0;
    s.info.borderColor = vk::BorderColor{};
    s.info.unnormalizedCoordinates = 0;
  }

  ////////////////////
  //
  // Setters
  //
  SamplerMaker &flags(vk::SamplerCreateFlags value) { s.info.flags = value; return *this; }

  /// Set the magnify filter value. (for close textures)
  /// Options are: vk::Filter::eLinear and vk::Filter::eNearest
  SamplerMaker &magFilter(vk::Filter value) { s.info.magFilter = value; return *this; }

  /// Set the minnify filter value. (for far away textures)
  /// Options are: vk::Filter::eLinear and vk::Filter::eNearest
  SamplerMaker &minFilter(vk::Filter value) { s.info.minFilter = value; return *this; }

  /// Set the minnify filter value. (for far away textures)
  /// Options are: vk::SamplerMipmapMode::eLinear and vk::SamplerMipmapMode::eNearest
  SamplerMaker &mipmapMode(vk::SamplerMipmapMode value) { s.info.mipmapMode = value; return *this; }
  SamplerMaker &addressModeU(vk::SamplerAddressMode value) { s.info.addressModeU = value; return *this; }
  SamplerMaker &addressModeV(vk::SamplerAddressMode value) { s.info.addressModeV = value; return *this; }
  SamplerMaker &addressModeW(vk::SamplerAddressMode value) { s.info.addressModeW = value; return *this; }
  SamplerMaker &mipLodBias(float value) { s.info.mipLodBias = value; return *this; }
  SamplerMaker &anisotropyEnable(vk::Bool32 value) { s.info.anisotropyEnable = value; return *this; }
  SamplerMaker &maxAnisotropy(float value) { s.info.maxAnisotropy = value; return *this; }
  SamplerMaker &compareEnable(vk::Bool32 value) { s.info.compareEnable = value; return *this; }
  SamplerMaker &compareOp(vk::CompareOp value) { s.info.compareOp = value; return *this; }
  SamplerMaker &minLod(float value) { s.info.minLod = value; return *this; }
  SamplerMaker &maxLod(float value) { s.info.maxLod = value; return *this; }
  SamplerMaker &borderColor(vk::BorderColor value) { s.info.borderColor = value; return *this; }
  SamplerMaker &unnormalizedCoordinates(vk::Bool32 value) { s.info.unnormalizedCoordinates = value; return *this; }

  /// Allocate a self-deleting image.
  [[nodiscard]] vk::UniqueSampler createUnique(vk::Device device) const {
    return device.createSamplerUnique(s.info);
  }

  /// Allocate a non self-deleting Sampler.
  [[nodiscard]] vk::Sampler create(vk::Device device) const {
    return device.createSampler(s.info);
  }

private:
  struct State {
    vk::SamplerCreateInfo info;
  };

  State s;
};

/// KTX files use OpenGL format values. This converts some common ones to Vulkan equivalents.
inline vk::Format GLtoVKFormat(uint32_t glFormat) {
  switch (glFormat) {
    case 0x1907: return vk::Format::eR8G8B8Unorm; // GL_RGB
    case 0x1908: return vk::Format::eR8G8B8A8Unorm; // GL_RGBA
    case 0x83F0: return vk::Format::eBc1RgbUnormBlock; // GL_COMPRESSED_RGB_S3TC_DXT1_EXT
    case 0x83F1: return vk::Format::eBc1RgbaUnormBlock; // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
    case 0x83F2: return vk::Format::eBc3UnormBlock; // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
    case 0x83F3: return vk::Format::eBc5UnormBlock; // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
    default:
      return vk::Format::eUndefined;
  }
}



/// Layout of a KTX file in a buffer.
class KTXFileLayout {
public:
  KTXFileLayout() = default;

  KTXFileLayout(uint8_t *begin, const uint8_t *end) {
    uint8_t *p = begin;
    if (p + sizeof(Header) > end) return;
    header = *reinterpret_cast<Header*>(p);
    static const uint8_t magic[] = {
      0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
    };


    if (memcmp(magic, header.identifier, sizeof(magic)) != 0) {
      return;
    }

    if (header.endianness != 0x04030201) {
      swap(header.glType);
      swap(header.glTypeSize);
      swap(header.glFormat);
      swap(header.glInternalFormat);
      swap(header.glBaseInternalFormat);
      swap(header.pixelWidth);
      swap(header.pixelHeight);
      swap(header.pixelDepth);
      swap(header.numberOfArrayElements);
      swap(header.numberOfFaces);
      swap(header.numberOfMipmapLevels);
      swap(header.bytesOfKeyValueData);
    }

    header.numberOfArrayElements = std::max(1U, header.numberOfArrayElements);
    header.numberOfFaces = std::max(1U, header.numberOfFaces);
    header.numberOfMipmapLevels = std::max(1U, header.numberOfMipmapLevels);
    header.pixelDepth = std::max(1U, header.pixelDepth);

    format_ = GLtoVKFormat(header.glFormat);
    if (format_ == vk::Format::eUndefined) return;

    p += sizeof(Header);
    if (p + header.bytesOfKeyValueData > end) return;

    for (uint32_t i = 0; i < header.bytesOfKeyValueData; ) {
      uint32_t keyAndValueByteSize = *reinterpret_cast<uint32_t*>(p + i);
      if (header.endianness != 0x04030201) swap(keyAndValueByteSize);
      std::string kv(p + i + 4, p + i + 4 + keyAndValueByteSize);
      i += keyAndValueByteSize + 4;
      i = (i + 3) & ~3;
    }

    p += header.bytesOfKeyValueData;
    for (uint32_t mipLevel = 0; mipLevel != header.numberOfMipmapLevels; ++mipLevel) {
      uint32_t imageSize = *reinterpret_cast<uint32_t*>(p);
      imageSize = (imageSize + 3) & ~3;
      uint32_t incr = imageSize * header.numberOfFaces * header.numberOfArrayElements;
      incr = (incr + 3) & ~3;

      if (p + incr > end) {
        // see https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glPixelStore.xhtml
        // fix bugs... https://github.com/dariomanesku/cmft/issues/29
        header.numberOfMipmapLevels = mipLevel;
        break;
      }

      if (header.endianness != 0x04030201) swap(imageSize);
      //printf("%08x: is=%08x / %08x\n", p-begin, imageSize, end - begin);
      p += 4;
      imageOffsets_.push_back(static_cast<uint32_t>(p - begin));
      imageSizes_.push_back(imageSize);
      p += incr;
    }

    ok_ = true;
  }

  [[nodiscard]] uint32_t offset(uint32_t mipLevel, uint32_t arrayLayer, uint32_t face) const {
    return imageOffsets_[mipLevel] + (arrayLayer * header.numberOfFaces + face) * imageSizes_[mipLevel];
  }

  [[nodiscard]] uint32_t size(uint32_t mipLevel) const {
    return imageSizes_[mipLevel];
  }

  [[nodiscard]] bool ok() const { return ok_; }
  [[nodiscard]] vk::Format format() const { return format_; }
  [[nodiscard]] uint32_t mipLevels() const { return header.numberOfMipmapLevels; }
  [[nodiscard]] uint32_t arrayLayers() const { return header.numberOfArrayElements; }
  [[nodiscard]] uint32_t faces() const { return header.numberOfFaces; }
  [[nodiscard]] uint32_t width(uint32_t mipLevel) const { return mipScale(header.pixelWidth, mipLevel); }
  [[nodiscard]] uint32_t height(uint32_t mipLevel) const { return mipScale(header.pixelHeight, mipLevel); }
  [[nodiscard]] uint32_t depth(uint32_t mipLevel) const { return mipScale(header.pixelDepth, mipLevel); }

  void upload(vk::Device device, vku::GenericImage &image, const std::vector<uint8_t> &bytes, vk::CommandPool commandPool, const vk::PhysicalDeviceMemoryProperties& memprops, vk::Queue queue) const {
    vku::GenericBuffer stagingBuffer(device, memprops, static_cast<vk::BufferUsageFlags>(vk::BufferUsageFlagBits::eTransferSrc), bytes.size(), vk::MemoryPropertyFlagBits::eHostVisible);
    stagingBuffer.updateLocal(device, (const void*)bytes.data(), bytes.size());

    // Copy the staging buffer to the GPU texture and set the layout.
    vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {
      vk::Buffer buf = stagingBuffer.buffer();
      for (uint32_t mipLevel = 0; mipLevel != mipLevels(); ++mipLevel) {
        auto width = this->width(mipLevel);
        auto height = this->height(mipLevel);
        auto depth = this->depth(mipLevel);
        for (uint32_t face = 0; face != faces(); ++face) {
          image.copy(cb, buf, mipLevel, face, width, height, depth, offset(mipLevel, 0, face));
        }
      }
      image.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal);
    });
  }

private:
  static void swap(uint32_t &value) {
    value = value >> 24 | (value & 0xff0000) >> 8 | (value & 0xff00) << 8 | value << 24;
  }

  struct Header {
    uint8_t identifier[12];
    uint32_t endianness;
    uint32_t glType;
    uint32_t glTypeSize;
    uint32_t glFormat;
    uint32_t glInternalFormat;
    uint32_t glBaseInternalFormat;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t numberOfArrayElements;
    uint32_t numberOfFaces;
    uint32_t numberOfMipmapLevels;
    uint32_t bytesOfKeyValueData;
  };

  Header header{};
  vk::Format format_;
  bool ok_ = false;
  std::vector<uint32_t> imageOffsets_;
  std::vector<uint32_t> imageSizes_;
};


} // namespace vku

#endif // VKU_HPP
