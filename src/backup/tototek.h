/*
tototek.h - General ToToTEK flash card programmer routines for uCON64

Copyright (c) 2004, 2020 dbjh


This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#ifndef TOTOTEK_H
#define TOTOTEK_H

#ifdef  HAVE_CONFIG_H
#include "config.h"
#endif


#ifdef  USE_PARALLEL
extern void ttt_init_io (unsigned short port);
extern void ttt_deinit_io (void);
extern void ttt_set_ai_data (unsigned char ai, unsigned char data);
extern void ttt_rom_enable (void);
extern void ttt_rom_disable (void);
extern void ttt_ram_enable (void);
extern void ttt_ram_disable (void);
extern void ttt_write_mem (unsigned int addr, unsigned char b);
extern unsigned short ttt_get_id (void);
extern void ttt_read_rom_b (unsigned int addr, unsigned char *buf);
extern void ttt_read_rom_w (unsigned int addr, unsigned char *buf);
extern void ttt_read_ram_b (unsigned int addr, unsigned char *buf);
extern void ttt_read_ram_w (unsigned int addr, unsigned char *buf);
extern void ttt_erase_block (unsigned int addr);
extern void ttt_write_byte_sharp (unsigned int addr, unsigned char b);
extern void ttt_write_byte_intel (unsigned int addr, unsigned char b);
extern void ttt_write_page_rom (unsigned int addr, unsigned char *buf);
extern void ttt_write_byte_ram (unsigned int addr, unsigned char b);
extern void ttt_write_page_ram (unsigned int addr, unsigned char *buf);
extern void ttt_write_page_ram2 (unsigned int addr, unsigned char *buf);
#endif

#endif
