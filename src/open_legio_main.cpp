/* Copyright (C) Noise Engineering
 * SPDX-License-Identifier: MIT
 */

#include "hardware/legio_brd.h"
#include <mcu/stm/h750/mcu_h750_sram_flash.h>
#include "vi_example.h"

int main()
{ return lac::legio_main<legio::vi_example<8>, 96000>(); }
