/*
gba.c - Game Boy Advance support for uCON64

Copyright (c) 2001 - 2005              NoisyB
Copyright (c) 2001 - 2005, 2015 - 2021 dbjh


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
#include "misc/bswap.h"
#include "misc/file.h"
#include "misc/misc.h"
#include "misc/property.h"
#include "misc/string.h"
#include "ucon64_misc.h"
#include "console/console.h"
#include "console/gba.h"
#include "backup/backup.h"
#include "backup/fal.h"


#define GBA_NAME_LEN 12
#define GBA_HEADER_START 0
#define GBA_HEADER_LEN (sizeof (st_gba_header_t))
#define GBA_MENU_SIZE 20916
#define GBA_BLANK_SIZE 52232
#define GBA_SAV_TEMPLATE_SIZE 65536
#define GBA_SCI_TEMPLATE_SIZE 458752

static int gba_chksum (void);


static st_ucon64_obj_t gba_obj[] =
  {
    {0, WF_DEFAULT},
    {0, WF_INIT | WF_PROBE | WF_STOP},
    {UCON64_GBA, WF_SWITCH},
    {UCON64_GBA, WF_DEFAULT}
  };

const st_getopt2_t gba_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "Game Boy Advance (SP)"/*"2001 Nintendo http://www.nintendo.com"*/,
      NULL
    },
    {
      UCON64_GBA_S, 0, 0, UCON64_GBA,
      NULL, "force recognition",
      &gba_obj[2]
    },
    {
      "n", 1, 0, UCON64_N,
      "NEW_NAME", "change internal ROM name to NEW_NAME",
      &gba_obj[0]
    },
    {
      "logo", 0, 0, UCON64_LOGO,
      NULL, "restore ROM logo character data (offset: 0x04-0x9F)",
      &gba_obj[0]
    },
    {
      "chk", 0, 0, UCON64_CHK,
      NULL, "fix ROM header checksum",
      &gba_obj[0]
    },
    {
      "sram", 0, 0, UCON64_SRAM,
      NULL, "patch ROM for SRAM saving",
      &gba_obj[3]
    },
    {
      "sc", 0, 0, UCON64_SC,
      NULL, "convert to Super Card (CF to GBA Adapter)\n"
#if 0
            "enables \"Saver patch\", \"restart to Menu\" and\n"
            "\"Real Time Save\""
#endif
            "(creates SAV and SCI templates)",
      &gba_obj[3]
    },
    {
      "crp", 1, 0, UCON64_CRP,
      "WAIT_TIME", "slow down ROM access (\"crash patch\");\n"
      "WAIT_TIME" OPTARG_S "0  default in most crash patches\n"
      "WAIT_TIME" OPTARG_S "4  faster than 0, slower than 8\n"
      "WAIT_TIME" OPTARG_S "8  faster than 4, slower than 28\n"
      "WAIT_TIME" OPTARG_S "12 slowest cartridge access speed\n"
      "WAIT_TIME" OPTARG_S "16 faster than 28, but slower than 20\n"
      "WAIT_TIME" OPTARG_S "20 default in most original cartridges\n"
      "WAIT_TIME" OPTARG_S "24 fastest cartridge access speed\n"
      "WAIT_TIME" OPTARG_S "28 faster than 8 but slower than 16",
      &gba_obj[3]
    },
//  "n 0 and 28, with a stepping of 4. I.e. 0, 4, 8, 12 ...\n"
    {
      "multi", 1, 0, UCON64_MULTI,
      "SIZE", "make multi-game file for use with FAL/F2A flash card, truncated\n"
      "to SIZE Mbit; file with loader must be specified first, then\n"
      "all the ROMs, multi-game file to create last",
      &gba_obj[1]
    },
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };

/*
Offset 00h-03h - Start  address - A 32 bit ARM B command with jump destination
                 to  the  start  address of the program, cannot be manipulated
                 with this tool, there's no reason.

Offset 04h-9fh - Nintendo logo character data - The fix Nintendo logo graphics
                 needed  to  start a ROM on the real machine as it is verified
                 by it.

Offset a0h-abh - Game  title  -  The game title is an ASCII string, officially
                 can  use only ASCII characters between the ASCII code 20h and
                 60h.  Although it is not a strict rule for hobby programmers,
                 it  is  fun  to  follow such a rules in my opinion. As I know
                 developers  can  choose  their own game title here describing
                 the product in short.

Offset ach-afh - Game  code  -  The  4 bytes long code of the game is an ASCII
                 string  too, officially can use only ASCII characters between
                 the ASCII code 20h and 60h. The first letter is always A as I
                 know,  probably  stands  for GBA, so it won't change unless a
                 higher   hardware   with  backwards  compatibility  won't  be
                 introduced  and  this letter could hold some more infos about
                 it. The second and third letters are the shortened version of
                 the  name of the game. And the fourth letter is the territory
                 code. Don't afraid, there's no territory lockout, this is for
                 information  purposes  only.  So  far  as I know J stands for
                 Japan  and  Asia,  E  stands  for  USA and the whole American
                 continent  and  P  stands  for  Europe,  Australia and Africa
                 (probably  came  from  that  these are the PAL video standard
                 territories,  but  I  could  be  wrong). Although it is not a
                 strict rule for hobby programmers, it is fun to follow such a
                 rules  in my opinion. Developers get this 4 letter code right
                 from Nintendo and they have to use that.

Offset b0h-b1h - Maker  code  - The 2 bytes long code of the developer company
                 is  an  ASCII  string  too,  officially  can  use  only ASCII
                 characters between the ASCII code 20h and 60h. Although it is
                 not  a strict rule for hobby programmers, it is fun to follow
                 such a rules in my opinion. Developers get this 2 letter code
                 right from Nintendo and they have to use that.

Offset b2h-b2h - 96h - Fixed 96h byte without any useful information.

Offset b3h-b3h - Main  unit  code  -  This hexadecimal byte is the destination
                 hardware  code.  It  is always 00h at the moment as it stands
                 for  Game Boy Advance, so it won't change in the future either
                 unless  a  higher hardware with backwards compatibility won't
                 be  introduced and this byte could hold some more infos about
                 it.  There's  no  reason  to  change  this or write something
                 different than 00h into it.

Offset b4h-b4h - Device type -  This hexadecimal byte is the device type code.
                 It  is always 00h as the only other possible value stands for
                 a  debugger  cart  what  I  assume  won't be available on the
                 streets  and  I  assume even if a developer works with such a
                 hardware, he or she doesn't have to change this byte, however
                 he  or  she  easily  can  of  course. So there's no reason to
                 change this or write something different than 00h into it.

Offset b5h-bbh - Reserved  area  -  Fixed,  00h filled area without any useful
                 information.

Offset bch-bch - Mask  ROM  version  number  - This hexadecimal byte holds the
                 version  number  of  the ROM. As I know it works somehow that
                 way,  the  first  published  (and released on the streets) is
                 always  the first version and for that 00h is stored here. In
                 the  case it is getting updated, so in the same territory the
                 very  same  game with the very same title is getting replaced
                 with a new version, what is happening rarely, the number here
                 is  getting  increased by one. So usually this byte holds 00h
                 and  there  isn't too much reason to write something here and
                 something else than 00h.

Offset bdh-bdh - Complement   check   -  This  hexadecimal  byte  have  to  be
                 calculated  automatically,  when  the  whole header is in its
                 final  state,  so nothing will change inside of it. (Manually
                 it  would be hard to calculate.) Add the bytes between offset
                 a0h  and bch together, take the number's two's complement and
                 add  19h  to  the  result.  Store  the lowest 8 bits here. Or
                 calculate   automatically   with   GBARM.   The  hardware  is
                 verifying  this  byte  just  like the Nintendo logo character
                 data  and  in the case it isn't correct, the game won't start
                 on the real machine.

Offset beh-bfh - Reserved  area  -  Fixed,  00h filled area without any useful
                 information.
*/
typedef struct st_gba_header
{
  unsigned char start[4];                       // 0x00
  unsigned char logo[GBA_LOGODATA_LEN];         // 0x04
  unsigned char name[GBA_NAME_LEN];             // 0xa0
  unsigned char game_id_prefix;                 // 0xac
  unsigned char game_id_low;                    // 0xad
  unsigned char game_id_high;                   // 0xae
  unsigned char game_id_country;                // 0xaf
  unsigned char maker_high;                     // 0xb0
  unsigned char maker_low;                      // 0xb1
  unsigned char pad1;
  unsigned char gba_type;                       // 0xb3
  unsigned char device_type;                    // 0xb4
  unsigned char pad2[7];
  unsigned char version;                        // 0xbc
  unsigned char checksum;                       // 0xbd
  unsigned char pad3[2];
} st_gba_header_t;

