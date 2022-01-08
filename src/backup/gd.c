/*
gd.c - Game Doctor support for uCON64

Copyright (c) 2002 - 2003              John Weidman
Copyright (c) 2002 - 2006, 2015 - 2021 dbjh


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
#include "misc/archive.h"
#include "misc/file.h"
#include "misc/misc.h"
#include "misc/parallel.h"
#include "misc/property.h"
#include "misc/string.h"
#include "misc/term.h"
#include "ucon64_misc.h"
#include "console/snes.h"                       // for snes_gd_make_names() &
#include "backup/gd.h"                          //  snes_get_snes_hirom()


#ifdef  USE_PARALLEL
static st_ucon64_obj_t gd_obj[] =
  {
    {UCON64_SNES, WF_DEFAULT | WF_STOP | WF_NO_ROM},
    {UCON64_SNES, WF_STOP | WF_NO_ROM}
  };
#endif

const st_getopt2_t gd_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "Game Doctor SF3(SF6/SF7)/Professor SF(SF 2)"/*"19XX Bung Enterprises Ltd http://www.bung.com.hk"*/,
      NULL
    },
#ifdef  USE_PARALLEL
    {
      "xgd3", 0, 0, UCON64_XGD3, // supports split files
      NULL, "send ROM to Game Doctor SF3/SF6/SF7; " OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "this option uses the Game Doctor SF3 protocol",
      &gd_obj[0]
    },
    {
      "xgd6", 0, 0, UCON64_XGD6, // supports split files
      NULL, "send/receive ROM to/from Game Doctor SF6/SF7; " OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "receives automatically when ROM does not exist\n"
      "this option uses the Game Doctor SF6 protocol",
      &gd_obj[0]
    },
    {
      "xgd3s", 0, 0, UCON64_XGD3S,
      NULL, "send SRAM to Game Doctor SF3/SF6/SF7; " OPTION_LONG_S "port" OPTARG_S "PORT",
      &gd_obj[1]
    },
    {
      "xgd6s", 0, 0, UCON64_XGD6S,
      NULL, "send/receive SRAM to/from Game Doctor SF6/SF7; " OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "receives automatically when SRAM does not exist",
      &gd_obj[1]
    },
    {
      "xgd3r", 0, 0, UCON64_XGD3R,
      NULL, "send saver (RTS) data to Game Doctor SF3/SF6/SF7;" OPTION_LONG_S "port" OPTARG_S "PORT",
      &gd_obj[1]
    },
    {
      "xgd6r", 0, 0, UCON64_XGD6R,
      NULL, "send/receive saver (RTS) data to/from Game Doctor SF6/SF7;\n" OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "receives automatically when saver file does not exist",
      &gd_obj[1]
    },
#endif // USE_PARALLEL
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };


#ifdef  USE_PARALLEL

#define BUFFERSIZE 8192
#define GD_OK 0
#define GD_ERROR 1
#define GD3_PROLOG_STRING "DSF3"
#define GD6_READ_PROLOG_STRING "GD6R"           // GD reading, PC writing
#define GD6_WRITE_PROLOG_STRING "GD6W"          // GD writing, PC reading
#define GD6_TIMEOUT_ATTEMPTS 0x4000
#define GD6_RX_SYNC_TIMEOUT_ATTEMPTS 0x2000
#define GD6_SYNC_RETRIES 16

#define STOCKPPDEV_MSG "WARNING: This will not work with a stock ppdev on PC. See the FAQ, question 55"

typedef enum { ROM, SRAM, SAVER } data_t;

#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
typedef struct st_add_filename_data
{
  unsigned int index;
  char **names;
} st_add_filename_data_t;
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

static void deinit_io (void);
static int gd6_read_data (const char *filename, unsigned short parport,
                          data_t data_type);
static int gd_write_rom (const char *filename, unsigned short parport,
                         st_ucon64_nfo_t *rominfo, const char *prolog_str);
static int gd_write_sram (const char *filename, unsigned short parport,
                          const char *prolog_str);
static int gd_write_saver (const char *filename, unsigned short parport,
                           const char *prolog_str);


static int (*gd_send_prolog_bytes) (unsigned char *data, unsigned int len);
static int (*gd_send_bytes) (unsigned char *data, unsigned int len);
static unsigned short gd_port;
static unsigned int gd6_send_byte_delay, gd_bytessent, gd_fsize, gd_header_len;
static time_t gd_starttime;
static char gd_destfname[FILENAME_MAX] = "";
static FILE *gd_destfile;


static void
init_io (unsigned short port)
{
  gd_port = port;

#if     (defined __unix__ && !defined __MSDOS__) || defined __BEOS__
  init_conio ();
#endif
  if (register_func (deinit_io) == -1)
    {
      fputs ("ERROR: Could not register function with register_func()\n", stderr);
      exit (1);
    }

  parport_print_info ();

  gd6_send_byte_delay = get_property_int (ucon64.configfile, "gd6_send_byte_delay");
}


