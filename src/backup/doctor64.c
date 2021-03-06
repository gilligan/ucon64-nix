/*
doctor64.c - Bung Doctor V64 support for uCON64

Copyright (c) 1999 - 2001             NoisyB
Copyright (c) 2015, 2017 - 2018, 2020 dbjh


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
#endif
#include <stdlib.h>
#include <string.h>
#include "misc/archive.h"
#include "misc/file.h"
#include "ucon64.h"
#include "ucon64_misc.h"
#include "backup/doctor64.h"


#ifdef  USE_PARALLEL
static st_ucon64_obj_t doctor64_obj[] =
  {
    {UCON64_N64, WF_DEFAULT | WF_STOP | WF_NO_ROM}
  };
#endif

const st_getopt2_t doctor64_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "Doctor V64"/*"19XX Bung Enterprises Ltd http://www.bung.com.hk"*/,
      NULL
    },
#ifdef  USE_PARALLEL
    {
      "xv64", 0, 0, UCON64_XV64,
      NULL, "send/receive ROM to/from Doctor V64; " OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "receives automatically when ROM does not exist",
      &doctor64_obj[0]
    },
#endif
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };


#ifdef USE_PARALLEL

#define SYNC_MAX_CNT 8192
#define SYNC_MAX_TRY 32
#define SEND_MAX_WAIT 0x300000
#define REC_HIGH_NIBBLE 0x80
#define REC_LOW_NIBBLE 0x00
#define REC_MAX_WAIT SEND_MAX_WAIT

#define STOCKPPDEV_MSG "WARNING: This will not work with a stock ppdev on PC. See the FAQ, question 55"


static int
parport_write (char src[], size_t len, unsigned short parport)
{
  size_t i;

  for (i = 0; i < len; i++)
    {
      int maxwait = SEND_MAX_WAIT;

      if ((inportb (parport + 2) & 1) == 0)     // check ~strobe
        {
          while (((inportb (parport + 2) & 2) != 0) && maxwait--)
            ;                                   // wait for
          if (maxwait <= 0)
            return 1;                           // auto feed == 0
          outportb (parport, src[i]);           // write data
          outportb (parport + 2, 5);            // ~strobe = 1
        }
      else
        {
          while (((inportb (parport + 2) & 2) == 0) && maxwait--)
            ;                                   // wait for
          if (maxwait <= 0)
            return 1;                           // auto feed == 1
          outportb (parport, src[i]);           // write data
          outportb (parport + 2, 4);            // ~strobe = 0
        }
    }
  return 0;
}


static size_t
parport_read (char dest[], size_t len, unsigned short parport)
{
  size_t i;

  for (i = 0; i < len; i++)
    {
      int maxwait = REC_MAX_WAIT;
      unsigned char c;

      outportb (parport, REC_HIGH_NIBBLE);
      while (((inportb (parport + 1) & 0x80) == 0) && maxwait--)
        ;                                       // wait for ~busy=1
      if (maxwait <= 0)
        return len - i;
      c = (inportb (parport + 1) >> 3) & 0x0f;  // ~ack, pe, slct, ~error

      outportb (parport, REC_LOW_NIBBLE);
      maxwait = REC_MAX_WAIT;
      while (((inportb (parport + 1) & 0x80) != 0) && maxwait--)
        ;                                       // wait for ~busy=0
      if (maxwait <= 0)
        return len - i;
      c |= (inportb (parport + 1) << 1) & 0xf0; // ~ack, pe, slct, ~error

      dest[i] = c;
    }
  outportb (parport, REC_HIGH_NIBBLE);
  return 0;
}


static int
syncHeader (unsigned short baseport)
{
  int i = 0;

  outportb (baseport, 0);                       // data = 00000000
  outportb (baseport + 2, 4);                   // ~strobe=0
  while (i < SYNC_MAX_CNT)
    {
      if ((inportb (baseport + 2) & 8) == 0)    // wait for select=0
        {
          outportb (baseport, 0xaa);            // data = 10101010
          outportb (baseport + 2, 0);           // ~strobe=0, ~init=0
          while (i < SYNC_MAX_CNT)
            {
              if ((inportb (baseport + 2) & 8) != 0) // wait for select=1
                {
                  outportb (baseport + 2, 4);   // ~strobe=0
                  while (i < SYNC_MAX_CNT)
                    {
                      if ((inportb (baseport + 2) & 8) == 0) // w for select=0
                        {
                          outportb (baseport, 0x55); // data = 01010101
                          outportb (baseport + 2, 0); // ~strobe=0, ~init=0
                          while (i < SYNC_MAX_CNT)
                            {
                              if ((inportb (baseport + 2) & 8) != 0) // w select=1
                                {
                                  outportb (baseport + 2, 4); // ~strobe=0
                                  while (i < SYNC_MAX_CNT)
                                    {
                                      if ((inportb (baseport + 2) & 8) == 0) // select=0
                                        return 0;
                                      i++;
                                    }
                                }
                              i++;
                            }
                        }
                      i++;
                    }
                }
              i++;
            }
          i++;
        }
      i++;
    }
  outportb (baseport + 2, 4);
  return 1;
}


static int
initCommunication (unsigned short port)
{
  int i;

  for (i = 0; i < SYNC_MAX_TRY; i++)
    {
      if (syncHeader (port) == 0)
        break;
    }
  if (i >= SYNC_MAX_TRY)
    return -1;
  return 0;
}


