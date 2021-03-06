/*
n64.c - Nintendo 64 support for uCON64

Copyright (c) 1999 - 2001                    NoisyB
Copyright (c) 2002 - 2005, 2015, 2017 - 2021 dbjh
Copyright (c) 2005                           Parasyte

05-06-2005 / Parasyte:
  Added support for remaining CIC algorithms in n64_chksum()


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
#include "misc/archive.h"
#include "misc/chksum.h"
#include "misc/file.h"
#include "misc/misc.h"
#include "ucon64_misc.h"
#include "console/n64.h"
#include "backup/backup.h"
#include "backup/doctor64.h"
#include "backup/z64.h"


#define N64_HEADER_LEN (sizeof (st_n64_header_t))
#define N64_SRAM_SIZE  512
#define N64_NAME_LEN   20
#define N64_BC_SIZE    (0x1000 - N64_HEADER_LEN)
#define LAC_ROM_SIZE   1310720


static st_ucon64_obj_t n64_obj[] =
  {
    {0, WF_SWITCH},
    {0, WF_DEFAULT},
    {0, WF_INIT | WF_PROBE},
    {UCON64_N64, WF_SWITCH},
    {UCON64_N64, WF_DEFAULT},
    {UCON64_N64, WF_INIT | WF_PROBE}
  };

const st_getopt2_t n64_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "Nintendo 64"/*"1996 Nintendo http://www.nintendo.com"*/,
      NULL
    },
    {
      UCON64_N64_S, 0, 0, UCON64_N64,
      NULL, "force recognition",
      &n64_obj[3]
    },
    {
      "int", 0, 0, UCON64_INT,
      NULL, "force ROM is in interleaved format (2143, V64)",
      &n64_obj[0]
    },
    {
      "nint", 0, 0, UCON64_NINT,
      NULL, "force ROM is not in interleaved format (1234, Z64)",
      &n64_obj[0]
    },
    {
      "n", 1, 0, UCON64_N,
      "NEW_NAME", "change internal ROM name to NEW_NAME",
      &n64_obj[1]
    },
    {
      "v64", 0, 0, UCON64_V64,
      NULL, "convert to Doctor V64 (and compatibles/interleaved)",
      &n64_obj[4]
    },
    {
      "z64", 0, 0, UCON64_Z64,
      NULL, "convert to Mr. Backup Z64 (not interleaved)",
      &n64_obj[4]
    },
    {
      "dint", 0, 0, UCON64_DINT,
      NULL, "convert ROM to (non-)interleaved format (1234 <-> 2143)",
      &n64_obj[2]
    },
    {
      "swap", 0, 0, UCON64_SWAP,
      NULL, "same as " OPTION_LONG_S "dint, byte-swap ROM",
      &n64_obj[2]
    },
    {
      "swap2", 0, 0, UCON64_SWAP2,
      NULL, "word-swap ROM (1234 <-> 3412)",
      NULL
    },
#if 0
    {
      "f", 0, 0, UCON64_F,
      NULL, "remove NTSC/PAL protection",
      NULL
    },
#endif
    {
      "bot", 1, 0, UCON64_BOT,
      "BOOTCODE", "replace/extract BOOTCODE (4032 Bytes) in/from ROM;\n"
      "extracts automatically if BOOTCODE does not exist",
      &n64_obj[4]
    },
    {
      "lsram", 1, 0, UCON64_LSRAM,
      "SRAM", "LaC's SRAM upload tool; ROM should be LaC's ROM image\n"
      "the SRAM must have a size of 512 Bytes\n"
      "this option generates a ROM which can be used to transfer\n"
      "SRAMs to your cartridge's SRAM (EEPROM)",
      &n64_obj[5]
    },
    {
      "usms", 1, 0, UCON64_USMS,
      "SMSROM", "Jos Kwanten's UltraSMS (Sega Master System/Game Gear emulator);\n"
      "ROM should be Jos Kwanten's UltraSMS ROM image\n"
      "works only for SMS ROMs which are <= 4 Mb in size",
      &n64_obj[4]
    },
    {
      "chk", 0, 0, UCON64_CHK,
      NULL, "fix ROM checksum\n"
      "supports 6101, 6102, 6103, 6105 and 6106 boot codes",
      &n64_obj[1]
    },
