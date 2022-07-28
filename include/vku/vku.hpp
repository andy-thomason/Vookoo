////////////////////////////////////////////////////////////////////////////////
//
/// Vookoo high level C++ Vulkan interface.
//
/// (C) Andy Thomason 2017 MIT License
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

// Additions & Fixes -
// Jason Tully
// 2022
// (supports minimum spec Radeon 290, Hvidia GTX 970)

#pragma once
#ifndef VKU_HPP
#define VKU_HPP

#include <Utility/mem.h>
#pragma intrinsic(memcpy)
#pragma intrinsic(memset)

#define VK_NO_PROTOTYPES
#include "volk/volk.h"

#include <array>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <functional>
#include <cstddef>
#include <optional>
#include <variant>
#include <tbb/tbb.h>

#ifndef NDEBUG
//#define BREAK_ON_VALIDATION_ERROR 1  // set to 1 to enable debug break on vulkan validation errors. callstack can be used to find source of error.
#endif

#ifndef NDEBUG
//#define SYNC_VALIDATION_ONLY 1
#endif

// workaround, so volk gets used instead. note that vulkan is not staically linked using this method
// all vulkan function calls route properly through volk
// modify Vulkan->hpp (line 756)
// #if defined(VK_NO_PROTOTYPES)            // was: #if !defined(VK_NO_PROTOTYPES)
// class DispatchLoaderStatic
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 0 // disabled - using volk instead
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 0 // disabled - using volk instead
#define VULKAN_HPP_NO_NODISCARD_WARNINGS
#define VULKAN_HPP_NO_EXCEPTIONS 
#define VULKAN_HPP_ASSERT (void)

// ####################################################################################################################
#pragma component(browser, off, references) // warning BK4504 workaround
#include <spirv-headers/spirv.hpp11>
#include <vulkan/Vulkan.hpp>		// this is the one and only place Vulkan->hpp can be included, use cVulkan->h (prefer)
#pragma component(browser, on, references) // warning BK4504 workaround
// #####################################################################################################################

#include <fmt/fmt.h>
#include <Utility/stringconv.h>

#define VULKAN_API_VERSION_USED VK_API_VERSION_1_2

#include <vku/vku_addon.hpp>

#define VMA_STATIC_VULKAN_FUNCTIONS 1 // route to volk
#define VMA_VULKAN_VERSION 1002000 // Vulkan 1.2
#define VMA_DEDICATED_ALLOCATION 1
#define VMA_MEMORY_BUDGET 1

#ifndef NDEBUG 
#ifdef VKU_VMA_DEBUG_ENABLED 
#define VMA_DEBUG_MARGIN 32
#define VMA_DEBUG_DETECT_CORRUPTION 1
#define VMA_RECORDING_ENABLED 0
#define VMA_STATS_STRING_ENABLED 1
#define VMA_DEBUG_LOG(message, ...) { (fmt::printf("[VMA] " INVERSE_ANSI message INVERSE_ANSI_OFF "\n", __VA_ARGS__)); }
#endif
#define VMA_ASSERT(expr) assert_print(expr, "VMA Memory FAIL");
#else
#define VMA_DEBUG_MARGIN 0
#define VMA_DEBUG_DETECT_CORRUPTION 0
#define VMA_RECORDING_ENABLED 0
#define VMA_STATS_STRING_ENABLED 0
#define VMA_DEBUG_LOG(format, ...) (void)format;
#define VMA_ASSERT(expr) ((void)0)
#endif
#ifndef VULKAN_H_
#define VULKAN_H_ // sanity check prevent vma from additional inclusion of Vulkan->h which is covered by volk ONLY
#endif

#include <vku/vma/vk_mem_alloc.h>

namespace vku {

	extern VmaAllocator vma_;		//singleton - further initialized by vku_framework after device creation
	extern tbb::concurrent_unordered_map<VkCommandPool, vku::CommandBufferContainer<1>> pool_;

	enum eMappedAccess
	{
		Disabled = 0,
		Sequential = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		Random = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
	};
	
/// Printf-style formatting function.
template <class ... Args>
std::string format(const char *fmt, Args... args) {
  //int n = (snprintf(nullptr, 0, fmt, args...);
  //std::string result(n, '\0');
  //snprintf(&*result.begin(), n+1, fmt, args...);
  return(fmt::format(fmt, args));
}

template<bool const bAsync = true>
static inline void executeImmediately(vk::Device const& __restrict device, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue, std::function<void(vk::CommandBuffer cb)> const func) { // const std::function<void (vk::CommandBuffer cb)> 
    
	static constexpr uint64_t const umax = nanoseconds(milliseconds(500)).count();

	using pool_map = tbb::concurrent_unordered_map<VkCommandPool, vku::CommandBufferContainer<1>>;

	pool_map::const_iterator container(pool_.find(commandPool));

	if (pool_.cend() == container) {
		vk::CommandBufferAllocateInfo const cbai{ commandPool, vk::CommandBufferLevel::ePrimary, 2 }; // always allocating 2 command buffers on this command pools first usage

		auto const [iter, success] = pool_.emplace(commandPool, CommandBufferContainer<1>(device, cbai));
#ifndef NDEBUG
		assert_print(success, "Could not allocate command buffer! FAIL");
#endif
		container = iter;
	}

	// try first command buffer, no waiting (fast path)
	vk::CommandBuffer cb(*container->second.cb[0][0]);
	vk::Fence cbFence(container->second.fence[0][0]);
	
	if (vk::Result::eTimeout == device.waitForFences(cbFence, VK_FALSE, 0)) {
		// try second command buffer, waiting if neccessary
		cb = *container->second.cb[0][1];
		cbFence = container->second.fence[0][1];
		device.waitForFences(cbFence, VK_TRUE, umax);
	}

	cb.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	func(cb);
	cb.end();

	vk::SubmitInfo submit{};
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cb;

	device.resetFences(cbFence);
	queue.submit(submit, cbFence);
	
	if constexpr (!bAsync) {
		queue.waitIdle();
	}
}

// memory barrier helper
static inline void memory_barrier(vk::CommandBuffer& cb, vk::PipelineStageFlags const srcStageMask, vk::PipelineStageFlags const dstStageMask, vk::AccessFlags const srcAccessMask, vk::AccessFlags const dstAccessMask) {
	vk::MemoryBarrier mb(srcAccessMask, dstAccessMask);
	cb.pipelineBarrier(srcStageMask, dstStageMask, vk::DependencyFlagBits::eByRegion, mb, nullptr, nullptr);
}

/// Scale a value by mip level, but do not reduce to zero.
inline uint32_t mipScale(uint32_t value, uint32_t mipLevel) {
  return std::max(value >> mipLevel, (uint32_t)1);
}

/// Load a binary file into a vector.
/// The vector will be zero-length if this fails.
inline std::vector<uint8_t> loadFile(const std::string &filename) {
  std::ifstream is(filename, std::ios::binary|std::ios::ate);
  std::vector<uint8_t> bytes;
  if (!is.fail()) {
    size_t size = is.tellg();
    is.seekg(0);
    bytes.resize(size);
    is.read((char*)bytes.data(), size);
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
    case vk::Format::eR4G4B4A4UnormPack16: return BlockParams{1, 1, 2};
    case vk::Format::eB4G4R4A4UnormPack16: return BlockParams{1, 1, 2};
    case vk::Format::eR5G6B5UnormPack16: return BlockParams{1, 1, 2};
    case vk::Format::eB5G6R5UnormPack16: return BlockParams{1, 1, 2};
    case vk::Format::eR5G5B5A1UnormPack16: return BlockParams{1, 1, 2};
    case vk::Format::eB5G5R5A1UnormPack16: return BlockParams{1, 1, 2};
    case vk::Format::eA1R5G5B5UnormPack16: return BlockParams{1, 1, 2};
    case vk::Format::eR8Unorm: return BlockParams{1, 1, 1};
    case vk::Format::eR8Snorm: return BlockParams{1, 1, 1};
    case vk::Format::eR8Uscaled: return BlockParams{1, 1, 1};
    case vk::Format::eR8Sscaled: return BlockParams{1, 1, 1};
    case vk::Format::eR8Uint: return BlockParams{1, 1, 1};
    case vk::Format::eR8Sint: return BlockParams{1, 1, 1};
    case vk::Format::eR8Srgb: return BlockParams{1, 1, 1};
    case vk::Format::eR8G8Unorm: return BlockParams{1, 1, 2};
    case vk::Format::eR8G8Snorm: return BlockParams{1, 1, 2};
    case vk::Format::eR8G8Uscaled: return BlockParams{1, 1, 2};
    case vk::Format::eR8G8Sscaled: return BlockParams{1, 1, 2};
    case vk::Format::eR8G8Uint: return BlockParams{1, 1, 2};
    case vk::Format::eR8G8Sint: return BlockParams{1, 1, 2};
    case vk::Format::eR8G8Srgb: return BlockParams{1, 1, 2};
    case vk::Format::eR8G8B8Unorm: return BlockParams{1, 1, 3};
    case vk::Format::eR8G8B8Snorm: return BlockParams{1, 1, 3};
    case vk::Format::eR8G8B8Uscaled: return BlockParams{1, 1, 3};
    case vk::Format::eR8G8B8Sscaled: return BlockParams{1, 1, 3};
    case vk::Format::eR8G8B8Uint: return BlockParams{1, 1, 3};
    case vk::Format::eR8G8B8Sint: return BlockParams{1, 1, 3};
    case vk::Format::eR8G8B8Srgb: return BlockParams{1, 1, 3};
    case vk::Format::eB8G8R8Unorm: return BlockParams{1, 1, 3};
    case vk::Format::eB8G8R8Snorm: return BlockParams{1, 1, 3};
    case vk::Format::eB8G8R8Uscaled: return BlockParams{1, 1, 3};
    case vk::Format::eB8G8R8Sscaled: return BlockParams{1, 1, 3};
    case vk::Format::eB8G8R8Uint: return BlockParams{1, 1, 3};
    case vk::Format::eB8G8R8Sint: return BlockParams{1, 1, 3};
    case vk::Format::eB8G8R8Srgb: return BlockParams{1, 1, 3};
    case vk::Format::eR8G8B8A8Unorm: return BlockParams{1, 1, 4};
    case vk::Format::eR8G8B8A8Snorm: return BlockParams{1, 1, 4};
    case vk::Format::eR8G8B8A8Uscaled: return BlockParams{1, 1, 4};
    case vk::Format::eR8G8B8A8Sscaled: return BlockParams{1, 1, 4};
    case vk::Format::eR8G8B8A8Uint: return BlockParams{1, 1, 4};
    case vk::Format::eR8G8B8A8Sint: return BlockParams{1, 1, 4};
    case vk::Format::eR8G8B8A8Srgb: return BlockParams{1, 1, 4};
    case vk::Format::eB8G8R8A8Unorm: return BlockParams{1, 1, 4};
    case vk::Format::eB8G8R8A8Snorm: return BlockParams{1, 1, 4};
    case vk::Format::eB8G8R8A8Uscaled: return BlockParams{1, 1, 4};
    case vk::Format::eB8G8R8A8Sscaled: return BlockParams{1, 1, 4};
    case vk::Format::eB8G8R8A8Uint: return BlockParams{1, 1, 4};
    case vk::Format::eB8G8R8A8Sint: return BlockParams{1, 1, 4};
    case vk::Format::eB8G8R8A8Srgb: return BlockParams{1, 1, 4};
    case vk::Format::eA8B8G8R8UnormPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA8B8G8R8SnormPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA8B8G8R8UscaledPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA8B8G8R8SscaledPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA8B8G8R8UintPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA8B8G8R8SintPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA8B8G8R8SrgbPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2R10G10B10UnormPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2R10G10B10SnormPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2R10G10B10UscaledPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2R10G10B10SscaledPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2R10G10B10UintPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2R10G10B10SintPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2B10G10R10UnormPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2B10G10R10SnormPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2B10G10R10UscaledPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2B10G10R10SscaledPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2B10G10R10UintPack32: return BlockParams{1, 1, 4};
    case vk::Format::eA2B10G10R10SintPack32: return BlockParams{1, 1, 4};
    case vk::Format::eR16Unorm: return BlockParams{1, 1, 2};
    case vk::Format::eR16Snorm: return BlockParams{1, 1, 2};
    case vk::Format::eR16Uscaled: return BlockParams{1, 1, 2};
    case vk::Format::eR16Sscaled: return BlockParams{1, 1, 2};
    case vk::Format::eR16Uint: return BlockParams{1, 1, 2};
    case vk::Format::eR16Sint: return BlockParams{1, 1, 2};
    case vk::Format::eR16Sfloat: return BlockParams{1, 1, 2};
    case vk::Format::eR16G16Unorm: return BlockParams{1, 1, 4};
    case vk::Format::eR16G16Snorm: return BlockParams{1, 1, 4};
    case vk::Format::eR16G16Uscaled: return BlockParams{1, 1, 4};
    case vk::Format::eR16G16Sscaled: return BlockParams{1, 1, 4};
    case vk::Format::eR16G16Uint: return BlockParams{1, 1, 4};
    case vk::Format::eR16G16Sint: return BlockParams{1, 1, 4};
    case vk::Format::eR16G16Sfloat: return BlockParams{1, 1, 4};
    case vk::Format::eR16G16B16Unorm: return BlockParams{1, 1, 6};
    case vk::Format::eR16G16B16Snorm: return BlockParams{1, 1, 6};
    case vk::Format::eR16G16B16Uscaled: return BlockParams{1, 1, 6};
    case vk::Format::eR16G16B16Sscaled: return BlockParams{1, 1, 6};
    case vk::Format::eR16G16B16Uint: return BlockParams{1, 1, 6};
    case vk::Format::eR16G16B16Sint: return BlockParams{1, 1, 6};
    case vk::Format::eR16G16B16Sfloat: return BlockParams{1, 1, 6};
    case vk::Format::eR16G16B16A16Unorm: return BlockParams{1, 1, 8};
    case vk::Format::eR16G16B16A16Snorm: return BlockParams{1, 1, 8};
    case vk::Format::eR16G16B16A16Uscaled: return BlockParams{1, 1, 8};
    case vk::Format::eR16G16B16A16Sscaled: return BlockParams{1, 1, 8};
    case vk::Format::eR16G16B16A16Uint: return BlockParams{1, 1, 8};
    case vk::Format::eR16G16B16A16Sint: return BlockParams{1, 1, 8};
    case vk::Format::eR16G16B16A16Sfloat: return BlockParams{1, 1, 8};
    case vk::Format::eR32Uint: return BlockParams{1, 1, 4};
    case vk::Format::eR32Sint: return BlockParams{1, 1, 4};
    case vk::Format::eR32Sfloat: return BlockParams{1, 1, 4};
    case vk::Format::eR32G32Uint: return BlockParams{1, 1, 8};
    case vk::Format::eR32G32Sint: return BlockParams{1, 1, 8};
    case vk::Format::eR32G32Sfloat: return BlockParams{1, 1, 8};
    case vk::Format::eR32G32B32Uint: return BlockParams{1, 1, 12};
    case vk::Format::eR32G32B32Sint: return BlockParams{1, 1, 12};
    case vk::Format::eR32G32B32Sfloat: return BlockParams{1, 1, 12};
    case vk::Format::eR32G32B32A32Uint: return BlockParams{1, 1, 16};
    case vk::Format::eR32G32B32A32Sint: return BlockParams{1, 1, 16};
    case vk::Format::eR32G32B32A32Sfloat: return BlockParams{1, 1, 16};
    case vk::Format::eR64Uint: return BlockParams{1, 1, 8};
    case vk::Format::eR64Sint: return BlockParams{1, 1, 8};
    case vk::Format::eR64Sfloat: return BlockParams{1, 1, 8};
    case vk::Format::eR64G64Uint: return BlockParams{1, 1, 16};
    case vk::Format::eR64G64Sint: return BlockParams{1, 1, 16};
    case vk::Format::eR64G64Sfloat: return BlockParams{1, 1, 16};
    case vk::Format::eR64G64B64Uint: return BlockParams{1, 1, 24};
    case vk::Format::eR64G64B64Sint: return BlockParams{1, 1, 24};
    case vk::Format::eR64G64B64Sfloat: return BlockParams{1, 1, 24};
    case vk::Format::eR64G64B64A64Uint: return BlockParams{1, 1, 32};
    case vk::Format::eR64G64B64A64Sint: return BlockParams{1, 1, 32};
    case vk::Format::eR64G64B64A64Sfloat: return BlockParams{1, 1, 32};
    case vk::Format::eB10G11R11UfloatPack32: return BlockParams{1, 1, 4};
    case vk::Format::eE5B9G9R9UfloatPack32: return BlockParams{1, 1, 4};
    case vk::Format::eD16Unorm: return BlockParams{1, 1, 4};
    case vk::Format::eX8D24UnormPack32: return BlockParams{1, 1, 4};
    case vk::Format::eD32Sfloat: return BlockParams{1, 1, 4};
    case vk::Format::eS8Uint: return BlockParams{1, 1, 1};
    case vk::Format::eD16UnormS8Uint: return BlockParams{1, 1, 3};
    case vk::Format::eD24UnormS8Uint: return BlockParams{1, 1, 4};
    case vk::Format::eD32SfloatS8Uint: return BlockParams{0, 0, 0};
    case vk::Format::eBc1RgbUnormBlock: return BlockParams{4, 4, 8};
    case vk::Format::eBc1RgbSrgbBlock: return BlockParams{4, 4, 8};
    case vk::Format::eBc1RgbaUnormBlock: return BlockParams{4, 4, 8};
    case vk::Format::eBc1RgbaSrgbBlock: return BlockParams{4, 4, 8};
    case vk::Format::eBc2UnormBlock: return BlockParams{4, 4, 16};
    case vk::Format::eBc2SrgbBlock: return BlockParams{4, 4, 16};
    case vk::Format::eBc3UnormBlock: return BlockParams{4, 4, 16};
    case vk::Format::eBc3SrgbBlock: return BlockParams{4, 4, 16};
    case vk::Format::eBc4UnormBlock: return BlockParams{4, 4, 16};
    case vk::Format::eBc4SnormBlock: return BlockParams{4, 4, 16};
    case vk::Format::eBc5UnormBlock: return BlockParams{4, 4, 16};
    case vk::Format::eBc5SnormBlock: return BlockParams{4, 4, 16};
    case vk::Format::eBc6HUfloatBlock: return BlockParams{0, 0, 0};
    case vk::Format::eBc6HSfloatBlock: return BlockParams{0, 0, 0};
    case vk::Format::eBc7UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eBc7SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEtc2R8G8B8UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEtc2R8G8B8SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEtc2R8G8B8A1UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEtc2R8G8B8A1SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEtc2R8G8B8A8UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEtc2R8G8B8A8SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEacR11UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEacR11SnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEacR11G11UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eEacR11G11SnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc4x4UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc4x4SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc5x4UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc5x4SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc5x5UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc5x5SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc6x5UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc6x5SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc6x6UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc6x6SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc8x5UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc8x5SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc8x6UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc8x6SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc8x8UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc8x8SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc10x5UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc10x5SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc10x6UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc10x6SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc10x8UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc10x8SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc10x10UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc10x10SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc12x10UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc12x10SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc12x12UnormBlock: return BlockParams{0, 0, 0};
    case vk::Format::eAstc12x12SrgbBlock: return BlockParams{0, 0, 0};
    case vk::Format::ePvrtc12BppUnormBlockIMG: return BlockParams{0, 0, 0};
    case vk::Format::ePvrtc14BppUnormBlockIMG: return BlockParams{0, 0, 0};
    case vk::Format::ePvrtc22BppUnormBlockIMG: return BlockParams{0, 0, 0};
    case vk::Format::ePvrtc24BppUnormBlockIMG: return BlockParams{0, 0, 0};
    case vk::Format::ePvrtc12BppSrgbBlockIMG: return BlockParams{0, 0, 0};
    case vk::Format::ePvrtc14BppSrgbBlockIMG: return BlockParams{0, 0, 0};
    case vk::Format::ePvrtc22BppSrgbBlockIMG: return BlockParams{0, 0, 0};
    case vk::Format::ePvrtc24BppSrgbBlockIMG: return BlockParams{0, 0, 0};
  }
  return BlockParams{0, 0, 0};
}

/// Factory for instances.
class InstanceMaker {
public:
  InstanceMaker() {
  }