static void
deinit_io (void)
{
  // put possible transfer cleanup stuff here
#if     (defined __unix__ && !defined __MSDOS__) || defined __BEOS__
  deinit_conio ();
#endif
}


static void
io_error (const char *func_name)
{
  fflush (stdout);
  fprintf (stderr,
           "ERROR: Communication with Game Doctor failed in function %s\n",
           func_name);
  fflush (stderr);
  // calling fflush() seems to be necessary in Msys in order to make the error
  //  message be displayed before the "Removing <filename>" message
  exit (1);
}


static void
gd_checkabort (int status)
{
  if ((!ucon64.frontend ? kbhit () : 0) && getch () == 'q')
    {
      puts ("\nProgram aborted");
      exit (status);
    }
}


static void
remove_destfile (void)
{
  if (gd_destfname[0])
    {
      printf ("Removing %s\n", gd_destfname);
      fclose (gd_destfile);
      remove (gd_destfname);
      gd_destfname[0] = '\0';
    }
}


static int
gd3_send_prolog_byte (unsigned char data)
/*
  Prolog specific data output routine.
  We could probably get away with using the general routine but the
  transfer program I (JW) traced to analyze the protocol did this for
  the bytes used to set up the transfer so here it is.
*/
{
  // wait until SF3 is not busy
  do
    {
      if ((inportb (gd_port + PARPORT_STATUS) & 0x08) == 0)
        return GD_ERROR;
    }
  while ((inportb (gd_port + PARPORT_STATUS) & 0x80) == 0);

  outportb (gd_port + PARPORT_DATA, data);      // set data
  outportb (gd_port + PARPORT_CONTROL, 5);      // clock data out to SF3
#if 0
  inportb (gd_port + PARPORT_CONTROL);          // let data "settle down"
#endif
  outportb (gd_port + PARPORT_CONTROL, 4);

  return GD_OK;
}


static int
gd3_send_prolog_bytes (unsigned char *data, unsigned int len)
{
  unsigned int i;

  for (i = 0; i < len; i++)
    if (gd3_send_prolog_byte (data[i]) != GD_OK)
      return GD_ERROR;
  return GD_OK;
}


static void
gd3_send_byte (unsigned char data)
/*
  General data output routine.
  Use this routine for sending ROM data bytes to the Game Doctor SF3 (SF6/SF7
  too).
*/
{
  // wait until SF3 is not busy
  while ((inportb (gd_port + PARPORT_STATUS) & 0x80) == 0)
    ;

  outportb (gd_port + PARPORT_DATA, data);      // set data
  outportb (gd_port + PARPORT_CONTROL, 5);      // clock data out to SF3
#if 0
  inportb (gd_port + PARPORT_CONTROL);          // let data "settle down"
#endif
  outportb (gd_port + PARPORT_CONTROL, 4);
}


static int
gd3_send_bytes (unsigned char *data, unsigned int len)
{
  unsigned int i;

  for (i = 0; i < len; i++)
    {
      gd3_send_byte (data[i]);
      gd_bytessent++;
      if ((gd_bytessent - gd_header_len) % BUFFERSIZE == 0)
        {
          ucon64_gauge (gd_starttime, gd_bytessent, gd_fsize);
          gd_checkabort (2);                    // 2 to return something other than 1
        }
    }
  return GD_OK;
}


static inline void
microwait2 (unsigned int nmicros)
{
#if     (defined __unix__ && !defined __MSDOS__) || defined __APPLE__ || \
        defined __BEOS__ || defined _WIN32
  microwait (nmicros);
#elif   defined __i386__
  /*
    On x86, the in and out instructions are slow by themselves (without actual
    hardware mapped), even on modern hardware. I have read claims of about 1
    microsecond (for both in and out) and a rough test performed by myself
    suggested around 600 nanoseconds for inb for an unmapped address.
  */
  unsigned int n;

  for (n = 0; n < nmicros * 2; n++)
    inportb (gd_port + PARPORT_STATUS);
#endif
}


