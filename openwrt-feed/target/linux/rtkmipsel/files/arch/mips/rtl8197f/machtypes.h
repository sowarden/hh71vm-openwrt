/*
 * Machine types Realtek RTL8197F
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#ifndef _RTL8197_MACHTYPE_H
#define _RTL8197_MACHTYPE_H

#include <asm/mips_machine.h>

/*
 * HH71VM is intentionally numbered 0.
 *
 * mips_machine_setup() selects a machine by comparing with mips_machtype, and the initial
 * value mips_machtype - 0. Therefore, the board with number 0 works in both
 * cases: and when the bootloader passed "board=HH71VM" on the command line
 * (then mips_machtype_setup() will find the machine by string identifier),
 * and when there was no command line at all. For a device where the console is on
 * early boot is the only source of diagnosis, the second case is important:
 * otherwise setup of the board will not be executed silently.
 */
enum rtl8197_mach_type {
	RTL8197_MACH_HH71VM = 0,	/* Alcatel LINKHUB HH71VM */
};

#endif /* _RTL8197_MACHTYPE_H */
