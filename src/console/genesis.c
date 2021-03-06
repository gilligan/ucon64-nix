/*
genesis.c - Sega Genesis/Mega Drive support for uCON64

Copyright (c) 1999 - 2001              NoisyB
Copyright (c) 2002 - 2005, 2015 - 2021 dbjh


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
#ifdef  HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif
#include <ctype.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#include <io.h>
#pragma warning(pop)
#endif
#include <stdlib.h>
#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <sys/stat.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include "misc/archive.h"
#include "misc/chksum.h"
#include "misc/file.h"
#include "misc/misc.h"
#include "ucon64_misc.h"
#include "console/genesis.h"
#include "backup/md-pro.h"
#include "backup/mgd.h"
#include "backup/mgh.h"
#include "backup/smd.h"


#define GENESIS_HEADER_START 256
#define GENESIS_HEADER_LEN (sizeof (st_genesis_header_t))
#define GENESIS_NAME_LEN 48

static int genesis_chksum (unsigned char *rom_buffer);
static unsigned char *load_rom (st_ucon64_nfo_t *rominfo, const char *name,
                                unsigned char *rom_buffer);
static void save_rom (st_ucon64_nfo_t *rominfo, const char *name,
                      unsigned char **buffer, unsigned int size);


static st_ucon64_obj_t genesis_obj[] =
  {
    {0, WF_SWITCH},
    {0, WF_DEFAULT},
    {0, WF_DEFAULT | WF_NO_SPLIT},
    {0, WF_INIT | WF_PROBE},
    {0, WF_INIT | WF_PROBE | WF_STOP},
    {UCON64_GEN, WF_SWITCH},
    {UCON64_GEN, WF_DEFAULT},
    {UCON64_GEN, WF_DEFAULT | WF_NO_SPLIT}
  };

const st_getopt2_t genesis_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "Genesis/Sega Mega Drive/Sega CD/32X/Nomad"/*"1989/19XX/19XX Sega http://www.sega.com"*/,
      NULL
    },
    {
      UCON64_GEN_S, 0, 0, UCON64_GEN,
      NULL, "force recognition",
      &genesis_obj[5]
    },
    {
      "int", 0, 0, UCON64_INT,
      NULL, "force ROM is in interleaved format (SMD)",
      &genesis_obj[0]
    },
    {
      "int2", 0, 0, UCON64_INT2,
      NULL, "force ROM is in interleaved format 2 (MGD2)",
      &genesis_obj[0]
    },
    {
      "nint", 0, 0, UCON64_NINT,
      NULL, "force ROM is not in interleaved format (BIN/RAW)",
      &genesis_obj[0]
    },
    {
      "n", 1, 0, UCON64_N,
      "NEW_NAME", "change foreign ROM name to NEW_NAME",
      &genesis_obj[1]
    },
    {
      "n2", 1, 0, UCON64_N2,
      "NEW_NAME", "change Japanese ROM name to NEW_NAME",
      &genesis_obj[6]
    },
    {
      "smd", 0, 0, UCON64_SMD,
      NULL, "convert to Magic Drive Plus/Super Magic Drive/SMD",
      &genesis_obj[2]
    },
    {
      "smds", 0, 0, UCON64_SMDS,
      NULL, "convert *.srm (SRAM) file to Super Magic Drive/SMD",
      NULL
    },
    {
      "bin", 0, 0, UCON64_BIN,
      NULL, "convert to Magicom/BIN/RAW",
      &genesis_obj[7]
    },
    {
      "mgd", 0, 0, UCON64_MGD,
      NULL, "convert to Multi Game Doctor 2/MGD2",
      &genesis_obj[2]
    },
    {
      "mgh", 0, 0, UCON64_MGH,
      NULL, "convert to Multi Game Hunter/MGH",
      &genesis_obj[2]
    },
#if 0
    {
      "gf", 0, 0, UCON64_GF,
      NULL, "convert Sega CD country code to Europe; ROM=CD_IMAGE",
      NULL
    },
    {
      "ga", 0, 0, UCON64_GA,
      NULL, "convert Sega CD country code to U.S.A.; ROM=CD_IMAGE",
      NULL
    },
    {
      "gym", 0, 0, UCON64_GYM,
      NULL, "convert GYM (Genecyst) sound to WAV; ROM=GYMFILE",
      NULL
    },
    {
      "cym", 0, 0, UCON64_CYM,
      NULL, "convert CYM (Callus emulator) sound to WAV; ROM=CYMFILE",
      NULL
    },
#endif
    {
      "stp", 0, 0, UCON64_STP,
      NULL, "convert SRAM from backup unit for use with an emulator\n"
      OPTION_LONG_S "stp just strips the first 512 bytes",
      NULL
    },
    {
      "j", 0, 0, UCON64_J,
      NULL, "join split ROM",
      &genesis_obj[3]
    },
    {
      "s", 0, 0, UCON64_S,
      NULL, "split ROM; default part size is 8 Mb (4 Mb for SMD)",
      &genesis_obj[2]
    },
    {
      "smgh", 0, 0, UCON64_SMGH,
      NULL, "split ROM for Multi Game Hunter/MGH",
      &genesis_obj[2]
    },
    {
      "ssize", 1, 0, UCON64_SSIZE,
      "SIZE", "specify split part size in Mbit",
      &genesis_obj[0]
    },
    {
      "f", 0, 0, UCON64_F,
      NULL, "remove NTSC/PAL protection",
      &genesis_obj[1]
    },
    {
      "chk", 0, 0, UCON64_CHK,
      NULL, "fix ROM checksum",
      &genesis_obj[1]
    },
    {
      "1991", 0, 0, UCON64_1991,
      NULL, "fix old third party ROMs to work with consoles build after\n"
      "October 1991 by inserting \"(C) SEGA\" and \"(C)SEGA\"",
      &genesis_obj[6]
    },
    {
      "multi", 1, 0, UCON64_MULTI,
      "SIZE", "make multi-game file for use with MD-PRO flash card, truncated\n"
      "to SIZE Mbit; file with loader must be specified first, then\n"
      "all the ROMs, multi-game file to create last",
      &genesis_obj[4]
    },
    {
      "region", 1, 0, UCON64_REGION,
      "CODE", "enable region function; use with " OPTION_LONG_S "multi\n"
      "CODE" OPTARG_S "0 force NTSC/Japan for all games\n"
      "CODE" OPTARG_S "1 force NTSC/U.S.A. for all games\n"
      "CODE" OPTARG_S "2 force PAL for all games\n"
      "CODE" OPTARG_S "x use whatever setting games expect",
      &genesis_obj[5]
    },
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };

const st_getopt2_t bin_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "Magicom/BIN/RAW",
      NULL
    },
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };

typedef struct st_genesis_header
{
  char pad[256];
} st_genesis_header_t;

static st_genesis_header_t genesis_header;
static genesis_copier_t copier_type;
static unsigned int genesis_rom_size, genesis_has_ram, genesis_tv_standard,
                    genesis_japanese;


genesis_copier_t
genesis_get_copier_type (void)
{
  return copier_type;
}