  /// Set the default layers and extensions.
  InstanceMaker &defaultLayers() {
#ifndef NDEBUG
	
#if defined(SYNC_VALIDATION_ONLY) && SYNC_VALIDATION_ONLY
	layer("VK_LAYER_KHRONOS_synchronization2");
#else
	layer("VK_LAYER_KHRONOS_validation");
#endif
	
	extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    
#ifdef VKU_SURFACE
	extension(VKU_SURFACE);
#endif
	extension(VK_KHR_SURFACE_EXTENSION_NAME);
	extension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    return *this;
  }

  /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
  InstanceMaker &layer(const char *layer) {
    layers_.push_back(layer);
    return *this;
  }

  /// Add an extension. eg. VK_EXT_DEBUG_UTILS_EXTENSION_NAME
  InstanceMaker &extension(const char * extension) {
    instance_extensions_.push_back(extension);
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
  vk::UniqueInstance createUnique() {

	  // common to both release and debug builds //
	  vk::UniqueInstance instance;

	  vk::InstanceCreateInfo instanceInfo{
		  {}, &app_info_, (uint32_t)layers_.size(),
		  layers_.data(), (uint32_t)instance_extensions_.size(),
		  instance_extensions_.data()
	  };

#ifndef NDEBUG
	  // *******Debug - enable extra validation here (performance warnings galore)
	  //vk::ValidationFeatureEnableEXT const enabledValidation[]{ /*vk::ValidationFeatureEnableEXT::eGpuAssisted, vk::ValidationFeatureEnableEXT::eGpuAssistedReserveBindingSlot,*/ vk::ValidationFeatureEnableEXT::eBestPractices};
	  //vk::ValidationFeaturesEXT enabledValidationFeatures(_countof(enabledValidation), enabledValidation);
	  
	  // pNext linked list chain start:
	  //enabledValidationFeatures.pNext = instanceInfo.pNext;
	  //instanceInfo.pNext = &enabledValidationFeatures;

#else // *******Release
	  // ensure validation is disabled on release builds //
	  vk::ValidationFeatureDisableEXT const disabledValidation[]{ vk::ValidationFeatureDisableEXT::eAll };
	  vk::ValidationFeaturesEXT disabledValidationFeatures({}, 0, _countof(disabledValidation), disabledValidation);

	  vk::ValidationCheckEXT const disableValidation[]{ vk::ValidationCheckEXT::eAll };
	  vk::ValidationFlagsEXT disabledValidationFlags{ _countof(disableValidation), disableValidation };

	  // pNext linked list chain start:
	  disabledValidationFeatures.pNext = instanceInfo.pNext;
	  disabledValidationFlags.pNext = &disabledValidationFeatures;
	  instanceInfo.pNext = &disabledValidationFlags;

#endif

	  // instance creation //
	  instance = vk::createInstanceUnique(instanceInfo).value;
	
	  volkLoadInstanceOnly(*instance); // volk

	return(instance);
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
  DeviceMaker() {
  }

  /// Set the default layers and extensions.
  DeviceMaker &defaultLayers() {
	  
#ifndef NDEBUG
		
#if defined(SYNC_VALIDATION_ONLY) && SYNC_VALIDATION_ONLY
	//layer("VK_LAYER_KHRONOS_synchronization2");
#else
	layer("VK_LAYER_LUNARG_standard_validation");
	layer("VK_LAYER_LUNARG_assistant_layer");
#endif
	
#endif
	extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    return *this;
  }

  /// Add a layer. eg. "VK_LAYER_LUNARG_standard_validation"
  DeviceMaker &layer(const char *layer) {
    layers_.push_back(layer);
    return *this;
  }

  /// Add an extension. eg. VK_EXT_DEBUG_UTILS_EXTENSION_NAME
  DeviceMaker &extension(const char *extension) {
    device_extensions_.push_back(extension);
    return *this;
  }

  /// Add one or more queues to the device from a certain family.
  //template< VkQueueGlobalPriorityEXT const global_priority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT >	// *bugfix - NVIDIA does not support this extension. It's not really needed - all queue priorities were the same anyway.
  DeviceMaker &queue(uint32_t const familyIndex, uint32_t const n = 1u) {
    queue_priorities_.emplace_back(n, 1.0f);

	vk::DeviceQueueCreateInfo new_queue(vk::DeviceQueueCreateFlags{},
										familyIndex, n,
										queue_priorities_.back().data());

	//if constexpr(VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT != global_priority) {	// only if not default global priority
	//
	//	static VkDeviceQueueGlobalPriorityCreateInfoEXT global_priority_ext;	// static life time required for deferred init from this function
																				// only unique because of the template specialization
	//	global_priority_ext.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT;
	//	global_priority_ext.globalPriority = global_priority;
	//	global_priority_ext.pNext = nullptr;

	//	new_queue.pNext = &global_priority_ext;
	//}
    qci_.emplace_back(new_queue);

    return *this;
  }

  /// Create a new logical device.
  vk::UniqueDevice createUnique(vk::PhysicalDevice physical_device, vk::PhysicalDeviceFeatures const& enabledFeatures, const void* const __restrict pNext = nullptr) {

	  vk::DeviceCreateInfo createInfo{
		{}, (uint32_t)qci_.size(), qci_.data(),
		(uint32_t)layers_.size(), layers_.data(),
		(uint32_t)device_extensions_.size(), device_extensions_.data(),
		&enabledFeatures };

	  createInfo.pNext = pNext;
	  
	  vk::UniqueDevice device = physical_device.createDeviceUnique(createInfo).value;

	  volkLoadDevice(*device); // volk

	  return(device);
  }
private:
  std::vector<const char *> layers_;
  std::vector<const char *> device_extensions_;
  std::vector<std::vector<float> > queue_priorities_;
  std::vector<vk::DeviceQueueCreateInfo> qci_;
  vk::ApplicationInfo app_info_;
};

#ifndef NDEBUG
class DebugCallback {
public:
  DebugCallback() {
  }

  DebugCallback(
    vk::Instance instance,
    vk::DebugUtilsMessageSeverityFlagsEXT const severityFlags =
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
  ) : instance_(instance) {
#ifndef NDEBUG
	  vk::DebugUtilsMessageTypeFlagsEXT const typeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
	  vk::DebugUtilsMessengerCreateFlagBitsEXT const flags{};

	  auto ci = vk::DebugUtilsMessengerCreateInfoEXT(flags, severityFlags, typeFlags, &debugCallback);

    auto vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)instance_.getProcAddr(
			"vkCreateDebugUtilsMessengerEXT");

	VkDebugUtilsMessengerEXT mess;
	vkCreateDebugUtilsMessengerEXT(
      instance_, &(const VkDebugUtilsMessengerCreateInfoEXT&)ci,
      nullptr, &mess
    );
    messenger_ = mess;
#endif
  }

  ~DebugCallback() {
    //reset(); // handled by messenger_ object at exit
  }

  void reset() {
    if (messenger_) {
      auto vkDestroyDebugUtilsMessengerEXT =
          (PFN_vkDestroyDebugUtilsMessengerEXT)instance_.getProcAddr(
              "vkDestroyDebugUtilsMessengerEXT");
	  vkDestroyDebugUtilsMessengerEXT(instance_, messenger_, nullptr);
	  messenger_ = vk::DebugUtilsMessengerEXT{};
    }
  }

  void acquireDeviceFunctionPointers( vk::Device const& device ) {

	  device_ = device;
	  
	  pfn_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)device.getProcAddr("vkSetDebugUtilsObjectNameEXT");
	  pfn_vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)device.getProcAddr("vkCmdInsertDebugUtilsLabelEXT");
  }
private:
	/*

	typedef VkBool32 (VKAPI_PTR *PFN_vkDebugUtilsMessengerCallbackEXT)(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
	void*                                            pUserData);

	*/
	// Messagner callback, outputing to console any errors or warnings.
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void *pUserData) {

	  static constexpr int32_t const granularity = 24 << 2;
	  static std::string szLast = "";
	  constinit static int32_t iReplicateCnt = granularity;
	 
 	  if (iReplicateCnt <= 0 || szLast.find(std::string(pCallbackData->pMessageIdName).substr(1, granularity>>1)) == std::string::npos) {
		  
		  vk::DebugUtilsMessageSeverityFlagsEXT const severity(messageSeverity);
		  vk::DebugUtilsMessageTypeFlagsEXT const types(messageTypes);

		  fmt::print(fg(fmt::color::orange), "\n ++++ ");
		  fmt::print("\n{:s}-{:s} : {:s} {:s}\n", vk::to_string(severity), vk::to_string(types), pCallbackData->pMessage, (iReplicateCnt<=0?"REPEATED":""));
		  
		  uint32_t index(0);
		  uint32_t queueLabelCount(pCallbackData->queueLabelCount);
		  while (queueLabelCount--) {
			  std::string_view const label(pCallbackData->pQueueLabels[index].pLabelName ? pCallbackData->pQueueLabels[index].pLabelName : "null");
			  fmt::print("\n\t queue: {:s}", label);

			  ++index;
		  }

		  index = 0;
		  uint32_t cmdbufferLabelCount(pCallbackData->cmdBufLabelCount);
		  while (cmdbufferLabelCount--) {
			  std::string_view const label(pCallbackData->pCmdBufLabels[index].pLabelName ? pCallbackData->pCmdBufLabels[index].pLabelName : "null");
			  fmt::print("\n\t command buffer: {:s}", label);

			  ++index;
		  }

		  index = 0;
		  uint32_t objectLabelCount(pCallbackData->objectCount);
		  while (objectLabelCount--) {

			  vk::ObjectType const type((vk::ObjectType)pCallbackData->pObjects[index].objectType);

			  std::string_view const label(pCallbackData->pObjects[index].pObjectName ? pCallbackData->pObjects[index].pObjectName : "null");
			  fmt::print("\n\t {:s}: {:s}", vk::to_string(type), label);

			  ++index;
		  }

		  fmt::print(fg(fmt::color::orange), "\n ++++ \n");

		  szLast = pCallbackData->pMessageIdName;
		  iReplicateCnt = granularity;
	  }
	  else {
		  --iReplicateCnt;
	  }

#if defined(BREAK_ON_VALIDATION_ERROR)
#if BREAK_ON_VALIDATION_ERROR
		  DebugBreak();
		  quick_exit(1);
#endif
#endif

    return VK_FALSE;
  }
  vk::DebugUtilsMessengerEXT messenger_;
  vk::Instance instance_;
  
  public:
	  static inline vk::Device device_;
	  static inline PFN_vkSetDebugUtilsObjectNameEXT pfn_vkSetDebugUtilsObjectNameEXT = nullptr; 
	  static inline PFN_vkCmdInsertDebugUtilsLabelEXT pfn_vkCmdInsertDebugUtilsLabelEXT = nullptr;
};

// use vk::ObjectType::_xxx_
/*
enum class ObjectType
  {
	eUnknown = VK_OBJECT_TYPE_UNKNOWN,
	eInstance = VK_OBJECT_TYPE_INSTANCE,
	ePhysicalDevice = VK_OBJECT_TYPE_PHYSICAL_DEVICE,
	eDevice = VK_OBJECT_TYPE_DEVICE,
	eQueue = VK_OBJECT_TYPE_QUEUE,
	eSemaphore = VK_OBJECT_TYPE_SEMAPHORE,
	eCommandBuffer = VK_OBJECT_TYPE_COMMAND_BUFFER,
	eFence = VK_OBJECT_TYPE_FENCE,
	eDeviceMemory = VK_OBJECT_TYPE_DEVICE_MEMORY,
	eBuffer = VK_OBJECT_TYPE_BUFFER,
	eImage = VK_OBJECT_TYPE_IMAGE,
	eEvent = VK_OBJECT_TYPE_EVENT,
	eQueryPool = VK_OBJECT_TYPE_QUERY_POOL,
	eBufferView = VK_OBJECT_TYPE_BUFFER_VIEW,
	eImageView = VK_OBJECT_TYPE_IMAGE_VIEW,
	eShaderModule = VK_OBJECT_TYPE_SHADER_MODULE,
	ePipelineCache = VK_OBJECT_TYPE_PIPELINE_CACHE,
	ePipelineLayout = VK_OBJECT_TYPE_PIPELINE_LAYOUT,
	eRenderPass = VK_OBJECT_TYPE_RENDER_PASS,
	ePipeline = VK_OBJECT_TYPE_PIPELINE,
	eDescriptorSetLayout = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
	eSampler = VK_OBJECT_TYPE_SAMPLER,
	eDescriptorPool = VK_OBJECT_TYPE_DESCRIPTOR_POOL,
	eDescriptorSet = VK_OBJECT_TYPE_DESCRIPTOR_SET,
	eFramebuffer = VK_OBJECT_TYPE_FRAMEBUFFER,
	eCommandPool = VK_OBJECT_TYPE_COMMAND_POOL,
	eSamplerYcbcrConversion = VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION,
	eDescriptorUpdateTemplate = VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE,
	eSurfaceKHR = VK_OBJECT_TYPE_SURFACE_KHR,
	eSwapchainKHR = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
	eDisplayKHR = VK_OBJECT_TYPE_DISPLAY_KHR,
	eDisplayModeKHR = VK_OBJECT_TYPE_DISPLAY_MODE_KHR,
	eDebugReportCallbackEXT = VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT,
	eObjectTableNVX = VK_OBJECT_TYPE_OBJECT_TABLE_NVX,
	eIndirectCommandsLayoutNVX = VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NVX,
	eDebugUtilsMessengerEXT = VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT,
	eValidationCacheEXT = VK_OBJECT_TYPE_VALIDATION_CACHE_EXT,
	eAccelerationStructureNV = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV,
	ePerformanceConfigurationINTEL = VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL,
	eDescriptorUpdateTemplateKHR = VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR,
	eSamplerYcbcrConversionKHR = VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_KHR
  };
  */
#define VKU_SET_OBJECT_NAME(type, object, objectname) \
{ \
	vk::DebugUtilsObjectNameInfoEXT const obj{ \
		type, (uint64_t)(object), objectname \
	}; \
	vku::DebugCallback::pfn_vkSetDebugUtilsObjectNameEXT((VkDevice const)vku::DebugCallback::device_, ((VkDebugUtilsObjectNameInfoEXT const* const)&obj)); \
} 
#define VKU_SET_CMD_BUFFER_LABEL(cb, labelname) \
{ \
	vk::DebugUtilsLabelEXT const label{ labelname }; \
	vku::DebugCallback::pfn_vkCmdInsertDebugUtilsLabelEXT((VkCommandBuffer const)cb, ((VkDebugUtilsLabelEXT const* const)&label)); \
}
		
#else // NDEBUG

#define VKU_SET_OBJECT_NAME(type, object, objectname) { (void)type; (void)object; (void)objectname; }
#define VKU_SET_CMD_BUFFER_LABEL(cb, labelname) { (void)cb; (void)labelname; }

#endif

/// Factory for renderpasses.
/// example:
///     RenderpassMaker rpm;
///     rpm.subpassBegin(vk::PipelineBindPoint::eGraphics);
///     rpm.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal);
///    
///     rpm.attachmentDescription(attachmentDesc);
///     rpm.subpassDependency(dependency);
///     s.renderPass_ = rpm.createUnique(device);
// ********* Color Attachments should be grouped together, same with Input Attachments *********
class RenderpassMaker {
public:
  RenderpassMaker() {
  }

  /// Begin an attachment description.
  /// After this you can call attachment* many times
  void attachmentBegin(vk::Format format) {
    s.attachmentDescriptions.emplace_back(vk::AttachmentDescription{ {}, format });
  }

  void attachmentFlags(vk::AttachmentDescriptionFlags const value) { s.attachmentDescriptions.back().flags = value; };
  void attachmentFormat(vk::Format const value) { s.attachmentDescriptions.back().format = value; };
  void attachmentSamples(vk::SampleCountFlagBits const value) { s.attachmentDescriptions.back().samples = value; };
  void attachmentLoadOp(vk::AttachmentLoadOp const value) { s.attachmentDescriptions.back().loadOp = value; };
  void attachmentStoreOp(vk::AttachmentStoreOp const value) { s.attachmentDescriptions.back().storeOp = value; };
  void attachmentStencilLoadOp(vk::AttachmentLoadOp const value) { s.attachmentDescriptions.back().stencilLoadOp = value; };
  void attachmentStencilStoreOp(vk::AttachmentStoreOp const value) { s.attachmentDescriptions.back().stencilStoreOp = value; };
  void attachmentInitialLayout(vk::ImageLayout const value) { s.attachmentDescriptions.back().initialLayout = value; };
  void attachmentFinalLayout(vk::ImageLayout const value) { s.attachmentDescriptions.back().finalLayout = value; };

  /// Start a subpass description.
  /// After this you can can call subpassColorAttachment many times
  /// and subpassDepthStencilAttachment once.
  void subpassBegin(vk::PipelineBindPoint const bp) {
    vk::SubpassDescription desc{};
    desc.pipelineBindPoint = bp;
    s.subpassDescriptions.emplace_back(desc);
  }

  void subpassColorAttachment(vk::ImageLayout const layout, uint32_t const attachment) {
    vk::SubpassDescription &subpass = s.subpassDescriptions.back();
    auto * const p = getAttachmentReference();
    p->layout = layout;
    p->attachment = attachment;
    if (0 == subpass.colorAttachmentCount) {
      subpass.pColorAttachments = p;
    }
    ++subpass.colorAttachmentCount;
  }

  void subpassInputAttachment(vk::ImageLayout const layout, uint32_t const attachment) {
	  vk::SubpassDescription& subpass = s.subpassDescriptions.back();
	  auto* const p = getAttachmentReference();
	  p->layout = layout;
	  p->attachment = attachment;
	  if (0 == subpass.inputAttachmentCount) {
		  subpass.pInputAttachments = p;
	  }
	  ++subpass.inputAttachmentCount;
  }

  void subpassDepthStencilAttachment(vk::ImageLayout const layout, uint32_t const attachment) {
    vk::SubpassDescription &subpass = s.subpassDescriptions.back();
    auto * const p = getAttachmentReference();
    p->layout = layout;
    p->attachment = attachment;
    subpass.pDepthStencilAttachment = p;
  }

  void subpassResolveSkipAttachment() {
	  vk::SubpassDescription& subpass = s.subpassDescriptions.back();
	  auto* const p = getAttachmentReference();
	  p->layout = vk::ImageLayout::eUndefined;
	  p->attachment = VK_ATTACHMENT_UNUSED;
	  if (nullptr == subpass.pResolveAttachments) {
		  subpass.pResolveAttachments = p;
	  }
  }

  void subpassResolveAttachment(vk::ImageLayout const layout, uint32_t const attachment) {
	  vk::SubpassDescription& subpass = s.subpassDescriptions.back();
	  auto* const p = getAttachmentReference();
	  p->layout = layout;
	  p->attachment = attachment;
	  if (nullptr == subpass.pResolveAttachments) {
		  subpass.pResolveAttachments = p;
	  }
  }

  void subpassPreserveAttachment(uint32_t attachment) {
	  vk::SubpassDescription& subpass = s.subpassDescriptions.back();
	  auto* const p = getPreserveAttachmentReference();
	  *p = attachment;
	  if (0 == subpass.preserveAttachmentCount) {
		  subpass.pPreserveAttachments = p;
	  }
	  ++subpass.preserveAttachmentCount;
  }

  vk::UniqueRenderPass createUnique(const vk::Device &device) const {
    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.attachmentCount = (uint32_t)s.attachmentDescriptions.size();
    renderPassInfo.pAttachments = s.attachmentDescriptions.data();
    renderPassInfo.subpassCount = (uint32_t)s.subpassDescriptions.size();
    renderPassInfo.pSubpasses = s.subpassDescriptions.data();
    renderPassInfo.dependencyCount = (uint32_t)s.subpassDependencies.size();
    renderPassInfo.pDependencies = s.subpassDependencies.data();
    return device.createRenderPassUnique(renderPassInfo).value;
  }