static int
checkSync (unsigned short baseport)
{
  int i, j;

  for (i = 0; i < SYNC_MAX_CNT; i++)
    {
      if (((inportb (baseport + 2) & 3) == 3)
          || ((inportb (baseport + 2) & 3) == 0))
        {
          outportb (baseport, 0);               // ~strobe, auto feed
          for (j = 0; j < SYNC_MAX_CNT; j++)
            {
              if ((inportb (baseport + 1) & 0x80) == 0) // wait for ~busy=0
                {
                  return 0;
                }
            }
          return 1;
        }
    }
  return 1;
}


static int
sendFilename (unsigned short baseport, char name[])
{
  int i;
  char *c = strrchr(name, DIR_SEPARATOR), mname[11];

  memset (mname, ' ', 11);
  if (c == NULL)
    c = name;
  else
    c++;
  for (i = 0; i < 8 && *c != '.' && *c != '\0'; i++, c++)
    mname[i] = (char) toupper ((int) *c);
  if ((c = strrchr(c, '.')) != NULL)
    {
      c++;
      for (i = 8; i < 11 && *c != '\0'; i++, c++)
        mname[i] = (char) toupper ((int) *c);
    }

  return parport_write (mname, 11, baseport);
}


static int
sendUploadHeader (unsigned short baseport, char name[], int len)
{
  char lenbuffer[4], protocolId[] = "GD6R\1";

  if (parport_write (protocolId, strlen (protocolId), baseport) != 0)
    return 1;

  lenbuffer[0] = (char) len;
  lenbuffer[1] = (char) (len >> 8);
  lenbuffer[2] = (char) (len >> 16);
  lenbuffer[3] = (char) (len >> 24);
  if (parport_write (lenbuffer, 4, baseport) != 0)
    return 1;

  if (sendFilename (baseport, name) != 0)
    return 1;
  return 0;
}


static int
sendDownloadHeader (unsigned short baseport, int *len)
{
  char mname[11], protocolId[] = "GD6W";
  unsigned char recbuffer[15];

  if (parport_write (protocolId, strlen (protocolId), baseport) != 0)
    return 1;
  memset (mname, ' ', 11);
  if (parport_write (mname, 11, baseport) != 0)
    return 1;
  if (checkSync (baseport) != 0)
    return 1;

  if (parport_read ((char *) recbuffer, 1, baseport) != 0)
    return 1;
  if (recbuffer[0] != 1)
    return -1;
  if (parport_read ((char *) recbuffer, 15, baseport) != 0)
    return 1;
  *len = (int) recbuffer[0] |
         ((int) recbuffer[1] << 8) |
         ((int) recbuffer[2] << 16) |
         ((int) recbuffer[3] << 24);
  return 0;
}


int
doctor64_read (const char *filename, unsigned short parport)
{
  char buf[MAXBUFSIZE];
  FILE *fh;
  int size, bytesreceived = 0;
  time_t init_time;

#ifdef  USE_PPDEV
  puts (STOCKPPDEV_MSG);
#endif

  parport_print_info ();
  if (initCommunication (parport) == -1)
    {
      fputs (ucon64_msg[PARPORT_ERROR], stderr);
      exit (1);
    }

  init_time = time (NULL);

  if (sendDownloadHeader (parport, &size) != 0)
    {
      fputs (ucon64_msg[PARPORT_ERROR], stderr);
      exit (1);
    }
  if ((fh = fopen (filename, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], filename);
      exit (1);
    }
  printf ("Receive: %d Bytes (%.4f Mb)\n\n", size, (float) size / MBIT);

  for (;;)
    {
      if (parport_read (buf, sizeof buf, parport) != 0)
        break;
      bytesreceived += sizeof buf;
      fwrite (buf, 1, sizeof buf, fh);
      ucon64_gauge (init_time, bytesreceived, size);
    }
  fclose (fh);
  return 0;
}


int
doctor64_write (const char *filename, int start, int len, unsigned short parport)
{
  char buf[MAXBUFSIZE];
  FILE *fh;
  unsigned int size;
  size_t bytessent = 0;
  time_t init_time;

#ifdef  USE_PPDEV
  puts (STOCKPPDEV_MSG);
#endif

  parport_print_info ();
  size = len - start;
  if (initCommunication (parport) == -1)
    {
      fputs (ucon64_msg[PARPORT_ERROR], stderr);
      exit (1);
    }
  init_time = time (NULL);

  strcpy (buf, filename);
  if (sendUploadHeader (parport, buf, size) != 0)
    {
      fputs (ucon64_msg[PARPORT_ERROR], stderr);
      exit (1);
    }

  if ((fh = fopen (filename, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], filename);
      exit (1);
    }

  printf ("Send: %u Bytes (%.4f Mb)\n\n", size, (float) size / MBIT);

  for (;;)
    {
      size_t pos;

      if ((pos = fread (buf, 1, sizeof buf, fh)) == 0)
        break;
      if (parport_write (buf, pos, parport) != 0)
        break;
      bytessent += pos;
      ucon64_gauge (init_time, bytessent, size);
    }
  fclose (fh);
  return 0;
}

#endif // USE_PARALLEL