static int
gd6_sync_hardware (void)
// sets the SF7 up for an SF6/SF7 protocol transfer
{
  unsigned int retries;
  volatile int delay;

  for (retries = GD6_SYNC_RETRIES; retries > 0; retries--)
    {
      unsigned int timeout = GD6_TIMEOUT_ATTEMPTS;

      outportb (gd_port + PARPORT_CONTROL, 4);
      outportb (gd_port + PARPORT_DATA, 0);
      outportb (gd_port + PARPORT_CONTROL, 4);

      for (delay = 0x1000; delay > 0; delay--)  // a delay may not be necessary here
        ;

      outportb (gd_port + PARPORT_DATA, 0xaa);
      outportb (gd_port + PARPORT_CONTROL, 0);

      if (gd6_send_byte_delay)
        microwait2 (2000);
      else
        {
          while ((inportb (gd_port + PARPORT_CONTROL) & 8) == 0)
            if (--timeout == 0)
              break;
          if (timeout == 0)
            continue;
        }

      outportb (gd_port + PARPORT_CONTROL, 4);

      if (gd6_send_byte_delay)
        microwait2 (2000);
      else
        {
          while ((inportb (gd_port + PARPORT_CONTROL) & 8) != 0)
            if (--timeout == 0)
              break;
          if (timeout == 0)
            continue;
        }

      outportb (gd_port + PARPORT_DATA, 0x55);
      outportb (gd_port + PARPORT_CONTROL, 0);

      if (gd6_send_byte_delay)
        microwait2 (2000);
      else
        {
          while ((inportb (gd_port + PARPORT_CONTROL) & 8) == 0)
            if (--timeout == 0)
              break;
          if (timeout == 0)
            continue;
        }

      outportb (gd_port + PARPORT_CONTROL, 4);

      if (gd6_send_byte_delay)
        microwait2 (2000);
      else
        {
          while ((inportb (gd_port + PARPORT_CONTROL) & 8) != 0)
            if (--timeout == 0)
              break;
          if (timeout == 0)
            continue;
        }

      return GD_OK;
    }
  return GD_ERROR;
}


static int
gd6_sync_receive_start (void)
// sync with the start of the received data
{
  unsigned int timeout = GD6_RX_SYNC_TIMEOUT_ATTEMPTS;

  if (gd6_send_byte_delay)
    microwait2 (2000);
  else
    while (((inportb (gd_port + PARPORT_CONTROL) & 3) != 3) &&
           ((inportb (gd_port + PARPORT_CONTROL) & 3) != 0))
      if (--timeout == 0)
        return GD_ERROR;

  outportb (gd_port + PARPORT_DATA, 0);

  timeout = GD6_RX_SYNC_TIMEOUT_ATTEMPTS;
  while ((inportb (gd_port + PARPORT_STATUS) & 0x80) != 0)
    if (--timeout == 0)
      return GD_ERROR;

  return GD_OK;
}


static inline int
gd6_send_byte (unsigned char data)
{
  static unsigned char send_toggle = 0;
  unsigned int timeout = 0x1e0000;

  if (gd6_send_byte_delay)
    microwait2 (gd6_send_byte_delay);
  else
    while (((inportb (gd_port + PARPORT_CONTROL) >> 1) & 1) != send_toggle)
      if (--timeout == 0)
        return GD_ERROR;

  outportb (gd_port + PARPORT_DATA, data);
  send_toggle ^= 1;
  outportb (gd_port + PARPORT_CONTROL, 4 | send_toggle);

  return GD_OK;
}


static int
gd6_send_prolog_bytes (unsigned char *data, unsigned int len)
{
  unsigned int i;

  for (i = 0; i < len; i++)
    if (gd6_send_byte (data[i]) != GD_OK)
      return GD_ERROR;
  return GD_OK;
}


static int
gd6_send_bytes (unsigned char *data, unsigned int len)
{
  unsigned int i;

  for (i = 0; i < len; i++)
    {
      if (gd6_send_byte (data[i]) != GD_OK)
        return GD_ERROR;
      gd_bytessent++;
      if ((gd_bytessent - gd_header_len) % BUFFERSIZE == 0)
        {
          ucon64_gauge (gd_starttime, gd_bytessent, gd_fsize);
          gd_checkabort (2);                    // 2 to return something other than 1
        }
    }
  return GD_OK;
}


static int
gd6_receive_bytes (unsigned char *buffer, unsigned int len)
{
  unsigned int i, timeout = 0x1e0000;

  outportb (gd_port + PARPORT_DATA, 0x80);      // signal the SF6/SF7 to send the next nibble
  for (i = 0; i < len; i++)
    {
      while ((inportb (gd_port + PARPORT_STATUS) & 0x80) == 0)
        if (--timeout == 0)
          return GD_ERROR;

      buffer[i] = (inportb (gd_port + PARPORT_STATUS) >> 3) & 0x0f;
      outportb (gd_port + PARPORT_DATA, 0);     // signal the SF6/SF7 to send the next nibble

      while ((inportb (gd_port + PARPORT_STATUS) & 0x80) != 0)
        if (--timeout == 0)
          return GD_ERROR;

      buffer[i] |= (inportb (gd_port + PARPORT_STATUS) << 1) & 0xf0;
      outportb (gd_port + PARPORT_DATA, 0x80);
    }
  return GD_OK;
}


static void
gd_add_filename (const char *filename, void *cb_data)
{
  st_add_filename_data_t *add_filename_data = (st_add_filename_data_t *) cb_data;

  if (add_filename_data->index < GD3_MAX_UNITS)
    {
      char buf[12], *p;

      strncpy (buf, basename2 (filename), 11)[11] = '\0';
      p = strrchr (buf, '.');
      if (p)
        *p = '\0';
      strcpy (add_filename_data->names[add_filename_data->index], buf);
      add_filename_data->index++;
    }
}