  void dependencyBegin(uint32_t srcSubpass, uint32_t dstSubpass) {
    vk::SubpassDependency desc{};
    desc.srcSubpass = srcSubpass;
    desc.dstSubpass = dstSubpass;
    s.subpassDependencies.emplace_back(desc);
  }

  void dependencySrcSubpass(uint32_t const value) { s.subpassDependencies.back().srcSubpass = value; };
  void dependencyDstSubpass(uint32_t const value) { s.subpassDependencies.back().dstSubpass = value; };
  void dependencySrcStageMask(vk::PipelineStageFlags const value) { s.subpassDependencies.back().srcStageMask = value; };
  void dependencyDstStageMask(vk::PipelineStageFlags const value) { s.subpassDependencies.back().dstStageMask = value; };
  void dependencySrcAccessMask(vk::AccessFlags const value) { s.subpassDependencies.back().srcAccessMask = value; };
  void dependencyDstAccessMask(vk::AccessFlags const value) { s.subpassDependencies.back().dstAccessMask = value; };
  void dependencyDependencyFlags(vk::DependencyFlags const value) { s.subpassDependencies.back().dependencyFlags = value; };
private:
  constexpr static int const max_refs = 16;

  vk::AttachmentReference *getAttachmentReference() {
    return (s.num_refs < max_refs) ? &s.attachmentReferences[s.num_refs++] : nullptr;
  }
  uint32_t* getPreserveAttachmentReference() {
	  return (s.num_preserve_refs < (max_refs >> 1)) ? &s.preserveReferences[s.num_preserve_refs++] : nullptr;
  }

  struct State {
    std::vector<vk::AttachmentDescription> attachmentDescriptions;
    std::vector<vk::SubpassDescription> subpassDescriptions;
    std::vector<vk::SubpassDependency> subpassDependencies;
    std::array<vk::AttachmentReference, max_refs> attachmentReferences;
	std::array<uint32_t, (max_refs>>1)> preserveReferences;
	int num_refs = 0, num_preserve_refs = 0;
    bool ok_ = false;
  };

  State s;
};

class SpecializationConstant {
public:
	SpecializationConstant()
		: value{}, constant_id{}
	{}

	template<typename... Args>
	explicit SpecializationConstant(uint32_t const constant_id_, Args&&... args)
		: value(std::forward<Args>(args)...), constant_id(constant_id_)
	{}

	std::variant<int, float> const  value;
	uint32_t const					constant_id;
};
/// Class for building shader modules and extracting metadata from shaders.
class ShaderModule {
public:
  ShaderModule() {
  }

  /// Construct a shader module from a file
  ShaderModule(const vk::Device &device, std::wstring_view const filename, std::optional< std::vector< SpecializationConstant > const > constants_ = std::nullopt) {
	  {
		  auto file = std::ifstream(filename.data(), std::ios::binary | std::ios::in);
		  if (file.bad()) {
			  file.close();
			  return;
		  }

		  file.seekg(0, std::ios::end);
		  int const length = (int)file.tellg();

		  std::vector<uint32_t> opcodes;
		  opcodes.reserve((size_t)(length / 4));
		  opcodes.resize((size_t)(length / 4));
		  file.seekg(0, std::ios::beg);
		  file.read((char*)opcodes.data(), opcodes.size() * 4);

		  vk::ShaderModuleCreateInfo ci;
		  ci.codeSize = opcodes.size() * 4;
		  ci.pCode = opcodes.data();
		  s.module_ = device.createShaderModuleUnique(ci).value;

		  std::string const shaderFile(stringconv::ws2s(filename.substr(filename.find_last_of('/') + 1, filename.size())));
		
		  VKU_SET_OBJECT_NAME(vk::ObjectType::eShaderModule, (VkShaderModule)(*s.module_), shaderFile.c_str());
		
		  file.close();
	  }
	if (constants_ && !constants_->empty()) {

		std::vector< SpecializationConstant > const& constants(*constants_);

		// get total size needed for value buffer
		{
			size_t total_value_size(0);
			for (auto const& constant : constants) {

				std::visit([&total_value_size, &constant](auto&& arg) {

					total_value_size += sizeof(arg);

				}, constant.value);
			}
			// allocate memory for the data buffer containing values
			special.info.dataSize = total_value_size;
			special.values = new std::byte[special.info.dataSize];
		}

		// set num of entries
		special.info.mapEntryCount = (uint32_t)constants.size();

		// build series of constant map entries 
		// build data buffer containg values
		size_t current_buffer_size(0);

		for (auto const& constant : constants) {

			std::visit([this, &current_buffer_size, &constant](auto&& arg) {

				size_t const value_type_size(sizeof(arg));
				
				// map entry
				special.constant_descs.emplace_back(vk::SpecializationMapEntry(constant.constant_id, (uint32_t)current_buffer_size, value_type_size));
				
				// value 
				memcpy_s(&(special.values + current_buffer_size)[0], special.info.dataSize, &arg, value_type_size);
				
				current_buffer_size += value_type_size;

			}, constant.value);
		}

		// all of this memory is resident until shader module is dtor
		// required as this data isn't used until pipeline is actually created 

		// point to specialization constant map entries
		special.info.pMapEntries = special.constant_descs.data();

		// point to specialization constant value buffer
		special.info.pData = special.values;

		// set specialization constants active (reference to the memory containg the "special.info")
		// in PipelineMaker (vk::PipelineShaderStageCreateInfo) will set a reference accordingly
		special.hasSpecialization = true;
	}

    s.ok_ = true;
  }

  /// Construct a shader module from a memory
  template<class InIter>
  ShaderModule(const vk::Device &device, InIter begin, InIter end) {

	std::vector<uint32_t> opcodes;
    opcodes.assign(begin, end);
    vk::ShaderModuleCreateInfo ci;
    ci.codeSize = opcodes.size() * 4;
    ci.pCode = opcodes.data();
    s.module_ = device.createShaderModuleUnique(ci);

    s.ok_ = true;
  }

  bool const ok() const { return s.ok_; }
  VkShaderModule const shadermodule() const { return(*s.module_); }

  bool const hasSpecialization() const { return(special.hasSpecialization); }
  vk::SpecializationInfo const* const specialization() const { return(&special.info); }

private:
  struct State {
    vk::UniqueShaderModule module_;
    bool ok_ = false;
  };

  struct Specialization {
	  std::byte*							  values;
	  std::vector<vk::SpecializationMapEntry> constant_descs;
	  vk::SpecializationInfo				  info;
	  bool									  hasSpecialization;

	  Specialization()
		  : values(nullptr), hasSpecialization(false)
	  {}
	  ~Specialization()
	  {
		  if (values) {
			  delete [] values;
			  values = nullptr;
		  }
	  }
  };

  State s;
  Specialization  special;
};

/// A class for building pipeline layouts.
/// Pipeline layouts describe the descriptor sets and push constants used by the shaders.
class PipelineLayoutMaker {
public:
  PipelineLayoutMaker() {}

  /// Create a self-deleting pipeline layout object.
  vk::UniquePipelineLayout createUnique(const vk::Device &device) const {
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        {}, (uint32_t)setLayouts_.size(),
        setLayouts_.data(), (uint32_t)pushConstantRanges_.size(),
        pushConstantRanges_.data()};
    return device.createPipelineLayoutUnique(pipelineLayoutInfo).value;
  }

  /// Add a descriptor set layout to the pipeline.
  void descriptorSetLayout(vk::DescriptorSetLayout layout) {
    setLayouts_.push_back(layout);
  }

  /// Add a push constant range to the pipeline.
  /// These describe the size and location of variables in the push constant area.
  void pushConstantRange(vk::ShaderStageFlags stageFlags_, uint32_t offset_, uint32_t size_) {
    pushConstantRanges_.emplace_back(stageFlags_, offset_, size_);
  }

private:
  std::vector<vk::DescriptorSetLayout> setLayouts_;
  std::vector<vk::PushConstantRange> pushConstantRanges_;
};

/// A class for building pipelines.
/// All the state of the pipeline is exposed through individual calls.
/// The pipeline encapsulates all the OpenGL state in a single object.
/// This includes vertex buffer layouts, blend operations, shaders, line width etc.
/// This class exposes all the values as individuals so a pipeline can be customised.
/// The default is to generate a working pipeline.
class PipelineMaker {
public:
  PipelineMaker(uint32_t const width, uint32_t const height) {
    inputAssemblyState_.topology = vk::PrimitiveTopology::eTriangleList;
	//viewport_ = vk::Viewport{ 0.0f, float(height), float(width), -float(height), 0.0f, 1.0f };  // reference on how to - for inversion to make Up Y+
	viewport_ = vk::Viewport{ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f }; // default Up is Y- for Vulkan

    scissor_ = vk::Rect2D{{0, 0}, {width, height}};
    rasterizationState_.lineWidth = 1.0f;

    depthStencilState_.depthTestEnable = VK_FALSE;
    depthStencilState_.depthWriteEnable = VK_FALSE;
    depthStencilState_.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depthStencilState_.depthBoundsTestEnable = VK_FALSE;
    depthStencilState_.back.failOp = vk::StencilOp::eKeep;
    depthStencilState_.back.passOp = vk::StencilOp::eKeep;
    depthStencilState_.back.compareOp = vk::CompareOp::eAlways;
    depthStencilState_.stencilTestEnable = VK_FALSE;
    depthStencilState_.front = depthStencilState_.back;
  }

  vk::Pipeline create(const vk::Device &device,
                      const vk::PipelineCache &pipelineCache,
                      const vk::PipelineLayout &pipelineLayout,
                      const vk::RenderPass &renderPass) {

    // Add default colour blend attachment if necessary.
	if (colorBlendAttachments_.empty()) {
      vk::PipelineColorBlendAttachmentState blend{};
      blend.blendEnable = VK_FALSE;
      blend.srcColorBlendFactor = vk::BlendFactor::eOne;
      blend.dstColorBlendFactor = vk::BlendFactor::eZero;
      blend.colorBlendOp = vk::BlendOp::eAdd;
      blend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
      blend.dstAlphaBlendFactor = vk::BlendFactor::eZero;
      blend.alphaBlendOp = vk::BlendOp::eAdd;
      typedef vk::ColorComponentFlagBits ccbf;
      blend.colorWriteMask = ccbf::eR|ccbf::eG|ccbf::eB|ccbf::eA;
      colorBlendAttachments_.emplace_back(blend);
    }

    auto count = (uint32_t)colorBlendAttachments_.size();
    colorBlendState_.attachmentCount = count;
    colorBlendState_.pAttachments = count ? colorBlendAttachments_.data() : nullptr;

    vk::PipelineViewportStateCreateInfo viewportState{
        {}, 1, &viewport_, 1, &scissor_};

    vk::PipelineVertexInputStateCreateInfo vertexInputState;
    vertexInputState.vertexAttributeDescriptionCount = (uint32_t)vertexAttributeDescriptions_.size();
    vertexInputState.pVertexAttributeDescriptions = vertexAttributeDescriptions_.data();
    vertexInputState.vertexBindingDescriptionCount = (uint32_t)vertexBindingDescriptions_.size();
    vertexInputState.pVertexBindingDescriptions = vertexBindingDescriptions_.data();

    vk::PipelineDynamicStateCreateInfo dynState{{}, (uint32_t)dynamicState_.size(), dynamicState_.data()};

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.stageCount = (uint32_t)modules_.size();
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

    return( std::move(device.createGraphicsPipeline(pipelineCache, pipelineInfo).value) );
  }

  /// Add a shader module to the pipeline.
  void shader(vk::ShaderStageFlagBits stage, vku::ShaderModule const& __restrict shader) {
    vk::PipelineShaderStageCreateInfo info{};
    info.module = shader.shadermodule();
    info.pName = "main";  // required to always be main - limitation of glsl spec they did it on purpose
    info.stage = stage;

	// if specialization constants were defined for the shader module, use them
	if (shader.hasSpecialization()) {
		info.pSpecializationInfo = shader.specialization();
	}

    modules_.emplace_back(info);
  }
  // mostly for live shader, index is in order the shader stages were added
  void replace_shader(uint32_t const index, vk::ShaderStageFlagBits stage, vku::ShaderModule& shader) {
	  vk::PipelineShaderStageCreateInfo info{};
	  info.module = shader.shadermodule();
	  info.pName = "main";
	  info.stage = stage;

	  // if specialization constants were defined for the shader module, use them
	  if (shader.hasSpecialization()) {
		  info.pSpecializationInfo = shader.specialization();
	  }

#ifndef NDEBUG
	  assert_print(modules_[index].stage == info.stage, "FAIL liveshader : mismatch on shader stages, replace shader with wrong stage.");
#endif
	  modules_[index] = info;
  }

  /// Add a blend state to the pipeline for one colour attachment.
  /// If you don't do this, a default is used.
  void colorBlend(const vk::PipelineColorBlendAttachmentState &state) {
    colorBlendAttachments_.emplace_back(state);
  }

  void subPass(uint32_t subpass) {
    subpass_ = subpass;
  }

  /// Begin setting colour blend value
  /// If you don't do this, a default is used.
  /// Follow this with blendEnable() blendSrcColorBlendFactor() etc.
  /// Default is a opaque.
  void blendBegin(vk::Bool32 enable) {
    colorBlendAttachments_.emplace_back();
    auto &blend = colorBlendAttachments_.back();
    blend.blendEnable = enable;
    blend.srcColorBlendFactor = vk::BlendFactor::eOne;
    blend.dstColorBlendFactor = vk::BlendFactor::eZero;
    blend.colorBlendOp = vk::BlendOp::eAdd;
    blend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    blend.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    blend.alphaBlendOp = vk::BlendOp::eAdd;
    typedef vk::ColorComponentFlagBits ccbf;
    blend.colorWriteMask = ccbf::eR|ccbf::eG|ccbf::eB|ccbf::eA;
  }

  /// Enable or disable blending (called after blendBegin())
  void blendEnable(vk::Bool32 value) { colorBlendAttachments_.back().blendEnable = value; }

  /// Source colour blend factor (called after blendBegin())
  void blendSrcColorBlendFactor(vk::BlendFactor value) { colorBlendAttachments_.back().srcColorBlendFactor = value; }

  /// Destination colour blend factor (called after blendBegin())
  void blendDstColorBlendFactor(vk::BlendFactor value) { colorBlendAttachments_.back().dstColorBlendFactor = value; }

  /// Blend operation (called after blendBegin())
  void blendColorBlendOp(vk::BlendOp value) { colorBlendAttachments_.back().colorBlendOp = value; }

  /// Source alpha blend factor (called after blendBegin())
  void blendSrcAlphaBlendFactor(vk::BlendFactor value) { colorBlendAttachments_.back().srcAlphaBlendFactor = value; }

  /// Destination alpha blend factor (called after blendBegin())
  void blendDstAlphaBlendFactor(vk::BlendFactor value) { colorBlendAttachments_.back().dstAlphaBlendFactor = value; }

  /// Alpha operation (called after blendBegin())
  void blendAlphaBlendOp(vk::BlendOp value) { colorBlendAttachments_.back().alphaBlendOp = value; }

  /// Colour write mask (called after blendBegin())
  void blendColorWriteMask(vk::ColorComponentFlags value) { colorBlendAttachments_.back().colorWriteMask = value; }

  /// Add a vertex attribute to the pipeline.
  void vertexAttribute(uint32_t location_, uint32_t binding_, vk::Format format_, uint32_t offset_) {
    vertexAttributeDescriptions_.push_back({location_, binding_, format_, offset_});
  }

  /// Add a vertex attribute to the pipeline.
  void vertexAttribute(const vk::VertexInputAttributeDescription &desc) {
    vertexAttributeDescriptions_.push_back(desc);
  }

  /// Add a vertex binding to the pipeline.
  /// Usually only one of these is needed to specify the stride.
  /// Vertices can also be delivered one per instance.
  void vertexBinding(uint32_t binding_, uint32_t stride_, vk::VertexInputRate inputRate_ = vk::VertexInputRate::eVertex) {
    vertexBindingDescriptions_.push_back({binding_, stride_, inputRate_});
  }

  /// Add a vertex binding to the pipeline.
  /// Usually only one of these is needed to specify the stride.
  /// Vertices can also be delivered one per instance.
  void vertexBinding(const vk::VertexInputBindingDescription &desc) {
    vertexBindingDescriptions_.push_back(desc);
  }

  /// Specify the topology of the pipeline.
  /// Usually this is a triangle list, but points and lines are possible too.
  PipelineMaker &topology( vk::PrimitiveTopology topology ) { inputAssemblyState_.topology = topology; return *this; }

  /// Enable or disable primitive restart.
  /// If using triangle strips, for example, this allows a special index value (0xffff or 0xffffffff) to start a new strip.
  PipelineMaker &primitiveRestartEnable( vk::Bool32 primitiveRestartEnable ) { inputAssemblyState_.primitiveRestartEnable = primitiveRestartEnable; return *this; }

  /// Set a whole new input assembly state.
  /// Note you can set individual values with their own call
  PipelineMaker &inputAssemblyState(const vk::PipelineInputAssemblyStateCreateInfo &value) { inputAssemblyState_ = value; return *this; }

  /// Set the viewport value.
  /// Usually there is only one viewport, but you can have multiple viewports active for rendering cubemaps or VR stereo pair
  PipelineMaker &viewport(const vk::Viewport &value) { viewport_ = value; return *this; }

  /// Set the scissor value.
  /// This defines the area that the fragment shaders can write to. For example, if you are rendering a portal or a mirror.
  PipelineMaker &scissor(const vk::Rect2D &value) { scissor_ = value; return *this; }

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
  PipelineMaker &front(vk::StencilOpState value) { depthStencilState_.front = value; return *this; }
  PipelineMaker &back(vk::StencilOpState value) { depthStencilState_.back = value; return *this; }
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
  vk::Viewport viewport_;
  vk::Rect2D scissor_;
  vk::PipelineRasterizationStateCreateInfo rasterizationState_;
  vk::PipelineMultisampleStateCreateInfo multisampleState_;
  vk::PipelineDepthStencilStateCreateInfo depthStencilState_;
  vk::PipelineColorBlendStateCreateInfo colorBlendState_;
  std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments_;
  std::vector<vk::PipelineShaderStageCreateInfo> modules_;
  std::vector<vk::VertexInputAttributeDescription> vertexAttributeDescriptions_;
  std::vector<vk::VertexInputBindingDescription> vertexBindingDescriptions_;
  std::vector<vk::DynamicState> dynamicState_;
  uint32_t subpass_ = 0;
};

/// A class for building compute pipelines.
class ComputePipelineMaker {
	friend class Framework;
	constinit static inline bool fullsubgroups_supported = false;
public:
  ComputePipelineMaker() {
  }

  /// Add a shader module to the pipeline.
  void shader(vk::ShaderStageFlagBits stage, vku::ShaderModule const& __restrict shader,
                 const char *entryPoint = "main") {
    stage_.module = shader.shadermodule();
    stage_.pName = entryPoint;
    stage_.stage = stage;
	
	if (shader.hasSpecialization()) {
		stage_.pSpecializationInfo = shader.specialization();
	}
  }

  /// Set the compute shader module.
  ComputePipelineMaker & shadermodule(const vk::PipelineShaderStageCreateInfo &value) {
    stage_ = value;

	// if specialization constants were defined for the shader module, use them
	
    return *this;
  }

  /// Create a managed handle to a compute shader.
  vk::UniquePipeline createUnique(vk::Device device, const vk::PipelineCache &pipelineCache, const vk::PipelineLayout &pipelineLayout) {
    vk::ComputePipelineCreateInfo pipelineInfo{};

    pipelineInfo.stage = stage_;
	pipelineInfo.layout = pipelineLayout;
	
	vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT fullsubgroups_info(32); // both nvidia and amd now use 32. AMD used to use 64, NVIDIA always 32. So for best compatibility set the subgroup size to the new standard ( 32 ). Now it's consistent on AMD & NVIDIA.
	if (fullsubgroups_supported) {
		stage_.flags = vk::PipelineShaderStageCreateFlagBits::eAllowVaryingSubgroupSizeEXT | vk::PipelineShaderStageCreateFlagBits::eRequireFullSubgroupsEXT; // only for compute shaders, require full subgroups
		stage_.pNext = &fullsubgroups_info;
	}

    return device.createComputePipelineUnique(pipelineCache, pipelineInfo).value;
  }
private:
  vk::PipelineShaderStageCreateInfo stage_;
};

