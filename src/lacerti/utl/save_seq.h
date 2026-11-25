/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "bug.h"
#include "std.h"
#include <bug/eabi.h>
#include <cstring>
#include <stdarg.h>
#include <stdint.h>

namespace lac
{
	template <typename save_state_t, uint32_t magic>
	struct save_seq_state
	{
		using this_t = save_seq_state<save_state_t, magic>;

		const static uint32_t MAGIC = magic;

		uint32_t	 magic0	  = 0;
		uint32_t	 sequence = 0;
		save_state_t state;
		uint32_t	 magic1 = 0;

		void defaults()
		{
			magic0	 = MAGIC;
			state	 = {};
			sequence = 0;
			magic1	 = MAGIC;
		}

		bool is_valid() const { return magic0 == MAGIC && magic1 == MAGIC && state.valid() && sequence != 0xFFFFFFFF; }

		bool is_newer(const this_t& old) { return sequence > old.sequence; }

		void next() { sequence += 1; }

		bool differs(const this_t& dif) { return !lac::same(*this, dif); }
	};

	template <typename timer_t,
			  typename flash_t,
			  typename save_t,
			  uint32_t save_size,
			  uint32_t save_count,
			  uint32_t save_start = 0>
	struct save_seq
	{
		static_assert(sizeof(save_t) <= save_size, "save_size must be larger than save size");
		static_assert(sizeof(save_t) % 4 == 0, "save_size must be multiple of 4");

		const static uint32_t SAVE_SIZE	  = save_size;
		const static uint32_t SAVE_COUNT  = save_count;
		const static uint32_t SAVE_START  = save_start;
		const static uint32_t TOTAL_BYTES = SAVE_SIZE * SAVE_COUNT;
		const static uint32_t SAVE_END	  = save_start + TOTAL_BYTES;

		using write_t = typename flash_t::template pol_program<sizeof(save_t)>;

		bool	 found_invalid	= false;
		bool	 found_valid	= false;
		bool	 found_no_valid = false;
		bool	 write_active	= false;
		bool	 is_different	= false;
		uint32_t current_index	= 0;
		uint32_t different_time = 0;
		uint32_t SAVE_DELAY_CLK = timer_t::ms_to_clk(3000);
		uint32_t sequence		= 0;
		save_t	 current;
		save_t	 delay;
		save_t	 check;
		write_t	 write;

		constexpr static bool show_trace = false;

		constexpr void trace() {}

		static void trace(const char* fmt, ...)
		{
			if constexpr(show_trace)
			{
				va_list ap;
				va_start(ap, fmt);
				bug::print_va(fmt, &ap);
			}
		}

		bool is_busy() const { return write_active; }

		uint32_t get_addr(uint32_t slot) { return slot * SAVE_SIZE + SAVE_START; }

		const save_t& load(const char* name = "")
		{
			save_t tmp;

			current.defaults();

			lac_assert(current.is_valid());

			bool had_valid_data = false;

			for(uint32_t i = 0; i < SAVE_COUNT; i++)
			{
				flash_t::read(get_addr(i), sizeof(save_t), (uint8_t*)&tmp);

				if(tmp.is_valid())
				{
					if(tmp.sequence >= current.sequence)
					{
						found_valid	   = true;
						current		   = tmp;
						current_index  = i;
						had_valid_data = true;
						sequence	   = tmp.sequence;
						trace("%s valid new %d %d\n", name, i, tmp.sequence);
					}
					else
					{
						trace("%s valid old %d %d\n", name, i, tmp.sequence);
					}
				}
				else
				{
					tmp.defaults();
					write_slot(i, tmp);
					found_invalid = true;
					trace("%s invalid %d %d\n", name, i, tmp.sequence);
				}
			}

			if(!had_valid_data)
			{
				trace("%s no valid save data\n", name);
				found_no_valid = true;
				current.defaults();
			}
			else
			{
				trace("%s loaded %d %d\n", name, current_index, current.sequence);
			}

			delay = current;

			return current;
		}

		void blk_write(save_t& save)
		{
			current = save;
			block_write();
		}

		void block_write()
		{
			current_index = (current_index + 1) % SAVE_COUNT;
			block_write(current_index);
		}

		void block_write(uint32_t index)
		{
			current.sequence = ++sequence;

			trace("write block %d\n", current.sequence);

			write_slot(index, current);
		}

		void write_slot(uint32_t index, save_t& save)
		{
			const uint32_t ofs = get_addr(index);

			if(!bug::is_build_ship())
			{
				flash_t::read(ofs, sizeof(check), (uint8_t*)&check);
			}

			flash_t::write_block(ofs, sizeof(save), (uint8_t*)&save);

			if(!bug::is_build_ship())
			{
				memset(&check, 0, sizeof(check));

				flash_t::read(ofs, sizeof(check), (uint8_t*)&check);

				lac_assert(!std::memcmp(&check, &save, sizeof(check)));
			}
		}

		void reset()
		{
			for(uint32_t i = 0; i < SAVE_COUNT; i++)
			{
				block_write();
			}
		}

		bool save_delay(save_t& save)
		{
			bool ret = false;

			if(write_active)
			{
				if(write.step())
				{
					lac_assert(!std::memcmp(&check, &current, sizeof(check)));
					delay		  = current;
					save.sequence = current.sequence;
					ret			  = true;
					write_active  = false;
				}
			}
			else if(is_different)
			{
				if(!current.differs(save))
				{
					delay		 = save;
					is_different = false;
				}
				else if(delay.differs(save))
				{
					different_time = timer_t::get_clk();
					delay		   = save;
				}
				else if(timer_t::get_clk() - different_time > SAVE_DELAY_CLK)
				{
					if(current.differs(save))
					{
						trace("pre save %d %d %d \n", current_index, current.sequence, save.sequence);

						current			 = save;
						current_index	 = (current_index + 1) % SAVE_COUNT;
						current.sequence = ++sequence;

						memset(&check, 0, sizeof(check));

						new(&write) write_t(get_addr(current_index), sizeof(current), (uint8_t*)&current, (uint8_t*)&check);
						write_active = true;
						ret			 = true;

						trace("post save %d %d\n", current_index, current.sequence);
					}
				}
			}
			else if(current.differs(save))
			{
				delay		   = save;
				is_different   = true;
				different_time = timer_t::get_clk();
			}

			return ret;
		}
	};
} // namespace lac