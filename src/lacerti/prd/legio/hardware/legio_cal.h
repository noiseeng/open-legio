/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <utl/cv_cal.h>

namespace legio
{

	struct legio_cal_cfg
	{
		static constexpr float	SCALE	  = 1.033f;
		static constexpr float	OFFSET	  = -0.009;
		static constexpr float	CAL_MINV  = -2.0f;
		static constexpr float	CAL_MAXV  = 5.2f;
		static constexpr float	REF_VOLTS = 2.5f;
		static constexpr float	MINV	  = -2.0f;
		static constexpr float	MAXV	  = 5.2f;
		static constexpr size_t AVE_COUNT = 256;
	};

	using calibration = lac::cv_calibration<legio_cal_cfg>;

	template <typename timer_t>
	using calibrate = lac::cv_calibrate<timer_t, legio_cal_cfg>;

	static_assert(sizeof(calibration) == 20);
	static_assert(offsetof(calibration, valid) == 0);
	static_assert(offsetof(calibration, scale) == 4);
	static_assert(offsetof(calibration, offset) == 8);
	static_assert(offsetof(calibration, minv) == 12);
	static_assert(offsetof(calibration, maxv) == 16);
} // namespace legio