/// A generic buffer that may be used as a vertex buffer, uniform buffer or other kinds of memory resident data.
/// Buffers require memory objects which represent GPU and CPU resources.
class GenericBuffer {
public:
	constexpr GenericBuffer() {} // every member is zero initialized (see below) - constexpr of the default ctor allows constinit optimization for private voxel data in cVoxelWorld.cpp file.
  

  GenericBuffer(vk::BufferUsageFlags const usage, vk::DeviceSize const size, vk::MemoryPropertyFlags const memflags = vk::MemoryPropertyFlagBits::eDeviceLocal, VmaMemoryUsage const gpu_usage = VMA_MEMORY_USAGE_UNKNOWN, uint32_t const mapped_access = (uint32_t)eMappedAccess::Disabled, bool const bDedicatedMemory = false, bool const bPersistantMapping = false) {
	  
	  vk::BufferCreateInfo ci{};
	  ci.size = maxsizebytes_ = size;
	  ci.usage = usage;
	  ci.sharingMode = vk::SharingMode::eExclusive;
	 
	  VmaAllocationCreateInfo allocInfo{};
	  allocInfo.usage = gpu_usage ? gpu_usage : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;  // default to gpu only if 0/unknown is passed in
	  allocInfo.requiredFlags = (VkMemoryPropertyFlags)memflags;
	  allocInfo.preferredFlags = allocInfo.requiredFlags;
	  allocInfo.flags = (bDedicatedMemory ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : (VmaAllocationCreateFlags)0)
					  | (bPersistantMapping ? VMA_ALLOCATION_CREATE_MAPPED_BIT : (VmaAllocationCreateFlags)0);

	  if ((uint32_t)eMappedAccess::Disabled == mapped_access) { // only if not provided
		
		  if (bPersistantMapping) { // only if known to be mapped
			  // default to sequential access (important if forgotten)
			  allocInfo.flags |= (uint32_t)eMappedAccess::Sequential;
		  }
	  }
	  else {

		  allocInfo.flags |= mapped_access;
	  }
	
	  vmaCreateBuffer(vma_, (VkBufferCreateInfo const* const)&ci, &allocInfo, (VkBuffer*)&buffer_, &allocation_, &mem_);
  }

  // for clearing staging buffers
  __SAFE_BUF void clearLocal() const {
      void* const __restrict ptr(map());
	  memset(ptr, 0, (size_t)maxsizebytes_);
      unmap();
      flush(maxsizebytes_);
  }

  // checks alignment of source, assumes alignment of dst is same or greater
  template<bool const bClear, typename T, size_t const alignment = alignof(T)>
  __SAFE_BUF void updateLocal(T const* const __restrict src, vk::DeviceSize const size) const {
	  T* const __restrict ptr( static_cast<T* const __restrict>(map()) );

	  vk::DeviceSize flush_size(size);
	
		if constexpr (bClear) {

			if constexpr (alignment >= 16) {
				if (maxsizebytes_ > 4096) {
					___memset_threaded<alignment>(ptr, 0, (size_t)maxsizebytes_);		// alignment is known
				}
				else {
					memset(ptr, 0, (size_t)maxsizebytes_);							
				}
			}
			else {
				memset(ptr, 0, (size_t)maxsizebytes_);			// alignment is unknown, can only assume minimum 16 byte alignment
			}
			flush_size = maxsizebytes_;
		}
		if constexpr (alignment >= 16) {
			if (size > 4096) {
				___memcpy_threaded<alignment>(ptr, src, (size_t)size);				// alignment is known
			}
			else {
				memcpy(ptr, src, (size_t)size);										
			}
		}
		else {
			memcpy(ptr, src, (size_t)size);									// alignment is unknown, can only assume minimum 16 byte alignment
		}

	  unmap();
	  flush(flush_size);
  }

  /// For a host visible buffer, copy memory to the buffer object.
  template<typename T, size_t const alignment = alignof(T)>
  __SAFE_BUF void updateLocal(T const* const __restrict value, vk::DeviceSize const size) {

	  if (size != maxsizebytes_) {
		  updateLocal<(true), T>(value, size);
	  }
	  else {
		  updateLocal<(false), T>(value, size);
	  }

	  bActiveDelta = (size != activesizebytes_);
	  activesizebytes_ = size;
  }
  void createAsGPUBuffer(vk::Device const device, vk::CommandPool const commandPool, vk::Queue const queue, vk::DeviceSize const maxsize, vk::BufferUsageFlagBits const bits) // good for gpu->gpu copies, used to reset buffers shared_buffer & subgroup_layer_count_max
  {
	  if (maxsize == 0) return;
	  using buf = vk::BufferUsageFlagBits;
	  using pfb = vk::MemoryPropertyFlagBits;

	  if (0 == maxsizebytes()) { // only allocate once
		  *this = vku::GenericBuffer(bits | buf::eTransferDst, maxsize); // device local, gpu only buffer - here default params get a device allocated buffer (gpu), with *no* mapping capability - can be initialized or set at any time with a staging buffer and upload to this buffer only.
		  activesizebytes_ = maxsizebytes();

		  // upload temporary staging buffer to clear
		  vku::GenericBuffer tmp(buf::eTransferSrc, maxsize, pfb::eHostCoherent | pfb::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, (uint32_t)vku::eMappedAccess::Sequential, false, false);
		  tmp.clearLocal();

		  vku::executeImmediately<false>(device, commandPool, queue, [&](vk::CommandBuffer cb) {
			  vk::BufferCopy bc{ 0, 0, maxsize };
			  cb.copyBuffer(tmp.buffer(), buffer_, bc);
		  });
	  }
  }
  
  void createAsCPUToGPUBuffer(vk::DeviceSize const maxsize, uint32_t const mapped_access = (uint32_t)vku::eMappedAccess::Disabled, bool const bDedicatedMemory = false, bool const bPersistantMapping = false)
  {
	  if (maxsize == 0) return;
	  using buf = vk::BufferUsageFlagBits;
	  using pfb = vk::MemoryPropertyFlagBits;

	  if (0 == maxsizebytes()) { // only allocate once
		  *this = vku::GenericBuffer(buf::eTransferSrc, maxsize, pfb::eHostCoherent | pfb::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, mapped_access, bDedicatedMemory, bPersistantMapping);
		  activesizebytes_ = maxsizebytes();
		  clearLocal();
	  }
  }
  void createAsStagingBuffer(vk::DeviceSize const maxsize, uint32_t const mapped_access = (uint32_t)vku::eMappedAccess::Disabled, bool const bDedicatedMemory = false, bool const bPersistantMapping = false)
  {
	  if (maxsize == 0) return;
	  using buf = vk::BufferUsageFlagBits;
	  using pfb = vk::MemoryPropertyFlagBits;

	  if (0 == maxsizebytes()) { // only allocate once
		  *this = vku::GenericBuffer(buf::eTransferSrc, maxsize, pfb::eHostCoherent | pfb::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, mapped_access, bDedicatedMemory, bPersistantMapping);
		  activesizebytes_ = maxsizebytes();
		  clearLocal();
	  }
  }
  // intended for use with frame work's dynamic command buffer
  // that gets queued up before the static command buffer
  template<typename T>
  void uploadDeferred(vk::CommandBuffer& __restrict cb, vku::GenericBuffer& __restrict stagingBuffer, const T * __restrict value, vk::DeviceSize const size, vk::DeviceSize const maxsize) {
	  if (size == 0) return;
	  using buf = vk::BufferUsageFlagBits;
	  using pfb = vk::MemoryPropertyFlagBits;

	  if (0 == stagingBuffer.maxsizebytes()) { // only allocate once
		  stagingBuffer = vku::GenericBuffer(buf::eTransferSrc, maxsize, pfb::eHostCoherent | pfb::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, vku::eMappedAccess::Sequential, false, true); // *bugfix - hidden options not exposed thru this path, default to persistant mapping.
	  }
	  stagingBuffer.updateLocal(value, size);
	  bActiveDelta = (activesizebytes_ != size);
	  activesizebytes_ = size;

	  vk::BufferCopy bc{ 0, 0, size };
	  cb.copyBuffer(stagingBuffer.buffer(), buffer_, bc);
  }
  // this is for a staging buffer that has already been updated(mappped/unmapped) and has updated the active size
  void setActiveSizeBytes(vk::DeviceSize const size)
  {
	  bActiveDelta = (activesizebytes_ != size);
	  activesizebytes_ = size;
  }
  // fast path, **no clear done** intended for usage with map/unmap of staging buffers
  void uploadDeferred(vk::CommandBuffer& __restrict cb, vku::GenericBuffer const& __restrict stagingBuffer) {

	  vk::DeviceSize const size(stagingBuffer.activesizebytes());

	  bActiveDelta = (activesizebytes_ != size);
	  activesizebytes_ = size;

	  if (0 == size)
		  return;

	  vk::BufferCopy const bc{ 0, 0, activesizebytes_ };
	  cb.copyBuffer(stagingBuffer.buffer(), buffer_, bc);
  }
  // fast path, **no clear done** intended for usage with map/unmap of staging buffers
  void uploadDeferred(vk::CommandBuffer& __restrict cb, vku::GenericBuffer const& __restrict stagingBuffer, vku::GenericBuffer const& __restrict stagingBufferAppended) {

	  vk::DeviceSize const firstactivesizebytes(stagingBuffer.activesizebytes()), secondactivesizebytes(stagingBufferAppended.activesizebytes());
	  vk::DeviceSize const size(firstactivesizebytes + secondactivesizebytes);

	  bActiveDelta = (activesizebytes_ != size);
	  activesizebytes_ = size;

	  if (0 == size)
		  return;

	  {
		  vk::BufferCopy const bc{ 0, 0, firstactivesizebytes };
		  cb.copyBuffer(stagingBuffer.buffer(), buffer_, bc);
	  }

	  if (0 == secondactivesizebytes)
		  return;

	  {
		  vk::BufferCopy const bc{ 0, firstactivesizebytes, secondactivesizebytes };
		  cb.copyBuffer(stagingBufferAppended.buffer(), buffer_, bc);
	  }
  }
  /// For a purely device local buffer, copy memory to the buffer object immediately.
  /// Note that this will stall the pipeline!
  template<typename T = void>
  void upload(vk::Device device, vk::CommandPool commandPool, vk::Queue queue, const T * __restrict value, vk::DeviceSize const size) {
    if (size == 0) return;
    using buf = vk::BufferUsageFlagBits;
    using pfb = vk::MemoryPropertyFlagBits;
    auto tmp = vku::GenericBuffer(buf::eTransferSrc, size, pfb::eHostCoherent | pfb::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, eMappedAccess::Sequential);
	maxsizebytes_ = tmp.maxsizebytes_;

	tmp.updateLocal(value, size);
	
	bActiveDelta = (activesizebytes_ != size);
	activesizebytes_ = size;

    vku::executeImmediately<false>(device, commandPool, queue, [&](vk::CommandBuffer cb) {
      vk::BufferCopy bc{ 0, 0, size};
      cb.copyBuffer(tmp.buffer(), buffer_, bc);
    });
  }

  template<typename T>
  void upload(vk::Device device, vk::CommandPool commandPool, vk::Queue queue, const std::vector<T> &value) {
    upload<T>(device, commandPool, queue, value.data(), value.size() * sizeof(T));
  }

  template<typename T>
  void upload(vk::Device device, vk::CommandPool commandPool, vk::Queue queue, const T &value) {
    upload<T>(device, commandPool, queue, &value, sizeof(value));
  }

  template<typename T>
  void uploadDeferred(vk::Device const& __restrict device, vk::CommandBuffer& __restrict cb, vku::GenericBuffer& __restrict stagingBuffer, const std::vector<T, tbb::scalable_allocator<T> > & __restrict value, size_t const maxreservecount = 0) {
	  uploadDeferred<T>(device, cb, stagingBuffer, value.data(), value.size() * sizeof(T), (0 == maxreservecount ? value.size() : maxreservecount) * sizeof(T));
  }
  
  void barrier(vk::CommandBuffer const& __restrict cb, vk::PipelineStageFlags const srcStageMask, vk::PipelineStageFlags const dstStageMask, vk::DependencyFlags const dependencyFlags, vk::AccessFlags const srcAccessMask, vk::AccessFlags const dstAccessMask, uint32_t const srcQueueFamilyIndex, uint32_t const dstQueueFamilyIndex) const {
    vk::BufferMemoryBarrier bmb{srcAccessMask, dstAccessMask, srcQueueFamilyIndex, dstQueueFamilyIndex, buffer_, 0, maxsizebytes_ };
    cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, nullptr, bmb, nullptr);
  }
  // batched / multiple barriers
  template<size_t const buffer_count>
  static void barrier(std::array<vku::GenericBuffer const* const, buffer_count> const& __restrict buffers,
	  vk::CommandBuffer const& __restrict cb, vk::PipelineStageFlags const srcStageMask, vk::PipelineStageFlags const dstStageMask, vk::DependencyFlags const dependencyFlags, vk::AccessFlags const srcAccessMask, vk::AccessFlags const dstAccessMask, uint32_t const srcQueueFamilyIndex, uint32_t const dstQueueFamilyIndex) {
	  std::array<vk::BufferMemoryBarrier, buffer_count> bmbs;
	  for (uint32_t i = 0; i < buffer_count; ++i) {
		  bmbs[i] = vk::BufferMemoryBarrier{ srcAccessMask, dstAccessMask, srcQueueFamilyIndex, dstQueueFamilyIndex, buffers[i]->buffer_, 0, buffers[i]->maxsizebytes_ };
	  }
	   
	  cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, nullptr, bmbs, nullptr);
  }

  template<class Type, class Allocator>
  void updateLocal(const std::vector<Type, Allocator> &value) {
    updateLocal((void*)value.data(), vk::DeviceSize(value.size() * sizeof(Type)));
  }

  template<class Type>
  void updateLocal(const Type &value) {
    updateLocal((void*)&value, vk::DeviceSize(sizeof(Type)));
  }

  __SAFE_BUF void * const __restrict map() const {
	  
	  if (mem_.pMappedData)		// if persistantly mapped just return the pointer to memory
		  return(mem_.pMappedData);
	  
	  // size no longer used : https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/memory_mapping.html#memory_mapping_mapping_functions

	  void* __restrict mapped;
	  vmaMapMemory(vma_, allocation_, (void**)&mapped);

	  return(mapped);
  };
  __SAFE_BUF void unmap() const {

	  if (mem_.pMappedData)		// if persistantly mapped just return, don't do anything
		  return;

	  vmaUnmapMemory(vma_, allocation_); 
  };

// required after writing to memory (IF BUFFER is HOST COHERENT - flushing can be skipped)
  __SAFE_BUF void flush(vk::DeviceSize const bytes_flushed) const {
	  vmaFlushAllocation(vma_, allocation_, 0, bytes_flushed); // flushes memory only for this buffers memory
  }
  __SAFE_BUF void flush() const {
	  vmaFlushAllocation(vma_, allocation_, 0, maxsizebytes_); // flushes memory only for this buffers memory
  }

  // required before reading memory (IF BUFFER is HOST COHERENT - invalidation can be skipped)
  __SAFE_BUF void invalidate(vk::DeviceSize const bytes_invalidated) const {
	  vmaInvalidateAllocation(vma_, allocation_, 0, bytes_invalidated); // invalidates memory only for this buffers memory
  }
  __SAFE_BUF void invalidate() const {
	  vmaInvalidateAllocation(vma_, allocation_, 0, maxsizebytes_); // invalidates memory only for this buffers memory
  }

  void release()
  {
	  if (allocation_) {
		  vmaDestroyBuffer(vma_, buffer_, allocation_);
		  buffer_ = nullptr;
		  allocation_ = nullptr;
		  mem_ = {};
	  }
  }

  GenericBuffer(GenericBuffer&& relegate)
  {
	  buffer_ = std::move(relegate.buffer_);
	  allocation_ = std::move(relegate.allocation_);
	  mem_ = std::move(relegate.mem_);
	  activesizebytes_ = relegate.activesizebytes_;
	  maxsizebytes_ = relegate.maxsizebytes_;
	  bActiveDelta = relegate.bActiveDelta;

	  relegate.allocation_ = nullptr;
	  relegate.buffer_ = nullptr;
	  relegate.mem_ = {};
	  relegate.activesizebytes_ = 0;
	  relegate.maxsizebytes_ = 0;
	  relegate.bActiveDelta = false;
  }
  GenericBuffer& operator=(GenericBuffer&& relegate)
  {
	  buffer_ = std::move(relegate.buffer_);
	  allocation_ = std::move(relegate.allocation_);
	  mem_ = std::move(relegate.mem_);
	  activesizebytes_ = relegate.activesizebytes_;
	  maxsizebytes_ = relegate.maxsizebytes_;
	  bActiveDelta = relegate.bActiveDelta;

	  relegate.allocation_ = nullptr;
	  relegate.buffer_ = nullptr;
	  relegate.mem_ = {};
	  relegate.activesizebytes_ = 0;
	  relegate.maxsizebytes_ = 0;
	  relegate.bActiveDelta = false;

	  return(*this);
  }
  ~GenericBuffer()
  {
	  release();
  }

  vk::Buffer const& __restrict buffer() const { return(buffer_); }
  vk::DeviceSize const activesizebytes() const { return(activesizebytes_); }
  vk::DeviceSize const maxsizebytes() const { return(maxsizebytes_); }
  bool const isBufferActiveSizeDelta() const { return(bActiveDelta); }
private:
  vk::Buffer		buffer_{};
  VmaAllocation		allocation_{};
  VmaAllocationInfo mem_{};
  vk::DeviceSize activesizebytes_ = 0,
				 maxsizebytes_ = 0;
  bool			 bActiveDelta = false;

private:
	GenericBuffer(GenericBuffer const&) = delete;
	GenericBuffer& operator=(GenericBuffer const&) = delete;
};

/// This class is a specialisation of GenericBuffer for high performance vertex buffers on the GPU.
/// You must upload the contents before use.
class VertexBuffer : public GenericBuffer {
public:
  VertexBuffer() {
  }

  VertexBuffer(size_t const size, bool const bDedicatedMemory = false) : GenericBuffer(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, size, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, bDedicatedMemory) {
  }

  VertexBuffer(VertexBuffer&& relegate)
	  : GenericBuffer(std::forward<VertexBuffer&&>(relegate))
  {
  }
  VertexBuffer& operator=(VertexBuffer&& relegate)
  {
	  GenericBuffer::operator=(std::forward<VertexBuffer&&>(relegate));
	  return(*this);
  }
private:
	VertexBuffer(VertexBuffer const&) = delete;
	VertexBuffer& operator=(VertexBuffer const&) = delete;
};

struct VertexBufferPartition
{
	uint32_t active_vertex_count = 0, vertex_start_offset = 0;

	VertexBufferPartition() = default;
};

class DynamicVertexBuffer : public GenericBuffer {
public:
	DynamicVertexBuffer() : partition_(nullptr) {
	}

	DynamicVertexBuffer(size_t size, bool const bDedicatedMemory = false)
		: partition_(nullptr), GenericBuffer(
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, size, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, bDedicatedMemory) {
	}

	~DynamicVertexBuffer()
	{
		SAFE_DELETE_ARRAY(partition_);
	}

	DynamicVertexBuffer(DynamicVertexBuffer&& relegate)
		: GenericBuffer(std::forward<DynamicVertexBuffer&&>(relegate))
	{
	}
	DynamicVertexBuffer& operator=(DynamicVertexBuffer&& relegate)
	{
		GenericBuffer::operator=(std::forward<DynamicVertexBuffer&&>(relegate));

		partition_ = relegate.partition_;
		relegate.partition_ = nullptr;
		partition_count_ = relegate.partition_count_;
		relegate.partition_count_ = 0;

		return(*this);
	}

