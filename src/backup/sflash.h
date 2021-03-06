/*
sflash.h - Super Flash support for uCON64

Copyright (c) 2004             JohnDie
Copyright (c) 2015, 2017, 2020 dbjh


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
#ifndef SFLASH_H
#define SFLASH_H

#ifdef  HAVE_CONFIG_H
#include "config.h"
#endif
#include "misc/getopt2.h"                       // st_getopt2_t


extern const st_getopt2_t sflash_usage[];

#ifdef  USE_PARALLEL
extern int sf_read_rom (const char *filename, unsigned short parport,
                        unsigned int size);
extern int sf_write_rom (const char *filename, unsigned short parport);
extern int sf_read_sram (const char *filename, unsigned short parport);
extern int sf_write_sram (const char *filename, unsigned short parport);
#endif

#endif
