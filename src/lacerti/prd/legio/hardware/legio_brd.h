/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <mcu/stm/h7/per_h7_adc.h>
#include <mcu/stm/h7/per_h7_tim.h>
#include <mcu/stm/h7/per_h7_usb_dfu_flash.h>
#include <mcu/stm/h7/reg_h7_dmamux.h>
#include <mcu/stm/h750/adr_h750.h>
#include <mcu/stm/stm32/per_adc.h>
#include <type_traits>
#include <utl/slug.h>
#include <utl/std.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bug/eabi.h>
#include <cmath>
#include <cstring>
#include <dev/dev_led_rgb.h>
#include <dev/dev_led_rgb_pwm.h>
#include <hui/hui_color_mode.h>
#include <mcu/es/brd_es_dfm.h>
#include <mcu/es/brd_es_dfm_sdram.h>
#include <mcu/stm/h750/STM32H750IBKx.pin.h>
#include <numerus/color.h>
#include <stdint.h>
#include <utl/bug.h>
#include <utl/save_seq.h>
#include <utl/timer_interval.h>
#include <utl/timer_poll.h>

#include "legio_cal.h"
#include "legio_hui.h"
#include "legio_usb.h"
#include <mcu/stm/h7/per_h7_usb_dfu.h>
#include <mcu/stm/h7/per_h7_usb_dfu_log.h>
#include <mcu/stm/h750/mcu_h750.h>
#include <prd/legio/legio_inp.h>
#include <prd/legio/legio_out.h>
#include <utl/audio_io.h>

namespace lac
{

	template <typename dsp_t, uint32_t HARDWARE_FRAME_RATE>
	struct legio_brd
	{
		using dfm_t	 = brd_es_dfm<1, brd_es_dfm_sdram>;
		using pin_t	 = pin_stm32h750ibkx;
		using mcu_t	 = dfm_t::mcu_t;
		using tim_t	 = dfm_t::tim2_t;
		using this_t = legio_brd<dsp_t, HARDWARE_FRAME_RATE>;

		constexpr static uint32_t		FRAME_COUNT = dsp_t::FRAME_COUNT;
		constexpr static numerus::color COLOR_LEFT	= dsp_t::COLOR_LEFT;
		constexpr static numerus::color COLOR_RIGHT = dsp_t::COLOR_RIGHT;

		enum adc_channel
		{
			ADC_PITCH0,
			ADC_PITCH1,
			ADC_CV0,
			ADC_CV1
		};

		struct vi_adc_pinspec_enc
		{
			static constexpr std::array<adc_pin_config, 4> pins = {{
				ADC_PIN(pin_t::adc1p_a5),
				ADC_PIN(pin_t::adc1p_a4),
				ADC_PIN(pin_t::adc1p_a6),
				ADC_PIN(pin_t::adc1p_c4),
			}};
		};

		using enc_s_t  = pin_t::gpipu_c11;
		using enc_a_t  = pin_t::gpipu_c10;
		using enc_b_t  = pin_t::gpipu_c9;
		using togl_a_t = pin_t::gpipu_b15;
		using togl_b_t = pin_t::gpipu_b14;
		using togr_a_t = pin_t::gpipu_b1;
		using togr_b_t = pin_t::gpipu_c0;
		using sync_t   = pin_t::gpi_a7;

		using vi_cal  = pin_t::gpio_c1;
		using vi_led1 = dev_led_rgb<pin_t::gpo_a2, pin_t::gpo_c12, pin_t::gpo_d2, 1>;
		using vi_led2 = dev_led_rgb<pin_t::gpo_d11, pin_t::gpo_a0, pin_t::gpo_a1, 1>;
		using vi_pwm1 = dev_led_rgb_pwm<vi_led1, 6>;
		using vi_pwm2 = dev_led_rgb_pwm<vi_led2, 6>;
		using vi_adc  = per_h7_adc<mcu_t, adr_h750::ADC1, vi_adc_pinspec_enc, DMA1_STREAM_2, ADC_TIME_8_5, 3, 16, 4, 0, tim_t>;
		using dsp_ctl_t = typename dsp_t::ctl_t;
		using audio_t	= dfm_t::template dfm_audio_t<HARDWARE_FRAME_RATE, 2 * 2 * FRAME_COUNT>;

