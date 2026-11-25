/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <numerus/color.h>
#include <stdint.h>
#include <utl/bug.h>

namespace lac
{
	template <uint32_t color_count>
	struct hui_color_mode
	{
		hui_color_mode* next = nullptr;

		std::array<numerus::color, color_count> colors = {};

		bool	enabled	  = false;
		bool	use_timer = false;
		int32_t time_left = 0;

		int32_t last_time = 0;

		constexpr void update(int32_t time_passed)
		{
			lac_assert(next != this);

			for(hui_color_mode* m = this; m; m = m->next)
			{
				if(!m->use_timer)
				{
					continue;
				}

				if(m->time_left)
				{
					m->time_left -= time_passed;
				}

				if(m->time_left <= 0)
				{
					m->time_left = 0;
					m->enabled	 = false;
				}
			}
		}

		constexpr const numerus::color* get(int32_t current_time)
		{
			update(current_time - last_time);
			last_time = current_time;

			return get();
		}

		constexpr const numerus::color* get() const
		{
			for(const hui_color_mode* m = this; m; m = m->next)
			{
				if(m->enabled || !m->next)
				{
					return m->colors.data();
				}
			}

			return colors.data();
		}
	};
} // namespace lac