static st_gba_header_t gba_header;
const unsigned char gba_logodata[] =            // NOTE: not a static variable
  {
                            0x24, 0xff, 0xae, 0x51,
    0x69, 0x9a, 0xa2, 0x21, 0x3d, 0x84, 0x82, 0x0a,
    0x84, 0xe4, 0x09, 0xad, 0x11, 0x24, 0x8b, 0x98,
    0xc0, 0x81, 0x7f, 0x21, 0xa3, 0x52, 0xbe, 0x19,
    0x93, 0x09, 0xce, 0x20, 0x10, 0x46, 0x4a, 0x4a,
    0xf8, 0x27, 0x31, 0xec, 0x58, 0xc7, 0xe8, 0x33,
    0x82, 0xe3, 0xce, 0xbf, 0x85, 0xf4, 0xdf, 0x94,
    0xce, 0x4b, 0x09, 0xc1, 0x94, 0x56, 0x8a, 0xc0,
    0x13, 0x72, 0xa7, 0xfc, 0x9f, 0x84, 0x4d, 0x73,
    0xa3, 0xca, 0x9a, 0x61, 0x58, 0x97, 0xa3, 0x27,
    0xfc, 0x03, 0x98, 0x76, 0x23, 0x1d, 0xc7, 0x61,
    0x03, 0x04, 0xae, 0x56, 0xbf, 0x38, 0x84, 0x00,
    0x40, 0xa7, 0x0e, 0xfd, 0xff, 0x52, 0xfe, 0x03,
    0x6f, 0x95, 0x30, 0xf1, 0x97, 0xfb, 0xc0, 0x85,
    0x60, 0xd6, 0x80, 0x25, 0xa9, 0x63, 0xbe, 0x03,
    0x01, 0x4e, 0x38, 0xe2, 0xf9, 0xa2, 0x34, 0xff,
    0xbb, 0x3e, 0x03, 0x44, 0x78, 0x00, 0x90, 0xcb,
    0x88, 0x11, 0x3a, 0x94, 0x65, 0xc0, 0x7c, 0x63,
    0x87, 0xf0, 0x3c, 0xaf, 0xd6, 0x25, 0xe4, 0x8b,
    0x38, 0x0a, 0xac, 0x72, 0x21, 0xd4, 0xf8, 0x07
  };


