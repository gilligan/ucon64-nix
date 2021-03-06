/*
fig.c - Super PRO Fighter support for uCON64

Copyright (c) 1999 - 2002                          NoisyB
Copyright (c) 2001 - 2004, 2015, 2017, 2019 - 2021 dbjh
Copyright (c) 2003 - 2004                          JohnDie


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
#include <stdlib.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include <string.h>
#include "misc/archive.h"
#include "misc/file.h"
#include "ucon64_misc.h"
#include "console/snes.h"                       // for snes_get_snes_hirom()
#include "backup/ffe.h"


#ifdef  USE_PARALLEL
static st_ucon64_obj_t fig_obj[] =
  {
    {UCON64_SNES, WF_DEFAULT | WF_STOP | WF_NO_SPLIT | WF_NO_ROM},
    {UCON64_SNES, WF_STOP | WF_NO_ROM}
  };
#endif

const st_getopt2_t fig_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "Super Pro Fighter (Q/Q+)/Pro Fighter X (Turbo 2)/Double Pro Fighter (X Turbo)"
      /*"1993/1994/19XX China Coach Limited/CCL http://www.ccltw.com.tw"*/,
      NULL
    },
#ifdef  USE_PARALLEL
    {
      "xfig", 0, 0, UCON64_XFIG,
      NULL, "send/receive ROM to/from *Pro Fighter*/FIG; " OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "receives automatically when ROM does not exist",
      &fig_obj[0]
    },
    {
      "xfigs", 0, 0, UCON64_XFIGS,
      NULL, "send/receive SRAM to/from *Pro Fighter*/FIG; " OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "receives automatically when SRAM does not exist",
      &fig_obj[1]
    },
    {
      "xfigc", 0, 0, UCON64_XFIGC, NULL,
      "send/receive SRAM to/from cartridge in *Pro Fighter*/FIG;\n" OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "receives automatically when SRAM does not exist",
//      "Press q to abort; ^C might cause invalid state of backup unit"
      &fig_obj[1]
    },
#endif
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };


#ifdef  USE_PARALLEL

#define BUFFERSIZE 8192                         // don't change, only 8192 works!

static unsigned int receive_rom_info (unsigned char *buffer);
static unsigned char get_rom_size (unsigned char *info_block);
static int check1 (unsigned char *info_block, int index);
static int check2 (unsigned char *info_block, int index, unsigned char value);
static int check3 (unsigned char *info_block, int index1, int index2, int size);
static void handle_swc_header (unsigned char *header);

static unsigned int hirom;


#if     BUFFERSIZE < 512
#error receive_rom_info() and fig_read_sram() expect BUFFERSIZE to be at least \
       512 bytes.
#endif
static unsigned int
receive_rom_info (unsigned char *buffer)
/*
  - returns size of ROM in Mb (128 kB) units
  - sets global 'hirom'
*/
{
  unsigned short n;
  volatile int m;
  unsigned char size;

  ffe_send_command0 (0xe00c, 0);

  if (UCON64_ISSET (ucon64.snes_hirom))
    hirom = ucon64.snes_hirom ? 1 : 0;
  else
    {
      unsigned char byte;

      ffe_send_command (5, 3, 0);
      byte = ffe_send_command1 (0xbfd5);
      if ((byte & 1 && byte != 0x23) || byte == 0x3a) // & 1 => 0x21, 0x31, 0x35
        hirom = 1;
    }

  for (n = 0; n < FIG_HEADER_LEN; n++)
    {
      for (m = 0; m < 65536; m++)               // a delay is necessary here
        ;
      ffe_send_command (5, 0x200 + n, 0);
      buffer[n] = ffe_send_command1 (0xa0a0);
    }

  size = get_rom_size (buffer);
  if (hirom)
    size <<= 1;

  return size;
}


