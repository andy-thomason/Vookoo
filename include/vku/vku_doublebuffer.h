#pragma once
#include <Utility/class_helper.h>

namespace vku
{
	// Helper Class //
	template<typename T>
	struct alignas(CACHE_LINE_BYTES) double_buffer : no_copy
	{
		static constexpr uint32_t const count = 2u;

		T								data[count];

		__declspec(safebuffers) __forceinline T const& __restrict operator[](uint32_t const i) const {
			return(data[i]);
		}
		__declspec(safebuffers) __forceinline T& __restrict operator[](uint32_t const i) {
			return(data[i]);
		}

		double_buffer(T&& __restrict a, T&& __restrict b)
			: data{ std::forward<T&& __restrict>(a), std::forward<T&& __restrict>(b) }
		{}
		double_buffer(T const& __restrict a, T const& __restrict b)
			: data{ a, b }
		{}

		double_buffer(double_buffer&& __restrict moved)
			: data{ std::move(moved.data) }
		{}
		double_buffer const& operator=(double_buffer&& __restrict moved) {

			std::move(moved.data, moved.data + 1, data);
			return(*this);
		}

		constexpr double_buffer() // allow constinit optimization. data also must have constexpr ctor
			: data{}
		{}
		~double_buffer() = default;
	};

}; // end ns



