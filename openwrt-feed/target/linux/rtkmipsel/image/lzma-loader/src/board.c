/*
 * LZMA compressed kernel loader for Realtek 819X
 *
 * Copyright (C) 2011 Gabor Juhos <juhosg@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Edit for HH71VM: the offset of the transfer register UART0 is determined at runtime.
 *
 * For RTL8197F and RTL8197F_VG, the THR register is at different offsets from the UART0 base:
 * +0x024 for the regular 8197F and +0x000 for the VG variant (vendor bspchip.h,
 * BSP_UART0_THR_8197F / BSP_UART0_THR_8197F_VG). Forked version of this file
 * AC10 knew only the +0x024 variant. The kernel detects the variant itself (prom.c),
 * but the loader is executed before the kernel - and if it makes a mistake, then on the first
 * loading, exactly that output will disappear, from which you can only see how far
 * unpacking has arrived. The cost of checking is one register read.
 *
 * Sign of the variant - register BSP_ECO_SN (0xB8000000): the most significant 20 bits are equal
 * 0x81970 for VG and 0x8197F for the regular one (macros IS_8197F_VG()/IS_8197F()).
 *
 * The port speed is not programmable here - it remains the one set
 * ROM-bootloader (our device has 38400).
 */

#include <stddef.h>

#define BSP_ECO_SN          0xB8000000

#define BSP_UART0_BASE      0xB8147000
#define UART_THR_8197F      (BSP_UART0_BASE + 0x024)
#define UART_THR_8197F_VG   (BSP_UART0_BASE + 0x000)
#define UART_LSR            (BSP_UART0_BASE + 0x014)

#define REG8(reg)   (*(volatile unsigned char   *)((unsigned int)reg))
#define REG32(reg)  (*(volatile unsigned int    *)((unsigned int)reg))

static unsigned int uart_thr(void)
{
	if ((REG32(BSP_ECO_SN) & 0xFFFFF000) == 0x81970000)
		return UART_THR_8197F_VG;

	return UART_THR_8197F;
}

void serial_outc(char c)
{
        int i=0;

        while (1)
        {
                i++;
                if (i >=0x6000)
                        break;
                if (REG8(UART_LSR) & 0x20)
                        break;
        }
        REG8(uart_thr()) = (c);
}


void board_putc(int ch)
{
	serial_outc(ch);
}

void board_init(void)
{
}
