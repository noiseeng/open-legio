/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "legio_inp.h"
#include "legio_out.h"
#include <numerus/color.h>

namespace legio
{
	template <uint32_t frame_count>
	struct vi_example
	{
		using stereo = std::array<float, 2>;

		struct rgb
		{
			float r = 0.0f;
			float g = 0.0f;
			float b = 0.0f;
		};

		enum input_mode : int32_t
		{
			INPUT_OFF		  = 0,
			INPUT_PASSTHROUGH = 1,
			INPUT_PM		  = 2,
		};

		struct controls
		{
			float	pitch		 = 0.0f;
			float	knob_left	 = 0.0f;
			float	knob_right	 = 0.0f;
			int32_t switch_left	 = 0;
			int32_t switch_right = 0;
			bool	sync		 = false;

			constexpr controls() = default;

			constexpr controls(const legio_inp& vi)
			: pitch(vi.p0), knob_left(vi.k1), knob_right(vi.k2), switch_left(vi.swl), switch_right(vi.swr), sync(vi.sync_trig)
			{}
		};

		using ctl_t = controls;

		constexpr static const char*	NAME		  = "vi_example";
		constexpr static uint32_t		FRAME_COUNT	  = frame_count;
		constexpr static numerus::color COLOR_LEFT	  = numerus::color::yellow();
		constexpr static numerus::color COLOR_RIGHT	  = numerus::color::cyan();
		constexpr static bool			ENC_ACCEL	  = false;
		constexpr static bool			SHIPPED		  = false;
		constexpr static bool			IS_OSCILLATOR = true;

		constexpr static uint32_t				 MAX_VOICES	 = 7;
		constexpr static std::array<uint32_t, 3> VOICES		 = {1, 3, 7};
		constexpr static std::array<float, 3>	 SPREAD		 = {0.0f, 0.1f, 0.3f};
		constexpr static float					 C0_HZ		 = 16.35f;
		constexpr static float					 FOLD_GAIN	 = 8.0f;
		constexpr static float					 PM_DEPTH	 = 0.5f;
		constexpr static float					 OSC_LEVEL	 = 0.5f;
		constexpr static float					 INPUT_LEVEL = 0.5f;
		constexpr static float					 MAX_HZ		 = 20000.0f;

		float						  frame_rate = 96000.0f;
		std::array<float, MAX_VOICES> phase		 = {};
		std::array<float, MAX_VOICES> dt		 = {};
		uint32_t					  voices	 = 1;
		float						  morph		 = 0.0f;
		float						  fold		 = 1.0f;
		input_mode					  mode		 = INPUT_OFF;

		template <class alloc_t>
		void set_frame_rate(float fps, alloc_t*)
		{ frame_rate = fps; }

		void silence() { phase = {}; }

		static float clamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

		static uint32_t position(int32_t sw) { return (uint32_t)(sw < 0 ? 0 : (sw > 2 ? 2 : sw)); }

		static float voice_offset(uint32_t i, uint32_t n)
		{
			if(n == 1)
			{
				return 0.0f;
			}
			const float half = (float)(n - 1) * 0.5f;
			return ((float)i - half) / half;
		}

		static constexpr numerus::color to_color(const rgb& c)
		{ return {(int)(c.r * 255.0f), (int)(c.g * 255.0f), (int)(c.b * 255.0f)}; }

		legio_out control(const controls& ctl)
		{
			const uint32_t pos = position(ctl.switch_right);
			voices			   = VOICES[pos];
			morph			   = clamp(ctl.knob_left, 0.0f, 1.0f);
			fold			   = 1.0f + clamp(ctl.knob_right, 0.0f, 1.0f) * FOLD_GAIN;
			mode			   = (input_mode)position(ctl.switch_left);

			const float hz = C0_HZ * std::exp2(ctl.pitch);
			for(uint32_t i = 0; i < voices; i++)
			{
				const float detune = std::exp2(voice_offset(i, voices) * SPREAD[pos] / 12.0f);
				dt[i]			   = clamp(hz * detune, 0.0f, MAX_HZ) / frame_rate;
			}

			if(ctl.sync)
			{
				silence();
			}

			const float k2	  = clamp(ctl.knob_right, 0.0f, 1.0f);
			const rgb	left  = {morph, morph, 0.0f};
			const rgb	right = {0.0f, k2, k2};
			return legio_out{to_color(left), to_color(right)};
		}

		static float poly_blep(float t, float dt)
		{
			if(t < dt)
			{
				t /= dt;
				return t + t - t * t - 1.0f;
			}
			if(t > 1.0f - dt)
			{
				t = (t - 1.0f) / dt;
				return t * t + t + t + 1.0f;
			}
			return 0.0f;
		}

		static float wrap(float x) { return x - std::floor(x); }

		static float nsin(float t)
		{
			const float s = t > 0.5f ? -8.0f : 8.0f;

			t = std::fabs(t - 0.5f);

			return s * (-2.0f * t * t + t);
		}

		static float fold_tri(float x) { return 1.0f - std::fabs(x + 1.0f - 4.0f * std::floor((x + 1.0f) * 0.25f) - 2.0f); }

		float voice(uint32_t i, float pm)
		{
			const float t	 = wrap(phase[i] + pm);
			const float sine = nsin(t);
			const float saw	 = 2.0f * t - 1.0f - poly_blep(t, dt[i]);
			phase[i]		 = wrap(phase[i] + dt[i]);
			return sine + (saw - sine) * morph;
		}

		stereo operator()(stereo in)
		{
			const float pm = mode == INPUT_PM ? (in[0] + in[1]) * 0.5f * PM_DEPTH : 0.0f;

			stereo	 sum	 = {};
			uint32_t count_l = 0;
			uint32_t count_r = 0;
			for(uint32_t i = 0; i < voices; i++)
			{
				const float v	   = voice(i, pm);
				const float offset = voice_offset(i, voices);
				if(offset <= 0.0f)
				{
					sum[0] += v;
					count_l++;
				}
				if(offset >= 0.0f)
				{
					sum[1] += v;
					count_r++;
				}
			}

			stereo out = {fold_tri(sum[0] / std::sqrt((float)count_l) * fold) * OSC_LEVEL,
						  fold_tri(sum[1] / std::sqrt((float)count_r) * fold) * OSC_LEVEL};

			if(mode == INPUT_PASSTHROUGH)
			{
				out[0] = clamp(out[0] + in[0] * INPUT_LEVEL, -1.0f, 1.0f);
				out[1] = clamp(out[1] + in[1] * INPUT_LEVEL, -1.0f, 1.0f);
			}

			return out;
		}
	};
} // namespace legio
