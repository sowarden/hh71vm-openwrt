/*
 * Board support Alcatel LINKHUB HH71VM (Realtek RTL8197F + RTL8812FE)
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/leds.h>

#include "machtypes.h"
#include "dev_leds_gpio.h"
#include "dev-gpio-buttons.h"

/*
 * STATUS: LEDs and buttons on this board are intentionally not described here.
 *
 * The HH71VM GPIO assignments have not been established from the stock system.
 * Assignments from other RTL8197F boards (Kinkan, Komikan, or Tenda AC10) cannot be
 * reused safely because each board has different wiring, and driving an unverified pin
 * could apply a level to an unrelated SoC signal.
 *
 * What is known for certain: button codes in the vendor’s stock firmware database
 * (factory_info.db3, table Key) - WPS 529 (KEY_WPS_BUTTON), Reset 278
 * (KEY_RESTART), Power 116 (KEY_POWER). These are Linux entry codes, not GPIO numbers.
 *
 * To fill out the tables, you first need to trace the pins through the hardware - for example,
 * remove registers GPIO on a live stock system while pressed and released
 * button, or find them in vendor display control scripts.
 * Up to this point, the board rises without LEDs and buttons: to boot
 * they are not required, and fictitious meanings would yield silently incorrect hardware.
 */

static void __init hh71vm_setup(void)
{
}

MIPS_MACHINE(RTL8197_MACH_HH71VM, "HH71VM", "Alcatel LINKHUB HH71 series",
	     hh71vm_setup);