static unsigned char
get_rom_size (unsigned char *info_block)
// returns size of ROM in Mb units
{
  if (check1 (info_block, 0))
    return 0;
  if (check2 (info_block, 0x10, 0x84))
    return 0;
  if (check3 (info_block, 0, 0x20, 0x20))
    return 2;
  if (check3 (info_block, 0, 0x40, 0x20))
    return 4;
  if (check3 (info_block, 0x40, 0x60, 0x20))
    return 6;
  if (check3 (info_block, 0, 0x80, 0x10))
    return 8;
  if (check1 (info_block, 0x80))
    return 8;
  if (check3 (info_block, 0x80, 0x90, 0x10))
    return 8;
  if (check2 (info_block, 0x80, 0xa0))
    return 8;
  if (check3 (info_block, 0x80, 0xa0, 0x20))
    return 0xa;
  if (check1 (info_block, 0xc0))
    return 0xc;
  if (check2 (info_block, 0xc0, 0xb0))
    return 0xc;
  if (check3 (info_block, 0x80, 0xc0, 0x20))
    return 0xc;
  if (check3 (info_block, 0x100, 0, 0x10))
    return 0x10;
  if (check2 (info_block, 0x100, 0xc0))
    return 0x10;
  if (check3 (info_block, 0x100, 0x120, 0x10))
    return 0x12;
  if (check3 (info_block, 0x100, 0x140, 0x10))
    return 0x14;
  if (check2 (info_block, 0x140, 0xd0))
    return 0x14;
  if (check3 (info_block, 0x100, 0x180, 0x10))
    return 0x18;
  if (check2 (info_block, 0x180, 0xe0))
    return 0x18;
  if (check3 (info_block, 0x180, 0x1c0, 0x10))
    return 0x1c;
  if (check3 (info_block, 0x1f0, 0x1f0, 0x10))
    return 0x20;

  return 0;
}


static int
check1 (unsigned char *info_block, int index)
{
  int n;

  for (n = 0; n < 16; n++)
    if (info_block[n + index] != info_block[index])
      return 0;

  return 1;
}


static int
check2 (unsigned char *info_block, int index, unsigned char value)
{
  int n;

  for (n = 0; n < 4; n++)
    if (info_block[n + index] != value)
      return 0;

  return 1;
}


static int
check3 (unsigned char *info_block, int index1, int index2, int size)
{
  int n;

  for (n = 0; n < size; n++)
    if (info_block[n + index1] != info_block[n + index2])
      return 0;

  return 1;
}


static void
handle_swc_header (unsigned char *header)
{
  if ((header[2] & 0x10) == 0x10)
    {                                            // HiROM
      header[3] |= 0x80;

      if ((header[2] & 0x0c) == 0x0c)            // 0 Kbit SRAM
        {
          header[4] = 0x77;
          header[5] = 0x83;
        }
      else if (((header[2] & 0x0c) == 0x08) ||   // 16 *or* 64 Kbit SRAM
               ((header[2] & 0x0c) == 0x04))
        {
          header[4] = 0xdd;
          header[5] = 0x82;
        }
      else                                       // 256 Kbit SRAM
        {
          header[4] = 0xdd;
          header[5] = 0x02;
        }
    }
  else
    {                                            // LoROM
      header[3] &= 0x7f;

      if ((header[2] & 0x0c) == 0x0c)            // 0 Kbit SRAM
        {
          header[4] = 0x77;
          header[5] = 0x83;
        }
      else if (((header[2] & 0x0c) == 0x08) ||   // 16 *or* 64 Kbit SRAM
               ((header[2] & 0x0c) == 0x04))
        {
          header[4] = 0x00;
          header[5] = 0x80;
        }
      else                                       // 256 Kbit SRAM
        {
          header[4] = 0x00;
          header[5] = 0x00;
        }
    }
}


