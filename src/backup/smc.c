/*
smc.c - Super Magic Card support for uCON64

Copyright (c) 2003, 2019 - 2020 dbjh


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
#include "ucon64.h"
#include "ucon64_misc.h"
#include "backup/ffe.h"
#include "backup/smc.h"


#ifdef  USE_PARALLEL
static st_ucon64_obj_t smc_obj[] =
  {
    {UCON64_NES, WF_DEFAULT | WF_STOP | WF_NO_SPLIT},
    {UCON64_NES, WF_STOP | WF_NO_ROM}
  };
#endif

const st_getopt2_t smc_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "Super Magic Card"/*"1993/1994/1995/19XX Front Far East/FFE http://www.front.com.tw"*/,
      NULL
    },
#ifdef  USE_PARALLEL
    {
      "xsmc", 0, 0, UCON64_XSMC, // send only
      NULL, "send ROM (in FFE format) to Super Magic Card; " OPTION_LONG_S "port" OPTARG_S "PORT",
      &smc_obj[0]
    },
    {
      "xsmcr", 0, 0, UCON64_XSMCR,
      NULL, "send/receive RTS data to/from Super Magic Card; " OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "receives automatically when RTS file does not exist",
      &smc_obj[1]
    },
#endif
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };


#ifdef USE_PARALLEL

#define BUFFERSIZE 8192


static unsigned int get_blocks1 (unsigned char *header);
static unsigned int get_blocks2 (unsigned char *header);
static unsigned int get_blocks3 (unsigned char *header);


unsigned int
get_blocks1 (unsigned char *header)
{
  if (header[7] == 0xaa)
    return header[3];
  if (header[0] & 0x30)
    return header[0] & 0x20 ? 32 : 16;          // 0x10 => 16; 0x20/0x30 => 32
  else
    switch (header[1] >> 5)
      {
      case 0:
      case 4:
        return 16;
      case 1:
      case 2:
      case 3:
        return 32;
      default:                                  // 5/6/7
        return 4;
      }
}


unsigned int
get_blocks2 (unsigned char *header)
{
  if (header[0] & 0x30)
    return header[0] & 0x10 ? 32 : 16;          // 0x10/0x30 => 32; 0x20 => 16
  else
    return 0;
}


unsigned int
get_blocks3 (unsigned char *header)
{
  if (header[7] == 0xaa)
    return header[4];
  if (header[0] & 0x30)
    return 0;
  else
    switch (header[1] >> 5)
      {
      default:                                  // 0/1/2/3
        return 0;
      case 4:
      case 5:
        return 4;
      case 6:
        return 2;
      case 7:
        return 1;
      }
}


int
smc_write_rom (const char *filename, unsigned short parport)
{
  FILE *file;
  unsigned char *buffer;
  size_t bytesread, bytessent;
  unsigned int size, offset, n_blocks1, n_blocks2, n_blocks3, n;
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

  ffe_send_command0 (0x4500, 2);
  ffe_send_command0 (0x42fd, 0x20);
  ffe_send_command0 (0x43fc, 0);

  fread_checked (buffer, 1, SMC_HEADER_LEN, file);

  n_blocks1 = get_blocks1 (buffer);
  n_blocks2 = get_blocks2 (buffer);
  n_blocks3 = get_blocks3 (buffer);

  size = (n_blocks1 + n_blocks2 + n_blocks3) * 8 * 1024 + 8 +
         (buffer[0] & SMC_TRAINER ? 512 : 0);
  printf ("Send: %u Bytes (%.4f Mb)\n", size, (float) size / MBIT);

  ffe_send_block (0x5020, buffer, 8);           // send "file control block"
  bytessent = 8;

  if (buffer[1] >> 5 > 4)
    offset = 12;
  else
    offset = 0;

  if (buffer[0] & SMC_TRAINER)                  // send trainer if present
    {
      fread_checked (buffer, 1, 512, file);
      ffe_send_block (0x600, buffer, 512);
      bytessent += 512;
    }

  puts ("Press q to abort\n");
  starttime = time (NULL);

  for (n = 0; n < n_blocks1; n++)
    {
      ffe_send_command0 (0x4507, (unsigned char) (n + offset));
      if ((bytesread = fread (buffer, 1, BUFFERSIZE, file)) == 0)
        break;
      ffe_send_block (0x6000, buffer, (unsigned short) bytesread);

      bytessent += bytesread;
      ucon64_gauge (starttime, bytessent, size);
      ffe_checkabort (2);
    }

  for (n = 0; n < n_blocks2; n++)
    {
      ffe_send_command0 (0x4507, (unsigned char) (n + 32));
      if ((bytesread = fread (buffer, 1, BUFFERSIZE, file)) == 0)
        break;
      ffe_send_block (0x6000, buffer, (unsigned short) bytesread);

      bytessent += bytesread;
      ucon64_gauge (starttime, bytessent, size);
      ffe_checkabort (2);
    }

  ffe_send_command0 (0x2001, 0);

  for (n = 0; n < n_blocks3; n++)
    {
      if (n == 0)
        {
          ffe_send_command0 (0x4500, 0x22);
          ffe_send_command0 (0x42ff, 0x30);
          if ((bytesread = fread (buffer, 1, BUFFERSIZE, file)) == 0)
            break;
          ffe_send_block (0x6000, buffer, (unsigned short) bytesread);
        }
      else
        {
          int m;

          ffe_send_command0 (0x4500, 7);
          for (m = 0; m < 8; m++)
            ffe_send_command0 ((unsigned short) (0x4510 + m), (unsigned char) (n * 8 + m));
          if ((bytesread = fread (buffer, 1, BUFFERSIZE, file)) == 0)
            break;
          ffe_send_block2 (0, buffer, (unsigned short) bytesread);
        }

      bytessent += bytesread;
      ucon64_gauge (starttime, bytessent, size);
      ffe_checkabort (2);
    }

  for (n = 0x4504; n < 0x4508; n++)
    ffe_send_command0 ((unsigned short) n, 0);
  for (n = 0x4510; n < 0x451c; n++)
    ffe_send_command0 ((unsigned short) n, 0);

  ffe_send_command (5, 1, 0);

  free (buffer);
  fclose (file);

  return 0;
}


