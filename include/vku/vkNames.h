#pragma once

namespace vkNames
{
	namespace CommandBuffer
	{
		//[[deprecated]] [[maybe_unused]] static inline constexpr char const* const COMPUTE_TEXTURE = "[cb] Compute Textures";
		[[maybe_unused]] static inline constexpr char const* const COMPUTE_LIGHT = "[cb] Compute Light";
		[[maybe_unused]] static inline constexpr char const* const TRANSFER_LIGHT = "[cb] Transfer Light";
		[[maybe_unused]] static inline constexpr char const* const TRANSFER = "[cb] Transfer";
		[[maybe_unused]] static inline constexpr char const* const STATIC = "[cb] Static";
		[[maybe_unused]] static inline constexpr char const* const GPU_READBACK = "[cb] Gpu Readback";
		[[maybe_unused]] static inline constexpr char const* const DYNAMIC = "[cb] Dynamic";
		[[maybe_unused]] static inline constexpr char const* const PRESENT = "[cb] Present";
		[[maybe_unused]] static inline constexpr char const* const CLEAR = "[cb] Clears";
		[[maybe_unused]] static inline constexpr char const* const OVERLAY_RENDER = "[cb] Overlay Render";
		[[maybe_unused]] static inline constexpr char const* const OVERLAY_TRANSFER = "[cb] Overlay Transfer";
		[[maybe_unused]] static inline constexpr char const* const CHECKERBOARD = "[cb] Checkerboard";
	}; // end ns

	namespace FrameBuffer
	{
		[[maybe_unused]] static inline constexpr char const* const CLEAR = "[fb] Clears";
		[[maybe_unused]] static inline constexpr char const* const PRESENT = "[fb] Present";
		[[maybe_unused]] static inline constexpr char const* const POSTAA = "[fb] PostAA";
		[[maybe_unused]] static inline constexpr char const* const COLOR_DEPTH = "[fb] Color Depth";
		[[maybe_unused]] static inline constexpr char const* const OFFSCREEN = "[fb] Offscreen Color Depth";
		[[maybe_unused]] static inline constexpr char const* const MID_COLOR_DEPTH = "[fb] Mid Color Depth";
		[[maybe_unused]] static inline constexpr char const* const FULL_COLOR_ONLY = "[fb] Full Color Only";
		[[maybe_unused]] static inline constexpr char const* const HALF_COLOR_ONLY = "[fb] Half Color Only";
		[[maybe_unused]] static inline constexpr char const* const DEPTH = "[fb] Depth";
	}

	namespace Renderpass
	{
		[[maybe_unused]] static inline constexpr char const* const CLEAR = "[rp] Clears";
		[[maybe_unused]] static inline constexpr char const* const FINAL = "[rp] Final";
		[[maybe_unused]] static inline constexpr char const* const POSTAA = "[rp] PostAA";
		[[maybe_unused]] static inline constexpr char const* const OVERLAY = "[rp] Overlay";
		[[maybe_unused]] static inline constexpr char const* const OFFSCREEN = "[rp] Offscreen";
		[[maybe_unused]] static inline constexpr char const* const MIDPASS = "[rp] Mid";
		[[maybe_unused]] static inline constexpr char const* const UPPASS = "[rp] Up";
		[[maybe_unused]] static inline constexpr char const* const DOWNPASS = "[rp] Down";
		[[maybe_unused]] static inline constexpr char const* const ZPASS = "[rp] Z";
	}

	namespace Queue
	{
		[[maybe_unused]] static inline constexpr char const* const GRAPHICS = "[q] Graphics";
		[[maybe_unused]] static inline constexpr char const* const COMPUTE = "[q] Compute";
		[[maybe_unused]] static inline constexpr char const* const TRANSFER = "[q] Transfer";
	}

	namespace Image
	{
		[[maybe_unused]] static inline constexpr char const* const colorImage = "[img] colorImage";
		[[maybe_unused]] static inline constexpr char const* const depthImage = "[img] depthImage";
		[[maybe_unused]] static inline constexpr char const* const depthImageResolve = "[img] depthImageResolve";
		[[maybe_unused]] static inline constexpr char const* const mouseImage_multisampled = "[img] mouseImage_multisampled";
		[[maybe_unused]] static inline constexpr char const* const mouseImage_resolved = "[img] mouseImage_resolved";
		[[maybe_unused]] static inline constexpr char const* const stencilCheckerboard = "[img] stencilCheckerboard";
		[[maybe_unused]] static inline constexpr char const* const lastColorImage = "[img] lastColorImage";
		[[maybe_unused]] static inline constexpr char const* const colorVolumetricImage_checkered = "[img] colorVolumetricImage_checkered";
		[[maybe_unused]] static inline constexpr char const* const colorVolumetricImage_resolved = "[img] colorVolumetricImage_resolved";
		[[maybe_unused]] static inline constexpr char const* const colorVolumetricImage_upsampled = "[img] colorVolumetricImage_upsampled";
		[[maybe_unused]] static inline constexpr char const* const colorReflectionImage_checkered = "[img] colorReflectionImage_checkered";
		[[maybe_unused]] static inline constexpr char const* const colorReflectionImage_resolved = "[img] colorReflectionImage_resolved";
		[[maybe_unused]] static inline constexpr char const* const colorReflectionImage_upsampled = "[img] colorReflectionImage_upsampled";
		[[maybe_unused]] static inline constexpr char const* const guiImage = "[img] guiImage";
		[[maybe_unused]] static inline constexpr char const* const offscreenImage = "[img] offscreenImage";
		[[maybe_unused]] static inline constexpr char const* const PingPongMap = "[img] PingPongMap";
		[[maybe_unused]] static inline constexpr char const* const LightProbeMap = "[img] LightProbeMap";
		[[maybe_unused]] static inline constexpr char const* const LightMap_DistanceDirection = "[img] LightMap_DistanceDirection";
		[[maybe_unused]] static inline constexpr char const* const LightMap_Color = "[img] LightMap_Color";
		[[maybe_unused]] static inline constexpr char const* const LightMap_Reflection = "[img] LightMap_Reflection";
		[[maybe_unused]] static inline constexpr char const* const OpacityMap = "[img] OpacityMap";
	}

	namespace Buffer
	{
		[[maybe_unused]] static inline constexpr char const* const SUBGROUP_LAYER_COUNT = "[buf] Subgroup Layer Count";
		[[maybe_unused]] static inline constexpr char const* const SHARED = "[buf] Shared";
		[[maybe_unused]] static inline constexpr char const* const TERRAIN = "[buf] Terrain";
		[[maybe_unused]] static inline constexpr char const* const ROAD = "[buf] Road"; 
		[[maybe_unused]] static inline constexpr char const* const VOXEL_STATIC = "[buf] Voxel Static"; 
		[[maybe_unused]] static inline constexpr char const* const VOXEL_DYNAMIC = "[buf] Voxel Dynamic";
		[[maybe_unused]] static inline constexpr char const* const VOXEL_SHARED_UNIFORM = "[buf] Voxel Shared Uniform";
	}
}; // end ns



