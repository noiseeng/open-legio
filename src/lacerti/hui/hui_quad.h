/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "hui_const.h"
#include <array>
#include <cstddef>
#include <stdint.h>

namespace lac
{
	enum hui_quad : uint8_t
	{
		HUI_QUAD_INV,
		HUI_QUAD_SAM,
		HUI_QUAD_CW,
		HUI_QUAD_CCW
	};

	inline constexpr std::array<hui_quad, 16> HUI_QUAD_TABLE = {HUI_QUAD_SAM,
																HUI_QUAD_CW,
																HUI_QUAD_CCW,
																HUI_QUAD_INV,
																HUI_QUAD_CCW,
																HUI_QUAD_SAM,
																HUI_QUAD_INV,
																HUI_QUAD_CW,
																HUI_QUAD_CW,
																HUI_QUAD_INV,
																HUI_QUAD_SAM,
																HUI_QUAD_CCW,
																HUI_QUAD_INV,
																HUI_QUAD_CCW,
																HUI_QUAD_CW,
																HUI_QUAD_SAM};

	constexpr uint32_t hui_quad_bits(bool a, bool b)
	{ return (a ? 1u : 0u) + (b ? 2u : 0u); }

	constexpr hui_dir hui_quad_dir(hui_quad q)
	{
		switch(q)
		{
			case HUI_QUAD_CW: return HUI_UP;
			case HUI_QUAD_CCW: return HUI_DN;
			default: return HUI_NONE;
		}
	}

	inline constexpr std::array<hui_dir, 16> HUI_ENC_TABLE = []() constexpr {
		std::array<hui_dir, 16> arr{};
		for(size_t i = 0; i < 16; ++i)
		{
			arr[i] = hui_quad_dir(HUI_QUAD_TABLE[i]);
		}
		return arr;
	}();
} // namespace lac