int
gd3_read_rom (const char *filename, unsigned short parport)
{
  (void) filename;                              // warning remover
  (void) parport;                               // warning remover
  fflush (stdout);
  fputs ("ERROR: The function for dumping a cartridge is not yet implemented for the SF3", stderr);
  fflush (stderr);
  return -1;
}


int
gd6_read_rom (const char *filename, unsigned short parport)
{
  return gd6_read_data (filename, parport, ROM);
}


static int
gd6_read_data (const char *filename, unsigned short parport, data_t data_type)
{
  FILE *file;
  unsigned char *buffer, *name;
  unsigned int retry = 0, num_units, i, size, total_size, bytesreceived = 0;
  time_t starttime;

  init_io (parport);

#ifdef  USE_PPDEV
  if (!gd6_send_byte_delay)
    puts (STOCKPPDEV_MSG);
#endif

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

  // be nice to the user and automatically remove the file on an error (or abortion)
  strcpy (gd_destfname, filename);
  gd_destfile = file;
  register_func (remove_destfile);

  /*
    The BRAM (SRAM) filename doesn't have to exactly match any game loaded in
    the SF7. It needs to match any valid Game Doctor filename AND have an
    extension of .B## (where # is a digit from 0-9).

    We could make a GD filename from the real one, but a valid dummy name seems
    to work OK here. For saver data, you must have the proper game selected in
    the SF7 menu even if the real name is used.

    Dumping a cartridge (over a parallel port connection) consists of 2 steps:
    1. Copy the ROM to SF7 memory, by selecting "Back Up Card" under "Utility"
       in the SF7 menu.
    2. Reset the SNES in order to enter link mode and select the game you want
       to dump.
    So, it is only possible to dump previously loaded games, independent of any
    game cartridge.
  */
  switch (data_type)
    {
    case ROM:
      name = (unsigned char *) "SF16497    ";
      break;
    default:
    case SRAM:
      name = (unsigned char *) "SF16497 B00";
      break;
    case SAVER:
      name = (unsigned char *) "SF16497 S00";
      break;
    }

  for (;;)
    {
      if (gd6_sync_hardware () == GD_ERROR)
        {
          if (retry)
            fputc ('\n', stdout);
          io_error ("gd6_sync_hardware()");
        }
      if (gd6_send_prolog_bytes ((unsigned char *) GD6_WRITE_PROLOG_STRING, 4) == GD_ERROR)
        {
          if (retry)
            fputc ('\n', stdout);
          io_error ("gd6_send_prolog_bytes(4)");
        }

      if (gd6_send_prolog_bytes (name, 11) == GD_ERROR)
        {
          if (retry)
            fputc ('\n', stdout);
          io_error ("gd6_send_prolog_bytes(11)");
        }

      /*
        On Raspberry Pi 3B+, when microwait() was implemented using
        clock_nanosleep(CLOCK_MONOTONIC, ...) -xgd6s would successfully dump
        SRAM data only the first time after booting. After microwait() was
        implemented using only clock_gettime(CLOCK_MONOTONIC, ...) in order to
        improve its accuracy, it would never work. It worked again after adding
        calls to clock_nanosleep() during the transfer.
        On Raspberry Pi 2B the behavior is different in that only resetting the
        SNES was usually enough to be able to dump SRAM data again after having
        used -xgd6s once. However, sometimes resetting the SNES would no longer
        be enough and a reboot would be required. Another difference is that
        calling clock_nanosleep() during the transfer made no difference in the
        success rate of -xgd6s.
        On both Raspberry models first uploading either ROM or SRAM data using
        -xgd3 or -xgd3s respectively, would make dumping SRAM data with -xgd6s
        work again, once, provided there would be a delay of at least 3 seconds
        before dumping.
        Ultimately, the solution proved to be restarting the transfer after the
        first call to gd6_receive_bytes() or the call to
        gd6_sync_receive_start() fails. This makes -xgd6s work at least as well
        as after first uploading data, if not better. It can easily take 10
        retries before a transfer succeeds.
        The root cause of all this is the fact that the pi-parport does not
        support reading from the Control register. Because of that we have no
        way to synchronize with the GD when sending data to it. This directly
        results in us being unable to detect time-outs during communication
        caused by errors detected by the GD. Fortunately, gd6_receive_bytes()
        uses another way of synchronizing with the GD. If the first call to
        gd6_receive_bytes() succeeds transferring the actual SRAM data usually
        also succeeds. Synchronization errors can be detected in
        gd6_sync_receive_start(), but they are much more often detected in
        gd6_receive_bytes().
      */
      if (gd6_sync_receive_start () == GD_ERROR)
        {
          if (retry < GD6_SYNC_RETRIES)
            {
              retry++;
              printf ("\rWARNING: Failed to synchronize with Game Doctor. Retrying... (%u/%u)",
                      retry, GD6_SYNC_RETRIES);
              fflush (stdout);
              continue;
            }
          else
            {
              fputc ('\n', stdout);
              io_error ("gd6_sync_receive_start()");
            }
        }

      if (gd6_receive_bytes (buffer, 16) != GD_ERROR)
        {
          if (retry)
            fputc ('\n', stdout);
          break;
        }
      else if (retry < GD6_SYNC_RETRIES)
        {
          retry++;
          printf ("\rWARNING: Failed to synchronize with Game Doctor. Retrying... (%u/%u)",
                  retry, GD6_SYNC_RETRIES);
          fflush (stdout);
        }
      else
        {
          fputc ('\n', stdout);
          io_error ("gd6_receive_bytes(16)");
        }
    }

  num_units = buffer[0];
  size = buffer[1] | (buffer[2] << 8) | (buffer[3] << 16) | (buffer[4] << 24);
  {
    if (size == 0)
      {
        fprintf (stderr, "ERROR: %s transfer size from Game Doctor is 0 bytes\n",
                 data_type == ROM ? "ROM" : data_type == SRAM ? "SRAM" : "Saver");
        exit (1);
      }
    if (data_type == SRAM || data_type == SAVER)
      {
        unsigned int expected_size = data_type == SRAM ? 0x8000 : 0x38000;

        if (size > expected_size)
          {
            fprintf (stderr,
                     "ERROR: %s transfer size from Game Doctor > 0x%x bytes (0x%x bytes)\n",
                     data_type == SRAM ? "SRAM" : "Saver", expected_size, size);
            exit (1);
          }
        else if (size < expected_size)
          printf ("WARNING: %s transfer size from Game Doctor < 0x%x bytes (0x%x bytes)\n",
                  data_type == SRAM ? "SRAM" : "Saver", expected_size, size);

        total_size = num_units * size;          // num_units should be 1
      }
    else // data_type == ROM
      total_size = num_units * (size - GD_HEADER_LEN) + GD_HEADER_LEN; // this may be inaccurate
  }

  printf ("Receive: %u Bytes\n"
          "Press q to abort\n\n", total_size);

  starttime = time (NULL);
  for (i = 0; i < num_units; i++)
    {
      // size will be adjusted as we receive the next unit contents, see below
      while (bytesreceived < size)
        {
          unsigned int len = size >= bytesreceived + BUFFERSIZE ?
                               BUFFERSIZE : size - bytesreceived;

          if (gd6_receive_bytes (buffer, len) == GD_ERROR)
            {
              if (bytesreceived)
                fputc ('\n', stdout);
              io_error ("gd6_receive_bytes()");
            }
          fwrite (buffer, 1, len, file);

          bytesreceived += len;
          ucon64_gauge (starttime, bytesreceived, total_size);
          gd_checkabort (2);
        }
      if (i < num_units - 1)
        {
          if (gd6_receive_bytes (buffer, 15) == GD_ERROR)
            {
              fputc ('\n', stdout);
              io_error ("gd6_receive_bytes(15)");
            }
          size += buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
        }
    }

  unregister_func (remove_destfile);
  free (buffer);
  fclose (file);

  return 0;
}