#if 0
    {
      "bios", 1, 0, UCON64_BIOS,
      "BIOS", "enable backup in Doctor V64 BIOS",
      NULL
    },
#endif
    {NULL, 0, 0, 0, NULL, NULL, NULL}
};


typedef struct st_n64_header
{
  unsigned char pad[64];
#if 0
  unsigned char validation[2];
  unsigned char compression;
  unsigned char pad1;
  unsigned long clockrate;
  unsigned long programcounter;
  unsigned long release;
  unsigned long crc1;
  unsigned long crc2;
  unsigned char pad2[8];
  unsigned char name[20];
  unsigned char pad3[7];
  unsigned char maker;
  unsigned char cartridgeid[2];
  unsigned char countrycode;
  unsigned char pad4;
#endif
} st_n64_header_t;

static st_n64_header_t n64_header;

typedef struct st_n64_chksum
{
  unsigned int crc1;
  unsigned int crc2;
} st_n64_chksum_t;

static st_n64_chksum_t n64crc;
static int n64_chksum (st_ucon64_nfo_t *rominfo, const char *filename);


int
n64_v64 (st_ucon64_nfo_t *rominfo)
{
  char dest_name[FILENAME_MAX];

  strcpy (dest_name, ucon64.fname);
  set_suffix (dest_name, ".v64");
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");
  if (!rominfo->interleaved)
    ucon64_fbswap16 (dest_name, 0, ucon64.fsize);

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
n64_z64 (st_ucon64_nfo_t *rominfo)
{
  char dest_name[FILENAME_MAX];

  strcpy (dest_name, ucon64.fname);
  set_suffix (dest_name, ".z64");
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");
  if (rominfo->interleaved)
    ucon64_fbswap16 (dest_name, 0, ucon64.fsize);

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
n64_n (st_ucon64_nfo_t *rominfo, const char *name)
{
  char buf[N64_NAME_LEN], dest_name[FILENAME_MAX];
  size_t len = strnlen (name, N64_NAME_LEN);

  if (len < N64_NAME_LEN)                       // warning remover
    memset (buf + len, ' ', N64_NAME_LEN - len);
  strncpy (buf, name, len);

  if (rominfo->interleaved)
    ucon64_bswap16_n (buf, N64_NAME_LEN);

  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");
  ucon64_fwrite (buf, rominfo->backup_header_len + 32, N64_NAME_LEN, dest_name, "r+b");

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
n64_f (st_ucon64_nfo_t *rominfo)
{
  // TODO: PAL/NTSC fix
  (void) rominfo;                               // warning remover
  fputs ("ERROR: The function for cracking N64 region protections is not yet implemented\n", stderr);
  return 0;
}


static void
n64_update_chksum (st_ucon64_nfo_t *rominfo, const char *filename, char *buf)
{
  uint64_t crc;
  int x;

  // n64crc is set by n64_chksum() when called from n64_init()
  crc = (((uint64_t) n64crc.crc1) << 32) | n64crc.crc2;
  for (x = 0; x < 8; x++)
    {
      buf[x] = (char) (crc >> 56);
      crc <<= 8;
    }
  if (rominfo->interleaved)
    ucon64_bswap16_n (buf, 8);
  ucon64_fwrite (buf, rominfo->backup_header_len + 16, 8, filename, "r+b");
}


int
n64_chk (st_ucon64_nfo_t *rominfo)
{
  char buf[8], dest_name[FILENAME_MAX];

  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");

  n64_update_chksum (rominfo, dest_name, buf);
  dumper (stdout, buf, 8, rominfo->backup_header_len + 16, DUMPER_HEX);

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
n64_sram (st_ucon64_nfo_t *rominfo, const char *sramfile)
// Function to insert an SRAM file in LaC's SRAM upload tool (which is an N64
//  program)
{
  char sram[N64_SRAM_SIZE], dest_name[FILENAME_MAX], buf[8];

  if (access (sramfile, F_OK))
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], sramfile);
      return -1;
    }

  if (fsizeof (sramfile) != N64_SRAM_SIZE || ucon64.fsize != LAC_ROM_SIZE)
    {
      fprintf (stderr, "ERROR: ROM is not %d bytes and/or SRAM is not %d bytes\n",
               LAC_ROM_SIZE, N64_SRAM_SIZE);
      return -1;
    }

  ucon64_fread (sram, 0, N64_SRAM_SIZE, sramfile);

  if (rominfo->interleaved)
    ucon64_bswap16_n (sram, N64_SRAM_SIZE);

  strcpy (dest_name, ucon64.fname);
  ucon64_file_handler (dest_name, NULL, 0);
  fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");
  ucon64_fwrite (sram, 0x286c0, N64_SRAM_SIZE, dest_name, "r+b");
  if (n64_chksum (rominfo, dest_name) == 0)     // calculate the checksum of the modified file
    n64_update_chksum (rominfo, dest_name, buf);

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
n64_bot (st_ucon64_nfo_t *rominfo, const char *bootfile)
{
  char dest_name[FILENAME_MAX];

  if (!access (bootfile, F_OK))
    {
      char buf[N64_BC_SIZE];

      strcpy (dest_name, ucon64.fname);
      ucon64_fread (buf, 0, N64_BC_SIZE, bootfile);

      if (rominfo->interleaved)
        ucon64_bswap16_n (buf, N64_BC_SIZE);

      ucon64_file_handler (dest_name, NULL, 0);
      fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb");
      ucon64_fwrite (buf, rominfo->backup_header_len + N64_HEADER_LEN, N64_BC_SIZE,
                     dest_name, "r+b");
    }
  else
    {
      strcpy (dest_name, bootfile);
//      set_suffix (dest_name, ".bot");
      ucon64_file_handler (dest_name, NULL, OF_FORCE_BASENAME | OF_FORCE_SUFFIX);
      fcopy (ucon64.fname, rominfo->backup_header_len + N64_HEADER_LEN, N64_BC_SIZE,
             dest_name, "wb");

      if (rominfo->interleaved)
        ucon64_fbswap16 (dest_name, 0, (size_t) fsizeof (dest_name));
    }

  printf (ucon64_msg[WROTE], dest_name);
  return 0;
}


int
n64_usms (st_ucon64_nfo_t *rominfo, const char *smsrom)
{
  char dest_name[FILENAME_MAX], *usmsbuf;
  unsigned int size;

  if (access (smsrom, F_OK))
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], smsrom);
      return -1;
    }

  size = (unsigned int) fsizeof (smsrom);
  // must be smaller than 4 Mbit, 524288 bytes will be inserted
  //  from 0x1b410 to 0x9b40f (0x7ffff)
  if (size > 4 * MBIT)
    {
      fputs ("ERROR: The Sega Master System/Game Gear ROM must be 524288 bytes or less\n", stderr);
      return -1;
    }

  if ((usmsbuf = (char *) malloc (4 * MBIT)) == NULL)
    {
      fprintf (stderr, ucon64_msg[BUFFER_ERROR], 4 * MBIT);
      exit (1);
    }
  memset (usmsbuf, 0xff, 4 * MBIT);
  ucon64_fread (usmsbuf, 0, size, smsrom);

  if (rominfo->interleaved)
    ucon64_bswap16_n (usmsbuf, size);

  // Jos Kwanten's rominserter.exe produces a file named Patched.v64
  strcpy (dest_name, "Patched.v64");
  ucon64_file_handler (dest_name, NULL, OF_FORCE_BASENAME | OF_FORCE_SUFFIX);
  fcopy (ucon64.fname, rominfo->backup_header_len, ucon64.fsize, dest_name, "wb");
  ucon64_fwrite (usmsbuf, rominfo->backup_header_len + 0x01b410, 4 * MBIT, dest_name, "r+b");

  free (usmsbuf);
  printf (ucon64_msg[WROTE], dest_name);

  return 0;
}