int
gba_n (st_ucon64_nfo_t *rominfo, const char *name)
{
  char buf[GBA_NAME_LEN], dest_name[FILENAME_MAX];

#if     defined __GNUC__ && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
  strncpy (buf, name, GBA_NAME_LEN);
#if     defined __GNUC__ && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif
  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");
  ucon64_fwrite (buf, GBA_HEADER_START + rominfo->backup_header_len + 0xa0,
                 GBA_NAME_LEN, dest_name, "r+b");

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
gba_logo (st_ucon64_nfo_t *rominfo)
{
  char dest_name[FILENAME_MAX];

  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");
  ucon64_fwrite (gba_logodata, GBA_HEADER_START + rominfo->backup_header_len +
                 0x04, GBA_LOGODATA_LEN, dest_name, "r+b");

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
gba_chk (st_ucon64_nfo_t *rominfo)
{
  char buf, dest_name[FILENAME_MAX];

  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");

  buf = (char) rominfo->current_internal_crc;
  ucon64_fputc (dest_name, GBA_HEADER_START + rominfo->backup_header_len + 0xbd,
                buf, "r+b");

  dumper (stdout, &buf, 1, GBA_HEADER_START + rominfo->backup_header_len + 0xbd,
          DUMPER_HEX);

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
gba_sram (void)
// This function is based on Omar Kilani's gbautil 1.1
{
  unsigned char st_orig[2][10] =
    {
      { 0x0E, 0x48, 0x39, 0x68, 0x01, 0x60, 0x0E, 0x48, 0x79, 0x68 },
      { 0x13, 0x4B, 0x18, 0x60, 0x13, 0x48, 0x01, 0x60, 0x13, 0x49 }
    },
    st_repl[2][10] =
    {
      { 0x00, 0x48, 0x00, 0x47, 0x01, 0xFF, 0xFF, 0x08, 0x79, 0x68 },
      { 0x01, 0x4C, 0x20, 0x47, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0x08 }
    },
    fl_orig[2][24] =
    {
      {
        0xD0, 0x20, 0x00, 0x05, 0x01, 0x88, 0x01, 0x22, 0x08, 0x1C,
        0x10, 0x40, 0x02, 0x1C, 0x11, 0x04, 0x08, 0x0C, 0x00, 0x28,
        0x01, 0xD0, 0x1B, 0xE0
      },
      {
        0xD0, 0x21, 0x09, 0x05, 0x01, 0x23, 0x0C, 0x4A, 0x08, 0x88,
        0x18, 0x40, 0x00, 0x28, 0x08, 0xD1, 0x10, 0x78, 0x00, 0x28,
        0xF8, 0xD0, 0x08, 0x88
      }
    },
    fl_repl[2][24] =
    {
      {
        0xE0, 0x20, 0x00, 0x05, 0x01, 0x88, 0x01, 0x22, 0x08, 0x1C,
        0x10, 0x40, 0x02, 0x1C, 0x11, 0x04, 0x08, 0x0C, 0x00, 0x28,
        0x01, 0xD0, 0x1B, 0xE0
      },
      {
        0xE0, 0x21, 0x09, 0x05, 0x01, 0x23, 0x0C, 0x4A, 0x08, 0x88,
        0x18, 0x40, 0x00, 0x28, 0x08, 0xD1, 0x10, 0x78, 0x00, 0x28,
        0xF8, 0xD0, 0x08, 0x88
      }
    },
    p_repl[2][188] =
    {
      {
        0x39, 0x68, 0x27, 0x48, 0x81, 0x42, 0x23, 0xD0, 0x89, 0x1C,
        0x08, 0x88, 0x01, 0x28, 0x02, 0xD1, 0x24, 0x48, 0x78, 0x60,
        0x33, 0xE0, 0x00, 0x23, 0x00, 0x22, 0x89, 0x1C, 0x10, 0xB4,
        0x01, 0x24, 0x08, 0x68, 0x20, 0x40, 0x5B, 0x00, 0x03, 0x43,
        0x89, 0x1C, 0x52, 0x1C, 0x06, 0x2A, 0xF7, 0xD1, 0x10, 0xBC,
        0x39, 0x60, 0xDB, 0x01, 0x02, 0x20, 0x00, 0x02, 0x1B, 0x18,
        0x0E, 0x20, 0x00, 0x06, 0x1B, 0x18, 0x7B, 0x60, 0x39, 0x1C,
        0x08, 0x31, 0x08, 0x88, 0x09, 0x38, 0x08, 0x80, 0x16, 0xE0,
        0x15, 0x49, 0x00, 0x23, 0x00, 0x22, 0x10, 0xB4, 0x01, 0x24,
        0x08, 0x68, 0x20, 0x40, 0x5B, 0x00, 0x03, 0x43, 0x89, 0x1C,
        0x52, 0x1C, 0x06, 0x2A, 0xF7, 0xD1, 0x10, 0xBC, 0xDB, 0x01,
        0x02, 0x20, 0x00, 0x02, 0x1B, 0x18, 0x0E, 0x20, 0x00, 0x06,
        0x1B, 0x18, 0x08, 0x3B, 0x3B, 0x60, 0x0B, 0x48, 0x39, 0x68,
        0x01, 0x60, 0x0A, 0x48, 0x79, 0x68, 0x01, 0x60, 0x0A, 0x48,
        0x39, 0x1C, 0x08, 0x31, 0x0A, 0x88, 0x80, 0x21, 0x09, 0x06,
        0x0A, 0x43, 0x02, 0x60, 0x07, 0x48, 0x00, 0x47, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0E, 0x04, 0x00,
        0x00, 0x0E, 0xD4, 0x00, 0x00, 0x04, 0xD8, 0x00, 0x00, 0x04,
        0xDC, 0x00, 0x00, 0x04, 0xFF, 0xFF, 0xFF, 0x08
      },
      {
        0x22, 0x4C, 0x84, 0x42, 0x20, 0xD0, 0x80, 0x1C, 0x04, 0x88,
        0x01, 0x25, 0x2C, 0x40, 0x01, 0x2C, 0x02, 0xD1, 0x80, 0x1E,
        0x1E, 0x49, 0x2E, 0xE0, 0x00, 0x23, 0x00, 0x24, 0x80, 0x1C,
        0x40, 0xB4, 0x01, 0x26, 0x05, 0x68, 0x35, 0x40, 0x5B, 0x00,
        0x2B, 0x43, 0x80, 0x1C, 0x64, 0x1C, 0x06, 0x2C, 0xF7, 0xD1,
        0x40, 0xBC, 0xDB, 0x01, 0x02, 0x24, 0x24, 0x02, 0x1B, 0x19,
        0x0E, 0x24, 0x24, 0x06, 0x1B, 0x19, 0x19, 0x1C, 0x09, 0x3A,
        0x16, 0xE0, 0x12, 0x48, 0x00, 0x23, 0x00, 0x24, 0x40, 0xB4,
        0x01, 0x26, 0x05, 0x68, 0x35, 0x40, 0x5B, 0x00, 0x2B, 0x43,
        0x80, 0x1C, 0x64, 0x1C, 0x06, 0x2C, 0xF7, 0xD1, 0x40, 0xBC,
        0xDB, 0x01, 0x02, 0x24, 0x24, 0x02, 0x1B, 0x19, 0x0E, 0x24,
        0x24, 0x06, 0x1B, 0x19, 0x08, 0x3B, 0x18, 0x1C, 0x08, 0x4C,
        0x20, 0x60, 0x08, 0x4C, 0x21, 0x60, 0x08, 0x49, 0x80, 0x20,
        0x00, 0x06, 0x02, 0x43, 0x0A, 0x60, 0x06, 0x4C, 0x20, 0x47,
        0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0E, 0x04, 0x00,
        0x00, 0x0E, 0xD4, 0x00, 0x00, 0x04, 0xD8, 0x00, 0x00, 0x04,
        0xDC, 0x00, 0x00, 0x04, 0xFF, 0xFF, 0xFF, 0x08
      }
    },
    major, minor, micro, *buffer, *bufferptr, *ptr, value;
  char dest_name[FILENAME_MAX];
  int p_size[2] = { 188, 168 }, p_off, st_off;
  unsigned int fsize = (unsigned int) ucon64.fsize;
  FILE *destfile;

  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");

  if ((destfile = fopen (dest_name, "r+b")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], dest_name);
      return -1;
    }
  if ((buffer = (unsigned char *) malloc (fsize)) == NULL)
    {
      fprintf (stderr, ucon64_msg[ROM_BUFFER_ERROR], fsize);
      fclose (destfile);
      exit (1);
    }
  if (fread (buffer, 1, fsize, destfile) != fsize)
    {
      fprintf (stderr, ucon64_msg[READ_ERROR], dest_name);
      free (buffer);
      fclose (destfile);
      return -1;
    }

  bufferptr = buffer + 160 + 12 + 4;

  ptr = (unsigned char *) memmem2 (bufferptr, fsize, "EEPROM_", 7, 0);
  if (ptr == NULL)
    {
      puts ("This ROM does not appear to use EEPROM saving");
      free (buffer);
      fclose (destfile);
      return -1;
    }
  major = ptr[8] - '0';
  minor = ptr[9] - '0';
  micro = ptr[10] - '0';
  if (ucon64.quiet < 0)
    printf ("version: %u.%u.%u; offset: 0x%08x\n",
            major, minor, micro, (int) (ptr - buffer));
  if (minor - 1 >= 2)
    {
      fputs ("ERROR: ROMs with an EEPROM minor version other than 1 or 2 are not supported\n", stderr);
      free (buffer);
      fclose (destfile);
      return -1;
    }

  ptr = (unsigned char *) memmem2 (bufferptr, fsize,
                                   fl_orig[minor - 1], sizeof fl_orig[minor - 1], 0);
  if (ptr == NULL)
    {
      fputs ("ERROR: Could not find fl pattern. Perhaps this file is already patched?\n", stderr);
      free (buffer);
      fclose (destfile);
      return -1;
    }
  if (ucon64.quiet < 0)
    printf ("fl offset: 0x%08x\n", (int) (ptr - buffer));
  fseek (destfile, (long) (ptr - buffer), SEEK_SET);
  fwrite (fl_repl[minor - 1], 1, sizeof fl_repl[minor - 1], destfile);

  ptr = buffer + fsize - 1;
  value = *ptr;
  do
    ptr--;
  while (*ptr == value && ptr - buffer > 0);

  p_off = (ptr - buffer + 0xff) & ~0xff;        // align at 256 byte boundary
  if (ucon64.quiet < 0)
    printf ("p_off: 0x%08x\n", p_off);
  // if the SRAM function won't fit at the end of the ROM, abort
  if ((minor == 1 && (int) (fsize - 188) < p_off) ||
      (minor == 2 && (int) (fsize - 168) < p_off))
    {
      fputs ("ERROR: Not enough room for SRAM function at end of ROM\n", stderr);
      free (buffer);
      fclose (destfile);
      return -1;
    }

  ptr = (unsigned char *) memmem2 (bufferptr, fsize,
                                   st_orig[minor - 1], sizeof st_orig[minor - 1], 0);
  if (ptr == NULL)
    {
      fputs ("ERROR: Could not find st pattern\n", stderr);
      free (buffer);
      fclose (destfile);
      return -1;
    }
  st_off = (int) (ptr - buffer);
  if (ucon64.quiet < 0)
    printf ("st offset: 0x%08x\n", st_off);

  bufferptr = buffer + p_off;
  switch (minor)
    {
    case 1:
      // these are the offsets to the caller function, it handles all saving and
      //  is at st_off
      p_repl[minor - 1][184] = (unsigned char) (st_off + 0x21);
      p_repl[minor - 1][186] = (unsigned char) (st_off >> 16);

      if (*(bufferptr - 1) == 0xff)
        p_repl[minor - 1][185] = (unsigned char) (st_off >> 8);
      else
        {
          st_off += 0x1f;
          p_repl[minor - 1][185] = (unsigned char) (st_off >> 8);
        }

      // tell the calling function where the SRAM function is (p_off)
      st_repl[minor - 1][5] = (unsigned char) (p_off >> 8);
      st_repl[minor - 1][6] = (unsigned char) (p_off >> 16);
      break;
    case 2:
      // offsets to the caller function
      p_repl[minor - 1][164] = (unsigned char) (st_off + 0x13);
      p_repl[minor - 1][165] = (unsigned char) (st_off >> 8);
      p_repl[minor - 1][166] = (unsigned char) (st_off >> 16);

      // tell the calling function where the SRAM function is
      st_repl[minor - 1][7] = (unsigned char) (p_off >> 8);
      st_repl[minor - 1][8] = (unsigned char) (p_off >> 16);
      break;
    }
  fseek (destfile, st_off, SEEK_SET);
  fwrite (st_repl[minor - 1], 1, sizeof st_repl[minor - 1], destfile);
  fseek (destfile, p_off, SEEK_SET);
  fwrite (p_repl[minor - 1], 1, p_size[minor - 1], destfile);

  free (buffer);
  fclose (destfile);

  puts ("SRAM patch applied");
  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
gba_crp (st_ucon64_nfo_t *rominfo, const char *value)
{
  FILE *srcfile, *destfile;
  size_t bytesread;
  int n = 0;
  char buffer[32 * 1024], src_name[FILENAME_MAX], dest_name[FILENAME_MAX],
       replace[2], wait_time = (char) atoi (value);

  if (wait_time % 4 != 0 || wait_time > 28 || wait_time < 0)
    {
      fputs ("ERROR: You specified an incorrect WAIT_TIME value\n", stderr);
      return -1;
    }

  puts ("Applying crash patch...");

  strcpy (src_name, ucon64.fname);
  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, src_name, 0);
  if ((srcfile = fopen (src_name, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], src_name);
      return -1;
    }
  if ((destfile = fopen (dest_name, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], dest_name);
      fclose (srcfile);
      return -1;
    }
  if (rominfo->backup_header_len)               // copy header (if present)
    {
      fread_checked (buffer, 1, rominfo->backup_header_len, srcfile);
      fwrite (buffer, 1, rominfo->backup_header_len, destfile);
    }

  replace[0] = wait_time;
  replace[1] = 0x40;
  while ((bytesread = fread (buffer, 1, 32 * 1024, srcfile)) != 0)
    {                            // '!' == ASCII 33 (\x21), '*' == 42 (\x2a)
      n += change_mem (buffer, bytesread, "\x04\x02\x00\x04\x14\x40", 6, '*', '!', replace, 1, -1);
      n += change_mem (buffer, bytesread, "\x02\x00\x04\x14\x40\x00", 6, '*', '!', replace, 1, -2);
      n += change_mem (buffer, bytesread, "\x04\x02\x00\x04\xB4\x45", 6, '*', '!', replace, 2, -1);
      n += change_mem (buffer, bytesread, "\x3E\xE0\x00\x00\xB4\x45", 6, '*', '!', replace, 2, -1);
      n += change_mem (buffer, bytesread, "\x04\x02\x00\x04\x94\x44", 6, '*', '!', replace, 2, -1);

      fwrite (buffer, 1, bytesread, destfile);
    }
  fclose (srcfile);
  fclose (destfile);

  printf ("Found %d pattern%s\n", n, n != 1 ? "s" : "");
  printf (ucon64_msg[WROTE], dest_name);
  remove_temp_file ();
  return 0;
}


int
gba_init (st_ucon64_nfo_t *rominfo)
{
  int result = -1, value;
  unsigned int pos = (unsigned int) strlen (rominfo->misc);

  rominfo->backup_header_len = UCON64_ISSET2 (ucon64.backup_header_len, unsigned int) ?
                                 ucon64.backup_header_len : 0;

  ucon64_fread (&gba_header, GBA_HEADER_START +
                rominfo->backup_header_len, GBA_HEADER_LEN, ucon64.fname);
  if (/*gba_header.game_id_prefix == 'A' && */ // 'B' in Mario vs. Donkey Kong
      gba_header.start[3] == 0xea && gba_header.pad1 == 0x96 && gba_header.gba_type == 0)
    result = 0;
  else
    {
#if 0 // AFAIK (dbjh) GBA ROMs never have a header
      rominfo->backup_header_len = UCON64_ISSET (ucon64.backup_header_len, unsigned int) ?
                                     ucon64.backup_header_len : UNKNOWN_HEADER_LEN;

      ucon64_fread (&gba_header, GBA_HEADER_START +
                    rominfo->backup_header_len, GBA_HEADER_LEN, ucon64.fname);
      if (gba_header.game_id_prefix == 'A' && gba_header.gba_type == 0)
        result = 0;
      else
#endif
        result = -1;
    }
  if (ucon64.console == UCON64_GBA)
    result = 0;

  rominfo->header_start = GBA_HEADER_START;
  rominfo->header_len = GBA_HEADER_LEN;
  rominfo->header = &gba_header;

  // internal ROM name
  strncpy (rominfo->name, (char *) gba_header.name, GBA_NAME_LEN);
  rominfo->name[GBA_NAME_LEN] = '\0';

  // ROM maker
  {
    int ih = gba_header.maker_high <= '9' ?
               gba_header.maker_high - '0' : gba_header.maker_high - 'A' + 10,
        il = gba_header.maker_low <= '9' ?
               gba_header.maker_low - '0' : gba_header.maker_low - 'A' + 10;
    value = ih * 36 + il;
  }
  if (value < 0 || value >= NINTENDO_MAKER_LEN)
    value = 0;
  rominfo->maker = NULL_TO_UNKNOWN_S (nintendo_maker[value]);

  // ROM country
  rominfo->country =
    (gba_header.game_id_country == 'J') ? "Japan/Asia" :
    (gba_header.game_id_country == 'E') ? "U.S.A." :
    (gba_header.game_id_country == 'P') ? "Europe, Australia and Africa" :
    "Unknown country";

  // misc stuff
  pos += sprintf (rominfo->misc + pos, "Version: 1.%u\n", gba_header.version);
  pos += sprintf (rominfo->misc + pos, "Device type: 0x%02x\n", gba_header.device_type);

  /*
    start address = current address + (parameter of B instruction * 4) + 8
    gba_header.start[3] is opcode of B instruction (0xea)
  */
  value = 0x8000008 +
          (gba_header.start[2] << 18 | gba_header.start[1] << 10 | gba_header.start[0] << 2);
  pos += sprintf (rominfo->misc + pos, "Start address: 0x%08x\n", value);

  sprintf (rominfo->misc + pos, "Logo data: %s",
           memcmp (gba_header.logo, gba_logodata, GBA_LOGODATA_LEN) == 0 ?
#ifdef  USE_ANSI_COLOR
             ucon64.ansi_color ? "\x1b[01;32mOK\x1b[0m" : "OK" :
             ucon64.ansi_color ? "\x1b[01;31mBad\x1b[0m" : "Bad");
#else
             "OK" : "Bad");
#endif

  // internal ROM crc
  if (!UCON64_ISSET (ucon64.do_not_calc_crc) && result == 0)
    {
      rominfo->has_internal_crc = 1;
      rominfo->internal_crc_len = 1;
      rominfo->current_internal_crc = gba_chksum ();

      rominfo->internal_crc = gba_header.checksum;
      rominfo->internal_crc2[0] = 0;
    }

  rominfo->console_usage = gba_usage[0].help;
  // we use fal_usage, but we could just as well use f2a_usage
  rominfo->backup_usage = (!rominfo->backup_header_len ? fal_usage[0].help : unknown_backup_usage[0].help);

  return result;
}


int
gba_chksum (void)
// Note that this function only calculates the checksum of the internal header
{
  unsigned char sum = 0x19, *ptr = (unsigned char *) &gba_header + 0xa0;

  while (ptr < (unsigned char *) &gba_header + 0xbd)
    sum += *ptr++;
  sum = -sum;

  return sum;
}


int
gba_multi (unsigned int truncate_size, char *multi_fname)
// TODO: Check if 1024 Mbit multi-game files are supported by the FAL code
{
  size_t n, n_files, bytestowrite, byteswritten, totalsize = 0;
  unsigned int file_no, done, truncated = 0, size_pow2_lesser = 1,
               size_pow2 = 1, truncate_size_ispow2 = 0;
  struct stat fstate;
  FILE *srcfile, *destfile;
  char buffer[32 * 1024], fname[FILENAME_MAX], *fname_ptr;
  const char *p = NULL;

  if (truncate_size == 0)
    {
      fputs ("ERROR: Cannot make multi-game file of 0 bytes\n", stderr);
      return -1;
    }

#if 0
  if (truncate_size != 64 * MBIT && truncate_size != 128 * MBIT &&
      truncate_size != 256 * MBIT && truncate_size != 512 * MBIT &&
      truncate_size != 1024 * MBIT)
    {
      fputs ("ERROR: Truncate size must be 64, 128, 256, 512 or 1024\n", stderr);
      return -1;
    }
#endif

  if (multi_fname != NULL)                      // -xfalmulti
    {
      n_files = ucon64.argc;
      snprintf (fname, FILENAME_MAX, "%s", multi_fname);
    }
  else                                          // -multi
    {
      n_files = ucon64.argc - 1;
      snprintf (fname, FILENAME_MAX, "%s", ucon64.argv[n_files]);
    }
  fname[FILENAME_MAX - 1] = '\0';

  ucon64_file_handler (fname, NULL, OF_FORCE_BASENAME);
  if ((destfile = fopen (fname, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], fname);
      return -1;
    }
  printf ("Creating multi-game file for FAL(/F2A): %s\n", fname);

  file_no = 0;
  for (n = 1; n < n_files; n++)
    {
      if (access (ucon64.argv[n], F_OK))
        continue;                               // "file" does not exist (option)
      stat (ucon64.argv[n], &fstate);
      if (!S_ISREG (fstate.st_mode))
        continue;

      if (file_no == 0)
        {
          if (multi_fname != NULL)              // -xfalmulti
            {
              size_t len;

              p = get_property (ucon64.configfile, "gbaloader", PROPERTY_MODE_FILENAME);
              if (!p)
                p = "loader.bin";
              len = strnlen (p, sizeof fname - 1);
              strncpy (fname, p, len)[len] = '\0';
              if (access (fname, F_OK))
                {
                  fprintf (stderr, "ERROR: Cannot open loader binary (%s)\n", fname);
                  fclose (destfile);
                  return -1;
                }
              fname_ptr = fname;
              // NOTE: loop counter is modified, because we have to insert
              //       loader in the file list
              n--;
            }
          else                                  // -multi
            fname_ptr = ucon64.argv[n];

          printf ("Loader: %s\n", fname_ptr);
          if (fsizeof (fname_ptr) > 64 * 1024)
            printf ("WARNING: Are you sure %s is a loader binary?\n", fname_ptr);
        }
      else
        {
          fname_ptr = ucon64.argv[n];
          printf ("ROM%u: %s\n", file_no, fname_ptr);
        }

      if ((srcfile = fopen (fname_ptr, "rb")) == NULL)
        {
          fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], fname_ptr);
          continue;
        }
      done = 0;
      byteswritten = 0;                         // # of bytes written per file
      while (!done)
        {
          bytestowrite = fread (buffer, 1, sizeof buffer, srcfile);
          if (totalsize + bytestowrite > truncate_size)
            {
              bytestowrite = truncate_size - totalsize;
              done = 1;
              truncated = 1;
              printf ("Output file is %u Mbit, truncating %s, skipping %u bytes\n",
                      truncate_size / MBIT, fname_ptr,
                      (unsigned) (fsizeof (fname_ptr) - (byteswritten + bytestowrite)));
              // DON'T use fstate.st_size, because file could be compressed
            }
          totalsize += bytestowrite;
          if (bytestowrite == 0)
            done = 1;
          fwrite (buffer, 1, bytestowrite, destfile);
          byteswritten += bytestowrite;
        }
      fclose (srcfile);
      if (truncated)
        break;
      file_no++;
    }
  fclose (destfile);

  /*
    Display a notification if a truncate size was specified that is not exactly
    the size of one of the flash card sizes.
  */
  n = truncate_size;
  while (n >>= 1)
    size_pow2 <<= 1;
  if (truncate_size == size_pow2)
    truncate_size_ispow2 = 1;

  n = totalsize - 1;
  while (n >>= 1)
    size_pow2_lesser <<= 1;

  size_pow2 = size_pow2_lesser << 1;

  if (totalsize > 64 * MBIT && !truncate_size_ispow2)
    printf("\n"
           "NOTE: This multi-game file can only be written to a card >= %u Mbit.\n"
           "      Use -multi=%u to create a file truncated to %u Mbit.\n"
           "      Current size is %.5f Mbit\n", // 5 digits to have 1 byte resolution
           size_pow2 / MBIT, size_pow2_lesser / MBIT, size_pow2_lesser / MBIT,
           totalsize / (float) MBIT);

  return 0;
}