	// Total Size of buffer used converted to -> Vertices
	template<typename T>
	uint32_t const ActiveVertexCount() const { return((uint32_t const)(((size_t)activesizebytes()) / sizeof(T))); }

	uint32_t const										partition_count() const { return(partition_count_); }
	VertexBufferPartition* const __restrict& __restrict partitions() const { return(partition_); }

	void createPartitions(uint32_t const num_partitions) {

		partition_ = new VertexBufferPartition[num_partitions]{};
		partition_count_ = num_partitions;
	}
private:
	VertexBufferPartition* partition_;
	uint32_t			   partition_count_;

private:
	DynamicVertexBuffer(DynamicVertexBuffer const&) = delete;
	DynamicVertexBuffer& operator=(DynamicVertexBuffer const&) = delete;
};

/// This class is a specialisation of GenericBuffer for low performance vertex buffers on the host.
class HostVertexBuffer : public GenericBuffer {
public:
  HostVertexBuffer() {
  }

  template<class Type, class Allocator>
  HostVertexBuffer(const std::vector<Type, Allocator> &value) : GenericBuffer(vk::BufferUsageFlagBits::eVertexBuffer, value.size() * sizeof(Type), vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible) {
    updateLocal(value);
  }
};

/// This class is a specialisation of GenericBuffer for high performance index buffers.
/// You must upload the contents before use.
class IndexBuffer : public GenericBuffer {
public:
  IndexBuffer() {
  }

  IndexBuffer(vk::DeviceSize const size) : GenericBuffer(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, size, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
  }

  IndexBuffer(IndexBuffer&& relegate)
	  : GenericBuffer(std::forward<IndexBuffer&&>(relegate))
  {
  }
  IndexBuffer& operator=(IndexBuffer&& relegate)
  {
	  GenericBuffer::operator=(std::forward<IndexBuffer&&>(relegate));
	  return(*this);
  }
private:
	IndexBuffer(IndexBuffer const&) = delete;
	IndexBuffer& operator=(IndexBuffer const&) = delete;
};

class DynamicIndexBuffer : public GenericBuffer {
public:
	DynamicIndexBuffer() {
	}

	DynamicIndexBuffer(vk::DeviceSize const size)
		: GenericBuffer(
			vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, size, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
	}

	DynamicIndexBuffer(DynamicIndexBuffer&& relegate)
		: GenericBuffer(std::forward<DynamicIndexBuffer&&>(relegate))
	{
	}
	DynamicIndexBuffer& operator=(DynamicIndexBuffer&& relegate)
	{
		GenericBuffer::operator=(std::forward<DynamicIndexBuffer&&>(relegate));
		return(*this);
	}

private:
	DynamicIndexBuffer(DynamicIndexBuffer const&) = delete;
	DynamicIndexBuffer& operator=(DynamicIndexBuffer const&) = delete;
};

/// This class is a specialisation of GenericBuffer for low performance vertex buffers in CPU memory.
class HostIndexBuffer : public GenericBuffer {
public:
  HostIndexBuffer() {
  }

  template<class Type, class Allocator>
  HostIndexBuffer(const std::vector<Type, Allocator> &value) : GenericBuffer(vk::BufferUsageFlagBits::eIndexBuffer, value.size() * sizeof(Type), vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible) {
    updateLocal(value);
  }
};

/// This class is a specialisation of GenericBuffer for uniform buffers.
class UniformBuffer : public GenericBuffer {
public:
  UniformBuffer() {
  }

  /// Device local uniform buffer.
  UniformBuffer(size_t const size) : GenericBuffer(vk::BufferUsageFlagBits::eUniformBuffer|vk::BufferUsageFlagBits::eTransferDst, (vk::DeviceSize)size, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
  }

  UniformBuffer(UniformBuffer&& relegate)
	  : GenericBuffer(std::forward<UniformBuffer&&>(relegate))
  {
  }
  UniformBuffer& operator=(UniformBuffer&& relegate)
  {
	  GenericBuffer::operator=(std::forward<UniformBuffer&&>(relegate));
	  return(*this);
  }

private:
	UniformBuffer(UniformBuffer const&) = delete;
	UniformBuffer& operator=(UniformBuffer const&) = delete;
};

/// This class is a specialisation of GenericBuffer for uniform texel buffers.
class UniformTexelBuffer : public GenericBuffer {
public:
	UniformTexelBuffer() {
	}

	/// Device local uniform buffer.
	UniformTexelBuffer(vk::Device const& __restrict device, size_t const size, vk::Format const image_format, bool const bDedicatedMemory = false, bool const bPersistantMapping = false) : GenericBuffer(vk::BufferUsageFlagBits::eUniformTexelBuffer | vk::BufferUsageFlagBits::eTransferDst, (vk::DeviceSize)size, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, bDedicatedMemory, bPersistantMapping) {

		vk::BufferViewCreateInfo viewInfo{};
		viewInfo.buffer = buffer();
		viewInfo.range = maxsizebytes();
		viewInfo.format = image_format;

		bufferView_ = device.createBufferViewUnique(viewInfo).value;
	}

	UniformTexelBuffer(UniformTexelBuffer&& relegate)
		: GenericBuffer(std::forward<UniformTexelBuffer&&>(relegate))
	{
	}
	UniformTexelBuffer& operator=(UniformTexelBuffer&& relegate)
	{
		GenericBuffer::operator=(std::forward<UniformTexelBuffer&&>(relegate));
		return(*this);
	}

	vk::BufferView const bufferView() const { return(*bufferView_); }

private:
	vk::UniqueBufferView		bufferView_{};
private:
	UniformTexelBuffer(UniformTexelBuffer const&) = delete;
	UniformTexelBuffer& operator=(UniformTexelBuffer const&) = delete;

public:
	~UniformTexelBuffer()
	{
		bufferView_.release();
	}
};

class StorageBuffer : public GenericBuffer {
public:
	StorageBuffer() {
	}

    // additionalFlags : vk::BufferUsageFlagBits::eTransferDst, vk::BufferUsageFlagBits::eTransferSrc
    StorageBuffer(size_t const size, bool const bDedicatedMemory = false, vk::BufferUsageFlags const additionalFlags = {}) : GenericBuffer(vk::BufferUsageFlagBits::eStorageBuffer | additionalFlags, size, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, bDedicatedMemory) {
	}

	StorageBuffer(StorageBuffer&& relegate)
		: GenericBuffer(std::forward<StorageBuffer&&>(relegate))
	{
	}
	StorageBuffer& operator=(StorageBuffer&& relegate)
	{
		GenericBuffer::operator=(std::forward<StorageBuffer&&>(relegate));
		return(*this);
	}

private:
	StorageBuffer(StorageBuffer const&) = delete;
	StorageBuffer& operator=(StorageBuffer const&) = delete;
};

class HostStorageBuffer : public GenericBuffer {
public:
	HostStorageBuffer() {
	}

	template<class Type>
	HostStorageBuffer(const Type& value) : GenericBuffer(vk::BufferUsageFlagBits::eStorageBuffer, sizeof(Type), vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible) {
		updateLocal(value);
	}
	template<class Type, class Allocator>
	HostStorageBuffer(const std::vector<Type, Allocator>& value) : GenericBuffer(vk::BufferUsageFlagBits::eStorageBuffer, value.size() * sizeof(Type), vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible) {
		updateLocal(value);
	}
};

class IndirectBuffer : public GenericBuffer {
public:
	IndirectBuffer() {
	}

	IndirectBuffer(size_t const size, bool const bDedicatedMemory = false)
		: GenericBuffer(vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst, size, vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, bDedicatedMemory) {
	}

	IndirectBuffer(IndirectBuffer&& relegate)
		: GenericBuffer(std::forward<IndirectBuffer&&>(relegate))
	{
	}
	IndirectBuffer& operator=(IndirectBuffer&& relegate)
	{
		GenericBuffer::operator=(std::forward<IndirectBuffer&&>(relegate));
		return(*this);
	}

private:
	IndirectBuffer(IndirectBuffer const&) = delete;
	IndirectBuffer& operator=(IndirectBuffer const&) = delete;
};

/// Convenience class for updating descriptor sets (uniforms)
class DescriptorSetUpdater {
public:
  DescriptorSetUpdater(int const maxBuffers = MAX_NUM_STORAGE_BUFFERS, int const maxImages = MAX_NUM_IMAGES, int const maxBufferViews = MAX_NUM_BUFFER_VIEWS) {
    // we must pre-size these buffers as we take pointers to their members.
	bufferInfo_.reserve(maxBuffers); bufferInfo_.resize(maxBuffers);
	imageInfo_.reserve(maxImages); imageInfo_.resize(maxImages);
	bufferViews_.reserve(maxBufferViews); bufferViews_.resize(maxBufferViews);
  }

  /// Call this to begin a new descriptor set.
  void beginDescriptorSet(vk::DescriptorSet dstSet) {
    dstSet_ = dstSet;
  }

  /// Call this to begin a new set of images.
  void beginImages(uint32_t dstBinding, uint32_t dstArrayElement, vk::DescriptorType descriptorType) {
    vk::WriteDescriptorSet wdesc{};
    wdesc.dstSet = dstSet_;
    wdesc.dstBinding = dstBinding;
    wdesc.dstArrayElement = dstArrayElement;
    wdesc.descriptorCount = 0;
    wdesc.descriptorType = descriptorType;
    wdesc.pImageInfo = imageInfo_.data() + numImages_;
    descriptorWrites_.push_back(wdesc);
  }

  /// Call this to add a combined image sampler.
  void image(vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout) {
    if (!descriptorWrites_.empty() && numImages_ != imageInfo_.size() && descriptorWrites_.back().pImageInfo) {
      descriptorWrites_.back().descriptorCount++;
      imageInfo_[numImages_++] = vk::DescriptorImageInfo{sampler, imageView, imageLayout};
    } else {
#ifndef NDEBUG
		fmt::print(fg(fmt::color::red), "\n limit reached, cap of {:d} images\n", numImages_);
#endif
      ok_ = false;
    }
  }

  /// Call this to start defining buffers.
  void beginBuffers(uint32_t dstBinding, uint32_t dstArrayElement, vk::DescriptorType descriptorType) {
    vk::WriteDescriptorSet wdesc{};
    wdesc.dstSet = dstSet_;
    wdesc.dstBinding = dstBinding;
    wdesc.dstArrayElement = dstArrayElement;
    wdesc.descriptorCount = 0;
    wdesc.descriptorType = descriptorType;
    wdesc.pBufferInfo = bufferInfo_.data() + numBuffers_;
    descriptorWrites_.push_back(wdesc);
  }

  /// Call this to add a new buffer.
  void buffer(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range) {
    if (!descriptorWrites_.empty() && numBuffers_ != bufferInfo_.size() && descriptorWrites_.back().pBufferInfo) {
      descriptorWrites_.back().descriptorCount++;
      bufferInfo_[numBuffers_++] = vk::DescriptorBufferInfo{buffer, offset, range};
    } else {
#ifndef NDEBUG
		fmt::print(fg(fmt::color::red), "\n limit reached, cap of {:d} buffers\n", numBuffers_);
#endif
      ok_ = false;
    }
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
    if (!descriptorWrites_.empty() && numBufferViews_ != bufferViews_.size() && descriptorWrites_.back().pTexelBufferView) {
      descriptorWrites_.back().descriptorCount++;
      bufferViews_[numBufferViews_++] = view;
    } else {
#ifndef NDEBUG
		fmt::print(fg(fmt::color::red), "\n limit reached, cap of {:d} buffer views\n", numBufferViews_);
#endif
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
  bool ok() const { return ok_; }
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
  DescriptorSetLayoutMaker() {
  }

  void buffer(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, uint32_t descriptorCount) {
    s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
  }

  void image(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, uint32_t descriptorCount, vk::Sampler const* const __restrict immutableSamplers = nullptr) {
    s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, immutableSamplers);
  }

  void bufferView(uint32_t binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags, uint32_t descriptorCount) {
    s.bindings.emplace_back(binding, descriptorType, descriptorCount, stageFlags, nullptr);
  }

  /// Create a self-deleting descriptor set object.
  vk::UniqueDescriptorSetLayout createUnique(vk::Device device) const {
    
    return ( device.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo((vk::DescriptorSetLayoutCreateFlags const)0U, (uint32_t const)s.bindings.size(), s.bindings.data()) ).value );
  }

private:
  struct State {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
  };

  State s;
};

/// A factory class for descriptor sets (A set of uniform bindings)
class DescriptorSetMaker {
public:
  // Construct a new, empty DescriptorSetMaker.
  DescriptorSetMaker() {
  }

  /// Add another layout describing a descriptor set.
  void layout(vk::DescriptorSetLayout layout) {
    s.layouts.emplace_back(layout);
  }

  /// Allocate a vector of non-self-deleting descriptor sets
  /// Note: descriptor sets get freed with the pool, so this is the better choice.
  std::vector<vk::DescriptorSet> create(vk::Device device, vk::DescriptorPool descriptorPool) const {
    vk::DescriptorSetAllocateInfo dsai{};
    dsai.descriptorPool = descriptorPool;
    dsai.descriptorSetCount = (uint32_t)s.layouts.size();
    dsai.pSetLayouts = s.layouts.data();
    return device.allocateDescriptorSets(dsai).value;
  }

  /// Allocate a vector of self-deleting descriptor sets.
  /*
  std::vector<vk::UniqueDescriptorSet> createUnique(vk::Device device, vk::DescriptorPool descriptorPool) const {
    vk::DescriptorSetAllocateInfo dsai{};
    dsai.descriptorPool = descriptorPool;
    dsai.descriptorSetCount = (uint32_t)s.layouts.size();
    dsai.pSetLayouts = s.layouts.data();
    return device.allocateDescriptorSetsUnique(dsai);
  }*/

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
  GenericImage() {
  }

  GenericImage(vk::Device device, const vk::ImageCreateInfo &info, vk::ImageViewType viewType, vk::ImageAspectFlags aspectMask, bool makeHostImage) {
    create(device, info, viewType, aspectMask, makeHostImage);
  }

  __inline vk::Image const& __restrict image() const { return(s.image); }
  __inline vk::ImageView const imageView() const { return(*s.imageView); }

  // Ondemand creation of view specific to mip level - texture arrays not supported
  // *** lazy function - assumes it will be called in ascending order of mip levels, starrting at mip level 1
  // *** otherwise the mipViews will not correspond to the correct mip level, function could also crash accessing out of bounds index
  __inline vk::ImageView const mipView(uint32_t const mipLevel, vk::Device const& __restrict device, vk::ImageAspectFlags const aspectMask = vk::ImageAspectFlagBits::eColor) {
		
	vk::ImageViewCreateInfo viewInfo{};
	viewInfo.image = s.image;
	viewInfo.viewType = (vk::ImageViewType) s.info.imageType;  // only works for 1D, 2D, 3D , ImageType enum integer levels match ImageViewType
	viewInfo.format = s.info.format;
	viewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
	viewInfo.subresourceRange = vk::ImageSubresourceRange{ aspectMask, mipLevel, 1, 0, 1 };
	s.mipView.emplace_back(device.createImageViewUnique(viewInfo).value);

	return(*s.mipView[mipLevel]);
  }
  void create_mipViews(vk::Device const& __restrict device, vk::ImageAspectFlags const aspectMask = vk::ImageAspectFlagBits::eColor) // creates and stores image views for all mips in s.mipView
  {
	  for (uint32_t i = 0; i < s.info.mipLevels; ++i) {
		  mipView(i, device, aspectMask);
	  }
  }

  // **** lazy function - assumes mipView has already been created, that mipLevel passed in is valid (not greater than image mip levels) etc etc
  __inline vk::ImageView const mipView(uint32_t const mipLevel) const {  

	  return(*s.mipView[mipLevel]);
  }
  
  // *** note this is the slow way of clearing an image in vulkan, do not use every frame - good for startup and periodic loading only *** //
  
  // Clear the colour of an image. sets the transferdstoptimal layout by default, can be false for batching, etc
  template<bool const bSetLayout = true>
  void clear(vk::CommandBuffer& __restrict cb, std::array<uint32_t,4> const color = {0, 0, 0, 0}) {

	  if constexpr (bSetLayout) {
		  setLayout(cb, vk::ImageLayout::eTransferDstOptimal);
	  }
	  vk::ImageSubresourceRange const range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
      cb.clearColorImage(s.image, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(color), range);
  }
  
  // *** note this is the slow way of clearing an image in vulkan, do not use every frame - good for startup and periodic loading only *** //
 
  // *** vk::ImageUsageFlagBits::eTransferDst REQUIRED *** does not require a command buffer, executes immediately, good for clearing images that have just been created. (ensure 100% clear image, not random noise. GPU ZeroMemory)
  // leaves image in the layout it was originally in before the clear (no change)
  void clear(vk::Device const& __restrict device, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue, std::array<uint32_t, 4> const color = { 0, 0, 0, 0 }) {

	  vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {

		  auto const original_layout(s.currentLayout);

		  setLayout<true>(cb, vk::ImageLayout::eTransferDstOptimal);
		  vk::ImageSubresourceRange const range{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		  cb.clearColorImage(s.image, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(color), range);
		  setLayout(cb, original_layout);

	  });
  }
  /// Copy another image to this one. This also changes the layout. SPECIFIC MIPLEVEL
  void copy(vk::CommandBuffer& __restrict cb, vku::GenericImage& __restrict srcImage, uint32_t const mipLevel) {
	  srcImage.setLayout(cb, vk::ImageLayout::eTransferSrcOptimal);
	  setLayout(cb, vk::ImageLayout::eTransferDstOptimal);

	  vk::ImageCopy region{};
	  region.srcSubresource = { vk::ImageAspectFlagBits::eColor, mipLevel, 0, 1 };
	  region.dstSubresource = { vk::ImageAspectFlagBits::eColor, mipLevel, 0, 1 };
	  region.extent = s.info.extent;
	  cb.copyImage(srcImage.image(), vk::ImageLayout::eTransferSrcOptimal, s.image, vk::ImageLayout::eTransferDstOptimal, region);
	  
  }

  /// Copy another image to this one. This also changes the layout.
  void copy(vk::CommandBuffer& __restrict cb, vku::GenericImage & __restrict srcImage) {
    srcImage.setLayout(cb, vk::ImageLayout::eTransferSrcOptimal);
    setLayout(cb, vk::ImageLayout::eTransferDstOptimal);
    for (uint32_t mipLevel = 0; mipLevel != info().mipLevels; ++mipLevel) {
      vk::ImageCopy region{};
      region.srcSubresource = {vk::ImageAspectFlagBits::eColor, mipLevel, 0, 1};
      region.dstSubresource = {vk::ImageAspectFlagBits::eColor, mipLevel, 0, 1};
      region.extent = s.info.extent;
      cb.copyImage(srcImage.image(), vk::ImageLayout::eTransferSrcOptimal, s.image, vk::ImageLayout::eTransferDstOptimal, region);
    }
  }

  /// Copy a subimage in a buffer to this image.
  void copy(vk::CommandBuffer& __restrict cb, vk::Buffer const& __restrict  buffer, uint32_t const mipLevel, uint32_t const arrayLayer, uint32_t const width, uint32_t const height, uint32_t const depth, uint32_t const offset) { 
    setLayout(cb, vk::ImageLayout::eTransferDstOptimal);
    vk::BufferImageCopy region{};
    region.bufferOffset = offset;
    vk::Extent3D extent;
    extent.width = width;
    extent.height = height;
    extent.depth = depth;
    region.imageSubresource = {vk::ImageAspectFlagBits::eColor, mipLevel, arrayLayer, 1};
    region.imageExtent = extent;
    cb.copyBufferToImage(buffer, s.image, vk::ImageLayout::eTransferDstOptimal, region);
  }