int
gd3_write_rom (const char *filename, unsigned short parport, st_ucon64_nfo_t *rominfo)
{
  gd_send_prolog_bytes = gd3_send_prolog_bytes;
  gd_send_bytes = gd3_send_bytes;

  return gd_write_rom (filename, parport, rominfo, GD3_PROLOG_STRING);
}


int
gd6_write_rom (const char *filename, unsigned short parport, st_ucon64_nfo_t *rominfo)
{
  gd_send_prolog_bytes = gd6_send_prolog_bytes;
  gd_send_bytes = gd6_send_bytes;

  return gd_write_rom (filename, parport, rominfo, GD6_READ_PROLOG_STRING);
}


/*
  NOTE: On most Game Doctors the way you enter link mode to be able to upload
        the ROM to the unit is to hold down the R key on the controller while
        resetting the SNES. You will see the Game Doctor menu has a message that
        says "LINKING..".
  My Game Doctor SF7 V7.11 enters link mode automatically if it is connected to
  a parallel port -- no controller input is required. Holding down R while
  _starting_ the SNES prevents the Game Doctor from starting the first game that
  has already been uploaded. - dbjh
*/
static int
gd_write_rom (const char *filename, unsigned short parport, st_ucon64_nfo_t *rominfo,
              const char *prolog_str)
{
  FILE *file = NULL;
  unsigned char *buffer;
  char *names[GD3_MAX_UNITS], names_mem[GD3_MAX_UNITS][12] = { { 0 } },
       *filenames[GD3_MAX_UNITS], dir[FILENAME_MAX];
  unsigned int part_sizes[GD3_MAX_UNITS], num_units, i,
               hirom = snes_get_snes_hirom (), filename_len,
               gd6_protocol = !memcmp (prolog_str, GD6_READ_PROLOG_STRING, 4);
  st_add_filename_data_t add_filename_data = { 0, NULL };

  init_io (parport);

#ifdef  USE_PPDEV
  if (gd6_protocol && !gd6_send_byte_delay)
    puts (STOCKPPDEV_MSG);
#endif

  // we don't want to malloc() ridiculously small chunks (of 12 bytes)
  for (i = 0; i < GD3_MAX_UNITS; i++)
    names[i] = names_mem[i];
  add_filename_data.names = names;

  if (ucon64.split)                             // snes_init() sets ucon64.split
    num_units = ucon64_testsplit (filename, gd_add_filename, &add_filename_data);
  else
    num_units = snes_gd_make_names (filename, rominfo->backup_header_len, names);

  dirname2 (filename, dir);
  filename_len = (unsigned int) (strlen (dir) + strlen (names[0]) + 6); // file sep., suffix, ASCII-z => 6
  gd_header_len = rominfo->backup_header_len;
  for (i = 0; i < num_units; i++)
    {
      if ((filenames[i] = (char *) malloc (filename_len)) == NULL)
        {
          fprintf (stderr, ucon64_msg[BUFFER_ERROR], filename_len);
          exit (1);
        }
      sprintf (filenames[i], "%s" DIR_SEPARATOR_S "%s.078", dir, names[i]); // should match with what code of -s does

      if (ucon64.split)
        {
          unsigned int x = (unsigned int) fsizeof (filenames[i]);
          gd_fsize += x;
          part_sizes[i] = x;
        }
      else
        {
          if (!gd_fsize)
            gd_fsize = (unsigned int) ucon64.fsize;
          if (hirom)
            part_sizes[i] = (gd_fsize - gd_header_len) / num_units;
          else
            {
              if ((i + 1) * 8 * MBIT <= gd_fsize - gd_header_len)
                part_sizes[i] = 8 * MBIT;
              else
                part_sizes[i] = gd_fsize - gd_header_len - i * 8 * MBIT;
            }
          if (i == 0)                           // correct for header of first file
            part_sizes[i] += gd_header_len;
        }
    }

  if ((buffer = (unsigned char *) malloc (8 * MBIT + gd_header_len)) == NULL)
    { // a DRAM unit can hold 8 Mbit at maximum (+ extra space for header)
      fprintf (stderr, ucon64_msg[ROM_BUFFER_ERROR], 8 * MBIT + gd_header_len);
      exit (1);
    }

  printf ("Send: %u Bytes (%.4f Mb)\n", gd_fsize, (float) gd_fsize / MBIT);

  // Put this just before the real transfer begins or else the ETA won't be
  //  correct.
  gd_starttime = time (NULL);

  // send the ROM to the hardware
  if (gd6_protocol && gd6_sync_hardware () == GD_ERROR)
    io_error ("gd6_sync_hardware()");
  memcpy (buffer, prolog_str, 4);
  buffer[4] = (unsigned char) num_units;
  if (gd_send_prolog_bytes (buffer, 5) == GD_ERROR)
    io_error (gd6_protocol ? "gd6_send_prolog_bytes(5)" : "gd3_send_prolog_bytes(5)");

  puts ("Press q to abort\n");
  for (i = 0; i < num_units; i++)
    {
      unsigned int size = part_sizes[i];

      if (ucon64.split)
        {
          if ((file = fopen (filenames[i], "rb")) == NULL)
            {
              fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], filenames[i]);
              exit (1);
            }
        }
      else if (file == NULL && (file = fopen (filename, "rb")) == NULL)
        { // don't open the file more than once
          fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], filename);
          exit (1);
        }
      /*
        When sending a "pre-split" file, opening the next file often causes
        enough of a delay for the GDSF7. Sending an unsplit file without an
        extra delay often results in the infamous "File Size Error !".
        Adding the delay also results in the transfer code catching an I/O
        error more often, instead of simply hanging (while the GDSF7 displays
        "File Size Error !"). The delay seems *much* more important for HiROM
        games than for LoROM games.
        There's more to the "File Size Error !" than this as the transfer
        reliability is still far from 100%, but the delay helps me a lot.
        Note that this applies to the SF3 protocol. - dbjh
      */
      wait2 (100);                              // 100 ms seems a good value

      buffer[0] = (unsigned char) size;
      buffer[1] = (unsigned char) (size >> 8);
      buffer[2] = (unsigned char) (size >> 16);
      buffer[3] = (unsigned char) (size >> 24);
      {
        // No suffix is necessary but the name entry must be upper case and MUST
        //  be 11 characters long, padded at the end with spaces if necessary.
        size_t len = strlen (names[i]);
        memcpy (&buffer[4], strupr (names[i]), len);
        if (len < 11)                           // warning remover
          memset (buffer + 4 + len, ' ', 11 - len);
      }
      if (gd_send_prolog_bytes (buffer, 15) == GD_ERROR)
        {
          if (i)
            fputc ('\n', stdout);
          io_error (gd6_protocol ? "gd6_send_prolog_bytes(15)" : "gd3_send_prolog_bytes(15)");
        }
      fread_checked (buffer, 1, size, file);
      if (gd_send_bytes (buffer, size) == GD_ERROR)
        {
          if (i)
            fputc ('\n', stdout);
          io_error (gd6_protocol ? "gd6_send_bytes()" : "gd3_send_bytes()");
        }

      if (ucon64.split || i == num_units - 1)
        fclose (file);
    }

  for (i = 0; i < num_units; i++)
    free (filenames[i]);
  free (buffer);

  return 0;
}


