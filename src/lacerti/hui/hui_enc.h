/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "hui_accel.h"
#include "hui_const.h"
#include "hui_debounce.h"
#include "hui_detent.h"
#include "hui_quad.h"
#include "hui_tac.h"
#include <array>
#include <stdint.h>

namespace lac
{

	struct hui_conf_enc
	{
		int8_t	 bita	= 0;
		int8_t	 bitb	= 0;
		int8_t	 bits	= 0;
		hui_flip flip	= HUI_NOFLIP;
		int8_t	 div	= 4;
		int32_t	 settle = 0;
	};

	template <uint32_t count>
	using hui_array_enc = std::array<hui_conf_enc, count>;

	struct hui_value_enc
	{
		hui_value_tac tac;

		hui_dir motion_raw = HUI_NONE;
		int32_t count_raw  = 0;

		hui_dir motion_detent = HUI_NONE;
		int32_t motion_accel  = 0;

		constexpr bool moved() const { return motion_raw != 0; }
		constexpr bool is_up() const { return motion_raw == HUI_UP; }
		constexpr bool is_dn() const { return motion_raw == HUI_DN; }
	};

	struct hui_filter_quad
	{
		enum enc_state
		{
			STATE_IDLE,
			STATE_WAIT,
			STATE_DEBOUNCE
		};

		int32_t	 value	  = 0;
		uint32_t state	  = STATE_IDLE;
		uint32_t start	  = 0;
		uint32_t enc_temp = 0;
		uint32_t enc_curr = 0;

		constexpr int32_t operator()(int32_t now, const hui_conf_enc& c, bool va, bool vb)
		{
			if(c.settle == 0)
			{
				int32_t next_value = (int32_t)hui_quad_bits(va, vb);

				hui_dir raw = HUI_NONE;

				if(value != next_value)
				{
					raw	  = HUI_ENC_TABLE[(value << 2) | next_value];
					value = next_value;
				}

				return (int32_t)raw;
			}
			else
			{
				switch(state)
				{
					default:
					case STATE_IDLE:
						enc_temp = hui_quad_bits(va, vb);
						if((int32_t)enc_temp != value)
						{
							state	 = STATE_WAIT;
							enc_curr = enc_temp + 100;
							start	 = (uint32_t)now;
						}
						break;
					case STATE_WAIT:
						if((uint32_t)now - start > (uint32_t)c.settle)
						{
							state = STATE_DEBOUNCE;
						}
						break;
					case STATE_DEBOUNCE:
						if(enc_temp != enc_curr)
						{
							enc_temp = enc_curr;
							enc_curr = hui_quad_bits(va, vb);
						}
						else
						{
							if((int32_t)enc_curr != value)
							{
								hui_quad dir = HUI_QUAD_TABLE[(value << 2) | enc_curr];
								value		 = enc_curr;
								if(dir == HUI_QUAD_CW)
								{
									return 1;
								}
								else if(dir == HUI_QUAD_CCW)
								{
									return -1;
								}
							}
							state = STATE_IDLE;
						}
						break;
				}
				return 0;
			}
		}
	};

	struct hui_filter_enc
	{
		hui_filter_tac	 tactile;
		hui_debounce	 debounce;
		hui_filter_accel accel;
		hui_detent		 detent;

		hui_filter_quad quad;
		int32_t			count_raw	 = 1;
		int32_t			count_detent = 1;
		int32_t			count_accel	 = 1;

		constexpr hui_value_enc operator()(int32_t			   now,
										   int32_t			   debounce_time,
										   int32_t			   tap_timeout,
										   int32_t			   hold_time,
										   int32_t			   accel_time,
										   const hui_conf_enc& c,
										   bool				   va,
										   bool				   vb,
										   bool				   vs)
		{
			vs = flip_bool(c.flip, vs);

			hui_dir raw = (hui_dir)quad(now, c, va, vb);

			count_raw += raw;

			hui_dir dt = detent(c.div, raw);
			count_detent += dt;
			int32_t ac = accel(now, accel_time, raw);
			count_accel += ac;

			return {tactile(now, debounce_time, tap_timeout, hold_time, vs), raw, count_raw, dt, ac};
		}
	};

} // namespace lac