		using irq_timer_t  = per_h7_tim<mcu_t, adr_h750::TIM3, 240000000>;
		using cpu_timer_t  = timer_interval<tim_t>;
		using debug_poll_t = timer_poll<tim_t, false>;

		constexpr static uint32_t frame_rate = HARDWARE_FRAME_RATE;

		constexpr static float ENC_SCALE_FINE	= 1.0f / 50 / 32 / 2;
		constexpr static float ENC_SCALE_COARSE = 1.0f / 12 / 4;
		constexpr static float PITCH_MIN		= -4.0f;
		constexpr static float PITCH_MAX		= 10.0f;
		constexpr static float ENC_NORM_STEP	= 1.0f / 256.0f;

		using ctl_t = legio::legio_inp;
		using cal_t = legio::calibration;

		using calibrate_t = legio::calibrate<tim_t>;

		struct save_t
		{
			constexpr static uint32_t MAGIC = 0x9635658F;

			uint32_t magic0		  = 0;
			uint32_t sequence	  = 0;
			cal_t	 cal		  = {};
			float	 pitch		  = 0;
			float	 encoder	  = 0;
			int32_t	 tap_toggle	  = 0;
			float	 encoder_hold = 0;
			uint32_t magic1		  = 0;

			constexpr void defaults()
			{
				magic0		 = MAGIC;
				cal			 = {true};
				pitch		 = 2;
				sequence	 = 1;
				encoder		 = 0.5f;
				tap_toggle	 = 0;
				encoder_hold = 0.5f;
				magic1		 = MAGIC;
			}

			constexpr bool is_valid() const
			{
				return magic0 == MAGIC && magic1 == MAGIC && pitch <= PITCH_MAX && pitch >= PITCH_MIN && cal.valid
					   && encoder >= 0.0f && encoder <= 1.0f && (tap_toggle == 0 || tap_toggle == 1) && encoder_hold >= 0.0f
					   && encoder_hold <= 1.0f && sequence != 0 && sequence != 0xFFFFFFFF;
			}

			constexpr bool is_newer(const save_t& old) { return sequence > old.sequence; }

			constexpr void next() { sequence += 1; }

			bool differs(const save_t& dif) { return !lac::same(*this, dif); }
		};

		using hui_t = legio::legio_hui<tim_t>;

		using save_flash   = save_seq<tim_t, typename dfm_t::flash_t, save_t, dfm_t::flash_t::SECTOR_BYTES, 8>;
		using color_mode_t = hui_color_mode<2>;

		dfm_t dfm;

		enc_a_t enc_a;
		enc_b_t enc_b;
		enc_s_t enc_s;

		togl_a_t togl_a;
		togl_b_t togl_b;
		togr_a_t togr_a;
		togr_b_t togr_b;
		sync_t	 sync;

		vi_adc::values*		 values		  = mcu_t::template alloc_dma<typename vi_adc::values>();
		audio_t::dma_buffer* audio_buffer = mcu_t::template alloc_dma<typename audio_t::dma_buffer>();
		mcu_t::mcu_alloc_t	 alloc		  = {};

		vi_adc	adc	  = {*values};
		audio_t audio = {*audio_buffer};

		std::conditional_t<lac::use_log(),
						   lac::per_h7_dfu_log<mcu_t, u"Legio Hot Swap", lac::usbd_dfu_log_h750_legio>,
						   lac::per_h7_dfu<mcu_t, u"Legio Hot Swap", true, lac::usbd_dfu_h750_legio>>
			dfu = {};

		save_t		   save			= {};
		save_flash	   flash		= {};
		vi_pwm1		   led1			= {};
		vi_pwm2		   led2			= {};
		numerus::color ledcolors[2] = {};
		irq_timer_t	   timer		= {};
		cpu_timer_t	   cpu			= cpu_timer_t{cpu_timer_t::budget_of(FRAME_COUNT, HARDWARE_FRAME_RATE)};
		debug_poll_t   wait_debug	= {tim_t::ms_to_clk(1000)};
		dsp_t		   dsp			= {};
		ctl_t		   ctl			= {};
		hui_t		   hui			= {};

		bool test_mode	= false;
		bool calibrate	= false;
		bool calibrated = false;

		uint32_t quiet_time = 0;
		uint32_t fade_time	= 0;
		int32_t	 last_time	= tim_t::get_clk();
		uint32_t blink_time = tim_t::get_hz() / 5;