static int
gba_saver_patch (FILE *destfile, unsigned char *buffer, unsigned int fsize)
{
  // reverse engineered from SuperCard.exe
  const unsigned char saver_patch_orig[38] =
    {
      0xf0, 0xb5, 0xa0, 0xb0, 0x0d, 0x1c, 0x16, 0x1c,
      0x1f, 0x1c, 0x03, 0x04, 0x1c, 0x0c, 0x0f, 0x4a,
      0x10, 0x88, 0x0f, 0x49, 0x08, 0x40, 0x03, 0x21,
      0x08, 0x43, 0x10, 0x80, 0x0d, 0x48, 0x00, 0x68,
      0x01, 0x68, 0x80, 0x20, 0x80, 0x02
    },
    saver_patch_repl[38] =
    {
      0x70, 0xb5, 0xa0, 0xb0, 0x00, 0x03, 0x40, 0x18,
      0xe0, 0x21, 0x09, 0x05, 0x09, 0x18, 0x08, 0x78,
      0x10, 0x70, 0x01, 0x3b, 0x01, 0x32, 0x01, 0x31,
      0x00, 0x2b, 0xf8, 0xd1, 0x00, 0x20, 0x20, 0xb0,
      0x70, 0xbc, 0x02, 0xbc, 0x08, 0x47
    },
    saver_patch2_orig[38] =
    {
      0xf0, 0xb5, 0x90, 0xb0, 0x0f, 0x1c, 0x00, 0x04,
      0x04, 0x0c, 0x0f, 0x2c, 0x04, 0xd9, 0x01, 0x48,
      0x40, 0xe0, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00,
      0x20, 0x1c, 0xff, 0xf7, 0xd7, 0xfe, 0x00, 0x04,
      0x05, 0x0c, 0x00, 0x2d, 0x35, 0xd1
    },
    saver_patch2_repl[38] =
    {
      0x70, 0xb5, 0x00, 0x03, 0x0a, 0x1c, 0xe0, 0x21,
      0x09, 0x05, 0x41, 0x18, 0x01, 0x23, 0x1b, 0x03,
      0x10, 0x78, 0x08, 0x70, 0x01, 0x3b, 0x01, 0x32,
      0x01, 0x31, 0x00, 0x2b, 0xf8, 0xd1, 0x00, 0x20,
      0x70, 0xbc, 0x02, 0xbc, 0x08, 0x47
    },
    saver_patch3_orig[38] =
    {
      0xa2, 0xb0, 0x0d, 0x1c, 0x00, 0x04, 0x03, 0x0c,
      0x03, 0x48, 0x00, 0x68, 0x80, 0x88, 0x83, 0x42,
      0x05, 0xd3, 0x01, 0x48,
      0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,       // 6 byte wildcard
      0xff, 0x80, 0x00, 0x00,
      0x3f,                                     // 1 byte wildcard
      0x48, 0x06, 0x1c, 0x00, 0x68, 0x01, 0x7a
    },
    saver_patch3_repl[38] =
    {
      0x00, 0x04, 0x0a, 0x1c, 0x40, 0x0b, 0xe0, 0x21,
      0x09, 0x05, 0x41, 0x18, 0x07, 0x31, 0x00, 0x23,
      0x08, 0x78, 0x10, 0x70,
      0x01, 0x33, 0x01, 0x32, 0x01, 0x39,
      0x07, 0x2b, 0xf8, 0xd9,
      0x00,
      0x20, 0x70, 0xbc, 0x02, 0xbc, 0x08, 0x47
    },
    saver_patch4_orig[40] =
    {
      0x30, 0xb5,
      0xa9, 0xb0, 0x0d, 0x1c, 0x00, 0x04, 0x04, 0x0c,
      0x03, 0x48, 0x00, 0x68, 0x80, 0x88, 0x84, 0x42,
      0x05, 0xd3, 0x01, 0x48,
      0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,       // 6 byte wildcard
      0xff, 0x80, 0x00, 0x00, 0x0f, 0x48,
      0x00, 0x68, 0x00, 0x7a, 0x40, 0x00
    },
    saver_patch4_repl[40] =
    {
      0x70, 0xb5,
      0x00, 0x04, 0x0a, 0x1c, 0x40, 0x0b, 0xe0, 0x21,
      0x09, 0x05, 0x41, 0x18, 0x07, 0x31, 0x00, 0x23,
      0x10, 0x78, 0x08, 0x70,
      0x01, 0x33, 0x01, 0x32, 0x01, 0x39,
      0x07, 0x2b, 0xf8, 0xd9, 0x00, 0x20,
      0x70, 0xbc, 0x02, 0xbc, 0x08, 0x47
    },
    saver_patch5_orig[42] =
    {
      0xf0, 0xb5, 0x90, 0xb0, 0x0f, 0x1c, 0x00, 0x04,
      0x04, 0x0c, 0x03, 0x48, 0x00, 0x68, 0x40, 0x89,
      0x84, 0x42, 0x05, 0xd3, 0x01, 0x48, 0x41, 0xe0,
      0xf0, 0x89, 0x03, 0x02, 0xff, 0x80, 0x00, 0x00,
      0x20, 0x1c, 0xff, 0xf7, 0xd3, 0xfe, 0x00, 0x04,
      0x05, 0x0c
    },
    saver_patch5_repl[42] =
    {
      0x7c, 0xb5, 0x90, 0xb0, 0x00, 0x03, 0x0a, 0x1c,
      0xe0, 0x21, 0x09, 0x05, 0x09, 0x18, 0x01, 0x23,
      0x1b, 0x03, 0x10, 0x78, 0x08, 0x70, 0x01, 0x3b,
      0x01, 0x32, 0x01, 0x31, 0x00, 0x2b, 0xf8, 0xd1,
      0x00, 0x20, 0x10, 0xb0, 0x7c, 0xbc, 0x02, 0xbc,
      0x08, 0x47
    };
  int found = 0;
  unsigned char *ptr = NULL;

  for (ptr = buffer;
       (ptr = (unsigned char *) memmem2 (ptr, fsize - (ptr - buffer),
                                         saver_patch_orig, 38, 0)) != NULL;
       ptr++)
    {
      if (ucon64.quiet < 0)
        printf ("offset: 0x%08x\n", (int) (ptr - buffer));
      fseek (destfile, (long) (ptr - buffer), SEEK_SET);
      fwrite (saver_patch_repl, 1, 38, destfile);
      found++;
    }

  for (ptr = buffer;
       (ptr = (unsigned char *) memmem2 (ptr, fsize - (ptr - buffer),
                                         saver_patch2_orig, 38, 0)) != NULL;
       ptr++)
    {
      if (ucon64.quiet < 0)
        printf ("offset: 0x%08x\n", (int) (ptr - buffer));
      fseek (destfile, (long) (ptr - buffer), SEEK_SET);
      fwrite (saver_patch2_repl, 1, 38, destfile);
      found++;
    }

  for (ptr = buffer;
       (ptr = (unsigned char *) memmem2 (ptr, fsize - (ptr - buffer),
                                         saver_patch3_orig, 38, MEMMEM2_WCARD (0x3f))) != NULL;
       ptr++)
    {
      if (ucon64.quiet < 0)
        printf ("offset: 0x%08x\n", (int) (ptr - buffer));
      fseek (destfile, (long) (ptr - buffer), SEEK_SET);
      fwrite (saver_patch3_repl, 1, 38, destfile);
      found++;
    }

  for (ptr = buffer;
       (ptr = (unsigned char *) memmem2 (ptr, fsize - (ptr - buffer),
                                         saver_patch4_orig, 40, MEMMEM2_WCARD (0x3f))) != NULL;
       ptr++)
    {
      if (ucon64.quiet < 0)
        printf ("offset: 0x%08x\n", (int) (ptr - buffer));
      fseek (destfile, (long) (ptr - buffer), SEEK_SET);
      fwrite (saver_patch4_repl, 1, 40, destfile);
      found++;
    }

  for (ptr = buffer;
       (ptr = (unsigned char *) memmem2 (ptr, fsize - (ptr - buffer),
                                         saver_patch5_orig, 42, 0)) != NULL;
       ptr++)
    {
      if (ucon64.quiet < 0)
        printf ("offset: 0x%08x\n", (int) (ptr - buffer));
      fseek (destfile, (long) (ptr - buffer), SEEK_SET);
      fwrite (saver_patch5_repl, 1, 42, destfile);
      found++;
    }

  fprintf (stdout, "%d saver patches applied\n", found);

  return 0;
}