int
gd3_read_sram (const char *filename, unsigned short parport)
{
  (void) filename;                              // warning remover
  (void) parport;                               // warning remover
  fflush (stdout);
  fputs ("ERROR: The function for dumping SRAM is not yet implemented for the SF3", stderr);
  fflush (stderr);
  return -1;
}


int
gd6_read_sram (const char *filename, unsigned short parport)
{
  return gd6_read_data (filename, parport, SRAM);
}


int
gd3_write_sram (const char *filename, unsigned short parport)
{
  gd_send_prolog_bytes = gd3_send_prolog_bytes;
  gd_send_bytes = gd3_send_bytes;

  return gd_write_sram (filename, parport, GD3_PROLOG_STRING);
}


int
gd6_write_sram (const char *filename, unsigned short parport)
{
  gd_send_prolog_bytes = gd6_send_prolog_bytes;
  gd_send_bytes = gd6_send_bytes;

  return gd_write_sram (filename, parport, GD6_READ_PROLOG_STRING);
}


static int
gd_write_sram (const char *filename, unsigned short parport, const char *prolog_str)
{
  FILE *file;
  unsigned char *buffer;
  size_t bytesread;
  unsigned int gd6_protocol = !memcmp (prolog_str, GD6_READ_PROLOG_STRING, 4);

  init_io (parport);

#ifdef  USE_PPDEV
  if (gd6_protocol && !gd6_send_byte_delay)
    puts (STOCKPPDEV_MSG);
#endif

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

  gd_fsize = (unsigned int) ucon64.fsize;       // GD SRAM is 4*8 KB, emu SRAM often not
  if (gd_fsize == 0x8000 + GD_HEADER_LEN)
    {
      gd_header_len = GD_HEADER_LEN;            // temporary value, see below
      gd_fsize = 0x8000;
    }
  else if (gd_fsize != 0x8000)
    {
      fputs ("ERROR: Game Doctor SRAM file size must be 32768 or 33280 bytes\n", stderr);
      exit (1);
    }

  printf ("Send: %u Bytes\n", gd_fsize);
  fseek (file, gd_header_len, SEEK_SET);        // skip the header
  gd_header_len = 0;                            // final value, for gd_send_bytes()

  if (gd6_protocol && gd6_sync_hardware () == GD_ERROR)
    io_error ("gd6_sync_hardware()");
  memcpy (buffer, prolog_str, 4);
  buffer[4] = 1;
  if (gd_send_prolog_bytes (buffer, 5) == GD_ERROR)
    io_error (gd6_protocol ? "gd6_send_prolog_bytes(5)" : "gd3_send_prolog_bytes(5)");

  buffer[0] = (unsigned char) gd_fsize;
  buffer[1] = (unsigned char) (gd_fsize >> 8);
  buffer[2] = (unsigned char) (gd_fsize >> 16);
  buffer[3] = (unsigned char) (gd_fsize >> 24);
  if (gd_send_prolog_bytes (buffer, 4) == GD_ERROR)
    io_error (gd6_protocol ? "gd6_send_prolog_bytes(4)" : "gd3_send_prolog_bytes(4)");

  /*
    The BRAM (SRAM) filename doesn't have to exactly match any game loaded in
    the SF7. It needs to match any valid Game Doctor filename AND have an
    extension of .B## (where # is a digit from 0-9).

    TODO: We might need to make a GD filename from the real one.
  */
  if (gd_send_prolog_bytes ((unsigned char *) "SF8123  B00", 11) == GD_ERROR)
    io_error (gd6_protocol ? "gd6_send_prolog_bytes(11)" : "gd3_send_prolog_bytes(11)");

  puts ("Press q to abort\n");                  // print here, NOT before first GD I/O,
                                                //  because if we get here q works ;-)
  gd_starttime = time (NULL);
  while ((bytesread = fread (buffer, 1, BUFFERSIZE, file)) != 0)
    if (gd_send_bytes (buffer, (unsigned int) bytesread) == GD_ERROR)
      {
        if (gd_bytessent)
          fputc ('\n', stdout);
        io_error (gd6_protocol ? "gd6_send_bytes()" : "gd3_send_bytes()");
      }

  free (buffer);
  fclose (file);

  return 0;
}