		float			 meter[4]  = {0, 0, 0, 0};
		legio::legio_out led_out   = {};
		float			 boot_fade = 0.0f;

		color_mode_t  led_meters = {};
		color_mode_t  led_calib	 = {};
		color_mode_t  led_save	 = {};
		color_mode_t* led_norm	 = &led_save;
		color_mode_t  led_sync	 = {};
		color_mode_t  led_enc	 = {};
		color_mode_t  led_cvsw	 = {};
		color_mode_t* led_test	 = &led_sync;

		lac::slug slug = {};

		inline static this_t* that = 0;

		volatile int32_t adc_count = 0;

		static void adc_irq(uint32_t count, vi_adc::values& values) { that->adc_count += 1; }

		float get_pitch() const
		{
			float sum = (float)adc.get_median(ADC_PITCH0) + (float)adc.get_median(ADC_PITCH1);
			return sum / (float)(1 << vi_adc::PRECISION) / 2.0f;
		}

		float get_next_pitch()
		{
			const volatile int32_t start = adc_count;
			slug([&]() { return (int32_t)(adc_count - start) < (int32_t)vi_adc::OVERSAMPLE_COUNT; });
			return get_pitch();
		}

		legio_brd()
		{
			lac_assert(that == 0);

			that = this;

			init_usb();

			vi_cal::init_out();
			vi_cal::set();

			tim_t::delay_ms(50);

			test_mode = !enc_s.get();
			set_firmware_leds(test_mode);

			adc.eosie(adc_irq);
			adc.start();

			save = flash.load();

			const auto cal = calibrate_t::cal("pitch", [&]() { return get_next_pitch(); }, [&](bool x) { vi_cal::set(x); });

			if(cal.valid)
			{
				calibrated = true;
				save.cal   = cal;
			}

			vi_cal::set();

			adc.eosie(0);

			dsp.set_frame_rate(frame_rate, &alloc);
			audio.init(audio.silence);

			audio.callback = test_mode ? test_callback : callback;

			led_init();
		}

		void init_usb()
		{
			dfu.init();

			if constexpr(lac::use_log())
			{
				mcu_t::post_mortem = post_mortem_thunk;
			}
		}

		[[noreturn]] void post_mortem()
		{
			audio.mute();
			set_all_leds(numerus::color::magenta());

			for(;;)
			{
				poll_dfu();
				dfu.poll_log();
			}
		}

		static void post_mortem_thunk() { get()->post_mortem(); }

		void set_firmware_leds(bool test)
		{
			led1.set_solid(COLOR_LEFT);
			led2.set_solid(COLOR_RIGHT);
		}

		void step_hui()
		{
			uint16_t bits = 0;

			bits |= enc_a.get() ? (1 << legio::ENC_A) : 0;
			bits |= enc_b.get() ? (1 << legio::ENC_B) : 0;
			bits |= enc_s.get() ? (1 << legio::ENC_S) : 0;
			bits |= togl_a.get() ? (1 << legio::TOGL_A) : 0;
			bits |= togl_b.get() ? (1 << legio::TOGL_B) : 0;
			bits |= togr_a.get() ? (1 << legio::TOGR_A) : 0;
			bits |= togr_b.get() ? (1 << legio::TOGR_B) : 0;
			bits |= sync.get() ? (1 << legio::SYNC) : 0;

			hui(bits,
				{adc.get_median(ADC_PITCH0), adc.get_median(ADC_PITCH1), adc.get_median(ADC_CV0), adc.get_median(ADC_CV1)});
		}

		void step_enc()
		{
			if(hui.enc.moved())
			{
				int32_t motion = dsp_t::ENC_ACCEL ? hui.enc.motion_accel * 8 : (int32_t)hui.enc.motion_raw;

				save.pitch += (float)motion * (hui.enc.tac ? ENC_SCALE_COARSE : ENC_SCALE_FINE);
				save.pitch = std::clamp(save.pitch, PITCH_MIN, PITCH_MAX);

				save.encoder += (float)hui.enc.motion_raw * ENC_NORM_STEP;
				save.encoder = std::clamp(save.encoder, 0.0f, 1.0f);

				save.encoder_hold += (float)hui.enc.motion_raw * ENC_NORM_STEP;
				save.encoder_hold = std::clamp(save.encoder_hold, 0.0f, 1.0f);

				bug_print("enc %d %f %f\n", motion, (double)save.pitch, (double)save.encoder);
			}

			if(hui.enc.tac.pressed())
			{
				save.encoder = 0.5f;
			}
		}

