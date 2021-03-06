/*
smc.h - Super Magic Card support for uCON64

Copyright (c) 2003, 2020 dbjh


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
#ifndef SMC_H
#define SMC_H

#ifdef  HAVE_CONFIG_H
#include "config.h"
#endif
#include "misc/getopt2.h"                       // st_getopt2_t


extern const st_getopt2_t smc_usage[];

typedef struct st_smc_header
{
  unsigned char emulation1;
  unsigned char emulation2;
  unsigned char pad1[6];
  unsigned char id1;
  unsigned char id2;
  unsigned char type;
  unsigned char pad2[501];
} st_smc_header_t;

#define SMC_HEADER_LEN (sizeof (st_smc_header_t))
#define SMC_TRAINER 0x40

#ifdef  USE_PARALLEL
extern int smc_write_rom (const char *filename, unsigned short parport);
extern int smc_read_rts (const char *filename, unsigned short parport);
extern int smc_write_rts (const char *filename, unsigned short parport);
#endif

#endif
