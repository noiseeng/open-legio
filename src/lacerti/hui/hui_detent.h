/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "hui_const.h"
#include <stdint.h>

namespace lac
{
	struct hui_detent
	{
		int32_t count = 1;

		constexpr hui_dir operator()(int32_t div, hui_dir dir)
		{
			count += dir;

			if(count >= div)
			{
				count -= div;
				return HUI_UP;
			}

			if(count < 0)
			{
				count += div;
				return HUI_DN;
			}

			return HUI_NONE;
		}
	};
} // namespace lac