// We allocate one buffer for the templates and loader (not for each structure)
#if     GBA_SCI_TEMPLATE_SIZE < GBA_BLANK_SIZE || \
        GBA_SCI_TEMPLATE_SIZE < GBA_MENU_SIZE || \
        GBA_SCI_TEMPLATE_SIZE < GBA_SAV_TEMPLATE_SIZE
#error GBA_SCI_TEMPLATE_SIZE is too small
#endif

int
gba_sc (void)
/*
  reverse engineered from SuperCard.exe

  BOND EON only?
  a0 7f 00 03 00 80 00 03
  a0 7f 00 03 f0 7f 00 03

  0e 48 39 68  01 60 0e 48 -> 00 48 00 47  01 fb 7f 08
  0003756c  0a 1c 02 80  0e 48 39 68  01 60 0e 48  79 68 01 60 TONY HAWK 2!
  0003756c  0a 1c 02 80  00 48 00 47  01 fb 7f 08  79 68 01 60

  d0 -> e0
  00037842  7f 08 27 e0  d0 20 00 05  01 88 01 22 TONY HAWK 2!
  00037842  7f 08 27 e0  e0 20 00 05  01 88 01 22

  08 80 -> c0 46
  00039870  08 88 07 48  08 80 00 f0  e5 f8 01 f0  a9 f8 01 f0 PINOBEE only?
  00039870  08 88 07 48  c0 46 00 f0  e5 f8 01 f0  a9 f8 01 f0

  08 80 -> c0 46
  00000238  3a 4c 20 1c  08 80 3a 49  3a 48 08 60  4a 60 3a 48 PINOBEE
  00000238  3a 4c 20 1c  c0 46 3a 49  3a 48 08 60  4a 60 3a 48

  01 60 -> c0 46
  000000fa  09 02 14 31  01 60 27 49  28 48 08 60  40 21 09 03 CT SPECIAL F only?
  000000fa  09 02 14 31  c0 46 27 49  28 48 08 60  40 21 09 03
*/
{
  const unsigned char sc_orig[15][6] =
    {
      { 0x04, 0x02, 0x00, 0x04, 0x01, 0x20 },
      { 0x04, 0x02, 0x00, 0x04, 0x14, 0x40 },
      { 0x04, 0x02, 0x00, 0x04, 0x14, 0xb2 },
      { 0x04, 0x02, 0x00, 0x04, 0x38, 0x68 },
      { 0x04, 0x02, 0x00, 0x04, 0xa0, 0x0f },
      { 0x04, 0x02, 0x00, 0x04, 0xb4, 0x05 },
      { 0x04, 0x02, 0x00, 0x04, 0xb4, 0x45 },
      { 0x04, 0x02, 0x00, 0x04, 0xb6, 0x45 },
      { 0x04, 0x02, 0x00, 0x04, 0xb7, 0x45 },
      { 0x04, 0x02, 0x00, 0x04, 0xd4, 0x00 },
      { 0x04, 0x02, 0x00, 0x04, 0xe3, 0xff },
      { 0x04, 0x02, 0x00, 0x04, 0xf8, 0x20 },
      { 0x04, 0x02, 0x00, 0x04, 0xfc, 0xff },
      { 0x04, 0x02, 0x00, 0x04, 0xfe, 0x2f },
//      { 0x04, 0x02, 0x00, 0x04, 0xfe, 0xff },   // CASTLEVANIA1 + CROUCHING TI only?
      { 0x04, 0x02, 0x00, 0x04, 0xff, 0xf8 }    //  they have it but work also without it
    },
    sc_repl[4] =
    {
      0x00, 0x00, 0x00, 0x00                    // overwrite only the 0x04 0x02 0x00 0x04 part
    },
    sc2_orig[4] =
    {
      0xfc, 0x7f, 0x00, 0x03                    // replace this everywhere
    },
    sc2_repl = 0xa0,
    sc3_orig[92] =
    {
      // replace this at offset 0xee only
      0x80, 0xe3,
      0x81, 0x1f, 0xa0, 0xe3,
      0xb1, 0x00, 0x8c,                         // diff
      0xe1,
      0xd4, 0xc0, 0x8c, 0xe2, 0x74, 0x00, 0x9f, 0xe5,
      0x74, 0x10, 0x9f, 0xe5, 0x74, 0x30, 0x9f, 0xe5,
      0x01, 0x30, 0x43, 0xe0, 0x21, 0x23, 0xa0, 0xe3,
      0x43, 0x21, 0x82, 0xe1, 0x07, 0x00, 0x8c, 0xe8,
      0x03, 0x00, 0x80, 0xe0, 0x60, 0x10, 0x9f, 0xe5,
      0x60, 0x30, 0x9f, 0xe5, 0x01, 0x30, 0x43, 0xe0,
      0x21, 0x23, 0xa0, 0xe3, 0x43, 0x21, 0x82, 0xe1,
      0x07, 0x00, 0x8c, 0xe8, 0x64, 0x30, 0x9f, 0xe5,
      0x13, 0xff, 0x2f, 0xe1, 0x15, 0x49, 0x13, 0x48,
      0x16, 0x4b, 0x42, 0x1a, 0xd2, 0x1a, 0x02, 0x20,
      0xfe, 0x46
    },
    sc3_repl[92] =
    {
      0x80, 0xe3,
      0x81, 0x1f, 0xa0, 0xe3,
      0x00, 0x00, 0xa0,                         // diff
      0xe1,
      0xd4, 0xc0, 0x8c, 0xe2, 0x74, 0x00, 0x9f, 0xe5,
      0x74, 0x10, 0x9f, 0xe5, 0x74, 0x30, 0x9f, 0xe5,
      0x01, 0x30, 0x43, 0xe0, 0x21, 0x23, 0xa0, 0xe3,
      0x43, 0x21, 0x82, 0xe1, 0x07, 0x00, 0x8c, 0xe8,
      0x03, 0x00, 0x80, 0xe0, 0x60, 0x10, 0x9f, 0xe5,
      0x60, 0x30, 0x9f, 0xe5, 0x01, 0x30, 0x43, 0xe0,
      0x21, 0x23, 0xa0, 0xe3, 0x43, 0x21, 0x82, 0xe1,
      0x07, 0x00, 0x8c, 0xe8, 0x64, 0x30, 0x9f, 0xe5,
      0x13, 0xff, 0x2f, 0xe1, 0x15, 0x49, 0x13, 0x48,
      0x16, 0x4b, 0x42, 0x1a, 0xd2, 0x1a, 0x02, 0x20,
      0xfe, 0x46
    };
  int x = 0;
  unsigned int fsize = (unsigned int) ucon64.fsize, padded = 0;
  uint32_t address = 0, pos = 0;
  char dest_name[FILENAME_MAX], fname[FILENAME_MAX];
  unsigned char *buffer, *ptr = NULL;
  const char *p = NULL;
  FILE *destfile;

  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");

  if ((destfile = fopen (dest_name, "rb+")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], dest_name);
      return -1;
    }
  if ((buffer = (unsigned char *) malloc (fsize)) == NULL)
    {
      fprintf (stderr, ucon64_msg[ROM_BUFFER_ERROR], fsize);
      fclose (destfile);
      exit (1);
    }
  if (fread (buffer, 1, fsize, destfile) != fsize)
    {
      fprintf (stderr, ucon64_msg[READ_ERROR], dest_name);
      free (buffer);
      fclose (destfile);
      return -1;
    }

  // saver patch
  gba_saver_patch (destfile, buffer, fsize);

  // restart
  for (ptr = buffer;
       (ptr = (unsigned char *) memmem2 (ptr, fsize - (ptr - buffer),
                                         sc_orig[0], 4, 0)) != NULL &&
       (ptr - buffer) < 7445976;                // seems like < 7445976 is far enough
                                                // SONICPINBALL 64Mb (< 7445976)
                                                // AGB KIRBY DX (< 10223404) 128Mb
       ptr++)
    for (x = 0; x < 15; x++)
      if (!memcmp (ptr, sc_orig[x], 6))         // Do the last 2 bytes match?
        {
          if (ucon64.quiet < 0)
            printf ("offset: 0x%08x\n", (int) (ptr - buffer));
          fseek (destfile, (long) (ptr - buffer), SEEK_SET);
          fwrite (sc_repl, 1, 4, destfile);
        }

  for (ptr = buffer;
       (ptr = (unsigned char *) memmem2 (ptr, fsize - (ptr - buffer),
                                         sc2_orig, 4, 0)) != NULL &&
       (ptr - buffer) < 7445660;                // seems like < 7445660 is far enough
                                                // SONICPINBALL 64 Mb (< 7445660)
                                                // AGB KIRBY DX (< 10223428) 128Mb
       ptr++)
    {
      if (ucon64.quiet < 0)
        printf ("offset: 0x%08x\n", (int) (ptr - buffer));
      fseek (destfile, (long) (ptr - buffer), SEEK_SET);
      fputc (sc2_repl, destfile);
    }

  if (!memcmp (buffer + 0xee, &sc3_orig, 92))
    {
      if (ucon64.quiet < 0)
        printf ("offset: 0x%08x\n", 0xee);
      fseek (destfile, 0xee, SEEK_SET);
      fwrite (sc3_repl, 1, 92, destfile);
    }

  // write menu (intro) at end of ROM
  // SuperCard.exe ignores padding with anything else than 0xff Bytes
  fseek (destfile, -1, SEEK_END);
  if ((unsigned char) fgetc (destfile) == 0xff)
    padded = (unsigned int) ucon64_testpad (ucon64.fname);
  fseek (destfile, fsize - padded, SEEK_SET);

  printf ("Writing restart menu at offset: 0x%08x\n", fsize - padded);

  {
    // there is a 52232 bytes blank before the actual menu
    unsigned char *old_buffer = buffer;
    if ((buffer = (unsigned char *) realloc (old_buffer, GBA_SCI_TEMPLATE_SIZE)) == NULL)
      {
        fprintf (stderr, ucon64_msg[ROM_BUFFER_ERROR], GBA_SCI_TEMPLATE_SIZE);
        free (old_buffer);
        fclose (destfile);
        exit (1);
      }
  }
  memset (buffer, 0, GBA_BLANK_SIZE);
  fwrite (buffer, 1, GBA_BLANK_SIZE, destfile);

  /*
    menu[0x2f0]:
    0xe0,                               // some kind of chksum? changes into 0xee, 0xf0, ...
    0xff, 0x00, 0x0e, 0x01, 0x00, 0x00, 0x00,
    0xc0, 0x00, 0x00, 0x08,             // the old start address 0xc0 changes into 0xc8 sometimes
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xf0, 0xfd, 0x00, 0x0e,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08,             // address in menu
    0x00, 0x00, 0x00, 0x00,
  */
  // write the menu (the formulas will NOT be optimized)
  p = get_property (ucon64.configfile, "gbaloader_sc", PROPERTY_MODE_FILENAME);
  if (!p)
    p = "sc_menu.bin";
  {
    size_t len = strnlen (p, sizeof fname - 1);

    strncpy (fname, p, len)[len] = '\0';
  }
  if (ucon64_fread (buffer, 0, GBA_MENU_SIZE, fname) == 0)
    {
      fprintf (stderr, "ERROR: Could not load Super Card loader (%s)\n", fname);
      exit (1);                                 // fatal
    }
  fwrite (buffer, 1, GBA_MENU_SIZE, destfile);
  pos = ftell (destfile);                       // truncate() this later

  // calculate and write new start address
  printf ("New start address: 0x%08x\n", (int) (pos - GBA_MENU_SIZE + 0x8000000));
  address = ((pos - GBA_MENU_SIZE - 8) >> 2) & 0xffffff;
  fseek (destfile, 0, SEEK_SET);
