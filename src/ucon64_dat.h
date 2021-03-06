/*
ucon64_dat.h - support for DAT files as known from RomCenter, GoodXXXX, etc.

Copyright (c) 1999 - 2003 NoisyB
Copyright (c) 2003        dbjh


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
#ifndef UCON64_DAT_H
#define UCON64_DAT_H

#ifdef  HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include "misc/getopt2.h"                       // st_getopt2_t
#include "misc/itypes.h"


extern const st_getopt2_t ucon64_dat_usage[];

#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
typedef struct
{
  uint32_t crc32;                               // "official" CRC32 checksum of the ROM
  int8_t console;                               // integer for the console system
  char name[256];                               // name of the ROM
  const char *maker;                            // maker of the ROM
  const char *country;                          // country of the ROM
  char misc[25 * 80];                           // miscellaneous information about the ROM
  char fname[FILENAME_MAX];                     // filename of the ROM
  uint32_t fsize;                               // size in bytes

  char datfile[FILENAME_MAX];                   // name of the DAT file
  char author[100];                             // author of DAT file
  char version[100];                            // version of DAT file
  char date[20];                                // date of DAT file
  char comment[25 * 80];                        // comment of DAT file
  char refname[100];                            // ref name

  const char *console_usage;                    // console system usage
  const char *backup_usage;                     // backup unit usage
} st_ucon64_dat_t;
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

/*
  ucon64_dat_search()         search DAT files for crc and return ucon64_dat_t
  ucon64_dat_total_entries()  return # of ROMs in all DATs
  ucon64_dat_view()           display the complete DAT collection
  ucon64_dat_indexer()        create or update index file for DATs
  ucon64_dat_nfo()            view contents of ucon64_dat_t
*/
extern st_ucon64_dat_t *ucon64_dat_search (uint32_t crc32);
extern uint32_t ucon64_dat_total_entries (void);
extern int ucon64_dat_view (int console, int verbose);
extern int ucon64_dat_indexer (void);
extern void ucon64_dat_nfo (const st_ucon64_dat_t *dat, int display_dat_file_line);
extern int ucon64_create_dat (const char *dat_file_name, const char *filename,
                              int backup_header_len);

#endif // UCON64_DAT_H