int
genesis_smd (st_ucon64_nfo_t *rominfo)
{
  st_smd_header_t header;
  char dest_name[FILENAME_MAX];
  unsigned char *rom_buffer = NULL;

  if ((rom_buffer = load_rom (rominfo, ucon64.fname, rom_buffer)) == NULL)
    return -1;

  memset (&header, 0, SMD_HEADER_LEN);
  header.size = (unsigned char) (genesis_rom_size / 16384);
  header.id0 = 3;
  header.id1 = 0xaa;
  header.id2 = 0xbb;
  header.type = 6;

  strcpy (dest_name, ucon64.fname);
  set_suffix (dest_name, ".smd");
  ucon64_file_handler (dest_name, NULL, 0);

  ucon64_fwrite (&header, 0, SMD_HEADER_LEN, dest_name, "wb");
  smd_interleave (rom_buffer, genesis_rom_size);
  ucon64_fwrite (rom_buffer, SMD_HEADER_LEN, genesis_rom_size, dest_name, "ab");

  free (rom_buffer);
  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
genesis_smds (void)
{
  char dest_name[FILENAME_MAX];
  unsigned char buf[32768];
  st_smd_header_t header;

  memset (&header, 0, SMD_HEADER_LEN);
  memset (&buf, 0, 32768);
  ucon64_fread (buf, 0, (size_t) ucon64.fsize, ucon64.fname);

  header.id1 = 0xaa;
  header.id2 = 0xbb;
  header.type = 7;                              // SRAM file

  strcpy (dest_name, ucon64.fname);
  set_suffix (dest_name, ".sav");
  ucon64_file_handler (dest_name, NULL, 0);

  ucon64_fwrite (&header, 0, SMD_HEADER_LEN, dest_name, "wb");
  smd_interleave (buf, 32768);                  // SMD SRAM files are interleaved (Are they? - dbjh)
  ucon64_fwrite (buf, SMD_HEADER_LEN, 32768, dest_name, "ab");

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
genesis_bin (st_ucon64_nfo_t *rominfo)
{
  char dest_name[FILENAME_MAX];
  unsigned char *rom_buffer = NULL;

  if ((rom_buffer = load_rom (rominfo, ucon64.fname, rom_buffer)) == NULL)
    return -1;

  strcpy (dest_name, ucon64.fname);
  set_suffix (dest_name, ".bin");
  ucon64_file_handler (dest_name, NULL, 0);

  ucon64_fwrite (rom_buffer, 0, genesis_rom_size, dest_name, "wb");

  free (rom_buffer);
  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


// see src/backup/mgd.h for the file naming scheme
int
genesis_mgd (st_ucon64_nfo_t *rominfo)
{
  unsigned char *rom_buffer = NULL;
  char dest_name[FILENAME_MAX];

  // pad the file to the next valid MGD2 size if it doesn't have a valid size
  if (genesis_rom_size <= 1 * MBIT)
    genesis_rom_size = 1 * MBIT;
  else if (genesis_rom_size <= 2 * MBIT)
    genesis_rom_size = 2 * MBIT;
  else if (genesis_rom_size <= 4 * MBIT)
    genesis_rom_size = 4 * MBIT;
  else if (genesis_rom_size <= 8 * MBIT)
    genesis_rom_size = 8 * MBIT;
  else if (genesis_rom_size <= 16 * MBIT)
    genesis_rom_size = 16 * MBIT;
  else if (genesis_rom_size <= 20 * MBIT)
    genesis_rom_size = 20 * MBIT;
  else if (genesis_rom_size <= 24 * MBIT)
    genesis_rom_size = 24 * MBIT;
  else
    genesis_rom_size = 32 * MBIT;

  if ((rom_buffer = load_rom (rominfo, ucon64.fname, rom_buffer)) == NULL)
    return -1;

  mgd_make_name (ucon64.fname, UCON64_GEN, genesis_rom_size, dest_name);
  ucon64_file_handler (dest_name, NULL, OF_FORCE_BASENAME);

  mgd_interleave (&rom_buffer, genesis_rom_size);
  ucon64_fwrite (rom_buffer, 0, genesis_rom_size, dest_name, "wb");

  printf (ucon64_msg[WROTE], dest_name);
  free (rom_buffer);

  mgd_write_index_file ((char *) basename2 (dest_name), 1);

  return 0;
}


static void
write_mgh_name_file (st_ucon64_nfo_t *rominfo, const char *dest_name)
{
  unsigned char mgh_data[512] = "MGH\x1a";
  int x, y;
  const unsigned char mgh_charset[1024] =
    {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00, // !
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x3c, 0x66, 0x18, 0x3c, 0x66, 0x66, 0x3c, 0x02, // &
      0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, // '
      0x0c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0c, 0x00, // (
      0x30, 0x18, 0x18, 0x18, 0x18, 0x18, 0x30, 0x00, // )
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x08, // ,
      0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, // -
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, // .
      0x06, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x60, 0x00, // /
      0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, // 0
      0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, // 1
      0x3c, 0x66, 0x06, 0x06, 0x7c, 0x60, 0x7e, 0x00, // 2
      0x3c, 0x66, 0x06, 0x1c, 0x06, 0x66, 0x3c, 0x00, // 3
      0x18, 0x38, 0x58, 0x7c, 0x18, 0x18, 0x3c, 0x00, // 4
      0x7e, 0x60, 0x7c, 0x06, 0x06, 0x66, 0x3c, 0x00, // 5
      0x3c, 0x66, 0x60, 0x7c, 0x66, 0x66, 0x3c, 0x00, // 6
      0x7e, 0x06, 0x0c, 0x0c, 0x18, 0x18, 0x18, 0x00, // 7
      0x3c, 0x66, 0x66, 0x3c, 0x66, 0x66, 0x3c, 0x00, // 8
      0x3c, 0x66, 0x66, 0x3e, 0x06, 0x66, 0x3c, 0x00, // 9
      0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00, // :
      0x00, 0x00, 0x18, 0x00, 0x18, 0x08, 0x00, 0x00, // ;
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00, // =
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00, // A
      0x7c, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x7c, 0x00, // B
      0x3c, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3c, 0x00, // C
      0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x00, // D
      0x7e, 0x60, 0x60, 0x7e, 0x60, 0x60, 0x7e, 0x00, // E
      0x7e, 0x60, 0x60, 0x7e, 0x60, 0x60, 0x60, 0x00, // F
      0x3e, 0x60, 0x60, 0x6e, 0x66, 0x66, 0x3e, 0x00, // G
      0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00, // H
      0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, // I
      0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00, // J
      0x66, 0x6c, 0x78, 0x70, 0x78, 0x6c, 0x66, 0x00, // K
      0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00, // L
      0xc6, 0xee, 0xfe, 0xd6, 0xc6, 0xc6, 0xc6, 0x00, // M
      0x66, 0x76, 0x7e, 0x7e, 0x6e, 0x66, 0x66, 0x00, // N
      0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, // O
      0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x00, // P
      0x3c, 0x66, 0x66, 0x66, 0x66, 0x6e, 0x3e, 0x00, // Q
      0x7c, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0x66, 0x00, // R
      0x3c, 0x66, 0x60, 0x3c, 0x06, 0x66, 0x3c, 0x00, // S
      0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, // T
      0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00, // U
      0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x3c, 0x00, // V
      0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x2c, 0x00, // W
      0x66, 0x66, 0x3c, 0x10, 0x3c, 0x66, 0x66, 0x00, // X
      0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x00, // Y
      0x7e, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x7e, 0x00, // Z
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, // _
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x3e, 0x00, // a
      0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x7c, 0x00, // b
      0x00, 0x00, 0x3c, 0x66, 0x60, 0x66, 0x3c, 0x00, // c
      0x06, 0x06, 0x6e, 0x66, 0x66, 0x66, 0x3e, 0x00, // d
      0x00, 0x00, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00, // e
      0x0c, 0x18, 0x18, 0x3c, 0x18, 0x18, 0x3c, 0x00, // f
      0x00, 0x00, 0x3c, 0x66, 0x66, 0x3e, 0x06, 0x7c, // g
      0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00, // h
      0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, // i
      0x00, 0x0c, 0x00, 0x0c, 0x0c, 0x0c, 0x6c, 0x38, // j
      0x60, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0x00, // k
      0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, // l
      0x00, 0x00, 0x5c, 0x76, 0x56, 0x56, 0x56, 0x00, // m
      0x00, 0x00, 0x6c, 0x7e, 0x66, 0x66, 0x66, 0x00, // n
      0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x00, // o
      0x00, 0x00, 0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60, // p
      0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x06, // q
      0x00, 0x00, 0x18, 0x1a, 0x18, 0x18, 0x3c, 0x00, // r
      0x00, 0x00, 0x3e, 0x60, 0x3c, 0x06, 0x7c, 0x00, // s
      0x00, 0x18, 0x3c, 0x18, 0x18, 0x18, 0x3c, 0x00, // t
      0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3a, 0x00, // u
      0x00, 0x00, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x00, // v
      0x00, 0x00, 0x56, 0x56, 0x56, 0x56, 0x7e, 0x00, // w
      0x00, 0x00, 0x66, 0x66, 0x18, 0x66, 0x66, 0x00, // x
      0x00, 0x00, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x7c, // y
      0x00, 0x00, 0x7e, 0x0c, 0x18, 0x30, 0x7e, 0x00, // z
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00, // |
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

  mgh_data[31] = 0xff;
  for (x = 0; x < 15; x++)
    {
      for (y = 0; y < 4; y++)
        mgh_data[(x + 2) * 16 + y + 4] = mgh_charset[(rominfo->name[x] & 0x7f) * 8 + y];
      for (y = 4; y < 8; y++)
        mgh_data[(x + 2) * 16 + y + 244] = mgh_charset[(rominfo->name[x] & 0x7f) * 8 + y];
    }

  // If a backup would be created it would overwrite the backup of the ROM. The
  //  ROM backup is more important, so we don't write a backup of the MGH file.
  ucon64_fwrite (mgh_data, 0, sizeof mgh_data, dest_name, "wb");
  printf (ucon64_msg[WROTE], dest_name);
}


int
genesis_mgh (st_ucon64_nfo_t *rominfo)
{
  unsigned char *rom_buffer = NULL;
  char dest_name[FILENAME_MAX];

  if ((rom_buffer = load_rom (rominfo, ucon64.fname, rom_buffer)) == NULL)
    return -1;

  mgh_make_name (ucon64.fname, UCON64_GEN, genesis_rom_size, dest_name);
  ucon64_file_handler (dest_name, NULL, OF_FORCE_BASENAME);

  mgd_interleave (&rom_buffer, genesis_rom_size);
  ucon64_fwrite (rom_buffer, 0, genesis_rom_size, dest_name, "wb");

  printf (ucon64_msg[WROTE], dest_name);
  free (rom_buffer);

  set_suffix (dest_name, ".MGH");
  write_mgh_name_file (rominfo, dest_name);
  return 0;
}


int
genesis_s (st_ucon64_nfo_t *rominfo)
{
  st_smd_header_t smd_header;
  char dest_name[FILENAME_MAX], *p;
  unsigned int x, nparts, surplus, part_size,
               size = (unsigned int) ucon64.fsize - rominfo->backup_header_len;

  if (UCON64_ISSET (ucon64.part_size))
    {
      part_size = ucon64.part_size;
      // don't allow too small part sizes, see src/console/snes.c (snes_s())
      if (part_size < 4 * MBIT)
        {
          fputs ("ERROR: Split part size must be larger than or equal to 4 Mbit\n",
                 stderr);
          return -1;
        }
    }
  else
    {
      if (copier_type == SMD)
        part_size = 4 * MBIT;                   // SMD uses 4 Mb parts
      else
        part_size = 8 * MBIT;                   // MGD2 and Magicom ("BIN") use
    }                                           //  8 Mb parts

  if (size <= part_size)
    {
      printf ("NOTE: ROM size is smaller than or equal to %u Mbit -- will not be split\n",
              part_size / MBIT);
      return -1;
    }

  nparts = size / part_size;
  surplus = size % part_size;

  if (copier_type == SMD)
    {
      ucon64_fread (&smd_header, 0, SMD_HEADER_LEN, ucon64.fname);

      strcpy (dest_name, ucon64.fname);
      set_suffix (dest_name, ".1");
      ucon64_output_fname (dest_name, 0);
      p = strrchr (dest_name, '.') + 1;

      smd_header.size = (unsigned char) (part_size / 16384);
      smd_header.id0 = 3;
      // if smd_header.split bit 6 == 0 -> last file of the ROM
      smd_header.split |= 0x40;
      for (x = 0; x < nparts; x++)
        {
          if (surplus == 0 && x == nparts - 1)
            smd_header.split &= ~0x40;          // last file => clear bit 6

          // don't write backups of parts, because one name is used
          ucon64_fwrite (&smd_header, 0, SMD_HEADER_LEN, dest_name, "wb");
          fcopy (ucon64.fname, x * part_size + rominfo->backup_header_len, part_size, dest_name, "ab");
          printf (ucon64_msg[WROTE], dest_name);

          (*p)++;
        }

      if (surplus)
        {
          smd_header.size = (unsigned char) (surplus / 16384);
          smd_header.split &= ~0x40;            // last file => clear bit 6

          // don't write backups of parts, because one name is used
          ucon64_fwrite (&smd_header, 0, SMD_HEADER_LEN, dest_name, "wb");
          fcopy (ucon64.fname, x * part_size + rominfo->backup_header_len, surplus, dest_name, "ab");
          printf (ucon64_msg[WROTE], dest_name);
        }
    }
  else if (copier_type == MGD_GEN)
    {
      char *names[8], names_mem[8][9] = { { 0 } };
      const char *p0;
      unsigned int n, name_i = 0;

      if (surplus)
        nparts++;
      if (nparts > sizeof names / sizeof names[0])
        {
          fprintf (stderr,
                   "ERROR: Splitting this ROM would result in %u parts (of %u Mbit).\n"
                   "       %u is the maximum number of parts for Multi Game Doctor 2\n",
                   nparts, part_size / MBIT, (unsigned) (sizeof names / sizeof names[0]));
          return -1;
        }

      for (n = 0; n < sizeof names / sizeof names[0]; n++)
        names[n] = names_mem[n];

      mgd_make_name (ucon64.fname, UCON64_GEN, genesis_rom_size, dest_name);
      ucon64_output_fname (dest_name, OF_FORCE_BASENAME);
      p0 = basename2 (dest_name);
      p = strrchr (p0, '.') - 1;
      *p = 'A';
      n = (unsigned int) (p + 1 - p0);

      for (x = 0; x < nparts; x++)
        {
          unsigned int offset, chunk_size;

          offset = x * (part_size / 2);
          chunk_size = part_size / 2;
          if (offset + chunk_size > ucon64.fsize / 2)
            chunk_size = (unsigned int) ucon64.fsize / 2 - offset;
          // don't write backups of parts, because one name is used
          // write first half of file
          fcopy (ucon64.fname, offset, chunk_size, dest_name, "wb");
          // write second half of file; don't do: "(nparts / 2) * part_size"!
          fcopy (ucon64.fname, ucon64.fsize / 2 + offset, chunk_size,
                 dest_name, "ab");
          printf (ucon64_msg[WROTE], dest_name);

          strncpy (names[name_i++], p0, n)[n] = '\0';
          (*p)++;
        }
      mgd_write_index_file (names, name_i);
    }
  else // copier_type == BIN
    {
      strcpy (dest_name, ucon64.fname);
      set_suffix (dest_name, ".r01");
      ucon64_output_fname (dest_name, 0);
      p = strrchr (dest_name, '.') + 3;

      for (x = 0; x < nparts; x++)
        {
          if (surplus == 0 && x == nparts - 1)
            *p = '0';                           // last file should have suffix ".r00"
          fcopy (ucon64.fname, x * part_size + rominfo->backup_header_len,
                 part_size, dest_name, "wb");
          printf (ucon64_msg[WROTE], dest_name);
          (*p)++;
        }

      if (surplus != 0)
        {
          *p = '0';
          fcopy (ucon64.fname, x * part_size + rominfo->backup_header_len,
                 surplus, dest_name, "wb");
          printf (ucon64_msg[WROTE], dest_name);
        }
    }

  return 0;
}


int
genesis_smgh (st_ucon64_nfo_t *rominfo)
{
  const char *p0;
  char dest_name[FILENAME_MAX], *p, *suffix;
  unsigned int n, x, nparts, surplus, part_size,
               size = (unsigned int) ucon64.fsize - rominfo->backup_header_len;

  if (UCON64_ISSET (ucon64.part_size))
    {
      part_size = ucon64.part_size;
      // don't allow too small part sizes, see src/console/snes.c (snes_s())
      if (part_size < 4 * MBIT)
        {
          fputs ("ERROR: Split part size must be larger than or equal to 4 Mbit\n",
                 stderr);
          return -1;
        }
    }
  else
    part_size = 8 * MBIT;

  if (size <= part_size)
    {
      printf ("NOTE: ROM size is smaller than or equal to %u Mbit -- will not be split\n",
              part_size / MBIT);
      return -1;
    }

  nparts = size / part_size;
  surplus = size % part_size;

  if (surplus)
    nparts++;

  mgh_make_name (ucon64.fname, UCON64_GEN, genesis_rom_size, dest_name);
  ucon64_output_fname (dest_name, OF_FORCE_BASENAME);
  p0 = basename2 (dest_name);
  p = strrchr (p0, '.');
  suffix = strdup (p);
  n = (unsigned int) (p - p0);
  if (n <= 7)
    {
      memset (p, '_', 7 - n);
      p += 7 - n;
    }
  else // n == 8
    p--;
  *p = 'A';
  *(p + 1) = '\0';
  set_suffix (dest_name, suffix);
  free (suffix);

  for (x = 0; x < nparts; x++)
    {
      unsigned int offset, chunk_size;

      offset = x * (part_size / 2);
      chunk_size = part_size / 2;
      if (offset + chunk_size > ucon64.fsize / 2)
        chunk_size = (unsigned int) ucon64.fsize / 2 - offset;
      // don't write backups of parts, because one name is used
      // write first half of file
      fcopy (ucon64.fname, offset, chunk_size, dest_name, "wb");
      // write second half of file; don't do: "(nparts / 2) * part_size"!
      fcopy (ucon64.fname, ucon64.fsize / 2 + offset, chunk_size, dest_name,
             "ab");
      printf (ucon64_msg[WROTE], dest_name);

      (*p)++;
    }

  *p = 'A';
  set_suffix (dest_name, ".MGH");
  write_mgh_name_file (rominfo, dest_name);
  return 0;
}


int
genesis_j (st_ucon64_nfo_t *rominfo)
{
  char src_name[FILENAME_MAX], dest_name[FILENAME_MAX], *p;
  unsigned int block_size;

  if (copier_type == SMD)
    {
      unsigned int total_size = 0;

      strcpy (dest_name, ucon64.fname);
      set_suffix (dest_name, ".smd");
      ucon64_file_handler (dest_name, NULL, 0);

      strcpy (src_name, ucon64.fname);
      p = strrchr (src_name, '.') + 1;
      fcopy (src_name, 0, rominfo->backup_header_len, dest_name, "wb");
      block_size = (unsigned int) ucon64.fsize - rominfo->backup_header_len;
      while (fcopy (src_name, rominfo->backup_header_len, block_size, dest_name,
                    "ab") != -1)
        {
          printf ("Joined %s\n", src_name);
          total_size += block_size;
          (*p)++;
          block_size = (unsigned int) fsizeof (src_name) - rominfo->backup_header_len;
        }

      if (rominfo->backup_header_len)
        {                                       // fix header
          unsigned char buf[3];

          buf[0] = (unsigned char) (total_size / 16384); // # 16 kB blocks
          buf[1] = 3;                           // ID 0
          buf[2] = 0;                           // last file -> clear bit 6
          ucon64_fwrite (buf, 0, 3, dest_name, "r+b");
          buf[0] = 0xaa;                        // ID 1
          buf[1] = 0xbb;                        // ID 2
          buf[2] = 6;                           // type Genesis
          ucon64_fwrite (buf, 8, 3, dest_name, "r+b");
        }

      printf (ucon64_msg[WROTE], dest_name);
    }
  else if (copier_type == MGD_GEN)
    {
      /*
        file1 file2 file3 file4
        1/2   3/4   5/6   7/8 (1st half/2nd half)
        joined file
        1/3/5/7/2/4/6/8
      */
      strcpy (dest_name, ucon64.fname);
      set_suffix (dest_name, ".tmp");           // users should use -mgd to get
      ucon64_file_handler (dest_name, NULL, 0); //  a correct name
      remove (dest_name);

      strcpy (src_name, ucon64.fname);
      p = strrchr (src_name, '.') - 1;
      block_size = ((unsigned int) ucon64.fsize - rominfo->backup_header_len) / 2;
      while (fcopy (src_name, rominfo->backup_header_len, block_size, dest_name,
                    "ab") != -1)
        {
          (*p)++;
          // BUG ALERT: assume all parts have the same header length
          block_size = ((unsigned int) fsizeof (src_name) - rominfo->backup_header_len) / 2;
        }

      strcpy (src_name, ucon64.fname);
      p = strrchr (src_name, '.') - 1;
      block_size = ((unsigned int) ucon64.fsize - rominfo->backup_header_len) / 2;
      while (fcopy (src_name, rominfo->backup_header_len + block_size,
                    block_size, dest_name, "ab") != -1)
        {
          printf ("Joined %s\n", src_name);     // print this here, not in the
          (*p)++;                               //  previous loop
          // BUG ALERT: assume all parts have the same header length
          block_size = ((unsigned int) fsizeof (src_name) - rominfo->backup_header_len) / 2;
        }

      printf (ucon64_msg[WROTE], dest_name);
    }
  else if (copier_type == BIN)
    {
      int tried_r00 = 0;

      strcpy (dest_name, ucon64.fname);
      set_suffix (dest_name, ".bin");
      ucon64_file_handler (dest_name, NULL, 0);
      remove (dest_name);

      strcpy (src_name, ucon64.fname);
      p = strrchr (src_name, '.') + 3;
      *(p - 1) = '0';                           // be user friendly and avoid confusion
      *p = '1';                                 //  (.r01 is first file, not .r00)
      block_size = (unsigned int) ucon64.fsize - rominfo->backup_header_len;
      while (fcopy (src_name, rominfo->backup_header_len, block_size, dest_name,
                    "ab") != -1)
        {
          printf ("Joined %s\n", src_name);
          (*p)++;
          if (tried_r00)
            break;                              // quit after joining last file
          else if (access (src_name, F_OK) != 0)
            {                                   // file does not exist -> try .r00
              *p = '0';
              tried_r00 = 1;
            }
          block_size = (unsigned int) fsizeof (src_name) - rominfo->backup_header_len;
        }

      printf (ucon64_msg[WROTE], dest_name);
    }

  return 0;
}


static int
genesis_name (st_ucon64_nfo_t *rominfo, const char *name1, const char *name2)
{
  unsigned char *rom_buffer = NULL;
  char buf[FILENAME_MAX];

  if ((rom_buffer = load_rom (rominfo, ucon64.fname, rom_buffer)) == NULL)
    return -1;

  if (name1)
    {
      size_t len = strnlen (name1, GENESIS_NAME_LEN);

      if (len < GENESIS_NAME_LEN)               // warning remover
        memset (buf + len, ' ', GENESIS_NAME_LEN - len);
      strncpy (buf, name1, len);
      memcpy (&rom_buffer[GENESIS_HEADER_START + 32 + GENESIS_NAME_LEN], buf,
                GENESIS_NAME_LEN);
    }
  if (name2)
    {
      size_t len = strnlen (name2, GENESIS_NAME_LEN);

      if (len < GENESIS_NAME_LEN)               // warning remover
        memset (buf + len, ' ', GENESIS_NAME_LEN - len);
      strncpy (buf, name2, len);
      memcpy (&rom_buffer[GENESIS_HEADER_START + 32], buf, GENESIS_NAME_LEN);
    }

  strcpy (buf, ucon64.fname);
  ucon64_file_handler (buf, NULL, 0);
  save_rom (rominfo, buf, &rom_buffer, genesis_rom_size);

  free (rom_buffer);
  printf (ucon64_msg[WROTE], buf);
  return 0;
}


int
genesis_n (st_ucon64_nfo_t *rominfo, const char *name)
{
  return genesis_name (rominfo, name, NULL);
}


int
genesis_n2 (st_ucon64_nfo_t *rominfo, const char *name)
{
  return genesis_name (rominfo, NULL, name);
}


int
genesis_1991 (st_ucon64_nfo_t *rominfo)
{
  return genesis_name (rominfo, "(C)SEGA", "(C) SEGA");
}


int
genesis_chk (st_ucon64_nfo_t *rominfo)
{
  unsigned char *rom_buffer = NULL;
  char dest_name[FILENAME_MAX];

  if ((rom_buffer = load_rom (rominfo, ucon64.fname, rom_buffer)) == NULL)
    return -1;

  rom_buffer[GENESIS_HEADER_START + 143] =
    (unsigned char) rominfo->current_internal_crc; // low byte of checksum
  rom_buffer[GENESIS_HEADER_START + 142] =
    (unsigned char) (rominfo->current_internal_crc >> 8); // high byte of checksum

  dumper (stdout, &rom_buffer[GENESIS_HEADER_START + 0x8e], 2,
          GENESIS_HEADER_START + 0x8e, DUMPER_HEX);

  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, NULL, 0);
  save_rom (rominfo, dest_name, &rom_buffer, genesis_rom_size);

  free (rom_buffer);
  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


static int
genesis_fix_pal_protection (st_ucon64_nfo_t *rominfo)
// This function searches for PAL protection codes. If it finds one it will
//  fix the code so that the game will run on a Genesis (NTSC).
{
  char fname[FILENAME_MAX];
  unsigned char *rom_buffer = NULL;
  int offset = 0, n = 0, n_extra_patterns, n2;
  st_cm_pattern_t *patterns = NULL;

  strcpy (fname, "genpal.txt");
  // first try the current directory, then the configuration directory
  if (access (fname, F_OK | R_OK) == -1)
    {
      snprintf (fname, FILENAME_MAX, "%s" DIR_SEPARATOR_S "genpal.txt",
                ucon64.configdir);
      fname[FILENAME_MAX - 1] = '\0';
    }
  n_extra_patterns = build_cm_patterns (&patterns, fname);
  if (n_extra_patterns >= 0)
    printf ("Found %d additional code%s in %s\n",
            n_extra_patterns, n_extra_patterns != 1 ? "s" : "", fname);

  puts ("Attempting to fix PAL protection code...");

  if ((rom_buffer = load_rom (rominfo, ucon64.fname, rom_buffer)) == NULL)
    return -1;

  while ((n2 = (int) ucon64.fsize - offset) > 0)
    {
      int block_size = n2 >= 16 * 1024 ? 16 * 1024 : n2;
      for (n2 = 0; n2 < n_extra_patterns; n2++)
        n += change_mem2 ((char *) rom_buffer + offset, block_size,
                          patterns[n2].search,
                          patterns[n2].search_size,
                          patterns[n2].wildcard,
                          patterns[n2].escape,
                          patterns[n2].replace,
                          patterns[n2].replace_size,
                          patterns[n2].offset,
                          patterns[n2].sets);
      offset += 16 * 1024;
    }
  cleanup_cm_patterns (&patterns, n_extra_patterns);

  strcpy (fname, ucon64.fname);
  ucon64_file_handler (fname, NULL, 0);
  save_rom (rominfo, fname, &rom_buffer, genesis_rom_size);

  free (rom_buffer);
  printf ("Found %d pattern%s\n", n, n != 1 ? "s" : "");
  printf (ucon64_msg[WROTE], fname);
  return n;
}


static int
genesis_fix_ntsc_protection (st_ucon64_nfo_t *rominfo)
// This function searches for NTSC protection codes. If it finds one it will
//  fix the code so that the game will run on a Mega Drive (PAL).
{
  char fname[FILENAME_MAX];
  unsigned char *rom_buffer = NULL;
  int offset = 0, n = 0, n_extra_patterns, n2;
  st_cm_pattern_t *patterns = NULL;

  strcpy (fname, "mdntsc.txt");
  // first try the current directory, then the configuration directory
  if (access (fname, F_OK | R_OK) == -1)
    {
      snprintf (fname, FILENAME_MAX, "%s" DIR_SEPARATOR_S "mdntsc.txt",
                ucon64.configdir);
      fname[FILENAME_MAX - 1] = '\0';
    }
  n_extra_patterns = build_cm_patterns (&patterns, fname);
  if (n_extra_patterns >= 0)
    printf ("Found %d additional code%s in %s\n",
            n_extra_patterns, n_extra_patterns != 1 ? "s" : "", fname);

  puts ("Attempting to fix NTSC protection code...");

  if ((rom_buffer = load_rom (rominfo, ucon64.fname, rom_buffer)) == NULL)
    return -1;

  while ((n2 = (int) ucon64.fsize - offset) > 0)
    {
      int block_size = n2 >= 16 * 1024 ? 16 * 1024 : n2;
      for (n2 = 0; n2 < n_extra_patterns; n2++)
        n += change_mem2 ((char *) rom_buffer + offset, block_size,
                          patterns[n2].search,
                          patterns[n2].search_size,
                          patterns[n2].wildcard,
                          patterns[n2].escape,
                          patterns[n2].replace,
                          patterns[n2].replace_size,
                          patterns[n2].offset,
                          patterns[n2].sets);
      offset += 16 * 1024;
    }
  cleanup_cm_patterns (&patterns, n_extra_patterns);

  strcpy (fname, ucon64.fname);
  ucon64_file_handler (fname, NULL, 0);
  save_rom (rominfo, fname, &rom_buffer, genesis_rom_size);

  free (rom_buffer);
  printf ("Found %d pattern%s\n", n, n != 1 ? "s" : "");
  printf (ucon64_msg[WROTE], fname);
  return n;
}


int
genesis_f (st_ucon64_nfo_t *rominfo)
{
  /*
    In the Philipines the television standard is NTSC, but do games made
    for the Philipines exist?
    Just like with SNES we don't guarantee anything for files that needn't be
    fixed/cracked/patched.
  */
  if (genesis_tv_standard == 0)                 // NTSC (Japan, U.S.A. or Brazil ('4'))
    return genesis_fix_ntsc_protection (rominfo);
  else
    return genesis_fix_pal_protection (rominfo);
}


static unsigned char *
load_rom (st_ucon64_nfo_t *rominfo, const char *name, unsigned char *rom_buffer)
{
  FILE *file;
  size_t bytesread;

  if ((file = fopen (name, "rb")) == NULL)
    return NULL;
  if ((rom_buffer = (unsigned char *) malloc (genesis_rom_size)) == NULL)
    {
      fprintf (stderr, ucon64_msg[ROM_BUFFER_ERROR], genesis_rom_size);
      fclose (file);
      return NULL;
    }
  fseek (file, rominfo->backup_header_len, SEEK_SET); // don't do this only for SMD!
  bytesread = fread (rom_buffer, 1, genesis_rom_size, file);
  if (bytesread < genesis_rom_size)
    memset (rom_buffer + bytesread, 0, genesis_rom_size - bytesread);

  if (copier_type != BIN)
    {
      if (ucon64.fcrc32 == 0)
        ucon64.fcrc32 = crc32 (ucon64.fcrc32, rom_buffer, genesis_rom_size);

      if (copier_type == SMD)
        smd_deinterleave (rom_buffer, bytesread);
      else // copier_type == MGD_GEN
        mgd_deinterleave (&rom_buffer, bytesread, genesis_rom_size);
    }

  if (ucon64.crc32 == 0)                        // calculate the CRC32 only once
    ucon64.crc32 = crc32 (0, rom_buffer, (unsigned int) bytesread);

  fclose (file);
  return rom_buffer;
}


static void
save_rom (st_ucon64_nfo_t *rominfo, const char *name, unsigned char **buffer,
          unsigned int size)
{
  if (copier_type == SMD)
    {
      /*
        Copy the complete backup unit header length, no matter how strange or
        how large the size. fcopy() doesn't copy if source and destination are
        one and the same. Copying is necessary if source and destination are
        different so that we can use "r+b" when writing the contents of buffer
        to disk.
      */
      fcopy (ucon64.fname, 0, rominfo->backup_header_len, name, "wb");

      smd_interleave (*buffer, size);
      ucon64_fwrite (*buffer, rominfo->backup_header_len, size, name, "r+b");
      if ((uint64_t) size < ucon64.fsize - rominfo->backup_header_len)
        truncate2 (name, rominfo->backup_header_len + size);
    }
  else if (copier_type == MGD_GEN)
    {
      mgd_interleave (buffer, size);            // allocates new buffer
      ucon64_fwrite (*buffer, 0, size, name, "wb");
    }
  else // copier_type == BIN
    ucon64_fwrite (*buffer, 0, size, name, "wb");
}


static void
write_game_table_entry (FILE *destfile, int file_no, st_ucon64_nfo_t *rominfo,
                        size_t totalsize, unsigned int size)
{
  long int fpos = ftell (destfile);             // save file pointer
  int n;
  unsigned char name[0x1c], flags = 0;          // SRAM/region flags: F, D (reserved), E, P, V, T, S1, S0

  fseek (destfile, 0x8000 + (file_no - 1) * 0x20, SEEK_SET);
  fputc (0xff, destfile);                       // 0x0 = 0xff
  for (n = 0; n < 0x1b; n++)
    name[n] = isprint ((int) rominfo->name[n]) ?
                (unsigned char) toupper ((int) rominfo->name[n]) : '.';
  // according to Leo, MDPACKU4.BIN only supports upper case characters
  /*
    NOTE: uCON64 2.0.0 wrote 0 at offset 0x1d. uCON64 2.0.1 repurposed offset
          0x1d for the ROM size. However, the byte at offset 0x1d was used as
          string terminator, which is plain silly. Compare with Super Flash and
          PCE-PRO multi-game creation code. So, now I terminate the string at
          offset 0x1c while retaining the later function of offset 0x1d. - dbjh
  */
  name[0x1b] = '\0';
  fwrite (name, 1, 0x1c, destfile);             // 0x1 - 0x1c = name
  fputc (size / MBIT, destfile);                // 0x1d = ROM size (not used by loader, but by us)
  fputc ((int) (totalsize / (2 * MBIT)), destfile); // 0x1e = bank code

  flags = 0x80;                                 // set F (?, default)
  if (genesis_has_ram)
    {
      static int sram_page = 0, file_no_sram = 0;

      if (sram_page == 3)
        file_no_sram = file_no;
      else if (sram_page > 3)
        {
          printf ("WARNING: This ROM will share SRAM with ROM %d\n", file_no_sram);
          sram_page = 3;
        }
      flags |= sram_page++;
      if (size <= 16 * MBIT)
        flags &= ~0x80;                         // clear F (<=16 Mb & SRAM)
    }
  else
    flags |= 4;                                 // set T (no SRAM)

  /*
    J(apan) would probably be a more logical name for bit E(urope). I (dbjh)
    base that on information of SamIAm. According to him there are three types
    of the console:
    American Genesis                  NTSC/no Japanese switch
    Japanese Genesis                  NTSC/Japanese switch
    Mega Drive (rest of the world?)   PAL/no Japanese switch
    So, besides PAL and NTSC there is the variable whether the console contains
    a Japanese "switch". The region function is used to make the game "think"
    it's running on another type of console. For example, a Japanese game
    running on a (European) Mega Drive should have the P and E bit set to 0.
  */
  if (UCON64_ISSET (ucon64.region))
    {
      if (!genesis_japanese)
        flags |= 0x20;                          // set E(urope)
      if (genesis_tv_standard == 1)
        flags |= 0x10;                          // set P(AL)

      flags |= 8;                               // set V (enable region function)
    }
  fputc (flags, destfile);                      // 0x1f = flags

  fseek (destfile, fpos, SEEK_SET);             // restore file pointer
}


int
genesis_multi (unsigned int truncate_size)
{
  unsigned int n, n_files, file_no, done, truncated = 0, size,
               org_do_not_calc_crc = ucon64.do_not_calc_crc;
  size_t bytestowrite, byteswritten, totalsize = 0;
  struct stat fstate;
  FILE *srcfile, *destfile;
  char destname[FILENAME_MAX];
  unsigned char buffer[32 * 1024];              // must be a multiple of 16 kB

  if (truncate_size == 0)
    {
      fputs ("ERROR: Cannot make multi-game file of 0 bytes\n", stderr);
      return -1;
    }

  n_files = ucon64.argc - 1;
  snprintf (destname, FILENAME_MAX, "%s", ucon64.argv[n_files]);
  destname[FILENAME_MAX - 1] = '\0';

  ucon64_file_handler (destname, NULL, OF_FORCE_BASENAME);
  if ((destfile = fopen (destname, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], destname);
      return -1;
    }

  // do this check here, because one error message is enough (not for every game)
  if (UCON64_ISSET (ucon64.region))
    switch (ucon64.region)
      {
      case 0:                                   // NTSC/Japan
      case 1:                                   // NTSC/U.S.A.
      case 2:                                   // PAL
      case 256:
        break;
      default:
        puts ("WARNING: Invalid region code specified, using values games expect");
      }

  printf ("Creating multi-game file for MD-PRO: %s\n", destname);

  file_no = 0;
  for (n = 1; n < n_files; n++)
    {
      if (access (ucon64.argv[n], F_OK))
        continue;                               // "file" does not exist (option)
      stat (ucon64.argv[n], &fstate);
      if (!S_ISREG (fstate.st_mode))
        continue;
      if (file_no == 32)                        // loader + 31 games
        {
          puts ("WARNING: A multi-game file can contain a maximum of 31 games. The other files\n"
                "         are ignored");
          break;
        }

      ucon64.console = UCON64_UNKNOWN;
      ucon64.fname = ucon64.argv[n];
      ucon64.fsize = fsizeof (ucon64.fname);
      // DON'T use fstate.st_size, because file could be compressed
      ucon64.do_not_calc_crc = 1;
      if (genesis_init (ucon64.nfo) != 0)
        printf ("WARNING: %s does not appear to be a Genesis ROM\n", ucon64.fname);
      /*
        NOTE: This is NOT the place to mess with ucon64.console. When this
              function is entered ucon64.console must have been UCON64_GEN. We
              modify ucon64.console temporarily only to be able to help detect
              problems with incorrect files.
      */
      if (UCON64_ISSET (ucon64.region))
        switch (ucon64.region)
          {
          case 0:                               // NTSC/Japan
            genesis_tv_standard = 0;
            genesis_japanese = 1;
            break;
          case 1:                               // NTSC/U.S.A.
            genesis_tv_standard = 0;
            genesis_japanese = 0;
            break;
          case 2:                               // PAL
            genesis_tv_standard = 1;
            genesis_japanese = 0;
            break;
          case 256:
            // Do nothing. Use whatever values we found for genesis_tv_standard and
            //  genesis_japanese.
            break;
          // no default case, because we already checked value of ucon64.region
          }

      if ((srcfile = fopen (ucon64.fname, "rb")) == NULL)
        {
          fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], ucon64.fname);
          continue;
        }
      if (ucon64.nfo->backup_header_len)
        fseek (srcfile, ucon64.nfo->backup_header_len, SEEK_SET);
      size = (unsigned int) ucon64.fsize - ucon64.nfo->backup_header_len;

      if (file_no == 0)
        {
          printf ("Loader: %s\n", ucon64.fname);
          if (size != MD_PRO_LOADER_SIZE)
            printf ("WARNING: Are you sure %s is a loader binary?\n", ucon64.fname);
        }
      else
        {
          printf ("ROM%u: %s\n", file_no, ucon64.fname);
          write_game_table_entry (destfile, file_no, ucon64.nfo, totalsize,
                                  size);
        }
      file_no++;

      done = 0;
      byteswritten = 0;                         // # of bytes written per file
      while (!done)
        {
          if (ucon64.nfo->interleaved == 2)
            bytestowrite = fread_mgd (buffer, 1, sizeof buffer, srcfile);
          else
            {
              bytestowrite = fread (buffer, 1, sizeof buffer, srcfile);
              if (ucon64.nfo->interleaved)
                smd_deinterleave (buffer, sizeof buffer);
                // yes, sizeof buffer. bytestowrite might not be n * 16 kB
            }
          if (totalsize + bytestowrite > truncate_size)
            {
              bytestowrite = truncate_size - totalsize;
              done = 1;
              truncated = 1;
              printf ("Output file needs %u Mbit on flash card, truncating %s, skipping %u bytes\n",
                      truncate_size / MBIT, ucon64.fname,
                      (unsigned) (size - (byteswritten + bytestowrite)));
            }
          totalsize += bytestowrite;
          if (bytestowrite == 0)
            done = 1;
          fwrite (buffer, 1, bytestowrite, destfile);
          byteswritten += bytestowrite;
        }

      // Be sure the ROM size is a multiple of the buffer size used in
      //  md_write_rom(), which is 0x4000 (16 kB, so using "buffer" is OK).
      bytestowrite = ((size + 0x3fff) & ~0x3fff) - size;
      if (bytestowrite)
        {
          memset (buffer, 0xff, bytestowrite);
          totalsize += bytestowrite;
          fwrite (buffer, 1, bytestowrite, destfile);
        }

      // md_write_rom() handles alignment. Games have to be aligned to (start
      //  at) a 2 Mbit boundary.
      totalsize = (totalsize + 2 * MBIT - 1) & ~(2 * MBIT - 1);
      fclose (srcfile);
      if (truncated)
        break;
    }
  // fill the next game table entry
  fseek (destfile, 0x8000 + (file_no - 1) * 0x20, SEEK_SET);
  fputc (0, destfile);                          // indicate no next game

  // make it possible for md_write_rom() to detect that the file is a
  //  multi-game file
  fseek (destfile, 0x83f4, SEEK_SET);
#if     defined __GNUC__ && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
  strncpy ((char *) buffer, "uCON64 " UCON64_VERSION_S, 12)[12] = '\0';
#if     defined __GNUC__ && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif
  fwrite (buffer, 1, strlen ((char *) buffer), destfile);

  fclose (destfile);
  ucon64.console = UCON64_GEN;
  ucon64.do_not_calc_crc = org_do_not_calc_crc;

  return 0;
}


static int
genesis_testinterleaved (st_ucon64_nfo_t *rominfo)
{
  unsigned char buf[16384] = { 0 };

  ucon64_fread (buf, rominfo->backup_header_len,
                8192 + (GENESIS_HEADER_START + 4) / 2, ucon64.fname);
  if (!memcmp (buf + GENESIS_HEADER_START, "SEGA", 4))
    return 0;

  smd_deinterleave (buf, 16384);
  if (!memcmp (buf + GENESIS_HEADER_START, "SEGA", 4))
    return 1;

  q_fread_mgd (buf, rominfo->backup_header_len + GENESIS_HEADER_START, 4,
               ucon64.fname);
  if (!memcmp (buf, "SEGA", 4))
    return 2;

  return 0;                                     // unknown, act as if it's BIN
}


static void
check_split (const char *filename, void *cb_data)
{
  // byte at offset 2 is for SMD header "split"
  int *nsplit = (int *) cb_data, multi = ucon64_fgetc (filename, 2);

  if (multi != EOF && multi & 0x40)
    (*nsplit)++;
}


int
genesis_init (st_ucon64_nfo_t *rominfo)
{
  int result = -1, value = 0, y;
  size_t x;
  unsigned int pos = (unsigned int) strlen (rominfo->misc);
  unsigned char *rom_buffer = NULL, buf[MAXBUFSIZE], name[GENESIS_NAME_LEN + 1];
  static char maker[9], country[200]; // 200 characters should be enough for 5 country names
#define GENESIS_IO_MAX 0x58
  static const char *genesis_io[GENESIS_IO_MAX] =
    {
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, "Joystick for MS", NULL,
      NULL, NULL, "Team Play", NULL, "6 Button Pad",
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, "Control Ball", "CD-ROM", NULL, NULL,
      "Floppy Disk Drive", NULL, NULL, NULL, "3 Button Pad",
      "Keyboard", "Activator", "Mega Mouse", NULL, NULL,
      "Printer", NULL, "Serial RS232C", NULL, "Tablet",
      NULL, "Paddle Controller", NULL
    };

  ucon64_fread (buf, 0, 11, ucon64.fname);
  if (buf[8] == 0xaa && buf[9] == 0xbb && (buf[10] == 6 || buf[10] == 7))
    {
      copier_type = SMD;
      rominfo->backup_header_len = SMD_HEADER_LEN;
      if (buf[10] == 7)
        {
          strcpy (rominfo->name, "Name: N/A");
          rominfo->console_usage = NULL;
          rominfo->backup_usage = smd_usage[0].help;
          rominfo->maker = "Publisher: N/A";
          rominfo->country = "Country: N/A";
          rominfo->has_internal_crc = 0;
          sprintf (rominfo->misc + pos, "Type: Super Magic Drive SRAM file\n");
          ucon64.split = 0;                     // SRAM files are never split
          return 0;                             // rest is nonsense for SRAM file
        }
    }

  if (UCON64_ISSET2 (ucon64.backup_header_len, unsigned int)) // -hd, -nhd or -hdn switch was specified
    rominfo->backup_header_len = ucon64.backup_header_len;

  rominfo->interleaved = UCON64_ISSET (ucon64.interleaved) ?
                           ucon64.interleaved : genesis_testinterleaved (rominfo);

  if (rominfo->interleaved == 0)
    copier_type = BIN;
  else if (rominfo->interleaved == 1)
    copier_type = SMD;
  else if (rominfo->interleaved == 2)
    copier_type = MGD_GEN;

  if (copier_type == SMD)
    {
      genesis_rom_size = (((unsigned int) ucon64.fsize - rominfo->backup_header_len) /
                            16384) * 16384;
      if (genesis_rom_size != ucon64.fsize - rominfo->backup_header_len)
        rominfo->data_size = genesis_rom_size;

      memset (buf, 0, 16384);
      ucon64_fread (buf, rominfo->backup_header_len,
                    8192 + (GENESIS_HEADER_START + GENESIS_HEADER_LEN) / 2,
                    ucon64.fname);
      smd_deinterleave (buf, 16384);            // buf will contain the deinterleaved data
      memcpy (&genesis_header, buf + GENESIS_HEADER_START, GENESIS_HEADER_LEN);
    }
  else if (copier_type == MGD_GEN)
    {
      // We use rominfo->backup_header_len to make it user definable. Normally it
      //  should be 0 for MGD_GEN.
      genesis_rom_size = (unsigned int) ucon64.fsize - rominfo->backup_header_len;
      q_fread_mgd (&genesis_header, rominfo->backup_header_len +
                   GENESIS_HEADER_START, GENESIS_HEADER_LEN, ucon64.fname);
    }
  else // copier_type == BIN
    {
      // We use rominfo->backup_header_len to make it user definable.
      genesis_rom_size = (unsigned int) ucon64.fsize - rominfo->backup_header_len;
      ucon64_fread (&genesis_header, rominfo->backup_header_len +
                    GENESIS_HEADER_START, GENESIS_HEADER_LEN, ucon64.fname);
    }

  if (!UCON64_ISSET (ucon64.split))
    {
      if (copier_type == SMD)
        {
          int nsplit = 0,
              nparts = ucon64_testsplit (ucon64.fname, check_split, &nsplit);
          // force displayed info to be correct if not split (see ucon64.c)
          ucon64.split = nparts == nsplit + 1 ? nparts : 0;
        }
      else if (copier_type == BIN)
        {
          // this code isn't fool-proof, but it's not that important
          const char *ptr = get_suffix (ucon64.fname);
          if (strlen (ptr) == 4 && ptr[1] == 'r' &&
              ptr[2] >= '0' && ptr[2] <= '9' && ptr[3] >= '0' && ptr[3] <= '9')
            {
              int n_parts = 0;
              char fname[FILENAME_MAX], *digits;

              strcpy (fname, ucon64.fname);
              digits = fname + strlen (fname) - 2;
              do
                {
                  sprintf (digits, "%02d", n_parts);
                  if (access (fname, F_OK) == 0)
                    n_parts++;
                  else
                    break;
                }
              while (n_parts < 100);

              if (n_parts > 1)
                ucon64.split = n_parts;
            }
        }
    }

  if (!memcmp (&OFFSET (genesis_header, 0), "SEGA", 4) ||
      ucon64.console == UCON64_GEN)
    result = 0;
  else
    result = -1;

  rominfo->header_start = GENESIS_HEADER_START;
  rominfo->header_len = GENESIS_HEADER_LEN;
  rominfo->header = &genesis_header;

  // internal ROM name
  memcpy (rominfo->name, &OFFSET (genesis_header, 80), GENESIS_NAME_LEN);
  rominfo->name[GENESIS_NAME_LEN] = '\0';

  // ROM maker
  memcpy (maker, &OFFSET (genesis_header, 16), 8);
  if (maker[3] == 'T' && maker[4] == '-')
    {
      static const char *genesis_maker[0x100] =
        {
          NULL, "Accolade/Infogrames", "Virgin Games", "Parker Brothers", "Westone",
          NULL, NULL, NULL, NULL, "Westone",
          "Takara", "Taito/Accolade", "Capcom", "Data East", "Namco/Tengen",
          "Sunsoft", "Bandai", "Dempa", "Technosoft", "Technosoft",
          "Asmik", NULL, "Extreme/Micronet", "Vic Tokai", "American Sammy",
          "NCS", "Sigma Enterprises", "Toho", NULL, "Kyugo",
          NULL, NULL, "Wolfteam", "Kaneko", NULL,
          "Toaplan", "Tecmo", NULL, NULL, NULL,
          "Toaplan", "Unipac", "UFL Company Ltd.", "Human", NULL,
          "Game Arts", "Hot-B", "Sage's Creation", "Tengen/Time Warner",
            "Renovation/Telenet",
          "Electronic Arts", NULL, NULL, NULL, NULL,
          "Psygnosis", "Razorsoft", NULL, "Mentrix", NULL,
          "JVC/Victor Musical Industries", NULL, NULL, NULL, "IGS Corp.",
          NULL, NULL, "CRI/Home Data", "Arena", "Virgin Games",
          NULL, "Nichibutsu", NULL, "Soft Vision", "Palsoft",
          NULL, "KOEI", NULL, NULL, "U.S. Gold",
          NULL, "Acclaim/Flying Edge", NULL, "Gametek", NULL,
          NULL, "Absolute", "Mindscape", "Domark", "Parker Brothers",
          NULL, NULL, NULL, "Sony Imagesoft", "Sony Imagesoft",
          "Konami", NULL, "Tradewest/Williams", NULL, "Codemasters",
          "T*HQ Software", "TecMagik", NULL, "Takara", NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, "Hi Tech Entertainment/Designer Software", "Psygnosis", NULL,
          NULL, NULL, NULL, NULL, "Accolade",
          "Code Masters", NULL, NULL, NULL, "Spectrum HoloByte",
          "Interplay", NULL, NULL, NULL, NULL,
          "Activision", NULL, "Shiny & Playmates", NULL, NULL,
          NULL, NULL, NULL, NULL, "Viacom International",
          NULL, NULL, NULL, NULL, "Atlus",
          NULL, NULL, NULL, NULL, NULL,
          NULL, "Infogrames", NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, "Fox Interactive", NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, "Psygnosis",
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, "Disney Interactive",
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL
        };

      sscanf (&maker[5], "%03d", &value);
      rominfo->maker = NULL_TO_UNKNOWN_S (genesis_maker[value & 0xff]);
    }
  else
    {
      // Don't use genesis_maker here. If it would be corrected/updated an
      //  incorrect publisher name would be displayed.
      rominfo->maker =
        (!strncmp (maker, "(C)ACLD", 7)) ? "Ballistic" :
        (!strncmp (maker, "(C)AESI", 7)) ? "ASCII" :
        (!strncmp (maker, "(C)ASCI", 7)) ? "ASCII" :
        (!strncmp (maker, "(C)KANEKO", 9)) ? "Kaneko" :
        (!strncmp (maker, "(C)PPP", 6)) ? "Gametek" :
        (!strncmp (maker, "(C)RSI", 6)) ? "Razorsoft" : // or is it "(C)1RSI"?
        (!strncmp (maker, "(C)SEGA", 7)) ? "Sega" :
        (!strncmp (maker, "(C)TREC", 7)) ? "Treco" :
        (!strncmp (maker, "(C)VRGN", 7)) ? "Virgin Games" :
        (!strncmp (maker, "(C)WADN", 7)) ? "Parker Brothers" :
        (!strncmp (maker, "(C)WSTN", 7)) ? "Westone" : NULL;
      if (!rominfo->maker)
        {
          maker[8] = '\0';
          rominfo->maker = maker;
        }
    }

  genesis_tv_standard = 1;              // default to PAL; NTSC has higher precedence
  genesis_japanese = 0;

  country[0] = '\0';
  // ROM country
  for (x = 0; x < 5; x++)
    {
      int country_code = OFFSET (genesis_header, 240 + x);
#define GENESIS_COUNTRY_MAX 0x57
      const char *genesis_country[GENESIS_COUNTRY_MAX] =
        {
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, "Brazil", NULL, NULL,     // Brazil NTSC
          NULL, "Hong Kong", NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          "Asia", "Brazil", NULL, NULL, "Europe", // Brazil PAL
          "France", NULL, NULL, NULL, "Japan",
          NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL,
          "U.S.A.", NULL
        };

      if ((x > 0 && country_code == 0) || country_code == ' ')
        continue;
      if (country_code == 'J')
        genesis_japanese = 1;
      if (genesis_japanese || country_code == 'U' || country_code == '4')
        genesis_tv_standard = 0;        // Japan, the U.S.A. and Brazil ('4') use NTSC
      strcat (country, NULL_TO_UNKNOWN_S
                (genesis_country[MIN (country_code, GENESIS_COUNTRY_MAX - 1)]));
      strcat (country, ", ");
    }
  x = strlen (country);
  if (x >= 2 && country[x - 2] == ',' && country[x - 1] == ' ')
    country[x - 2] = '\0';
  rominfo->country = country;

  // misc stuff
  memcpy (name, &OFFSET (genesis_header, 32), GENESIS_NAME_LEN);
  name[GENESIS_NAME_LEN] = '\0';
  pos += sprintf (rominfo->misc + pos, "Japanese game name: %s\n", name);
  pos += sprintf (rominfo->misc + pos, "Date: %.8s\n", &OFFSET (genesis_header, 24));

  x = (OFFSET (genesis_header, 160) << 24) +
      (OFFSET (genesis_header, 161) << 16) +
      (OFFSET (genesis_header, 162) << 8) +
       OFFSET (genesis_header, 163);
  y = (OFFSET (genesis_header, 164) << 24) +
      (OFFSET (genesis_header, 165) << 16) +
      (OFFSET (genesis_header, 166) << 8) +
       OFFSET (genesis_header, 167);
  pos += sprintf (rominfo->misc + pos, "Internal size: %.4f Mb\n", (float) (y - x + 1) / MBIT);
  pos += sprintf (rominfo->misc + pos, "ROM start: %08x\n", (int) x);
  pos += sprintf (rominfo->misc + pos, "ROM end: %08x\n", y);

  genesis_has_ram = OFFSET (genesis_header, 176) == 'R' &&
                    OFFSET (genesis_header, 177) == 'A';
  if (genesis_has_ram)
    {
      x = (OFFSET (genesis_header, 180) << 24) +
          (OFFSET (genesis_header, 181) << 16) +
          (OFFSET (genesis_header, 182) << 8) +
           OFFSET (genesis_header, 183);
      y = (OFFSET (genesis_header, 184) << 24) +
          (OFFSET (genesis_header, 185) << 16) +
          (OFFSET (genesis_header, 186) << 8) +
           OFFSET (genesis_header, 187);
      pos += sprintf (rominfo->misc + pos, "Cartridge RAM: Yes, %d kBytes (%s)\n",
                      (int) ((y - x + 1) >> 10),
                      OFFSET (genesis_header, 178) & 0x40 ? "backup" : "non-backup");

      pos += sprintf (rominfo->misc + pos, "RAM start: %08x\n", (int) x);
      pos += sprintf (rominfo->misc + pos, "RAM end: %08x\n", y);
    }
  else
    pos += sprintf (rominfo->misc + pos, "Cartridge RAM: No\n");

  // Only checking for 'G' seems to give better results than checking for "GM".
  //  "Officially" "GM" indicates it's a game and "Al" that it's educational.
  pos += sprintf (rominfo->misc + pos, "Product type: %s\n",
                  (OFFSET (genesis_header, 128) == 'G') ? "Game" : "Educational");
  pos += sprintf (rominfo->misc + pos, "I/O device(s): %s",
                  NULL_TO_UNKNOWN_S (genesis_io[MIN ((int) OFFSET (genesis_header, 144),
                                       GENESIS_IO_MAX - 1)]));
  for (x = 0; x < 3; x++)
    {
      const char *io_device = genesis_io[MIN (OFFSET (genesis_header, 145 + x),
                                GENESIS_IO_MAX - 1)];
      if (!io_device)
        continue;
      pos += sprintf (rominfo->misc + pos, ", %s", io_device);
    }
  pos += sprintf (rominfo->misc + pos, "\n");

  pos += sprintf (rominfo->misc + pos, "Modem data: %.10s\n", &OFFSET (genesis_header, 188));
  pos += sprintf (rominfo->misc + pos, "Memo: %.40s\n", &OFFSET (genesis_header, 200));
  pos += sprintf (rominfo->misc + pos, "Product code: %.8s\n", &OFFSET (genesis_header, 131));
  sprintf (rominfo->misc + pos, "Version: 1.%c%c", OFFSET (genesis_header, 140),
           OFFSET (genesis_header, 141));

  // We can be stricter here than in ucon64.c/ucon64_rom_nfo(), because we know
  //  we shouldn't have ANSI escape sequences (which ucon64.c/toprint() allows).
  for (x = 0; rominfo->misc[x] != '\0'; x++)
    if (rominfo->misc[x] != '\n' && !isprint ((int) rominfo->misc[x]))
      rominfo->misc[x] = '.';

  // internal ROM crc
  if (!UCON64_ISSET (ucon64.do_not_calc_crc) && result == 0)
    {
      if ((rom_buffer = load_rom (rominfo, ucon64.fname, rom_buffer)) == NULL)
        return -1;

      rominfo->has_internal_crc = 1;
      rominfo->internal_crc_len = 2;

      rominfo->current_internal_crc = genesis_chksum (rom_buffer);
      rominfo->internal_crc = OFFSET (genesis_header, 143);          // low byte of checksum
      rominfo->internal_crc += (OFFSET (genesis_header, 142)) << 8;  // high byte of checksum

      rominfo->internal_crc2[0] = 0;
      free (rom_buffer);
    }
  rominfo->console_usage = genesis_usage[0].help;
  if (copier_type == SMD)
    rominfo->backup_usage = smd_usage[0].help;
  else if (copier_type == MGD_GEN)
    rominfo->backup_usage = mgd_usage[0].help;
  else // copier_type == BIN
    rominfo->backup_usage = bin_usage[0].help;

  return result;
}


static int
genesis_chksum (unsigned char *rom_buffer)
{
  int i, len = genesis_rom_size - 2;
  unsigned short checksum = 0;

  for (i = 512; i <= len; i += 2)
    checksum += (rom_buffer[i] << 8) + (rom_buffer[i + 1]);

  return checksum;
}
