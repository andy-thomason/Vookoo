#pragma once

// no includes of vulkan dependencies allowed in this file //

#define BETTER_ENUMS_CONSTEXPR
#define BETTER_ENUMS_NO_EXCEPTIONS
#include <Utility/enum.h>

#include <fmt/fmt.h>

#include "vkNames.h"
#include <Math/point2D_t.h>

#define SHADER_PATH SHADER_BINARY_DIR
#define SHADER_POSTQUAD L"postquad.vert.bin"
#define SHADER_CHECKERBOARD_EVEN L"stencilcheckerboard_even.frag.bin"
#define SHADER_CHECKERBOARD_ODD L"stencilcheckerboard_odd.frag.bin"
#define SHADER_CHECKERBOARD(path, odd) (odd ? path SHADER_CHECKERBOARD_ODD : path SHADER_CHECKERBOARD_EVEN)

#ifdef VKU_IMPLEMENTATION	// #define VKU_IMPLEMENTATION in a single cpp file just before inclusion of vku_framework.hpp
namespace vku
{
	struct resource_control
	{
	public:
		static __forceinline void stage_resources(uint32_t const resource_index);

	private:
		resource_control() = delete;
		~resource_control() = delete;
		resource_control(resource_control const&) = delete;
		resource_control(resource_control&&) = delete;
		resource_control const& operator-(resource_control const&) = delete;
		resource_control&& operator=(resource_control&&) = delete;
	};

}
#endif

#define PRINT_FEATURE(enabledFeature, text) { fmt::print(fg(enabledFeature ? (fmt::color::lime) : (fmt::color::red)), text "\n"); }

#define ADD_EXTENSION(extensions, dm, extensionname, result) \
{ \
	static constexpr char const szExtensionName[] = extensionname; \
	bool bFound(false); \
	for (auto extension : extensions) { \
		\
		if (0 == strncmp(extension.extensionName, szExtensionName, _countof(szExtensionName) + 1)) \
		{ \
			bFound = true; \
			break; \
		} \
	} \
	if (bFound) { \
		dm.extension(szExtensionName); \
		fmt::print(fg(fmt::color::lime), extensionname "\n"); \
		result = true; \
	} \
	else { \
		fmt::print(fg(fmt::color::red), extensionname "\n"); \
		result = false; \
	} \
} \

namespace vku {

	/* These constants are defined in vku_addon.hpp */ // vku.hpp line ~1550 for debuging when these constants are exceeded
	static constexpr uint32_t const // **even numbers only**
		MAX_NUM_DESCRIPTOR_SETS = 24,
		MAX_NUM_UNIFORM_BUFFERS = 8,
		MAX_NUM_IMAGES = 144,
		MAX_NUM_STORAGE_BUFFERS = 24,
		MAX_NUM_BUFFER_VIEWS = 2;

	static constexpr int32_t const
		ACCESS_READONLY(0),
		ACCESS_READWRITE(1),
		ACCESS_WRITEONLY(-1);

	STATIC_INLINE_PURE point2D_t const __vectorcall getDownResolution(point2D_t const frameBufferSz) {   // Downscaled resolution is always 50% of original resolution ** Half Resolution is almost identical, Third resolution is close but you can tell, Quarter resolution you can start to see the blockyness
		return(p2D_shiftr(frameBufferSz, 1));
	}

	BETTER_ENUM(eCheckerboard, uint32_t const,

		EVEN = 0,
		ODD
	);

	static constexpr uint32_t const STENCIL_CHECKER_REFERENCE = 0xffU;

	template<uint32_t const wide>
	struct CommandBufferContainer
	{
		std::vector<vk::UniqueCommandBuffer> cb[wide];
		std::vector<vk::Fence>				 fence[wide];
		std::vector<bool>					 queued[wide];
		std::vector<bool>					 recorded[wide];

		template<uint32_t const index_wide = 0U>
		uint32_t const size() const {
			return((uint32_t)cb[index_wide].size());
		}