		void update(ctl_t& ctl)
		{
			std::atomic_thread_fence(std::memory_order_seq_cst);

			step_hui();

			step_enc();

			ctl.swr = (int32_t)hui.tog3[0];
			ctl.swl = (int32_t)hui.tog3[1];
			ctl.k1	= (float)hui.adcf[ADC_CV0];
			ctl.k2	= (float)hui.adcf[ADC_CV1];

			const float sp = save.pitch;
			const float fp = save.cal(get_pitch());

			ctl.enc_norm  = save.encoder;
			ctl.enc_hold  = save.encoder_hold;
			ctl.p0		  = std::clamp(fp + sp + 2.0f, PITCH_MIN, PITCH_MAX);
			ctl.p0_norm	  = std::clamp(fp / 5.0f, 0.0f, 1.0f);
			ctl.sp		  = sp;
			ctl.fp		  = fp;
			ctl.enc		  = save.pitch;
			ctl.sync_gate = (bool)hui.tac;
			ctl.sync_trig = hui.tac.pressed();
			ctl.encs_gate = (bool)hui.enc.tac;
			ctl.encs_trig = hui.enc.tac.pressed();
			ctl.enc_moved = hui.enc.moved() || hui.enc.tac.pressed();

			save.tap_toggle = hui.enc.tac.pressed() ? !save.tap_toggle : save.tap_toggle;
			ctl.tap_toggle	= save.tap_toggle != 0;

			led1();
			led2();
		}

		void save_delay()
		{
			if(!test_mode)
			{
				if(flash.save_delay(save))
				{
					led_save.enabled   = true;
					led_save.time_left = blink_time;
				}
			}
		}

		static constexpr void vu_meter_filter(float in, float& out)
		{
			if(in > out)
			{
				out = in;
			}
			else
			{
				out -= out * 0.01f;
			}
		}

		void frame(uint32_t count, const int32_t* in, int32_t* out)
		{
			count /= 2;

			bug_assert(count == FRAME_COUNT);

			cpu.start();

			update(ctl);

			ctl.meter_inpl = meter[0];
			ctl.meter_inpr = meter[1];
			ctl.meter_outl = meter[2];
			ctl.meter_outr = meter[3];

			led_out = dsp.control(ctl);

			const auto peak = lac::audio_io::convert(FRAME_COUNT, in, out, [&](lac::audio_io::frame_t s) {
				const lac::audio_io::frame_t y = dsp(s);
				return lac::audio_io::frame_t{-y[0], -y[1]};
			});

			vu_meter_filter(peak.inp[0], meter[0]);
			vu_meter_filter(peak.inp[1], meter[1]);
			vu_meter_filter(peak.out[0], meter[2]);
			vu_meter_filter(peak.out[1], meter[3]);

			if(boot_fade < 1.0f)
			{
				boot_fade += (float)FRAME_COUNT / (float)frame_rate * 0.25f;
				if(boot_fade > 1.0f)
				{
					boot_fade = 1.0f;
				}
			}

			update_led();

			cpu.end();
		}

		void test_frame(uint32_t count, const int32_t* in, int32_t* out)
		{
			std::atomic_thread_fence(std::memory_order_seq_cst);

			update(ctl);

			lac::audio_io::convert(
				FRAME_COUNT, in, out, [&](lac::audio_io::frame_t s) { return lac::audio_io::frame_t{-s[0], -s[1]}; });

			update_led_test(ctl);
		}

		void led_init()
		{
			if(test_mode)
			{
				led_sync.next = &led_enc;
				led_enc.next  = &led_cvsw;

				led_sync.colors[0] = numerus::color::white();
				led_sync.colors[1] = numerus::color::white();

				led_enc.use_timer = true;

				led_norm = 0;
			}
			else
			{
				led_save.next  = &led_calib;
				led_calib.next = &led_meters;

				led_save.use_timer = true;
				led_save.colors[0] = numerus::color::black();
				led_save.colors[1] = numerus::color::black();

				led_calib.use_timer = true;
				led_calib.colors[0] = calibrated ? numerus::color::green() : numerus::color::red();
				led_calib.colors[1] = calibrated ? numerus::color::green() : numerus::color::red();

				led_meters.enabled = true;

				led_test = 0;
			}
		}