  // for a single layer upload, mipmapping levels not supported - only the source size for a layer of n bytes is considered. the target texture for upload must not have mipmaps, and should also enough layers (total layers > targetLayer)
  template< bool const DoSetFinalLayout = true, vk::ImageLayout const FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal, typename T >
  void upload(vk::Device device, T const* const __restrict bytes, size_t const sizeLayer, uint32_t const targetLayer, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue) {
	  vku::GenericBuffer stagingBuffer((vk::BufferUsageFlags)vk::BufferUsageFlagBits::eTransferSrc, (vk::DeviceSize)sizeLayer, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, vku::eMappedAccess::Sequential);
	  stagingBuffer.updateLocal<T>(bytes, sizeLayer);

	  // Copy the staging buffer to the GPU texture and set the layout.
	  vku::executeImmediately<false>(device, commandPool, queue, [&](vk::CommandBuffer cb) {
		  auto bp = getBlockParams(s.info.format);
		  vk::Buffer buf = stagingBuffer.buffer();
		  
		  auto width = mipScale(s.info.extent.width, 0);
		  auto height = mipScale(s.info.extent.height, 0);
		  auto depth = mipScale(s.info.extent.depth, 0);

		  copy(cb, buf, 0, targetLayer, width, height, depth, 0);
		
		  if constexpr (DoSetFinalLayout) {

			  setLayout(cb, FinalLayout);
		  }
	  });
  }

  // for a single layer upload, mipmapping levels not supported - only the source size for a layer of n bytes is considered. the target texture for upload must not have mipmaps, and should also enough layers (total layers > targetLayer)
  template< bool const DoSetFinalLayout = true, vk::ImageLayout const FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal >
  void upload(vk::Device device, vku::GenericBuffer const& __restrict stagingBuffer, uint32_t const targetLayer, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue) {

	  // Copy the staging buffer to the GPU texture and set the layout.
	  vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {
		  auto bp = getBlockParams(s.info.format);
		  vk::Buffer buf = stagingBuffer.buffer();

		  auto width = mipScale(s.info.extent.width, 0);
		  auto height = mipScale(s.info.extent.height, 0);
		  auto depth = mipScale(s.info.extent.depth, 0);

		  copy(cb, buf, 0, targetLayer, width, height, depth, 0);

		  if constexpr (DoSetFinalLayout) {

			  setLayout(cb, FinalLayout);
		  }
		  });
  }

  template< bool const DoSetFinalLayout = true, vk::ImageLayout const FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal, typename T >
  void upload(vk::Device device, T const* const __restrict bytes, size_t const size, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue) {
	  vku::GenericBuffer stagingBuffer((vk::BufferUsageFlags)vk::BufferUsageFlagBits::eTransferSrc, (vk::DeviceSize)size, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, vku::eMappedAccess::Sequential);
	  stagingBuffer.updateLocal<T>(bytes, size);

	  // Copy the staging buffer to the GPU texture and set the layout.
	  vku::executeImmediately<false>(device, commandPool, queue, [&](vk::CommandBuffer cb) {
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
		  
		  if constexpr (DoSetFinalLayout) {

			  setLayout(cb, FinalLayout);
		  }
	  });
  }

  template< bool const DoSetFinalLayout = true, vk::ImageLayout const FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal >
  void upload(vk::Device const& __restrict device, vku::GenericBuffer const& __restrict stagingBuffer, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue) {

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

		  if constexpr (DoSetFinalLayout) {

			  setLayout(cb, FinalLayout);
		  }
	  });
  }

  template< bool const DoSetFinalLayout = true, vk::ImageLayout const FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal >
  void uploadDeferred(vk::CommandBuffer& __restrict cb, vku::GenericBuffer const& __restrict stagingBuffer) {

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

	  if constexpr (DoSetFinalLayout) {

		  setLayout(cb, FinalLayout);
	  }
  }

  template< bool const DoSetFinalLayout = true, vk::ImageLayout const FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal >
  void upload(vk::Device device, std::vector<uint8_t> &bytes, vk::CommandPool commandPool, vk::Queue queue) {
	  upload< DoSetFinalLayout, FinalLayout >(device, bytes.data(), bytes.size(), commandPool, queue);
  }

  template< bool const DoSetFinalLayout = true, vk::ImageLayout const FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal, typename T>
  void upload(vk::Device const& __restrict device, T const* const __restrict bytes, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue) {
	  upload< DoSetFinalLayout, FinalLayout >(device, bytes, s.size, commandPool, queue);
  }

  void finalizeUpload(vk::Device const& __restrict device, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue,
	  vk::ImageLayout const FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal) 
  {
	  vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {
		 
		setLayout(cb, FinalLayout);

	  });
  }
  /* These constants are defined in vku_addon.hpp
  static constexpr int32_t const
	  ACCESS_READONLY(0),
	  ACCESS_READWRITE(1),
	  ACCESS_WRITEONLY(-1)
  */
  template< bool const bDontCareSrcUndefined = false>
  void setLayoutCompute(vk::CommandBuffer const& __restrict cb, int32_t const ComputeAccessRequired, vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) {
	  
	  // Put barrier on  top (default) - invalid for many access types
	  vk::PipelineStageFlags srcStageMask{ vk::PipelineStageFlagBits::eTopOfPipe };
	  vk::PipelineStageFlags dstStageMask{ vk::PipelineStageFlagBits::eTopOfPipe };
	  vk::DependencyFlags dependencyFlags{};
	  vk::AccessFlags srcMask{};
	  vk::AccessFlags dstMask{};
	  typedef vk::PipelineStageFlagBits psfb;
	  typedef vk::AccessFlagBits afb;
	  typedef vk::ImageLayout il;

	  /* newLayout */
      vk::ImageLayout oldLayout = s.currentLayout;

	  // input layout can be undefined if the contents are not needed to persist between frames
      if constexpr (bDontCareSrcUndefined) {
          oldLayout = s.currentLayout = il::eUndefined; // bugfix, ensure that layout for compute is set, regardless of currentLayout state. Fixes validation error at startup.
      }

      dstStageMask = psfb::eComputeShader;
	  if (0 == ComputeAccessRequired) {		// read only //
		  
		  if (il::eShaderReadOnlyOptimal == s.currentLayout) return;
		  s.currentLayout = il::eShaderReadOnlyOptimal;

		  dstMask = afb::eShaderRead;
	  }
	  else if (ComputeAccessRequired < 0) { // write only //
		  
		  if (il::eGeneral == s.currentLayout) return;
		  s.currentLayout = il::eGeneral;

		  dstMask = afb::eShaderWrite;
	  }
	  else {							    // read-write //
		  
		  if (il::eGeneral == s.currentLayout) return;
		  s.currentLayout = il::eGeneral;

		  dstMask = afb::eShaderWrite | afb::eShaderRead;
	  }

	  vk::ImageMemoryBarrier imageMemoryBarriers = {};
	  imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  imageMemoryBarriers.oldLayout = oldLayout;
	  imageMemoryBarriers.newLayout = s.currentLayout;
	  imageMemoryBarriers.image = s.image;
	  imageMemoryBarriers.subresourceRange = { aspectMask, 0, s.info.mipLevels, 0, s.info.arrayLayers };

	  // Is it me, or are these the same?
	  switch (oldLayout) {
	  case il::eUndefined: break;
	  case il::eGeneral: 
		  srcMask = afb::eShaderWrite;	// assumes only compute and it was write-only
		  srcStageMask = psfb::eComputeShader;
		  break;
	  case il::eColorAttachmentOptimal: srcMask = afb::eColorAttachmentWrite; srcStageMask = psfb::eColorAttachmentOutput; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eDepthStencilAttachmentOptimal: srcMask = afb::eDepthStencilAttachmentWrite; srcStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eDepthStencilReadOnlyOptimal: srcMask = afb::eDepthStencilAttachmentRead; srcStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eShaderReadOnlyOptimal: srcMask = afb::eShaderRead; srcStageMask = psfb::eFragmentShader | psfb::eComputeShader; /*assumes frag or compute shader*/ dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eTransferSrcOptimal: srcMask = afb::eTransferRead; srcStageMask = psfb::eTransfer;  break;
	  case il::eTransferDstOptimal: srcMask = afb::eTransferWrite; srcStageMask = psfb::eTransfer; break;
	  case il::ePreinitialized: srcMask = afb::eTransferWrite | afb::eHostWrite; break;
	  case il::ePresentSrcKHR: srcMask = afb::eMemoryRead; srcStageMask = psfb::eBottomOfPipe; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  }

	  imageMemoryBarriers.srcAccessMask = srcMask;
	  imageMemoryBarriers.dstAccessMask = dstMask;
	  auto memoryBarriers = nullptr;
	  auto bufferMemoryBarriers = nullptr;

	  if (srcStageMask != dstStageMask || (srcStageMask == dstStageMask && (srcMask != dstMask))) {
		  cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, memoryBarriers, bufferMemoryBarriers, imageMemoryBarriers);
	  }
  }

  void setLayoutFragmentFromCompute(vk::CommandBuffer const& __restrict cb, int32_t const ComputeAccessUsed, vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) {
	  typedef vk::ImageLayout il;
	  if (il::eShaderReadOnlyOptimal == s.currentLayout) return;
	  vk::ImageLayout oldLayout = s.currentLayout;
	  s.currentLayout = il::eShaderReadOnlyOptimal;

	  // 	case il::eShaderReadOnlyOptimal: dstMask = afb::eShaderRead; dstStageMask = psfb::eFragmentShader | psfb::eComputeShader; break; // assumes not texture access in vertex shader, etc
	  vk::ImageMemoryBarrier imageMemoryBarriers = {};
	  imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  imageMemoryBarriers.oldLayout = oldLayout;
	  imageMemoryBarriers.newLayout = s.currentLayout;
	  imageMemoryBarriers.image = s.image;
	  imageMemoryBarriers.subresourceRange = { aspectMask, 0, s.info.mipLevels, 0, s.info.arrayLayers };

	  // Put barrier on  top (default) - invalid for many access types
	  vk::PipelineStageFlags srcStageMask{ vk::PipelineStageFlagBits::eTopOfPipe };
	  vk::PipelineStageFlags dstStageMask{ vk::PipelineStageFlagBits::eTopOfPipe };
	  vk::DependencyFlags const dependencyFlags{ vk::DependencyFlagBits::eByRegion };
	  vk::AccessFlags srcMask{};
	  vk::AccessFlags dstMask{};

	  typedef vk::PipelineStageFlagBits psfb;
	  typedef vk::AccessFlagBits afb;

	  dstMask = afb::eShaderRead; dstStageMask = psfb::eFragmentShader; // assumes not texture access in vertex shader, etc

	  /* oldLayout */
	  switch (oldLayout) {
		 case il::eUndefined: break;
		 case il::eGeneral: 
		 {
			 srcStageMask = psfb::eComputeShader;
			 if (0 == ComputeAccessUsed) {		// read only //
#ifndef NDEBUG
				 assert_print(false, "setLayoutFragmentFromCompute logical error, old layout cannot be General while AccessUsed is readonly");
#endif
			 }
			 else if (ComputeAccessUsed < 0) { // write only //
				 srcMask = afb::eShaderWrite;
			 }
			 else {							    // read-write //
				 srcMask = afb::eShaderWrite | afb::eShaderRead;
			 }
			 break;
		 }
	  }
	
	  imageMemoryBarriers.srcAccessMask = srcMask;
	  imageMemoryBarriers.dstAccessMask = dstMask;
	  auto memoryBarriers = nullptr;
	  auto bufferMemoryBarriers = nullptr;

	  if (srcStageMask != dstStageMask || (srcStageMask == dstStageMask && (srcMask != dstMask))) {
		  cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, memoryBarriers, bufferMemoryBarriers, imageMemoryBarriers);
	  }
  }

  // For specifying the specific source and destination stages  vk::PipelineStageFlagBits (advanced usage):
  template< bool const bDontCareSrcUndefined = false>
  void setLayout(vk::CommandBuffer const& __restrict cb, vk::ImageLayout newLayout, vk::PipelineStageFlags srcStageMask, int32_t const AccessUsed, vk::PipelineStageFlags dstStageMask, int32_t const AccessRequired, vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor, vk::ImageLayout const ForceLayoutUpdate = vk::ImageLayout::eUndefined) {

	  vk::ImageLayout oldLayout;

	  if constexpr (bDontCareSrcUndefined) {
		  oldLayout = vk::ImageLayout::eUndefined;
		  srcStageMask = vk::PipelineStageFlagBits::eTopOfPipe;
	  }
	  else {
		  if (vk::ImageLayout::eUndefined == ForceLayoutUpdate) {
			  if (newLayout == s.currentLayout && AccessUsed == AccessRequired) return;

			  oldLayout = s.currentLayout;
		  }
		  else {
			  if (vk::ImageLayout::eUndefined != s.currentLayout)
				  oldLayout = ForceLayoutUpdate;
			  else
				  oldLayout = s.currentLayout;
		  }
	  }

	  s.currentLayout = newLayout;

	  vk::ImageMemoryBarrier imageMemoryBarriers = {};
	  imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	  imageMemoryBarriers.oldLayout = oldLayout;
	  imageMemoryBarriers.newLayout = newLayout;
	  imageMemoryBarriers.image = s.image;
	  imageMemoryBarriers.subresourceRange = { aspectMask, 0, s.info.mipLevels, 0, s.info.arrayLayers };

	  vk::DependencyFlags dependencyFlags{};
	  vk::AccessFlags srcMask{};
	  vk::AccessFlags dstMask{};

	  typedef vk::ImageLayout il;
	  typedef vk::PipelineStageFlagBits psfb;
	  typedef vk::AccessFlagBits afb;

	  // Is it me, or are these the same?
	  switch (oldLayout) {
	  case il::eUndefined: break;
	  case il::eGeneral: 
		  if (0 == AccessUsed) {		// read only //
			  /*case il::eGeneral:*/ srcMask = afb::eShaderRead;
		  }
		  else if (AccessUsed < 0) { // write only //
			  /*case il::eGeneral:*/ srcMask = afb::eShaderWrite;
		  }
		  else {							    // read-write //
			  /*case il::eGeneral:*/ srcMask = afb::eShaderWrite | afb::eShaderRead;
		  }
		  dependencyFlags = vk::DependencyFlagBits::eByRegion;
		  break; // srcstagemask must be suitably set
	  case il::eColorAttachmentOptimal: srcMask = afb::eColorAttachmentWrite; srcStageMask = psfb::eColorAttachmentOutput; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eDepthStencilAttachmentOptimal: srcMask = afb::eDepthStencilAttachmentWrite; srcStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eDepthStencilReadOnlyOptimal: srcMask = afb::eDepthStencilAttachmentRead; srcStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eShaderReadOnlyOptimal: // this is compatible w/compute and fragment by using correct passed in srcStageMask
		  srcMask = afb::eShaderRead; 
		  dependencyFlags = vk::DependencyFlagBits::eByRegion; 
		  break; // srcstagemask must be suitably set
	  case il::eTransferSrcOptimal: srcMask = afb::eTransferRead; srcStageMask = psfb::eTransfer;  break;
	  case il::eTransferDstOptimal: srcMask = afb::eTransferWrite; srcStageMask = psfb::eTransfer; break;
	  case il::ePreinitialized: srcMask = afb::eTransferWrite | afb::eHostWrite; break;
	  case il::ePresentSrcKHR: srcMask = afb::eMemoryRead; srcStageMask = psfb::eBottomOfPipe; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  }

	  switch (newLayout) {
	  case il::eUndefined: break;
	  case il::eGeneral: 
		  if (0 == AccessRequired) {		// read only //
#ifndef NDEBUG
			  assert_print(false, "setLayout logical error, new layout cannot be General while AccessRequired is readonly");
#endif
		  }
		  else if (AccessRequired < 0) { // write only //
			  /*case il::eGeneral:*/ dstMask = afb::eShaderWrite;
		  }
		  else {							    // read-write //
			  /*case il::eGeneral:*/ dstMask = afb::eShaderWrite | afb::eShaderRead;
		  }
		  dependencyFlags = vk::DependencyFlagBits::eByRegion;
		  break; // dststagemask must be suitably set
	  case il::eColorAttachmentOptimal: dstMask = afb::eColorAttachmentWrite; dstStageMask = psfb::eColorAttachmentOutput; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eDepthStencilAttachmentOptimal: dstMask = afb::eDepthStencilAttachmentWrite; dstStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eDepthStencilReadOnlyOptimal: dstMask = afb::eDepthStencilAttachmentRead; dstStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eShaderReadOnlyOptimal: // this is compatible w/compute and fragment by using correct passed in dstStageMask
		  dstMask = afb::eShaderRead; 
		  dependencyFlags = vk::DependencyFlagBits::eByRegion; 
		  break; // dststagemask must be suitably set
	  case il::eTransferSrcOptimal: dstMask = afb::eTransferRead; dstStageMask = psfb::eTransfer;  break;
	  case il::eTransferDstOptimal: dstMask = afb::eTransferWrite; dstStageMask = psfb::eTransfer; break;
	  case il::ePreinitialized: dstMask = afb::eTransferWrite | afb::eHostWrite; break;
	  case il::ePresentSrcKHR: dstMask = afb::eMemoryRead; dstStageMask = psfb::eBottomOfPipe; dependencyFlags = vk::DependencyFlagBits::eByRegion;  break;
	  }

	  imageMemoryBarriers.srcAccessMask = srcMask;
	  imageMemoryBarriers.dstAccessMask = dstMask;
	  auto memoryBarriers = nullptr;
	  auto bufferMemoryBarriers = nullptr;

	  if (srcStageMask != dstStageMask || (srcStageMask == dstStageMask && (srcMask != dstMask))) {
		  cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, memoryBarriers, bufferMemoryBarriers, imageMemoryBarriers);
	  }
  }

  /// Change the layout of this image using a memory barrier. (simple automatic usage - covers most cases)
  // ForceLayoutUpdate s good for recording those pesky cbs hat wont set the layout properly due to this classes state tracking
  // ForceLayoutUpdate should be set to the expected OLD layout being transitioned from
  template< bool const bDontCareSrcUndefined = false>
  void setLayout(vk::CommandBuffer const& __restrict cb, vk::ImageLayout newLayout, vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor, vk::ImageLayout const ForceLayoutUpdate = vk::ImageLayout::eUndefined) {
    
	  vk::ImageLayout oldLayout;

	  if constexpr (bDontCareSrcUndefined) {
		  oldLayout = vk::ImageLayout::eUndefined;
	  }
	  else {
		  if (vk::ImageLayout::eUndefined == ForceLayoutUpdate) {
			  if (newLayout == s.currentLayout) return;

			  oldLayout = s.currentLayout;
		  }
		  else {
			  if (vk::ImageLayout::eUndefined != s.currentLayout)
				  oldLayout = ForceLayoutUpdate;
			  else
				  oldLayout = s.currentLayout;
		  }
	  }
    
	s.currentLayout = newLayout;

    vk::ImageMemoryBarrier imageMemoryBarriers = {};
    imageMemoryBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers.oldLayout = oldLayout;
    imageMemoryBarriers.newLayout = newLayout;
    imageMemoryBarriers.image = s.image;
    imageMemoryBarriers.subresourceRange = {aspectMask, 0, s.info.mipLevels, 0, s.info.arrayLayers};

    // Put barrier on  top (default) - invalid for many access types
    vk::PipelineStageFlags srcStageMask{vk::PipelineStageFlagBits::eTopOfPipe};
    vk::PipelineStageFlags dstStageMask{vk::PipelineStageFlagBits::eTopOfPipe};
    vk::DependencyFlags dependencyFlags{};
    vk::AccessFlags srcMask{};
    vk::AccessFlags dstMask{};

    typedef vk::ImageLayout il;
	typedef vk::PipelineStageFlagBits psfb;
    typedef vk::AccessFlagBits afb;

    // Is it me, or are these the same?
    switch (oldLayout) {
      case il::eUndefined: break;
	  case il::eGeneral: srcMask = afb::eShaderWrite | afb::eShaderRead; srcStageMask = psfb::eComputeShader; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eColorAttachmentOptimal: srcMask = afb::eColorAttachmentWrite; srcStageMask = psfb::eColorAttachmentOutput; dependencyFlags = vk::DependencyFlagBits::eByRegion;  break;
	  case il::eDepthStencilAttachmentOptimal: srcMask = afb::eDepthStencilAttachmentWrite; srcStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
      case il::eDepthStencilReadOnlyOptimal: srcMask = afb::eDepthStencilAttachmentRead; srcStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	  case il::eShaderReadOnlyOptimal: srcMask = afb::eShaderRead; srcStageMask = psfb::eFragmentShader; dependencyFlags = vk::DependencyFlagBits::eByRegion; break; // assumes not texture access in vertex shader, etc
	  case il::eTransferSrcOptimal: srcMask = afb::eTransferRead; srcStageMask = psfb::eTransfer;  break;
      case il::eTransferDstOptimal: srcMask = afb::eTransferWrite; srcStageMask = psfb::eTransfer; break;
      case il::ePreinitialized: srcMask = afb::eTransferWrite|afb::eHostWrite; break;
	  case il::ePresentSrcKHR: srcMask = afb::eMemoryRead; srcStageMask = psfb::eBottomOfPipe; dependencyFlags = vk::DependencyFlagBits::eByRegion;  break;
    }

	switch (newLayout) {
	case il::eUndefined: break;
	case il::eGeneral: dstMask = afb::eShaderWrite; dstStageMask = psfb::eComputeShader; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	case il::eColorAttachmentOptimal: dstMask = afb::eColorAttachmentWrite; dstStageMask = psfb::eColorAttachmentOutput; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	case il::eDepthStencilAttachmentOptimal: dstMask = afb::eDepthStencilAttachmentWrite; dstStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	case il::eDepthStencilReadOnlyOptimal: dstMask = afb::eDepthStencilAttachmentRead; dstStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	case il::eShaderReadOnlyOptimal: dstMask = afb::eShaderRead; dstStageMask = psfb::eFragmentShader; dependencyFlags = vk::DependencyFlagBits::eByRegion; break; // assumes not texture access in vertex shader, etc
	case il::eTransferSrcOptimal: dstMask = afb::eTransferRead; dstStageMask = psfb::eTransfer;  break;
	case il::eTransferDstOptimal: dstMask = afb::eTransferWrite; dstStageMask = psfb::eTransfer; break;
	case il::ePreinitialized: dstMask = afb::eTransferWrite | afb::eHostWrite; break;
	case il::ePresentSrcKHR: dstMask = afb::eMemoryRead; dstStageMask = psfb::eBottomOfPipe; dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
	}

    imageMemoryBarriers.srcAccessMask = srcMask;
    imageMemoryBarriers.dstAccessMask = dstMask;
    auto memoryBarriers = nullptr;
    auto bufferMemoryBarriers = nullptr;

	if (srcStageMask != dstStageMask || (srcStageMask == dstStageMask && (srcMask != dstMask))) {
		cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, memoryBarriers, bufferMemoryBarriers, imageMemoryBarriers);
	}
  }

  // batched / multiple barriers stages src and dst are same for all images in batch
  // For specifying the specific source and destination stages  vk::PipelineStageFlagBits (advanced usage):
  template<size_t const image_count, bool const bDontCareSrcUndefined = false>
  static void setLayout(std::array<vku::GenericImage* const, image_count> const& __restrict images, 
	  vk::CommandBuffer const& __restrict cb, vk::ImageLayout const newLayout, vk::PipelineStageFlags srcStageMask, int32_t const AccessUsed, vk::PipelineStageFlags const dstStageMask, int32_t const AccessRequired, vk::ImageAspectFlags const aspectMask = vk::ImageAspectFlagBits::eColor, vk::ImageLayout const ForceLayoutUpdate = vk::ImageLayout::eUndefined) {

	  vk::DependencyFlags dependencyFlags{};
	  std::array<vk::ImageMemoryBarrier, image_count> imbs;
	  uint32_t used_image_count(0);
	  bool bDstMaskSrcMask(false);

	  for (uint32_t i = 0; i < image_count; ++i) {

		  vk::ImageLayout oldLayout;

		  if constexpr (bDontCareSrcUndefined) {
			  oldLayout = vk::ImageLayout::eUndefined;
			  srcStageMask = vk::PipelineStageFlagBits::eTopOfPipe;
		  }
		  else {
			  if (vk::ImageLayout::eUndefined == ForceLayoutUpdate) {
				  if (newLayout == images[i]->s.currentLayout && AccessUsed == AccessRequired) continue;

				  oldLayout = images[i]->s.currentLayout;
			  }
			  else {
				  if (vk::ImageLayout::eUndefined != images[i]->s.currentLayout)
					  oldLayout = ForceLayoutUpdate;
				  else
					  oldLayout = images[i]->s.currentLayout;
			  }
		  }

		  images[i]->s.currentLayout = newLayout;

		  imbs[used_image_count].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		  imbs[used_image_count].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		  imbs[used_image_count].oldLayout = oldLayout;
		  imbs[used_image_count].newLayout = newLayout;
		  imbs[used_image_count].image = images[i]->s.image;
		  imbs[used_image_count].subresourceRange = { aspectMask, 0, images[i]->s.info.mipLevels, 0, images[i]->s.info.arrayLayers };
		
		  vk::AccessFlags srcMask{};
		  vk::AccessFlags dstMask{};

		  typedef vk::ImageLayout il;
		  typedef vk::PipelineStageFlagBits psfb;
		  typedef vk::AccessFlagBits afb;

		  // Is it me, or are these the same?
		  switch (oldLayout) {
		  case il::eUndefined: break;
		  case il::eGeneral:
			  if (0 == AccessUsed) {		// read only //
#ifndef NDEBUG
				  assert_print(false, "setLayout logical error, old layout cannot be General while AccessUsed is readonly");
#endif
			  }
			  else if (AccessUsed < 0) { // write only //
				  /*case il::eGeneral:*/ srcMask = afb::eShaderWrite;
			  }
			  else {							    // read-write //
				  /*case il::eGeneral:*/ srcMask = afb::eShaderWrite | afb::eShaderRead;
			  }
			  dependencyFlags = vk::DependencyFlagBits::eByRegion;
			  break; // srcstagemask must be suitably set
		  case il::eColorAttachmentOptimal: srcMask = afb::eColorAttachmentWrite; /*srcStageMask = psfb::eColorAttachmentOutput;*/ dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
		  case il::eDepthStencilAttachmentOptimal: srcMask = afb::eDepthStencilAttachmentWrite; /*srcStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests;*/ dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
		  case il::eDepthStencilReadOnlyOptimal: srcMask = afb::eDepthStencilAttachmentRead; /*srcStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests;*/ dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
		  case il::eShaderReadOnlyOptimal: // this is compatible w/compute and fragment by using correct passed in srcStageMask
			  srcMask = afb::eShaderRead;
			  dependencyFlags = vk::DependencyFlagBits::eByRegion;
			  break; // srcstagemask must be suitably set
		  case il::eTransferSrcOptimal: srcMask = afb::eTransferRead; /*srcStageMask = psfb::eTransfer;*/  break;
		  case il::eTransferDstOptimal: srcMask = afb::eTransferWrite; /*srcStageMask = psfb::eTransfer;*/ break;
		  case il::ePreinitialized: srcMask = afb::eTransferWrite | afb::eHostWrite; break;
		  case il::ePresentSrcKHR: srcMask = afb::eMemoryRead; /*srcStageMask = psfb::eBottomOfPipe;*/ dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
		  }

		  switch (newLayout) {
		  case il::eUndefined: break;
		  case il::eGeneral:
			  if (0 == AccessRequired) {		// read only //
#ifndef NDEBUG
				  assert_print(false, "setLayout logical error, new layout cannot be General while AccessRequired is readonly");
#endif
			  }
			  else if (AccessRequired < 0) { // write only //
				  /*case il::eGeneral:*/ dstMask = afb::eShaderWrite;
			  }
			  else {							    // read-write //
				  /*case il::eGeneral:*/ dstMask = afb::eShaderWrite | afb::eShaderRead;
			  }
			  dependencyFlags = vk::DependencyFlagBits::eByRegion;
			  break; // dststagemask must be suitably set
		  case il::eColorAttachmentOptimal: dstMask = afb::eColorAttachmentWrite; /*dstStageMask = psfb::eColorAttachmentOutput;*/ dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
		  case il::eDepthStencilAttachmentOptimal: dstMask = afb::eDepthStencilAttachmentWrite; /*dstStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests;*/ dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
		  case il::eDepthStencilReadOnlyOptimal: dstMask = afb::eDepthStencilAttachmentRead; /*dstStageMask = psfb::eEarlyFragmentTests | psfb::eLateFragmentTests;*/ dependencyFlags = vk::DependencyFlagBits::eByRegion; break;
		  case il::eShaderReadOnlyOptimal: // this is compatible w/compute and fragment by using correct passed in dstStageMask
			  dstMask = afb::eShaderRead;
			  dependencyFlags = vk::DependencyFlagBits::eByRegion;
			  break; // dststagemask must be suitably set
		  case il::eTransferSrcOptimal: dstMask = afb::eTransferRead; /*dstStageMask = psfb::eTransfer;*/  break;
		  case il::eTransferDstOptimal: dstMask = afb::eTransferWrite; /*dstStageMask = psfb::eTransfer;*/ break;
		  case il::ePreinitialized: dstMask = afb::eTransferWrite | afb::eHostWrite; break;
		  case il::ePresentSrcKHR: dstMask = afb::eMemoryRead; /*dstStageMask = psfb::eBottomOfPipe;*/ dependencyFlags = vk::DependencyFlagBits::eByRegion;  break;
		  }

		  imbs[used_image_count].srcAccessMask = srcMask;
		  imbs[used_image_count].dstAccessMask = dstMask;
		  bDstMaskSrcMask |= (srcMask == dstMask);

		  ++used_image_count;
	  }
	  
	  if (srcStageMask != dstStageMask || (srcStageMask == dstStageMask && !bDstMaskSrcMask)) {
		  cb.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 0, nullptr, used_image_count, imbs.data());
	  }
  }

  // **** use these for "undefined" / "don't care" source instances:
  // 
  // setlayout when the data is in a don't care state (undefined)
  void setLayoutFromUndefined(vk::CommandBuffer const& __restrict cb, vk::ImageLayout const newLayout, vk::ImageAspectFlags const aspectMask = vk::ImageAspectFlagBits::eColor) {
	  setLayout<true>(cb, newLayout, aspectMask);
  }
  // batched version
  template<size_t const image_count>
  static void setLayoutFromUndefined(std::array<vku::GenericImage* const, image_count> const& __restrict images,
									 vk::CommandBuffer const& __restrict cb, vk::ImageLayout const newLayout, vk::PipelineStageFlags const dstStageMask, int32_t const AccessRequired, vk::ImageAspectFlags const aspectMask = vk::ImageAspectFlagBits::eColor) {
	  GenericImage::setLayout<image_count, true>(images, cb, newLayout, vk::PipelineStageFlagBits::eTopOfPipe, 0/*N/A*/, dstStageMask, AccessRequired, aspectMask);
  }
  /// Set what the image thinks is its current layout (ie. the old layout in an image barrier).
  void setCurrentLayout(vk::ImageLayout oldLayout) {
    s.currentLayout = oldLayout;
  }


  vk::Format format() const { return(s.info.format); }
  vk::Extent3D extent() const { return(s.info.extent); }
  vk::ImageCreateInfo const& __restrict info() const { return(s.info); }
  vk::DeviceSize const& __restrict size() const { return(s.size); }