#ifdef  WORDS_BIGENDIAN
  address = bswap_32 (address);
#endif
  fwrite (&address, 1, 3, destfile);

  // calculate and write new address at offset 0x60
  printf ("New offset 0x60 address: 0x%08x\n", (int) (pos - GBA_MENU_SIZE + 846));
  address = (pos - GBA_MENU_SIZE + 846 - 2) & 0xffffff;
  if ((pos - GBA_MENU_SIZE) > 128 * MBIT)
    address |= 0x09000000;                      // 0x09 == "large" jmp?
  else
    address |= 0x08000000;
  fseek (destfile, 0x60, SEEK_SET);
#ifdef  WORDS_BIGENDIAN
  address = bswap_32 (address);
#endif
  fwrite (&address, 1, 4, destfile);

  // calculate and write new address in menu
  printf ("New address in menu: 0x%08x\n", (int) (pos - GBA_MENU_SIZE + 846 - 53076));
  address = (pos - GBA_MENU_SIZE + 846 - 53076 - 2) & 0xffffff;
  if ((pos - GBA_MENU_SIZE) > 128 * MBIT)
    address |= 0x09000000;                      // 0x09 == "large" jmp?
  else
    address |= 0x08000000;
  fseek (destfile, pos - GBA_MENU_SIZE + 784, SEEK_SET);