		void update_led()
		{
			int32_t current_time = tim_t::get_clk();
			led_norm->update(current_time - last_time);
			last_time = current_time;

			led_meters.colors[0] = {led_out.led1.r, led_out.led1.g, led_out.led1.b, led_out.led1.a};
			led_meters.colors[1] = {led_out.led2.r, led_out.led2.g, led_out.led2.b, led_out.led2.a};

			const numerus::color* cs = led_norm->get();

			if(boot_fade >= 1.0f)
			{
				led1 = cs[0];
				led2 = cs[1];
			}
			else
			{
				const float boot = boot_fade * boot_fade;
				const auto	t	 = (uint16_t)(boot * 0xFFFF);

				led1 = numerus::color::blend(t, COLOR_LEFT, cs[0]);
				led2 = numerus::color::blend(t, COLOR_RIGHT, cs[1]);
			}
		}

		static uint16_t f2u16(float f) { return (uint16_t)(f * (float)0xFF); }

		void update_led_test(ctl_t& ctl)
		{
			int32_t current_time = tim_t::get_clk();
			led_test->update(current_time - last_time);
			last_time = current_time;

			led_cvsw.colors[0].r = f2u16(std::clamp(ctl.p0_norm, 0.0f, 1.0f));
			led_cvsw.colors[0].g = f2u16(ctl.k1);
			led_cvsw.colors[0].b = f2u16(ctl.k2);
			led_cvsw.colors[1]	 = numerus::color::rainbow(3 * ctl.swl + ctl.swr);

			led_sync.enabled = ctl.sync_gate || ctl.encs_gate;

			if(hui.enc.moved())
			{
				bool big = hui.enc.is_up();

				led_enc.colors[0] = big ? numerus::color::cyan() : numerus::color::magenta();
				led_enc.colors[1] = big ? numerus::color::cyan() : numerus::color::magenta();

				led_enc.enabled	  = true;
				led_enc.time_left = blink_time;
			}

			if(!calibrated)
			{
				led_sync.enabled   = true;
				led_sync.colors[0] = numerus::color::red();
				led_sync.colors[1] = numerus::color::red();
			}

			const numerus::color* cs = led_test->get();

			led1 = cs[0];
			led2 = cs[1];
		}

		void set_all_leds(numerus::color c)
		{
			led1 = c;
			led2 = c;
			led1();
			led2();
		}

		void poll_dfu()
		{
			dfu.poll_leds([this](lac::dfu_led s, bool bright) {
				switch(s)
				{
					case lac::dfu_led::pulse: set_all_leds(numerus::color::blue() * (bright ? 1.0f : 0.2f)); break;
					case lac::dfu_led::done: set_all_leds(numerus::color::green()); break;
					case lac::dfu_led::error: set_all_leds(numerus::color::red()); break;
				}
			});
		}

		void main()
		{
			std::atomic_thread_fence(std::memory_order_seq_cst);

			for(;;)
			{
				std::atomic_thread_fence(std::memory_order_seq_cst);

				save_delay();

				wait_debug([&]() { bug_print("cpu %d%% max %d%% overruns %d\n", cpu.percent, cpu.get_max(), cpu.overruns); });

				slug([&]() {});

				poll_dfu();
				dfu.poll_log();
			}
		}

		static void test_callback(uint32_t count, const int32_t* in, int32_t* out) { get()->test_frame(count, in, out); }

		static void callback(uint32_t count, const int32_t* in, int32_t* out)
		{
			if(get()->dfu.active())
			{
				std::memset(out, 0, sizeof(int32_t) * count);
				return;
			}
			get()->frame(count, in, out);
		}

		static this_t* get()
		{
			bug_assert(that);
			return that;
		}
	};

	template <typename dsp_t, uint32_t HARDWARE_FRAME_RATE>
	legio_brd<dsp_t, HARDWARE_FRAME_RATE> legio_board;

	template <typename dsp_t, uint32_t HARDWARE_FRAME_RATE>
	int legio_main()
	{
		legio_board<dsp_t, HARDWARE_FRAME_RATE>.main();

		return 0;
	}

} // namespace lac