int
fig_read_rom (const char *filename, unsigned short parport)
{
  FILE *file;
  unsigned char *buffer;
  int n, size, blocksleft, bytesreceived = 0;
  unsigned short address1, address2;
  time_t starttime;
  st_ucon64_nfo_t rominfo;

  ffe_init_io (parport);

  if ((file = fopen (filename, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], filename);
      exit (1);
    }
  if ((buffer = (unsigned char *) malloc (BUFFERSIZE)) == NULL)
    {
      fprintf (stderr, ucon64_msg[FILE_BUFFER_ERROR], BUFFERSIZE);
      exit (1);
    }

  size = receive_rom_info (buffer);
  if (size == 0)
    {
      fputs ("ERROR: There is no cartridge present in the Pro Fighter\n", stderr);
      fclose (file);
      remove (filename);
      exit (1);
    }
  blocksleft = size * 16;                       // 1 Mb (128 kB) unit == 16 8 kB units
  printf ("Receive: %u Bytes (%.4f Mb)\n", size * MBIT, (float) size);
  size *= MBIT;                                 // size in bytes for ucon64_gauge() below

  ffe_send_command (5, 0, 0);
  ffe_send_command0 (0xe00c, 0);
  ffe_send_command0 (0xe003, 0);
//  byte = ffe_send_command1 (0xbfd8);

  memset (buffer, 0, FIG_HEADER_LEN);
  fwrite (buffer, 1, FIG_HEADER_LEN, file);     // write temporary empty header

  if (hirom)
    blocksleft >>= 1;

  puts ("Press q to abort\n");                  // print here, NOT before first FIG I/O,
                                                //  because if we get here q works ;-)
  address1 = 0x300;                             // address1 = 0x100, address2 = 0 should
  address2 = 0x200;                             //  also work
  starttime = time (NULL);
  while (blocksleft > 0)
    {
      if (hirom)
        for (n = 0; n < 4; n++)
          {
            ffe_send_command (5, address1, 0);
            ffe_receive_block (0x2000, buffer, BUFFERSIZE);
            address1++;
            fwrite (buffer, 1, BUFFERSIZE, file);

            bytesreceived += BUFFERSIZE;
            ucon64_gauge (starttime, bytesreceived, size);
            ffe_checkabort (2);
          }

      for (n = 0; n < 4; n++)
        {
          ffe_send_command (5, address2, 0);
          ffe_receive_block (0xa000, buffer, BUFFERSIZE);
          blocksleft--;
          address2++;
          fwrite (buffer, 1, BUFFERSIZE, file);

          bytesreceived += BUFFERSIZE;
          ucon64_gauge (starttime, bytesreceived, size);
          ffe_checkabort (2);
        }
    }
  ffe_send_command (5, 0, 0);

  // Create a correct header. We can't obtain the header from the Pro Fighter
  //  unless a (the same) cartridge was just dumped to diskette...
  ucon64.fname = filename;
  ucon64.fsize = size + FIG_HEADER_LEN;
  // override everything we know for sure
  ucon64.console = UCON64_SNES;
  ucon64.backup_header_len = FIG_HEADER_LEN;
  ucon64.split = 0;
  ucon64.snes_hirom = hirom ? SNES_HIROM : 0;
  ucon64.interleaved = 0;
  memset (&rominfo, 0, sizeof (st_ucon64_nfo_t));

  fflush (file);
  snes_init (&rominfo);
  snes_set_fig_header ((st_fig_header_t *) buffer, rominfo.backup_header_len);
  fseek (file, 0, SEEK_SET);
  fwrite (buffer, 1, FIG_HEADER_LEN, file);     // write correct header

  free (buffer);
  fclose (file);

  return 0;
}


int
fig_write_rom (const char *filename, unsigned short parport)
{
  FILE *file;
  unsigned char *buffer, emu_mode_select;
  size_t bytesread = 0, bytessent;
  unsigned int blocksdone = 0, blocksleft, fsize, n;
  unsigned short address1, address2;
  time_t starttime;

  ffe_init_io (parport);

  if ((file = fopen (filename, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], filename);
      exit (1);
    }
  if ((buffer = (unsigned char *) malloc (BUFFERSIZE)) == NULL)
    {
      fprintf (stderr, ucon64_msg[FILE_BUFFER_ERROR], BUFFERSIZE);
      exit (1);
    }

  fsize = (unsigned int) ucon64.fsize;
  printf ("Send: %u Bytes (%.4f Mb)\n", fsize, (float) fsize / MBIT);

  ffe_send_command0 (0xc008, 0);
  fread_checked (buffer, 1, FIG_HEADER_LEN, file);

  if (snes_get_copier_type () == SWC)
    handle_swc_header (buffer);
  emu_mode_select = buffer[2];                  // this byte is needed later

  ffe_send_command (5, 0, 0);
  ffe_send_block (0x400, buffer, FIG_HEADER_LEN); // send header
  bytessent = FIG_HEADER_LEN;

  hirom = snes_get_snes_hirom ();
  if (hirom)
    ffe_send_command0 (0xe00f, 0);              // seems to enable HiROM mode,
                                                //  value doesn't seem to matter
  puts ("Press q to abort\n");                  // print here, NOT before first FIG I/O,
                                                //  because if we get here q works ;-)
  blocksleft = (fsize - FIG_HEADER_LEN + BUFFERSIZE - 1) / BUFFERSIZE; // round up
  address1 = 0x300;
  address2 = 0x200;
  starttime = time (NULL);
  while (blocksleft > 0)
    {
      if (hirom)
        for (n = 0; n < 4; n++)
          {
            bytesread = fread (buffer, 1, BUFFERSIZE, file);
            ffe_send_command0 (0xc010, (unsigned char) (blocksdone >> 9));
            ffe_send_command (5, address1, 0);
            ffe_send_block (0x0000, buffer, (unsigned short) bytesread);
            address1++;
            blocksleft--;
            blocksdone++;

            bytessent += bytesread;
            ucon64_gauge (starttime, bytessent, fsize);
            ffe_checkabort (2);
          }

      for (n = 0; n < 4; n++)
        {
          bytesread = fread (buffer, 1, BUFFERSIZE, file);
          ffe_send_command0 (0xc010, (unsigned char) (blocksdone >> 9));
          ffe_send_command (5, address2, 0);
          ffe_send_block (0x8000, buffer, (unsigned short) bytesread);
          address2++;
          blocksleft--;
          blocksdone++;

          bytessent += bytesread;
          ucon64_gauge (starttime, bytessent, fsize);
          ffe_checkabort (2);
        }
    }

  if (blocksdone > 0x200)                       // ROM dump > 512 8 kB blocks (=32 Mb (=4 MB))
    ffe_send_command0 (0xc010, 2);

  ffe_send_command (5, 0, 0);
  ffe_send_command (6, 1 | (emu_mode_select << 8), 0);

  free (buffer);
  fclose (file);

  return 0;
}