		template<uint32_t const index_wide = 0U>
		void allocate(vk::Device const& device, vk::CommandBufferAllocateInfo const& cbai) {

			cb[index_wide] = device.allocateCommandBuffersUnique(cbai).value;

			uint32_t const numBuffers = (uint32_t)cb[index_wide].size();

			fence[index_wide].reserve(numBuffers);
			queued[index_wide].reserve(numBuffers);
			recorded[index_wide].reserve(numBuffers);
			
			vk::CommandBufferBeginInfo bi(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
			vk::FenceCreateInfo fci{};
			fci.flags = vk::FenceCreateFlagBits::eSignaled;
			for (uint32_t i = 0; i < numBuffers; ++i) {

				fence[index_wide].emplace_back(device.createFence(fci).value);
				queued[index_wide].emplace_back(true);					// initial state is queued because fence is signaled initially too
				recorded[index_wide].emplace_back(false);

				vk::CommandBuffer cb_(*(cb[index_wide][i]));
				cb_.begin(bi);
				cb_.end();
				
			}
		}

		void release(vk::Device& device)
		{
			for (uint32_t i = 0; i < wide; ++i) {

				for (auto& f : fence[i]) {
					device.destroyFence(f);
				}
				for (auto& c : cb[i]) {
					c.reset();
				}
			}
		}

		CommandBufferContainer() = default;
		CommandBufferContainer(vk::Device const& device, vk::CommandBufferAllocateInfo const& cbai) {
			allocate(device, cbai);
		}
	};

	// for avoiding lamda heap
	typedef void const(* const execute_function)(vk::CommandBuffer&& __restrict);

	typedef struct {
		vk::CommandBuffer cb_transfer;
		vk::CommandBuffer cb_transfer_light;
		vk::CommandBuffer cb_render_light;
		// [[deprecated]] vk::CommandBuffer cb_render_texture;
		uint32_t resource_index;

	} compute_pass;
	typedef bool const(*const compute_function)(compute_pass&& __restrict);

	typedef struct {
		vk::CommandBuffer cb;
		uint32_t resource_index;

		vk::RenderPassBeginInfo&& __restrict rpbiZ;
		vk::RenderPassBeginInfo&& __restrict rpbiHalf;
		vk::RenderPassBeginInfo&& __restrict rpbiFull;
		vk::RenderPassBeginInfo&& __restrict rpbiMid;
		vk::RenderPassBeginInfo&& __restrict rpbiOff;

	} static_renderpass;
	typedef void(*const static_renderpass_function)(static_renderpass&& __restrict);
	typedef void(*static_renderpass_function_unconst)(static_renderpass&& __restrict);

	typedef struct {
		vk::CommandBuffer cb;
		uint32_t resource_index;

	} dynamic_renderpass;
	typedef void(*const dynamic_renderpass_function)(dynamic_renderpass&& __restrict);

	typedef struct {
		vk::CommandBuffer* __restrict cb_transfer;
		vk::CommandBuffer* __restrict cb_render;
		uint32_t resource_index;

		vk::RenderPassBeginInfo&& __restrict rpbi;
		
	} overlay_renderpass;
	typedef void(*const overlay_renderpass_function)(overlay_renderpass&& __restrict);


	typedef struct {
		
		vk::CommandBuffer cb;
		uint32_t resource_index;

		vk::RenderPassBeginInfo&& __restrict rpbi;
		
	} clear_renderpass;
	typedef void(* const clear_renderpass_function)(clear_renderpass&& __restrict);
	typedef void(*clear_renderpass_function_unconst)(clear_renderpass&& __restrict);
	
	typedef struct {
				
		vk::CommandBuffer cb;
		uint32_t resource_index;

		vk::RenderPassBeginInfo&& __restrict rpbi;

	} present_renderpass;
	typedef void(*const present_renderpass_function)(present_renderpass&& __restrict);
	typedef void(*present_renderpass_function_unconst)(present_renderpass&& __restrict);
	
	typedef void(* const gpu_readback_function)(vk::CommandBuffer&, uint32_t const);
}