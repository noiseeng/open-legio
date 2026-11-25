/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <hui/hui_adcf.h>
#include <hui/hui_const.h>
#include <hui/hui_enc.h>
#include <hui/hui_tac.h>
#include <hui/hui_tog3.h>

namespace legio
{
	enum legio_bits : int8_t
	{
		ENC_A,
		ENC_B,
		ENC_S,
		TOGL_A,
		TOGL_B,
		TOGR_A,
		TOGR_B,
		SYNC
	};

	template <typename timer_t>
	struct legio_hui
	{
		constexpr static size_t ADC_BITS = 20;
		constexpr static float	ADC_MIN	 = 0.02f;
		constexpr static float	ADC_MAX	 = 0.98f;
		constexpr static float	ADC_HIST = 0.0015f;
		constexpr static float	ADC_FILT = 0.5f;

		constexpr static uint32_t						 ADCF_COUNT = 4;
		constexpr static lac::hui_array_adcf<ADCF_COUNT> ADCF		= {{
			{ADC_BITS, false, 0.0f, 1.0f, ADC_HIST, ADC_FILT},
			{ADC_BITS, false, 0.0f, 1.0f, ADC_HIST, ADC_FILT},
			{ADC_BITS, true, ADC_MIN, ADC_MAX, ADC_HIST, ADC_FILT},
			{ADC_BITS, true, ADC_MIN, ADC_MAX, ADC_HIST, ADC_FILT},
		}};

		constexpr static lac::hui_conf_tac TAC = {SYNC};

		constexpr static lac::hui_conf_enc ENC = {ENC_B, ENC_A, ENC_S, lac::HUI_FLIP, 4};

		constexpr static uint32_t						 TOG3_COUNT = 2;
		constexpr static lac::hui_array_tog3<TOG3_COUNT> TOG3		= {{
			{TOGL_A, TOGL_B},
			{TOGR_A, TOGR_B},
		}};

		constexpr static int32_t DEBOUNCE_TIME = timer_t::us_to_clk(250);
		constexpr static int32_t TAP_TIMEOUT   = timer_t::ms_to_clk(4000);
		constexpr static int32_t HOLD_TIMEOUT  = timer_t::ms_to_clk(2000);
		constexpr static int32_t ACCEL_TIME	   = timer_t::ms_to_clk(8);

		uint32_t bits_this = 0;
		uint32_t bits_that = 0;
		int32_t	 t		   = timer_t::get_clk();
		int32_t	 dt		   = 0;

		std::array<lac::hui_filter_tog3, TOG3_COUNT> tog3_filter = {};
		lac::hui_filter_tac							 tac_filter	 = {};
		lac::hui_filter_enc							 enc_filter	 = {};
		std::array<lac::hui_filter_adcf, ADCF_COUNT> adcf_filter = {};

		std::array<lac::hui_value_tog3, TOG3_COUNT> tog3 = {};
		lac::hui_value_tac							tac	 = {};
		lac::hui_value_enc							enc	 = {};
		std::array<lac::hui_value_adcf, ADCF_COUNT> adcf = {};

		constexpr bool get(int8_t bit) const { return (bits_this >> bit) & 1; }
		constexpr bool get_last(int8_t bit) const { return (bits_that >> bit) & 1; }

		void operator()(uint32_t bits, const std::array<uint32_t, ADCF_COUNT>& adc)
		{
			bits_that = bits_this;
			bits_this = bits;

			const int32_t now = timer_t::get_clk();
			dt				  = lac::hui_clk_sub(now, t);
			t				  = now;

			for(uint32_t i = 0; i < TOG3_COUNT; i++)
			{
				tog3[i] = tog3_filter[i](get(TOG3[i].bita), get(TOG3[i].bitb), get_last(TOG3[i].bita), get_last(TOG3[i].bitb));
			}

			tac = tac_filter(
				timer_t::get_clk(), DEBOUNCE_TIME, TAP_TIMEOUT, HOLD_TIMEOUT, lac::flip_bool(TAC.flip, get(TAC.bit)));

			enc = enc_filter(timer_t::get_clk(),
							 DEBOUNCE_TIME,
							 TAP_TIMEOUT,
							 HOLD_TIMEOUT,
							 ACCEL_TIME,
							 ENC,
							 get(ENC.bita),
							 get(ENC.bitb),
							 get(ENC.bits));

			for(uint32_t i = 0; i < ADCF_COUNT; i++)
			{
				adcf[i] = adcf_filter[i](ADCF[i], adc[i]);
			}
		}
	};
} // namespace legio