#ifdef  WORDS_BIGENDIAN
  address = bswap_32 (address);
#endif
  fwrite (&address, 1, 4, destfile);

  fclose (destfile);

  puts ("Removing padded bytes");
  truncate2 (dest_name, pos);
  puts ("Super Card conversion done");
  printf (ucon64_msg[WROTE], dest_name);

  // write SAV template
  set_suffix (dest_name, ".sav");
  if ((destfile = fopen (dest_name, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], dest_name);
      return -1;
    }
  memset (buffer, 0, GBA_SAV_TEMPLATE_SIZE);
  buffer[2] = 0xaa;
  buffer[3] = 0x55;
  fwrite (buffer, 1, GBA_SAV_TEMPLATE_SIZE, destfile);
  fclose (destfile);
  printf (ucon64_msg[WROTE], dest_name);

  // write SCI template
  set_suffix (dest_name, ".sci");
  if ((destfile = fopen (dest_name, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], dest_name);
      return -1;
    }
  memset (buffer, 0, GBA_SCI_TEMPLATE_SIZE);
  buffer[2] = 0xaa;
  buffer[3] = 0x55;
  fwrite (buffer, 1, GBA_SCI_TEMPLATE_SIZE, destfile);
  fclose (destfile);
  printf (ucon64_msg[WROTE], dest_name);

  free (buffer);
  return 0;
}
