/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace legio
{
	struct legio_inp
	{
		int32_t enc		   = 0;		// sp rounded toward zero, whole octaves
		float	enc_norm   = 0.0f;	// encoder position, 0 to 1 in steps of 1/256, saved; a press sets it to 0.5
		float	enc_hold   = 0.0f;	// like enc_norm, but a press does not change it
		float	k1		   = 0.0f;	// left knob plus its CV, 0 to 1
		float	k2		   = 0.0f;	// right knob plus its CV, 0 to 1
		float	p0		   = 0.0f;	// pitch in octaves, -4 to 10: fp + sp + 2
		float	p0_norm	   = 0.0f;	// pitch CV only, 0 to 1 over 0 V to 5 V
		float	sp		   = 0.0f;	// encoder tuning in octaves, saved, starts at 2; hold the encoder for coarse steps
		float	fp		   = 0.0f;	// pitch CV in volts, calibrated, 1 V per octave
		bool	sync_gate  = false; // sync jack is high
		bool	sync_trig  = false; // sync jack went high this block
		bool	encs_gate  = false; // encoder is held down
		bool	encs_trig  = false; // encoder was pressed this block
		bool	enc_moved  = false; // encoder was turned or pressed this block
		int32_t swr		   = 0;		// right switch: 0 bottom, 1 middle, 2 top
		int32_t swl		   = 0;		// left switch: 0 bottom, 1 middle, 2 top
		bool	tap_toggle = 0;		// flips on each encoder press, saved
		float	meter_inpl = 0.0f;	// input and output peak levels with decay, 1 is full scale
		float	meter_inpr = 0.0f;
		float	meter_outl = 0.0f;
		float	meter_outr = 0.0f;
	};
} // namespace legio