int
smc_read_rts (const char *filename, unsigned short parport)
{
  FILE *file;
  unsigned char *buffer;
  unsigned int bytesreceived = 0, size, n;
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

  size = 0x68 + 4 * 1024 + 5 * 8 * 1024;
  printf ("Receive: %u Bytes\n", size);
  memset (buffer, 0, SMC_HEADER_LEN);
  buffer[8] = 0xaa;
  buffer[9] = 0xbb;
  buffer[10] = 1;

  puts ("Press q to abort\n");
  starttime = time (NULL);

  ffe_send_command (5, 3, 0);
  ffe_receive_block (0x5840, buffer + 0x100, 0x68);
  fwrite (buffer, 1, SMC_HEADER_LEN, file);
  bytesreceived += 0x68;

  ffe_send_command0 (0x4500, 0x32);
  ffe_send_command0 (0x42ff, 0x30);
  ffe_receive_block (0x6000, buffer, BUFFERSIZE / 2); // 0x1000
  fwrite (buffer, 1, BUFFERSIZE / 2, file);

  bytesreceived += BUFFERSIZE / 2;
  ucon64_gauge (starttime, bytesreceived, size);
  ffe_checkabort (2);

  for (n = 2; n <= 0x22; n += 0x20)
    {
      ffe_send_command0 (0x4500, (unsigned char) n);
      ffe_receive_block (0x6000, buffer, BUFFERSIZE); // 0x2000
      fwrite (buffer, 1, BUFFERSIZE, file);

      bytesreceived += BUFFERSIZE;
      ucon64_gauge (starttime, bytesreceived, size);
      ffe_checkabort (2);
    }

  for (n = 1; n <= 3; n++)
    {
      ffe_send_command0 (0x43fc, (unsigned char) n);
      if (n == 1)
        ffe_send_command0 (0x2001, 0);
      ffe_receive_block2 (0, buffer, BUFFERSIZE); // 0x2000
      fwrite (buffer, 1, BUFFERSIZE, file);

      bytesreceived += BUFFERSIZE;
      ucon64_gauge (starttime, bytesreceived, size);
      ffe_checkabort (2);
    }

  ffe_send_command0 (0x43fc, 0);
  ffe_send_command0 (0x2001, 0x6b);

  free (buffer);
  fclose (file);

  return 0;
}


int
smc_write_rts (const char *filename, unsigned short parport)
{
  FILE *file;
  unsigned char *buffer;
  unsigned int bytessent = 0, size, n;
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

  size = 0x68 + 4 * 1024 + 5 * 8 * 1024;
  printf ("Send: %u Bytes\n", size);
  fread_checked (buffer, 1, SMC_HEADER_LEN, file);

  puts ("Press q to abort\n");
  starttime = time (NULL);

  ffe_send_command (5, 3, 0);
  ffe_send_block (0x5840, buffer + 0x100, 0x68);
  bytessent += 0x68;

  ffe_send_command0 (0x4500, 0x32);
  ffe_send_command0 (0x42ff, 0x30);
  fread_checked (buffer, 1, BUFFERSIZE / 2, file);
  ffe_send_block (0x6000, buffer, BUFFERSIZE / 2); // 0x1000

  bytessent += BUFFERSIZE / 2;
  ucon64_gauge (starttime, bytessent, size);
  ffe_checkabort (2);

  for (n = 2; n <= 0x22; n += 0x20)
    {
      ffe_send_command0 (0x4500, (unsigned char) n);
      fread_checked (buffer, 1, BUFFERSIZE, file);
      ffe_send_block (0x6000, buffer, BUFFERSIZE); // 0x2000

      bytessent += BUFFERSIZE;
      ucon64_gauge (starttime, bytessent, size);
      ffe_checkabort (2);
    }

  for (n = 1; n <= 3; n++)
    {
      ffe_send_command0 (0x43fc, (unsigned char) n);
      if (n == 1)
        ffe_send_command0 (0x2001, 0);
      fread_checked (buffer, 1, BUFFERSIZE, file);
      ffe_send_block2 (0, buffer, BUFFERSIZE); // 0x2000

      bytessent += BUFFERSIZE;
      ucon64_gauge (starttime, bytessent, size);
      ffe_checkabort (2);
    }

  ffe_send_command0 (0x43fc, 0);
  ffe_send_command0 (0x2001, 0x6b);

  free (buffer);
  fclose (file);

  return 0;
}

#endif // USE_PARALLEL
