/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <bug/compiler.h>
#include <bug/eabi.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace lac
{
	template <typename cfg_t>
	struct cv_calibration
	{
		bool  valid	 = false;
		float scale	 = cfg_t::SCALE;
		float offset = cfg_t::OFFSET;
		float minv	 = cfg_t::CAL_MINV;
		float maxv	 = cfg_t::CAL_MAXV;

		static constexpr float lerp(float t, float a, float b) { return (b - a) * t + a; }

		constexpr float operator()(float value) const { return scale * lerp(value, maxv, minv) + offset; }
	};

	template <typename timer_t, typename cfg_t>
	struct cv_calibrate
	{
		using calibration = cv_calibration<cfg_t>;

		struct stable_reading
		{
			float	 value;
			float	 delta;
			uint32_t retries;
		};

		static constexpr float	ref_err	  = .2f;
		static constexpr float	ref_volts = cfg_t::REF_VOLTS;
		static constexpr float	minv	  = cfg_t::MINV;
		static constexpr float	maxv	  = cfg_t::MAXV;
		static constexpr float	precision = .001f;
		static constexpr size_t ave_count = cfg_t::AVE_COUNT;

		template <class get_pitch_t>
		static float average_pitch(get_pitch_t&& get_pitch)
		{
			float x = 0;

			for(uint32_t i = 0; i < ave_count; i++)
			{
				x += get_pitch();
			}

			x /= (float)ave_count;

			bug_assert(x >= 0.0f);
			bug_assert(x <= 1.0f);

			return calibration::lerp(x, maxv, minv);
		}

		template <class get_pitch_t>
		static stable_reading stable_pitch(get_pitch_t&& get_pitch)
		{
			float last = average_pitch([&]() { return get_pitch(); });
			float next = average_pitch([&]() { return get_pitch(); });

			uint32_t retries = 0;
			for(uint32_t i = 0; i < 10; i++)
			{
				if(std::fabs(next - last) < precision)
				{
					break;
				}

				last = next;
				next = average_pitch([&]() { return get_pitch(); });
				retries++;
			}

			float delta = std::fabs(next - last);

			return {last, delta, retries};
		}

		template <class get_pitch_t, class set_cal_t>
		static calibration cal(const char* input, get_pitch_t&& get_pitch, set_cal_t&& set_cal)
		{
			const uint32_t cal_start = timer_t::get_clk();

			float scale	 = 0.0f;
			float offset = 0.0f;

			set_cal(false);

			timer_t::delay_ms(100);

			stable_reading mid = stable_pitch([&]() { return get_pitch(); });

			set_cal(true);

			timer_t::delay_ms(100);

			stable_reading bot = stable_pitch([&]() { return get_pitch(); });

			float d0 = mid.value - bot.value;

			bool monotonic = mid.value > bot.value;
			bool in_range  = std::fabs(d0 - ref_volts) < ref_err;

			int elapsed_ms = (int)timer_t::clk_to_us(timer_t::get_clk() - cal_start) / 1000;

			if(monotonic && in_range)
			{
				float s0 = ref_volts / d0;

				scale  = s0;
				offset = -scale * bot.value;

				float fm = scale * mid.value;
				bug::ignore(fm);
				float fb = scale * bot.value;
				bug::ignore(fb);

				bug_assert(std::fabs(fm - fb - ref_volts) < precision);

				bug_print("cal %s win %dms ref %f d %f r %u gnd %f d %f r %u scale %f offset %f\n",
						  input,
						  elapsed_ms,
						  (double)mid.value,
						  (double)mid.delta,
						  (unsigned)mid.retries,
						  (double)bot.value,
						  (double)bot.delta,
						  (unsigned)bot.retries,
						  (double)scale,
						  (double)offset);

				return {true, scale, offset, minv, maxv};
			}
			else
			{
				bug_print("cal %s fail %dms ref %f d %f r %u gnd %f d %f r %u err %f mono %d range %d\n",
						  input,
						  elapsed_ms,
						  (double)mid.value,
						  (double)mid.delta,
						  (unsigned)mid.retries,
						  (double)bot.value,
						  (double)bot.delta,
						  (unsigned)bot.retries,
						  (double)std::fabs(d0 - ref_volts),
						  monotonic ? 1 : 0,
						  in_range ? 1 : 0);
				return {false};
			}
		}
	};
} // namespace lac
