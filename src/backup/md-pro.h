/*
md-pro.h - MD-PRO support for uCON64

Copyright (c) 2003 - 2005, 2015, 2017, 2020 dbjh

Based on Delphi source code by ToToTEK Multi Media. Information in that source
code has been used with permission. However, ToToTEK Multi Media explicitly
stated that the information in that source code may be freely distributed.


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
#ifndef MD_PRO_H
#define MD_PRO_H

#ifdef  HAVE_CONFIG_H
#include "config.h"
#endif
#include "misc/getopt2.h"                       // st_getopt2_t


#define MD_PRO_LOADER_SIZE (128 * 1024)

extern const st_getopt2_t mdpro_usage[];

#ifdef  USE_PARALLEL
extern int md_read_rom (const char *filename, unsigned short parport,
                        unsigned int size);
extern int md_write_rom (const char *filename, unsigned short parport);
extern int md_read_sram (const char *filename, unsigned short parport,
                         int start_bank);
extern int md_write_sram (const char *filename, unsigned short parport,
                          int start_bank);
#endif

#endif