int
fig_read_sram (const char *filename, unsigned short parport)
{
  FILE *file;
  unsigned char *buffer;
  int blocksleft, bytesreceived = 0;
  unsigned short address;
  time_t starttime;

  ffe_init_io (parport);

  if ((file = fopen (filename, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], filename);
      exit (1);
    }
  if ((buffer = (unsigned char *) malloc (BUFFERSIZE)) == NULL)
    {
      fprintf (stderr, ucon64_msg[FILE_BUFFER_ERROR], BUFFERSIZE);
      exit (1);
    }

  printf ("Receive: %d Bytes\n", 32 * 1024);
  memset (buffer, 0, FIG_HEADER_LEN);
#if 0 // not needed for FIG, as size is always 4 blocks
  buffer[0] = 4;                                // 32 kB == 4*8 kB, "size_high" is already 0
#endif
  fwrite (buffer, 1, FIG_HEADER_LEN, file);

  ffe_send_command (5, 0, 0);
  ffe_send_command0 (0xe00d, 0);
  ffe_send_command0 (0xc008, 0);

  puts ("Press q to abort\n");                  // print here, NOT before first FIG I/O,
                                                //  because if we get here q works ;-)
  blocksleft = 4;                               // SRAM is 4*8 kB
  address = 0x100;
  starttime = time (NULL);
  while (blocksleft > 0)
    {
      ffe_send_command (5, address, 0);
      ffe_receive_block (0x2000, buffer, BUFFERSIZE);
      blocksleft--;
      address++;
      fwrite (buffer, 1, BUFFERSIZE, file);

      bytesreceived += BUFFERSIZE;
      ucon64_gauge (starttime, bytesreceived, 32 * 1024);
      ffe_checkabort (2);
    }

  free (buffer);
  fclose (file);

  return 0;
}


int
fig_write_sram (const char *filename, unsigned short parport)
{
  FILE *file;
  unsigned char *buffer;
  size_t bytesread, bytessent = 0;
  unsigned int size;
  unsigned short address;
  time_t starttime;

  ffe_init_io (parport);

  if ((file = fopen (filename, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], filename);
      exit (1);
    }
  if ((buffer = (unsigned char *) malloc (BUFFERSIZE)) == NULL)
    {
      fprintf (stderr, ucon64_msg[FILE_BUFFER_ERROR], BUFFERSIZE);
      exit (1);
    }

  size = (unsigned int) ucon64.fsize - FIG_HEADER_LEN; // FIG SRAM is 4*8 kB, emu SRAM often not
  printf ("Send: %u Bytes\n", size);
  fseek (file, FIG_HEADER_LEN, SEEK_SET);       // skip the header

  ffe_send_command (5, 0, 0);
  ffe_send_command0 (0xe00d, 0);
  ffe_send_command0 (0xc008, 0);

  puts ("Press q to abort\n");                  // print here, NOT before first FIG I/O,
                                                //  because if we get here q works ;-)
  address = 0x100;
  starttime = time (NULL);
  while ((bytesread = fread (buffer, 1, BUFFERSIZE, file)) != 0)
    {
      ffe_send_command (5, address, 0);
      ffe_send_block (0x2000, buffer, (unsigned short) bytesread);
      address++;

      bytessent += bytesread;
      ucon64_gauge (starttime, bytessent, size);
      ffe_checkabort (2);
    }

  free (buffer);
  fclose (file);

  return 0;
}