protected:
  void create(vk::Device device, const vk::ImageCreateInfo &info, vk::ImageViewType const viewType, vk::ImageAspectFlags const aspectMask, bool const hostImage = false, bool const bDedicatedMemory = false) {
    s.currentLayout = info.initialLayout;
    s.info = info;

#ifndef NDEBUG
	if (vk::ImageUsageFlagBits::eStorage == (info.usage & vk::ImageUsageFlagBits::eStorage)) {
		assert_print(vk::SampleCountFlagBits::e1 == info.samples, "Storage bit enabled for multisampled image incorrectly\n");
	}
#endif

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = (hostImage ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);  // default to gpu only if 0/unknown is passed in
	allocInfo.requiredFlags = (VkMemoryPropertyFlags)(hostImage ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	allocInfo.preferredFlags = allocInfo.requiredFlags;
	allocInfo.flags = (bDedicatedMemory ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : (VmaAllocationCreateFlags)0);

	VmaAllocationInfo image_alloc_info{};
	vmaCreateImage(vma_, (VkImageCreateInfo const* const)&s.info, &allocInfo, (VkImage*)&s.image, &s.allocation, &image_alloc_info);

	s.size = image_alloc_info.size;

    if (!hostImage) {
      vk::ImageViewCreateInfo viewInfo{};
      viewInfo.image = s.image;
      viewInfo.viewType = viewType;
      viewInfo.format = info.format;
      viewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
      viewInfo.subresourceRange = vk::ImageSubresourceRange{aspectMask, 0, info.mipLevels, 0, info.arrayLayers};
      s.imageView = device.createImageViewUnique(viewInfo).value;
    }
  }

  public:
	vk::UniqueImageView createImageView(vk::Device const& __restrict device, vk::ImageViewType const viewType, uint32_t const baseMipLevel = 0, vk::ImageAspectFlags const aspectMask = vk::ImageAspectFlagBits::eColor)
	{
		vk::ImageViewCreateInfo viewInfo{};
		viewInfo.image = s.image;
		viewInfo.viewType = viewType;
		viewInfo.format = s.info.format;
		viewInfo.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
		viewInfo.subresourceRange = vk::ImageSubresourceRange{ aspectMask, baseMipLevel, s.info.mipLevels, 0, s.info.arrayLayers };

		return(device.createImageViewUnique(viewInfo).value);
	}
	void changeImageView(vk::Device const& __restrict device, vk::ImageViewType const viewType, uint32_t const baseMipLevel = 0, vk::ImageAspectFlags const aspectMask = vk::ImageAspectFlagBits::eColor)
	{
		// overloaded assignment opeator works on && reference, releasing the old image view and replacing it with the new one via std::move
		s.imageView = std::move(createImageView(device, viewType, baseMipLevel, aspectMask));
	}

  protected:
  struct State {
    vk::Image image;
    vk::UniqueImageView imageView;
	std::vector<vk::UniqueImageView> mipView;
    VmaAllocation allocation;
	vk::DeviceSize size{};
    vk::ImageLayout currentLayout;
    vk::ImageCreateInfo info;
		
	void release()
	{
		size = 0;
		for (auto& mip : mipView) {
			mip.release();
		}
		imageView.release();

		if (allocation) {
			vmaDestroyImage(vma_, image, allocation);
			image = nullptr;
			allocation = nullptr;
		}
	}

  };

  State s;

 public:
	  GenericImage & operator=(GenericImage && other)
	  {
		  s = std::move(other.s);

		  other.s.size = 0;
		  for (auto& mip : other.s.mipView) {
			  mip.reset(nullptr);
		  }
		  other.s.imageView.reset(nullptr);
		  other.s.image = nullptr;
		  other.s.allocation = nullptr;

		  return(*this);
	  }
	  void release()
	  {
		  s.release();
	  }
	  ~GenericImage()
	  {
		  release();
	  }
private:
};

class TextureImage1DArray : public GenericImage {
public:
	TextureImage1DArray() {
	}

	// For Immutable Simple 1D Texture Array resource
	TextureImage1DArray(vk::Device device, uint32_t const width, uint32_t const layers, vk::Format format = vk::Format::eB8G8R8A8Unorm, bool hostImage = false, bool const bDedicatedMemory = false) {
		vk::ImageCreateInfo info;
		info.flags = {};
		info.imageType = vk::ImageType::e1D;
		info.format = format;
		info.extent = vk::Extent3D{ width, 1U, 1U };
		info.mipLevels = 1;
		info.arrayLayers = layers;
		info.samples = vk::SampleCountFlagBits::e1;
		info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
		info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
		info.sharingMode = vk::SharingMode::eExclusive;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;
		info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
		create(device, info, vk::ImageViewType::e1DArray, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
	}
private:
};


class TextureImage2DArray : public GenericImage {
public:
	TextureImage2DArray() {
	}

	// For Immutable Simple 2D Texture Array resource
	TextureImage2DArray(vk::Device device, uint32_t const width, uint32_t const height, uint32_t const layers, vk::Format format = vk::Format::eB8G8R8A8Unorm, bool hostImage = false, bool const bDedicatedMemory = false) {
		vk::ImageCreateInfo info;
		info.flags = {};
		info.imageType = vk::ImageType::e2D;
		info.format = format;
		info.extent = vk::Extent3D{ width, height, 1U };
		info.mipLevels = 1;
		info.arrayLayers = layers;
		info.samples = vk::SampleCountFlagBits::e1;
		info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
		info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
		info.sharingMode = vk::SharingMode::eExclusive;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;
		info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
		create(device, info, vk::ImageViewType::e2DArray, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
	}
private:
};

/// A 2D texture image living on the GPU or a staging buffer visible to the CPU.
class TextureImage1D : public GenericImage {
public:
    TextureImage1D() {
    }