int
gd3_read_saver (const char *filename, unsigned short parport)
{
  (void) filename;                              // warning remover
  (void) parport;                               // warning remover
  fflush (stdout);
  fputs ("ERROR: The function for dumping saver data is not yet implemented for the SF3", stderr);
  fflush (stderr);
  return -1;
}


int
gd6_read_saver (const char *filename, unsigned short parport)
{
  return gd6_read_data (filename, parport, SAVER);
}


int
gd3_write_saver (const char *filename, unsigned short parport)
{
  gd_send_prolog_bytes = gd3_send_prolog_bytes;
  gd_send_bytes = gd3_send_bytes;

  return gd_write_saver (filename, parport, GD3_PROLOG_STRING);
}


int
gd6_write_saver (const char *filename, unsigned short parport)
{
  gd_send_prolog_bytes = gd6_send_prolog_bytes;
  gd_send_bytes = gd6_send_bytes;

  return gd_write_saver (filename, parport, GD6_READ_PROLOG_STRING);
}


static int
gd_write_saver (const char *filename, unsigned short parport, const char *prolog_str)
{
  FILE *file;
  unsigned char *buffer, gdfilename[12];
  const char *p;
  size_t bytesread;
  unsigned int fn_length, gd6_protocol = !memcmp (prolog_str, GD6_READ_PROLOG_STRING, 4);

  init_io (parport);

#ifdef  USE_PPDEV
  if (gd6_protocol && !gd6_send_byte_delay)
    puts (STOCKPPDEV_MSG);
#endif

  /*
    Check that filename is a valid Game Doctor saver filename.
    It should start with SF, followed by the game ID, followed by the extension.
    The extension is of the form .S## (where # is a digit from 0-9).
    E.g., SF16123.S00
    The saver base filename must match the base name of the game (loaded in the
    Game Doctor) that you are loading the saver data for.
  */

  // strip the path out of filename for use in the GD
  p = basename2 (filename);
  fn_length = (unsigned int) strlen (p);

  if (fn_length < 6 || fn_length > 11 ||        // 7 ("base") + 1 (period) + 3 (extension)
      toupper ((int) p[0]) != 'S' || toupper ((int) p[1]) != 'F' ||
      p[fn_length - 4] != '.' || toupper ((int) p[fn_length - 3]) != 'S')
    {
      fprintf (stderr, "ERROR: Filename (%s) is not a saver filename (SF*.S##)\n", p);
      exit (1);
    }

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

  gd_fsize = (unsigned int) ucon64.fsize;
  if (gd_fsize != 0x38000)              // GD saver size is always 0x38000 bytes -- no header
    {
      fputs ("ERROR: Game Doctor saver file size must be 229376 bytes\n", stderr);
      exit (1);
    }

  // make a GD filename from the real one
  memset (gdfilename, ' ', 11);                 // "pad" with spaces
  gdfilename[11] = '\0';                        // terminate string
  memcpy (gdfilename, p, fn_length - 4);        // copy name except extension
  memcpy (&gdfilename[8], "S00", 3);            // copy extension S00
  strupr ((char *) gdfilename);

  printf ("Send: %u Bytes\n", gd_fsize);

  if (gd6_protocol && gd6_sync_hardware () == GD_ERROR)
    io_error ("gd6_sync_hardware()");
  memcpy (buffer, prolog_str, 4);
  buffer[4] = 1;
  if (gd_send_prolog_bytes (buffer, 5) == GD_ERROR)
    io_error (gd6_protocol ? "gd6_send_prolog_bytes(5)" : "gd3_send_prolog_bytes(5)");

  // transfer 0x38000 bytes
  buffer[0] = (unsigned char) gd_fsize;
  buffer[1] = (unsigned char) (gd_fsize >> 8);
  buffer[2] = (unsigned char) (gd_fsize >> 16);
  buffer[3] = (unsigned char) (gd_fsize >> 24);
  if (gd_send_prolog_bytes (buffer, 4) == GD_ERROR)
    io_error (gd6_protocol ? "gd6_send_prolog_bytes(4)" : "gd3_send_prolog_bytes(4)");

  if (gd_send_prolog_bytes (gdfilename, 11) == GD_ERROR)
    io_error (gd6_protocol ? "gd6_send_prolog_bytes(11)" : "gd3_send_prolog_bytes(11)");

  puts ("Press q to abort\n");                  // print here, NOT before first GD I/O,
                                                //  because if we get here q works ;-)
  gd_starttime = time (NULL);
  while ((bytesread = fread (buffer, 1, BUFFERSIZE, file)) != 0)
    if (gd_send_bytes (buffer, (unsigned int) bytesread) == GD_ERROR)
      {
        if (gd_bytessent)
          fputc ('\n', stdout);
        io_error (gd6_protocol ? "gd6_send_bytes()" : "gd3_send_bytes()");
      }

  free (buffer);
  fclose (file);

  return 0;
}

#endif // USE_PARALLEL