int
n64_init (st_ucon64_nfo_t *rominfo)
{
  int result = -1;
  unsigned int value = 0;
#define N64_MAKER_MAX 0x50
  const char *n64_maker[N64_MAKER_MAX] =
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
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, "Nintendo", NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, "Nintendo", NULL
    },
#define N64_COUNTRY_MAX 0x5a
    *n64_country[N64_COUNTRY_MAX] =
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
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, "Germany", "U.S.A.",
      "France", NULL, NULL, "Italy", "Japan",
      NULL, NULL, NULL, NULL, NULL,
      "Europe", NULL, NULL, "Spain", NULL,
      "Australia", NULL, NULL, "France, Germany, The Netherlands", NULL // Holland is an incorrect name for The Netherlands
    };

  rominfo->backup_header_len = UCON64_ISSET2 (ucon64.backup_header_len, unsigned int) ?
                                 ucon64.backup_header_len : 0;

  ucon64_fread (&n64_header, rominfo->backup_header_len, N64_HEADER_LEN, ucon64.fname);

  value = OFFSET (n64_header, 0);
  value += OFFSET (n64_header, 1) << 8;
  value += OFFSET (n64_header, 2) << 16;
  value += OFFSET (n64_header, 3) << 24;
  /*
    0x41123780 and 0x12418037 can be found in te following files:
    2 Blokes & An Armchair - Nintendo 64 Remix Remix (PD)
    Zelda Boot Emu V1 (PD)
    Zelda Boot Emu V2 (PD)
  */
  if (value == 0x40123780 || value == 0x41123780) // 0x80371240, 0x80371241
    {
      rominfo->interleaved = 0;
      result = 0;
    }
  else if (value == 0x12408037 || value == 0x12418037) // 0x37804012, 0x37804112
    {
      rominfo->interleaved = 1;
      result = 0;
    }
  else
    result = -1;

  if (UCON64_ISSET (ucon64.interleaved))
    rominfo->interleaved = ucon64.interleaved;
  if (ucon64.console == UCON64_N64)
    result = 0;

  // internal ROM header
  rominfo->header_start = 0;
  rominfo->header_len = N64_HEADER_LEN;
  rominfo->header = &n64_header;

  // internal ROM name
  strncpy (rominfo->name, (char *) &OFFSET (n64_header, 32), N64_NAME_LEN);
  if (rominfo->interleaved)
    ucon64_bswap16_n (rominfo->name, N64_NAME_LEN);
  rominfo->name[N64_NAME_LEN] = '\0';

  // ROM maker
  rominfo->maker = NULL_TO_UNKNOWN_S (n64_maker[MIN (OFFSET
    (n64_header, 59 ^ rominfo->interleaved), N64_MAKER_MAX - 1)]);

  // ROM country
  rominfo->country = NULL_TO_UNKNOWN_S (n64_country[MIN (OFFSET
    (n64_header, 63 ^ (!rominfo->interleaved)), N64_COUNTRY_MAX - 1)]);

  // CRC stuff
  if (!UCON64_ISSET (ucon64.do_not_calc_crc) && result == 0)
    {
      int x;

      rominfo->has_internal_crc = 1;
      rominfo->internal_crc_len = 4;

      if (n64_chksum (rominfo, ucon64.fname) == 0)
        rominfo->current_internal_crc = n64crc.crc1;
      else
        rominfo->current_internal_crc = 0;

      value = 0;
      for (x = 0; x < 4; x++)
        {
          rominfo->internal_crc <<= 8;
          rominfo->internal_crc += OFFSET (n64_header, 16 + (x ^ rominfo->interleaved));
          value <<= 8;
          value += OFFSET (n64_header, 20 + (x ^ rominfo->interleaved));
        }

      sprintf (rominfo->internal_crc2,
               "2nd Checksum: %s, 0x%08x (calculated) %c= 0x%08x (internal)",
               n64crc.crc2 == value ?
#ifdef  USE_ANSI_COLOR
                 ucon64.ansi_color ? "\x1b[01;32mOK\x1b[0m" : "OK" :
                 ucon64.ansi_color ? "\x1b[01;31mBad\x1b[0m" : "Bad",
#else
                 "OK" : "Bad",
#endif
               n64crc.crc2,
               (n64crc.crc2 == value) ? '=' : '!', value);
    }

  rominfo->console_usage = n64_usage[0].help;
  rominfo->backup_usage = (!rominfo->backup_header_len ?
    ((!rominfo->interleaved) ? z64_usage[0].help : doctor64_usage[0].help) : unknown_backup_usage[0].help);

  return result;
}