int
fig_read_cart_sram (const char *filename, unsigned short parport)
{
  FILE *file;
  unsigned char *buffer, byte;
  int bytesreceived = 0, size;
  unsigned short address;
  time_t starttime;

  ffe_init_io (parport);

  if ((file = fopen (filename, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], filename);
      exit (1);
    }
  if ((buffer = (unsigned char *) malloc (BUFFERSIZE)) == NULL)
    {
      fprintf (stderr, ucon64_msg[FILE_BUFFER_ERROR], BUFFERSIZE);
      exit (1);
    }

  size = receive_rom_info (buffer);
  if (size == 0)
    {
      fputs ("ERROR: There is no cartridge present in the Pro Fighter\n", stderr);
      fclose (file);
      remove (filename);
      exit (1);
    }

  ffe_send_command (5, 3, 0);                   // detect cartridge SRAM size because
  ffe_send_command0 (0xe00c, 0);                //  we don't want to read too few data
  byte = ffe_send_command1 (0xbfd8);

  size = MAX ((byte ? 1 << (byte + 10) : 0), 32 * 1024);
  printf ("Receive: %d Bytes\n", size);

  memset (buffer, 0, FIG_HEADER_LEN);
#if 0 // Not needed for FIG, as size is always 4 blocks
  buffer[0] = 4;                                // 32 kB == 4*8 kB, "size_high" is already 0
#endif
  fwrite (buffer, 1, FIG_HEADER_LEN, file);

  ffe_send_command (5, 0, 0);
  ffe_send_command0 (0xe00c, 0);
//  ffe_send_command0 (0xc008, 0);

  puts ("Press q to abort\n");                  // print here, NOT before first FIG I/O,
                                                //  because if we get here q works ;-)
  address = hirom ? 0x2c3 : 0x1c0;

  starttime = time (NULL);
  while (bytesreceived < size)
    {
      ffe_send_command (5, address, 0);
      ffe_receive_block (hirom ? 0x6000 : 0x2000, buffer, BUFFERSIZE);
      fwrite (buffer, 1, BUFFERSIZE, file);
      address += hirom ? 4 : 1;

      bytesreceived += BUFFERSIZE;
      ucon64_gauge (starttime, bytesreceived, size);
      ffe_checkabort (2);
    }

  free (buffer);
  fclose (file);

  return 0;
}


int
fig_write_cart_sram (const char *filename, unsigned short parport)
{
  FILE *file;
  unsigned char *buffer, byte;
  size_t bytesread, bytessent = 0;
  unsigned int size;
  unsigned short address;
  time_t starttime;

  ffe_init_io (parport);

  if ((file = fopen (filename, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], filename);
      exit (1);
    }
  if ((buffer = (unsigned char *) malloc (BUFFERSIZE)) == NULL)
    {
      fprintf (stderr, ucon64_msg[FILE_BUFFER_ERROR], BUFFERSIZE);
      exit (1);
    }

  size = receive_rom_info (buffer);
  if (size == 0)
    {
      fputs ("ERROR: There is no cartridge present in the Pro Fighter\n", stderr);
      fclose (file);
      remove (filename);
      exit (1);
    }

  ffe_send_command (5, 3, 0);                   // detect cartridge SRAM size because we don't
  ffe_send_command0 (0xe00c, 0);                //  want to write more data than necessary
  byte = ffe_send_command1 (0xbfd8);

  size = (unsigned int) ucon64.fsize - FIG_HEADER_LEN; // FIG SRAM is 4*8 kB, emu SRAM often not
  size = MIN ((byte ? 1U << (byte + 10) : 0), size);

  printf ("Send: %u Bytes\n", size);
  fseek (file, FIG_HEADER_LEN, SEEK_SET);       // skip the header

  ffe_send_command (5, 0, 0);
  ffe_send_command0 (0xe00c, 0);
//  ffe_send_command0 (0xc008, 0);

  puts ("Press q to abort\n");                  // print here, NOT before first FIG I/O,
                                                //  because if we get here q works ;-)
  address = hirom ? 0x2c3 : 0x1c0;

  starttime = time (NULL);
  while (bytessent < size &&
         (bytesread = fread (buffer, 1, MIN (size, BUFFERSIZE), file)) != 0)
    {
      ffe_send_command (5, address, 0);
      ffe_send_block (hirom ? 0x6000 : 0x2000, buffer, (unsigned short) bytesread);
      address += hirom ? 4 : 1;

      bytessent += bytesread;
      ucon64_gauge (starttime, bytessent, size);
      ffe_checkabort (2);
    }

  free (buffer);
  fclose (file);

  return 0;
}

#endif // USE_PARALLEL