    // For Immutable Simple 2D Texture resource
    TextureImage1D(vk::Device device, uint32_t const width, vk::Format const format = vk::Format::eB8G8R8A8Unorm, bool const hostImage = false, bool const bDedicatedMemory = false) {
        vk::ImageCreateInfo info;
        info.flags = {};
        info.imageType = vk::ImageType::e1D;
        info.format = format;
        info.extent = vk::Extent3D{ width, 1U, 1U };
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = vk::SampleCountFlagBits::e1;
        info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
        info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
        info.sharingMode = vk::SharingMode::eExclusive;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
        info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
        create(device, info, vk::ImageViewType::e1D, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
    }

    TextureImage1D(vk::Device device, uint32_t const width, uint32_t const mipLevels = 1, vk::Format const format = vk::Format::eB8G8R8A8Unorm, bool const hostImage = false, bool const bDedicatedMemory = false) {
        vk::ImageCreateInfo info;
        info.flags = {};
        info.imageType = vk::ImageType::e2D;
        info.format = format;
        info.extent = vk::Extent3D{ width, 1U, 1U };
        info.mipLevels = mipLevels;
        info.arrayLayers = 1;
        info.samples = vk::SampleCountFlagBits::e1;
        info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
        info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
        info.sharingMode = vk::SharingMode::eExclusive;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
        info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
        create(device, info, vk::ImageViewType::e1D, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
    }
private:
};

// TextureImageStorage1DArray was tested - very fucking slow in compute shader vs a combined sampler equivalent

/// A 2D texture image living on the GPU or a staging buffer visible to the CPU.
class TextureImage2D : public GenericImage {
public:
  TextureImage2D() {
  }

  // For Immutable Simple 2D Texture resource
  TextureImage2D(vk::Device const& __restrict device, uint32_t const width, uint32_t const height, vk::Format const format = vk::Format::eB8G8R8A8Unorm, bool hostImage = false, bool const bDedicatedMemory = false) {
	  vk::ImageCreateInfo info;
	  info.flags = {};
	  info.imageType = vk::ImageType::e2D;
	  info.format = format;
	  info.extent = vk::Extent3D{ width, height, 1U };
	  info.mipLevels = 1;
	  info.arrayLayers = 1;
	  info.samples = vk::SampleCountFlagBits::e1;
	  info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
	  info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	  info.sharingMode = vk::SharingMode::eExclusive;
	  info.queueFamilyIndexCount = 0;
	  info.pQueueFamilyIndices = nullptr;
	  info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
	  create(device, info, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
  }

  TextureImage2D(vk::Device const& __restrict device, uint32_t const width, uint32_t const height, uint32_t const mipLevels=1, vk::Format const format = vk::Format::eB8G8R8A8Unorm, bool hostImage = false, bool const bDedicatedMemory = false) {
    vk::ImageCreateInfo info;
    info.flags = {};
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = mipLevels;
    info.arrayLayers = 1;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
    create(device, info, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
  }
private:
};

/// A 3D texture image living on the GPU or a staging buffer visible to the CPU.
class TextureImage3D : public GenericImage {
public:
	TextureImage3D() {
	}

	// For Immutable Simple 3D Texture resource
	TextureImage3D(vk::Device const& __restrict device, uint32_t const width, uint32_t const height, uint32_t const depth, vk::Format format = vk::Format::eB8G8R8A8Unorm, bool hostImage = false, bool const bDedicatedMemory = false) {
		vk::ImageCreateInfo info;
		info.flags = {};
		info.imageType = vk::ImageType::e3D;
		info.format = format;
		info.extent = vk::Extent3D{ width, height, depth };
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = vk::SampleCountFlagBits::e1;
		info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
		info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
		info.sharingMode = vk::SharingMode::eExclusive;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;
		info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
		create(device, info, vk::ImageViewType::e3D, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
	}

	// vk::ImageUsageFlagBits::eSampled (if image will be read from pixel shader)
	// vk::ImageUsageFlagBits::eTransferSrc (if image is a source for a transfer/copy from/... image operation)
	// vk::ImageUsageFlagBits::eTransferDst (if image is a destination for a transfer/copy to/clear image operation)
	TextureImage3D(vk::ImageUsageFlags const ImageUsage, vk::Device device, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels = 1U, vk::Format format = vk::Format::eB8G8R8A8Unorm, bool hostImage = false, bool const bDedicatedMemory = false) {
		vk::ImageCreateInfo info;
		info.flags = {};
		info.imageType = vk::ImageType::e3D;
		info.format = format;
		info.extent = vk::Extent3D{ width, height, depth };
		info.mipLevels = mipLevels;
		info.arrayLayers = 1;
		info.samples = vk::SampleCountFlagBits::e1;
		info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
		info.usage = ImageUsage;
		info.sharingMode = vk::SharingMode::eExclusive;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;
		info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
		create(device, info, vk::ImageViewType::e3D, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
	}

private:
};
class TextureImageStorage2D : public GenericImage {
public:
	TextureImageStorage2D() {
	}

	// Image Usage can be a combination of
	// vk::ImageUsageFlagBits::eSampled (if image will be read from pixel shader)
	// vk::ImageUsageFlagBits::eStorage (if image will be read or written to in computer shader)
	// vk::ImageUsageFlagBits::eTransferSrc (if image is a source for a transfer/copy from/... image operation)
	// vk::ImageUsageFlagBits::eTransferDst (if image is a destination for a transfer/copy to/clear image operation)
	// TextureImageStorage2D is specific for compute shaders
	// and the Image Usage should be specific aswell to optimize access to image resource
	TextureImageStorage2D(vk::ImageUsageFlags const ImageUsage, vk::Device device, uint32_t const width, uint32_t const height, uint32_t const mipLevels = 1U, vk::SampleCountFlagBits const msaaSamples = vk::SampleCountFlagBits::e1, vk::Format format = vk::Format::eB8G8R8A8Unorm, bool hostImage = false, bool const bDedicatedMemory = false) {
		vk::ImageCreateInfo info;
		info.flags = {};
		info.imageType = vk::ImageType::e2D;
		info.format = format;
		info.extent = vk::Extent3D{ width, height, 1U };
		info.mipLevels = mipLevels;
		info.arrayLayers = 1;
		info.samples = msaaSamples;
		info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
		info.usage = ImageUsage | vk::ImageUsageFlagBits::eStorage;
		info.sharingMode = vk::SharingMode::eExclusive;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;
		info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
		create(device, info, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
	}
private:
};

class TextureImageStorage3D : public GenericImage {
public:
	TextureImageStorage3D() {
	}

	// Image Usage can be a combination of
	// vk::ImageUsageFlagBits::eSampled (if image will be read from pixel shader)
	// vk::ImageUsageFlagBits::eStorage (if image will be read or written to in computer shader)
	// vk::ImageUsageFlagBits::eTransferSrc (if image is a source for a transfer/copy from/... image operation)
	// vk::ImageUsageFlagBits::eTransferDst (if image is a destination for a transfer/copy to/clear image operation)
	// TextureImageStorage3D is specific for compute shaders
	// and the Image Usage should be specific aswell to optimize access to image resource
	TextureImageStorage3D(vk::ImageUsageFlags const ImageUsage, vk::Device device, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels = 1U, vk::Format format = vk::Format::eB8G8R8A8Unorm, bool hostImage = false, bool const bDedicatedMemory = false) {
		vk::ImageCreateInfo info;
		info.flags = {};
		info.imageType = vk::ImageType::e3D;
		info.format = format;
		info.extent = vk::Extent3D{ width, height, depth };
		info.mipLevels = mipLevels;
		info.arrayLayers = 1;
		info.samples = vk::SampleCountFlagBits::e1;
		info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
		info.usage = ImageUsage | vk::ImageUsageFlagBits::eStorage;
		info.sharingMode = vk::SharingMode::eExclusive;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;
		info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
		create(device, info, vk::ImageViewType::e3D, vk::ImageAspectFlagBits::eColor, hostImage, bDedicatedMemory);
	}
private:
};

/// A cube map texture image living on the GPU or a staging buffer visible to the CPU.
class TextureImageCube : public GenericImage {
public:
  TextureImageCube() {
  }

  TextureImageCube(vk::Device device, uint32_t width, uint32_t height, uint32_t mipLevels=1, vk::Format format = vk::Format::eB8G8R8A8Unorm, bool hostImage = false) {
    vk::ImageCreateInfo info;
    info.flags = {vk::ImageCreateFlagBits::eCubeCompatible};
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = mipLevels;
    info.arrayLayers = 6;
    info.samples = vk::SampleCountFlagBits::e1;
    info.tiling = hostImage ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
    info.usage = vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eTransferDst;
    info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = hostImage ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined;
    create(device, info, vk::ImageViewType::eCube, vk::ImageAspectFlagBits::eColor, hostImage);
  }
private:
};

/// An image to use as a depth buffer on a renderpass.
class DepthAttachmentImage : public GenericImage {	// default format is 32bit float depth, no stencil
public:
	DepthAttachmentImage() {
  }
	// D32 FLOAT is the best choice, as in most cases it is faster than D16 !!! due to DCC (Compression) 
	// note: ** if sampled, subsequent operations (test/writes) on depth buffer are uncompressed for the current frame, resulting in a performance hit
    // D32 FLOAT format is same performance as D24 INT formats on GCN gpu hardware.
	// also noticed that D32 FLOAT with or without stencil is the only depth formats that can use "optimal" image memory (vulkan caps viewer)
    // D16 has a slight performance boost due to requiring less memory bandwidth
	// D16 Precision seems to be enough for orthographic projection (linear z buffer!!!) - noticed some artifacts while raymarching - switch to 32bit float instead
	DepthAttachmentImage(vk::Device device, uint32_t const width, uint32_t const height, vk::SampleCountFlagBits const msaaSamples, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue, bool const isSampled = false, bool const isInputAttachment = false) {
    vk::ImageCreateInfo info;
    info.flags = {};

    info.imageType = vk::ImageType::e2D;
    info.format = vk::Format::eD32Sfloat;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = msaaSamples;
    info.tiling = vk::ImageTiling::eOptimal;
	info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | (isSampled ? vk::ImageUsageFlagBits::eSampled : vk::ImageUsageFlagBits::eTransientAttachment) | (isInputAttachment ? vk::ImageUsageFlagBits::eInputAttachment : (vk::ImageUsageFlagBits)0);
	info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = vk::ImageLayout::eUndefined;
	
    typedef vk::ImageAspectFlagBits iafb;
    create(device, info, vk::ImageViewType::e2D, iafb::eDepth, false, true);

	vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {
		setLayout(cb, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth);
	});
  }
private:
};

/// An image to use as a stencil buffer on a renderpass.
class StencilAttachmentImage : public GenericImage {	// default format is stencil nothing else
public:
	StencilAttachmentImage() {
	}
	StencilAttachmentImage(vk::Device device, uint32_t const width, uint32_t const height, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue) {
		vk::ImageCreateInfo info;
		info.flags = {};

		info.imageType = vk::ImageType::e2D;
		info.format = vk::Format::eS8Uint;
		info.extent = vk::Extent3D{ width, height, 1U };
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = vk::SampleCountFlagBits::e1;
		info.tiling = vk::ImageTiling::eOptimal;
		info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		info.sharingMode = vk::SharingMode::eExclusive;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;
		info.initialLayout = vk::ImageLayout::eUndefined;

		typedef vk::ImageAspectFlagBits iafb;
		create(device, info, vk::ImageViewType::e2D, iafb::eStencil, false, true);

		vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {
			setLayout(cb, vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal, vk::ImageAspectFlagBits::eStencil);
		});
	}
private:
};


// for down sampled copy and full resolution copy too
class DepthImage : public GenericImage {	// default format is 32bit float color
public:
	DepthImage() {
	}
	// R32F
	DepthImage(vk::Device device, uint32_t const width, uint32_t const height, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue, bool const isColorAttachment, bool const isStorage, vk::Format const format = vk::Format::eR32Sfloat) {
		vk::ImageCreateInfo info;
		info.flags = {};

		info.imageType = vk::ImageType::e2D;
		info.format = format;
		info.extent = vk::Extent3D{ width, height, 1U };
		info.mipLevels = 1U;
		info.arrayLayers = 1;
		info.samples = vk::SampleCountFlagBits::e1;
		info.tiling = vk::ImageTiling::eOptimal;
		info.usage = vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled | (isColorAttachment ? vk::ImageUsageFlagBits::eColorAttachment : (vk::ImageUsageFlagBits)0) | (isStorage ? vk::ImageUsageFlagBits::eStorage : (vk::ImageUsageFlagBits)0);
		info.sharingMode = vk::SharingMode::eExclusive;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;
		info.initialLayout = vk::ImageLayout::eUndefined;

		typedef vk::ImageAspectFlagBits iafb;
		create(device, info, vk::ImageViewType::e2D, iafb::eColor, false, true);
	}
private:
};

/// An image to use as a colour buffer on a renderpass.
class ColorAttachmentImage : public GenericImage {
public:
  ColorAttachmentImage() {
  }

  ColorAttachmentImage(vk::Device device, uint32_t const width, uint32_t const height, vk::SampleCountFlagBits const msaaSamples, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue, bool const isSampled = false, bool const isInputAttachment = false, bool const isCopyable = false, vk::Format const format = vk::Format::eB8G8R8A8Unorm, vk::ImageUsageFlags const additional_flags = (vk::ImageUsageFlagBits)0 ) {
    vk::ImageCreateInfo info;
    info.flags = {};

    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = vk::Extent3D{ width, height, 1U };
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = msaaSamples;
    info.tiling = vk::ImageTiling::eOptimal;
	
	info.usage = vk::ImageUsageFlagBits::eColorAttachment | additional_flags;
	if (isSampled) {
		info.usage |= vk::ImageUsageFlagBits::eSampled;
	}
	if (isInputAttachment) {
		info.usage |= vk::ImageUsageFlagBits::eInputAttachment;
	}
	if (isCopyable) {
		info.usage |= vk::ImageUsageFlagBits::eTransferSrc;
	}

	if (!isSampled && !isCopyable) {
		info.usage |= vk::ImageUsageFlagBits::eTransientAttachment;
	}

	info.sharingMode = vk::SharingMode::eExclusive;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.initialLayout = vk::ImageLayout::eUndefined;
    typedef vk::ImageAspectFlagBits iafb;
    create(device, info, vk::ImageViewType::e2D, iafb::eColor, false, true);
	// bug fix - transition to colorattachmentoptimal immediately 
	// so that render target can be "loaded" instead of cleared if need be
	vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {
		setLayout(cb, vk::ImageLayout::eColorAttachmentOptimal);
	});

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
    s.info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    s.info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    s.info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    s.info.mipLodBias = 0.0f;
    s.info.anisotropyEnable = VK_FALSE;
    s.info.maxAnisotropy = 0.0f;
    s.info.compareEnable = 0;
    s.info.compareOp = vk::CompareOp::eAlways;
    s.info.minLod = 0;
    s.info.maxLod = 5.0f;
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
  SamplerMaker& addressModeUVW(vk::SamplerAddressMode const value) { s.info.addressModeU = s.info.addressModeV = s.info.addressModeW = value; return *this; }
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
  vk::UniqueSampler createUnique(vk::Device device) const {
    return device.createSamplerUnique(s.info).value;
  }

  /// Allocate a non self-deleting Sampler.
  vk::Sampler create(vk::Device device) const {
    return device.createSampler(s.info).value;
  }

private:
  struct State {
    vk::SamplerCreateInfo info;
  };

  State s;
};

/// KTX files use OpenGL format values. This converts some common ones to Vulkan equivalents.
STATIC_INLINE_PURE vk::Format const GLtoVKFormat(uint32_t const glFormat) {
  switch (glFormat) {
    case 0x8229: return vk::Format::eR8Unorm;
    case 0x1903: return vk::Format::eR8Unorm;

	case 0x822B: return vk::Format::eR8G8Unorm;
	case 0x8227: return vk::Format::eR8G8Unorm;

    case 0x1907: return vk::Format::eR8G8B8Unorm; // GL_RGB
	case 0x8C41: return vk::Format::eR8G8B8Srgb;  // GL_RGB

	case 0x8058: return vk::Format::eR8G8B8A8Unorm; // GL_RGBA
    case 0x1908: return vk::Format::eR8G8B8A8Unorm; // GL_RGBA
	case 0x8C43: return vk::Format::eR8G8B8A8Srgb; // GL_RGBA

    case 0x83F0: return vk::Format::eBc1RgbUnormBlock; // GL_COMPRESSED_RGB_S3TC_DXT1_EXT
    case 0x83F1: return vk::Format::eBc1RgbaUnormBlock; // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
    case 0x83F2: return vk::Format::eBc3UnormBlock; // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
    case 0x83F3: return vk::Format::eBc5UnormBlock; // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
	case 0x8E8C: return vk::Format::eBc7UnormBlock;	// GL_COMPRESSED_RGBA_BPTC_UNORM_ARB
	case 0x8E8D: return vk::Format::eBc7SrgbBlock;
  }
  return vk::Format::eUndefined;
}



/// Layout of a KTX file in a buffer.
template<bool const WorkaroundLayerSizeDoubledInFileBug = false>
class KTXFileLayout {
public:
  KTXFileLayout() {
  }

  KTXFileLayout(uint8_t const* const __restrict begin, uint8_t const* const __restrict end) {
    uint8_t const * p = begin;
    if (p + sizeof(Header) > end) return;
    header = *(Header*)p;
    static constexpr uint8_t magic[] = {
      0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
    };
    
    if (memcmp(magic, header.identifier, sizeof(magic))) {
      return;
    }

	static constexpr uint32_t const KTX_ENDIAN_REF(0x04030201);
    if (KTX_ENDIAN_REF != header.endianness) {
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

    format_ = GLtoVKFormat(header.glInternalFormat);
    if (format_ == vk::Format::eUndefined) return;

    p += sizeof(Header);
    if (p + header.bytesOfKeyValueData > end) return;

    for (uint32_t i = 0; i < header.bytesOfKeyValueData; ) {
      uint32_t keyAndValueByteSize = *(uint32_t*)(p + i);
      if (KTX_ENDIAN_REF != header.endianness) swap(keyAndValueByteSize);
      std::string kv(p + i + 4, p + i + 4 + keyAndValueByteSize);
      i += keyAndValueByteSize + 4;
      i = (i + 3) & ~3;
    }

    p += header.bytesOfKeyValueData;
    for (uint32_t mipLevel = 0; mipLevel != header.numberOfMipmapLevels; ++mipLevel) {

		// bugfix for arraylayers and faces not being factored into final size for this mip
        uint32_t layerImageSize;
        if constexpr (WorkaroundLayerSizeDoubledInFileBug) {  // KTX ImageViewer export array texture doubles layer size written to file, sometimes...
            layerImageSize = *(uint32_t*)(p) / header.numberOfArrayElements;
        }
        else {
            layerImageSize = *(uint32_t*)(p);
        }

	  layerImageSize = (layerImageSize + 3) & ~3;
	  if (KTX_ENDIAN_REF != header.endianness) swap(layerImageSize);

	  layerImageSizes_.push_back(layerImageSize);
	  
      uint32_t imageSize = layerImageSize * header.numberOfFaces * header.numberOfArrayElements;

	  imageSize = (imageSize + 3) & ~3;
	  if (KTX_ENDIAN_REF != header.endianness) swap(imageSize);

	  imageSizes_.push_back(imageSize);
	       
      p += 4; // offset for reading layer imagesize above
      imageOffsets_.push_back((uint32_t)(p - begin));

      if (p + imageSize > end) {
          // see https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glPixelStore.xhtml
          // fix bugs... https://github.com/dariomanesku/cmft/issues/29
          header.numberOfMipmapLevels = mipLevel + 1;
          break;
      }
      p += imageSize; // next mip offset if there is one
    }

    ok_ = true;
  }

  uint32_t offset(uint32_t mipLevel, uint32_t arrayLayer, uint32_t face) const {

    return imageOffsets_[mipLevel] + (arrayLayer * header.numberOfFaces + face) * layerImageSizes_[mipLevel];
  }

  uint32_t size(uint32_t mipLevel) {
    return imageSizes_[mipLevel];
  }

  bool ok() const { return ok_; }
  vk::Format format() const { return format_; }
  uint32_t mipLevels() const { return header.numberOfMipmapLevels; }
  uint32_t arrayLayers() const { return header.numberOfArrayElements; }
  uint32_t faces() const { return header.numberOfFaces; }
  uint32_t width(uint32_t mipLevel) const { return mipScale(header.pixelWidth, mipLevel); }
  uint32_t height(uint32_t mipLevel) const { return mipScale(header.pixelHeight, mipLevel); }
  uint32_t depth(uint32_t mipLevel) const { return mipScale(header.pixelDepth, mipLevel); }

  void upload(vk::Device device, vku::GenericImage & __restrict image, uint8_t const* const __restrict pFileBegin, vk::CommandPool commandPool, vk::Queue queue) const {
	  uint32_t totalActualSize(0);

	  for (auto const& size : imageSizes_) {
		  totalActualSize += size;
	  }

	  if (0 == totalActualSize)
		  return;

	  // bugfix: sometimes the image size is greater than the actual binary size of the data, due to an "upgrade" in alignment
	  // so source buffer must have the same size as the image being copied too. The copy into the source buffer only copies the actual size of data,
	  // with the rest being zeroed out.
	  vk::DeviceSize const alignedSize(std::max(image.size(), (vk::DeviceSize)totalActualSize));

	vku::GenericBuffer stagingBuffer((vk::BufferUsageFlags)vk::BufferUsageFlagBits::eTransferSrc, alignedSize, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, vku::eMappedAccess::Sequential);
    
	uint32_t const baseOffset = offset(0, 0, 0);
	stagingBuffer.updateLocal(pFileBegin + baseOffset, totalActualSize);

    // Copy the staging buffer to the GPU texture and set the layout.
    vku::executeImmediately<false>(device, commandPool, queue, [&](vk::CommandBuffer cb) {
      vk::Buffer buf = stagingBuffer.buffer();
      for (uint32_t mipLevel = 0; mipLevel != mipLevels(); ++mipLevel) {
        auto width = this->width(mipLevel); 
        auto height = this->height(mipLevel); 
        auto depth = this->depth(mipLevel); 
        for (uint32_t face = 0; face != arrayLayers(); ++face) {
          image.copy(cb, buf, mipLevel, face, width, height, depth, (offset(mipLevel, face, 0) - baseOffset));
        }
      }
    });
  }
  void finalizeUpload(vk::Device const& __restrict device, vku::GenericImage& __restrict image, vk::CommandPool const& __restrict commandPool, vk::Queue const& __restrict queue,
	  vk::ImageLayout const FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal) const
  {

	  vku::executeImmediately(device, commandPool, queue, [&](vk::CommandBuffer cb) {

		  image.setLayout(cb, FinalLayout);

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

  Header header;
  vk::Format format_;
  bool ok_ = false;
  std::vector<uint32_t> imageOffsets_;
  std::vector<uint32_t> imageSizes_;
  std::vector<uint32_t> layerImageSizes_;
};


} // namespace vku

#endif // VKU_HPP