/*
  ROM checksum routine is based on chksum64 V1.2 by Andreas Sterbenz
  <stan@sbox.tu-graz.ac.at>, a program to calculate the ROM checksum of
  Nintendo 64 ROMs.
*/
#define ROL(i, b) (((i) << (b)) | ((i) >> (32 - (b))))
#define BYTES2LONG(b, s) ( (b)[0 ^ (s)] << 24 | \
                           (b)[1 ^ (s)] << 16 | \
                           (b)[2 ^ (s)] <<  8 | \
                           (b)[3 ^ (s)] )

#define CHECKSUM_START   0x1000 //(N64_HEADER_LEN + N64_BC_SIZE)
#define CHECKSUM_LENGTH  0x100000
#define CHECKSUM_CIC6102 0xf8ca4ddc
#define CHECKSUM_CIC6103 0xa3886759
#define CHECKSUM_CIC6105 0xdf26f436
#define CHECKSUM_CIC6106 0x1fea617a
#define CALC_CRC32                              // see this as a marker, don't disable

static int
n64_chksum (st_ucon64_nfo_t *rominfo, const char *filename)
{
  unsigned char bootcode_buf[N64_BC_SIZE], chunk[MAXBUFSIZE & ~3]; // size must be a multiple of 4
  unsigned int i, c1, k1, k2, t1, t2, t3, t4, t5, t6, bootcode,
               clen = CHECKSUM_LENGTH, // using ucon64.fsize is OK for n64_init() & n64_sram()
               rlen = (unsigned int) ucon64.fsize - rominfo->backup_header_len,
               n = rlen;
  FILE *file;
#ifdef  CALC_CRC32
  unsigned int scrc32 = 0, fcrc32 = 0;          // search CRC32 & file CRC32
  int swap_data = ucon64.n64_dat_v64 ? !rominfo->interleaved : rominfo->interleaved;
#endif

  rlen -= MIN (rlen, CHECKSUM_START);
  if (rlen < CHECKSUM_LENGTH)
    {
#ifdef  CALC_CRC32
      unsigned char *crc32_mem;

      if ((crc32_mem = (unsigned char *) malloc (n)) == NULL)
        {
          fprintf (stderr, ucon64_msg[BUFFER_ERROR], n);
          return -1;
        }
      ucon64_fread (crc32_mem, rominfo->backup_header_len, n, filename);
      if (swap_data)
        {
          ucon64.fcrc32 = crc32 (0, crc32_mem, n);
          ucon64_bswap16_n (crc32_mem, n);
        }
      ucon64.crc32 = crc32 (0, crc32_mem, n);
      free (crc32_mem);
#endif
      return -1;                                // ROM is too small
    }

  if ((file = fopen (filename, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], filename);
      return -1;
    }

#ifdef  CALC_CRC32
  fseek (file, rominfo->backup_header_len, SEEK_SET);
  n = (unsigned int) fread (chunk, 1, N64_HEADER_LEN + N64_BC_SIZE, file);
  if (n < N64_HEADER_LEN + N64_BC_SIZE)
    {
      fprintf (stderr, ucon64_msg[READ_ERROR], filename);
      return -1;
    }
  memcpy (bootcode_buf, chunk + N64_HEADER_LEN, N64_BC_SIZE);
  if (swap_data)
    {
      fcrc32 = crc32 (0, chunk, n);
      ucon64_bswap16_n (chunk, n);
    }
  scrc32 = crc32 (0, chunk, n);
#endif
  if (rominfo->interleaved)
    ucon64_bswap16_n (bootcode_buf, N64_BC_SIZE);
  n = crc32 (0, bootcode_buf, N64_BC_SIZE);
  if (n == 0x0b050ee0)
    {
      bootcode = 6103;
      i = CHECKSUM_CIC6103;
    }
  else if (n == 0x98bc2c86)
    {
      bootcode = 6105;
      i = CHECKSUM_CIC6105;
    }
  else if (n == 0xacc8580a)
    {
      bootcode = 6106;
      i = CHECKSUM_CIC6106;
    }
  else
    {
      bootcode = 0;                             // everything else
      i = CHECKSUM_CIC6102;
    }

  t1 = i;
  t2 = i;
  t3 = i;
  t4 = i;
  t5 = i;
  t6 = i;

  for (;;)
    {
      if (rlen > 0)
        {
          if ((n = (unsigned int) fread (chunk, 1,
                                         MIN (sizeof chunk, clen), file)) != 0)
            {
#ifdef  CALC_CRC32
              if (swap_data)
                {
                  fcrc32 = crc32 (fcrc32, chunk, n);
                  ucon64_bswap16_n (chunk, n);
                }
              scrc32 = crc32 (scrc32, chunk, n);
#endif
            }
        }
      else
        n = MIN (sizeof chunk, clen);

      n &= ~3;
      if (n == 0)
        break;
      for (i = 0; i < n; i += 4)
        {
          c1 = BYTES2LONG (&chunk[i], rominfo->interleaved ^ swap_data);
          k1 = t6 + c1;
          if (k1 < t6)
            t4++;
          t6 = k1;
          t3 ^= c1;
          k2 = c1 & 0x1f;
          k1 = k2 > 0 ? ROL (c1, k2) : c1;
          t5 += k1;
          if (c1 < t2)
            t2 ^= k1;
          else
            t2 ^= t6 ^ c1;

          if (bootcode == 6105)
            {
              k1 = 0x710 + (i & 0xff);
              t1 += ((bootcode_buf[k1] << 24) | (bootcode_buf[k1 + 1] << 16) |
                     (bootcode_buf[k1 + 2] <<  8) | bootcode_buf[k1 + 3]) ^ c1;
            }
          else
            t1 += c1 ^ t5;
        }
      if (rlen > 0)
        {
          rlen -= MIN (rlen, n);
          if (rlen == 0)
            memset (chunk, 0, sizeof chunk);
        }
      clen -= n;
    }
  if (bootcode == 6103)
    {
      n64crc.crc1 = (t6 ^ t4) + t3;
      n64crc.crc2 = (t5 ^ t2) + t1;
    }
  else if (bootcode == 6106)
    {
      n64crc.crc1 = t6 * t4 + t3;
      n64crc.crc2 = t5 * t2 + t1;
    }
  else
    {
      n64crc.crc1 = t6 ^ t4 ^ t3;
      n64crc.crc2 = t5 ^ t2 ^ t1;
    }

#ifdef  CALC_CRC32
  while ((n = (unsigned int) fread (chunk, 1, sizeof chunk, file)) != 0)
    {
      if (swap_data)
        {
          fcrc32 = crc32 (fcrc32, chunk, n);
          ucon64_bswap16_n (chunk, n);
        }
      scrc32 = crc32 (scrc32, chunk, n);
    }

  ucon64.crc32 = scrc32;
  if (swap_data)
    ucon64.fcrc32 = fcrc32;
#endif

  fclose (file);
  return 0;
}
