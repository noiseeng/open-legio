/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "hui_const.h"
#include <algorithm>
#include <stdint.h>

namespace lac
{
	struct hui_filter_accel
	{
		int32_t time_since_last_enc = 0;
		int32_t accel_count			= 0;
		int32_t last_read			= 0;
		int32_t max_accel			= 8;

		constexpr int32_t operator()(int32_t now, int32_t delay, int32_t enc_read)
		{
			int32_t scale = 1;

			if(enc_read != 0)
			{
				if(hui_clk_sub(now, time_since_last_enc) < delay && last_read == enc_read)
				{
					accel_count += 1;
					scale *= std::min(accel_count, max_accel);
				}
				else
				{
					accel_count = 0;
				}

				time_since_last_enc = now;

				last_read = enc_read;
			}

			return scale * enc_read;
		}
	};
} // namespace lac