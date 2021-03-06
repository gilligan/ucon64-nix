/*
parallel.c - miscellaneous parallel port functions

Copyright (c) 1999 - 2004                    NoisyB
Copyright (c) 2001 - 2004, 2015, 2017 - 2021 dbjh
Copyright (c) 2001                           Caz (original BeOS code)
Copyright (c) 2002 - 2004                    Jan-Erik Karlsson (Amiga code)


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

#ifdef  USE_PARALLEL

#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif
#include <stdio.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include <stdlib.h>
#ifdef  HAVE_UNISTD_H
#include <unistd.h>                             // ioperm() (libc5)
#endif

#ifdef  USE_PPDEV                               // ppdev is a Linux parallel
#include <errno.h>                              //  port device driver
#include <fcntl.h>
#include <string.h>                             // strerror()
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#elif   defined __linux__ && defined __GLIBC__  // USE_PPDEV
#include <signal.h>
#include <string.h>                             // memset()
#ifdef  HAVE_SYS_IO_H                           // necessary for some Linux/PPC configs
#include <sys/io.h>                             // ioperm() (glibc), in{b, w}(), out{b, w}()
#else
#error No sys/io.h; configure with --disable-parallel
#endif
#elif   defined __OpenBSD__ || defined __NetBSD__ // __linux__ && __GLIBC__
#include <machine/sysarch.h>
#include <sys/types.h>
#elif   defined __BEOS__ || defined __FreeBSD__ // __OpenBSD__ || __NetBSD__
#include <fcntl.h>
#elif   defined AMIGA                           // __BEOS__ || __FreeBSD__
#include <fcntl.h>
#include <devices/parallel.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/types.h>
#elif   defined _WIN32                          // AMIGA
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4255) // 'function' : no function prototype given: converting '()' to '(void)'
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <windows.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include <conio.h>                              // inp{w}() & outp{w}()
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#include <io.h>                                 // access()
#pragma warning(pop)
#define F_OK 00
#endif
#include "misc/dlopen.h"
#elif   defined __CYGWIN__                      // _WIN32
#include <signal.h>
#include <string.h>                             // memset()
#include <windows.h>                            // definition of WINAPI
#undef  _WIN32
#include "misc/dlopen.h"
#endif
#include "misc/file.h"                          // DIR_SEPARATOR_S
#include "misc/property.h"
#include "ucon64.h"


#if     defined __i386__ || defined __x86_64__  // GCC && x86
static inline unsigned char i386_input_byte (unsigned short);
static inline unsigned short i386_input_word (unsigned short);
static inline void i386_output_byte (unsigned short, unsigned char);
static inline void i386_output_word (unsigned short, unsigned short);
#endif


#if     defined USE_PPDEV || defined __BEOS__ || defined __FreeBSD__
static int parport_io_fd;
#ifdef  USE_PPDEV
static enum { FORWARD = 0, REVERSE } parport_io_direction;
static int parport_io_mode;
#endif
#endif

#ifdef  __BEOS__
typedef struct st_ioport
{
  unsigned int port;
  unsigned char data8;
  unsigned short data16;
} st_ioport_t;
#endif

#ifdef  AMIGA
static struct IOStdReq *parport_io_req;
static struct MsgPort *parport;
#endif


#if     defined _WIN32 || defined __CYGWIN__

#define NODRIVER_MSG "ERROR: No (working) I/O port driver. Please see the FAQ, question 4\n"

static void *io_driver = NULL;

// the original inpout32.dll only has I/O functions for byte-sized I/O
static short (__stdcall *Inp32) (short) = NULL;
static void (__stdcall *Outp32) (short, short) = NULL;

static unsigned char inpout32_input_byte (unsigned short port) { return (unsigned char) Inp32 (port); }
static void inpout32_output_byte (unsigned short port, unsigned char byte) { Outp32 (port, byte); }

// io.dll has more functions then the ones we refer to here, but we don't need them
static char (WINAPI *PortIn) (short int) = NULL;
static short int (WINAPI *PortWordIn) (short int) = NULL;
static void (WINAPI *PortOut) (short int, char) = NULL;
static void (WINAPI *PortWordOut) (short int, short int) = NULL;
static short int (WINAPI *IsDriverInstalled) (void) = NULL;

static unsigned char io_input_byte (unsigned short port) { return PortIn (port); }
static unsigned short io_input_word (unsigned short port) { return PortWordIn (port); }
static void io_output_byte (unsigned short port, unsigned char byte) { PortOut (port, byte); }
static void io_output_word (unsigned short port, unsigned short word) { PortWordOut (port, word); }

// dlportio.dll has more functions then the ones we refer to here, but we don't need them
static unsigned char (__stdcall *DlPortReadPortUchar) (unsigned short) = NULL;
static unsigned short (__stdcall *DlPortReadPortUshort) (unsigned short) = NULL;
static void (__stdcall *DlPortWritePortUchar) (unsigned short, unsigned char) = NULL;
static void (__stdcall *DlPortWritePortUshort) (unsigned short, unsigned short) = NULL;

static unsigned char dlportio_input_byte (unsigned short port) { return DlPortReadPortUchar (port); }
static unsigned short dlportio_input_word (unsigned short port) { return DlPortReadPortUshort (port); }
static void dlportio_output_byte (unsigned short port, unsigned char byte) { DlPortWritePortUchar (port, byte); }
static void dlportio_output_word (unsigned short port, unsigned short word) { DlPortWritePortUshort (port, word); }

#if     defined __CYGWIN__ || defined __MINGW32__
// default to functions which are always available (but which generate an
//  exception on Windows NT/2000/XP/2003/Vista/7/8/8.1/10 without an I/O driver)
static unsigned char (*input_byte) (unsigned short) = i386_input_byte;
static unsigned short (*input_word) (unsigned short) = i386_input_word;
static void (*output_byte) (unsigned short, unsigned char) = i386_output_byte;
static void (*output_word) (unsigned short, unsigned short) = i386_output_word;

#elif   defined _WIN32
// the following four functions are needed because inp{w} and outp{w} seem to be macros
static unsigned char inp_func (unsigned short port) { return (unsigned char) inp (port); }
static unsigned short inpw_func (unsigned short port) { return inpw (port); }
static void outp_func (unsigned short port, unsigned char byte) { outp (port, byte); }
static void outpw_func (unsigned short port, unsigned short word) { outpw (port, word); }

// default to functions which are always available (but which generate an
//  exception on Windows NT/2000/XP/2003/Vista/7/8/8.1/10 without an I/O driver)
static unsigned char (*input_byte) (unsigned short) = inp_func;
static unsigned short (*input_word) (unsigned short) = inpw_func;
static void (*output_byte) (unsigned short, unsigned char) = outp_func;
static void (*output_word) (unsigned short, unsigned short) = outpw_func;

#endif
#endif // _WIN32 || __CYGWIN__


#if     defined __i386__ || defined __x86_64__  // GCC && x86
static inline unsigned char
i386_input_byte (unsigned short port)
{
  unsigned char byte;
  __asm__ __volatile__
  ("inb %1, %0"
    : "=a" (byte)
    : "d" (port)
  );
  return byte;
}


static inline unsigned short
i386_input_word (unsigned short port)
{
  unsigned short word;
  __asm__ __volatile__
  ("inw %1, %0"
    : "=a" (word)
    : "d" (port)
  );
  return word;
}


static inline void
i386_output_byte (unsigned short port, unsigned char byte)
{
  __asm__ __volatile__
  ("outb %1, %0"
    :
    : "d" (port), "a" (byte)
  );
}


static inline void
i386_output_word (unsigned short port, unsigned short word)
{
  __asm__ __volatile__
  ("outw %1, %0"
    :
    : "d" (port), "a" (word)
  );
}
#endif // __i386__ || __x86_64__


#ifdef  USE_PPDEV
static inline ssize_t
read2 (int fd, void *buf, size_t nbytes)
{
  size_t i = 0;

  do
    {
      ssize_t n = read (fd, &((unsigned char *) buf)[i], nbytes - i);
      if (n >= 0)
        i += n;
      else if (errno != EINTR)
        {
          fprintf (stderr, "ERROR: read() failed: %s\n", strerror (errno));
          exit (1);
        }
    }
  while (i < nbytes);

  return i;
}


static inline ssize_t
write2 (int fd, const void *buf, size_t nbytes)
{
  size_t i = 0;

  do
    {
      ssize_t n = write (fd, &((unsigned char *) buf)[i], nbytes - i);
      if (n >= 0)
        i += n;
      else if (errno != EINTR)
        {
          fprintf (stderr, "ERROR: write() failed: %s\n", strerror (errno));
          exit (1);
        }
    }
  while (i < nbytes);

  return i;
}
#endif


unsigned char
inportb (unsigned short port)
{
#ifdef  USE_PPDEV
  int ppreg = port - ucon64.parport;
  unsigned char byte;

  switch (ppreg)
    {
    case 0:                                     // data
      ioctl (parport_io_fd, PPRDATA, &byte);
      break;
    case 1:                                     // status
      ioctl (parport_io_fd, PPRSTATUS, &byte);
      break;
    case 2:                                     // control
      ioctl (parport_io_fd, PPRCONTROL, &byte);
      break;
    case 3:                                     // EPP address
      if (!(parport_io_mode & IEEE1284_ADDR))   // IEEE1284_DATA is 0!
        {
          parport_io_mode |= IEEE1284_ADDR;
          ioctl (parport_io_fd, PPSETMODE, &parport_io_mode);
        }
      read2 (parport_io_fd, &byte, 1);
      break;
    case 4:                                     // EPP data
      if (parport_io_mode & IEEE1284_ADDR)
        {
          parport_io_mode &= ~IEEE1284_ADDR;    // IEEE1284_DATA is 0
          ioctl (parport_io_fd, PPSETMODE, &parport_io_mode);
        }
      read2 (parport_io_fd, &byte, 1);
      break;
    default:
      fprintf (stderr,
               "ERROR: inportb() tried to read from an unsupported port (0x%x)\n",
               port);
      exit (1);
    }
  return byte;
#elif   defined __BEOS__
  st_ioport_t temp;

  temp.port = port;
  ioctl (parport_io_fd, 'r', &temp, 0);

  return temp.data8;
#elif   defined AMIGA
  (void) port;                                  // warning remover
  ULONG wait_mask;

  parport_io_req->io_Length = 1;
  parport_io_req->io_Command = CMD_READ;

/*
  SendIO ((struct IORequest *) parport_io_req);

  wait_mask = SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_F | 1L << parport->mp_SigBit;
  if (Wait (wait_mask) & (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_F))
    AbortIO ((struct IORequest *) parport_io_req);
  WaitIO ((struct IORequest *) parport_io_req);
*/

  /*
    The difference between using SendIO() and DoIO(), is that DoIO() handles
    messages etc. by itself but it will not return until the I/O is done.

    Probably have to do more error handling here :-)

    Can one CTRL-C a DoIO() request? (Or for that matter a SendIO().)
  */

  if (DoIO ((struct IORequest *) parport_io_req))
    {
      fprintf (stderr, "ERROR: Could not communicate with parallel port (%s, %u)\n",
               ucon64.parport_dev, ucon64.parport);
      exit (1);
    }

  return (unsigned char) parport_io_req->io_Data;
#elif   defined _WIN32 || defined __CYGWIN__
  return input_byte (port);
#elif   defined __i386__ || defined __x86_64__
  return i386_input_byte (port);
#elif   defined HAVE_SYS_IO_H
  return inb (port);
#endif
}


unsigned short
inportw (unsigned short port)
{
#ifdef  USE_PPDEV
  int ppreg = port - ucon64.parport;
  unsigned char buf[2];

  switch (ppreg)
    {
    case 3:                                     // EPP address
      if (!(parport_io_mode & IEEE1284_ADDR))   // IEEE1284_DATA is 0!
        {
          parport_io_mode |= IEEE1284_ADDR;
          ioctl (parport_io_fd, PPSETMODE, &parport_io_mode);
        }
      read2 (parport_io_fd, buf, 2);
      break;
    case 4:                                     // EPP data
      if (parport_io_mode & IEEE1284_ADDR)
        {
          parport_io_mode &= ~IEEE1284_ADDR;    // IEEE1284_DATA is 0
          ioctl (parport_io_fd, PPSETMODE, &parport_io_mode);
        }
      read2 (parport_io_fd, buf, 2);
      break;
    default:
      fprintf (stderr,
               "ERROR: inportw() tried to read from an unsupported port (0x%x)\n",
               port);
      exit (1);
    }
  return buf[0] | buf[1] << 8;                  // words are read in little endian format
#elif   defined __BEOS__
  st_ioport_t temp;

  temp.port = port;
  ioctl (parport_io_fd, 'r16', &temp, 0);

  return temp.data16;
#elif   defined AMIGA
  (void) port;                                  // warning remover
  ULONG wait_mask;

  parport_io_req->io_Length = 2;
  parport_io_req->io_Command = CMD_READ;

  if (DoIO ((struct IORequest *) parport_io_req))
    {
      fprintf (stderr, "ERROR: Could not communicate with parallel port (%s, %u)\n",
               ucon64.parport_dev, ucon64.parport);
      exit (1);
    }

  return (unsigned short) parport_io_req->io_Data;
#elif   defined _WIN32 || defined __CYGWIN__
  return input_word (port);
#elif   defined __i386__ || defined __x86_64__
  return i386_input_word (port);
#elif   defined HAVE_SYS_IO_H
  return inw (port);
#endif
}


void
outportb (unsigned short port, unsigned char byte)
{
#ifdef  USE_PPDEV
  int ppreg = port - ucon64.parport;

  switch (ppreg)
    {
    case 0:                                     // data
      ioctl (parport_io_fd, PPWDATA, &byte);
      break;
    case 2:                                     // control
      if (((byte & 0x20) >> 5) ^ parport_io_direction) // change direction?
        {
          parport_io_direction = byte & 0x20 ? REVERSE : FORWARD;
          ioctl (parport_io_fd, PPDATADIR, &parport_io_direction);
        }
      byte &= ~0x20;                            // prevent ppdev from repeating the above
      ioctl (parport_io_fd, PPWCONTROL, &byte);
      break;
    case 3:                                     // EPP address
      if (!(parport_io_mode & IEEE1284_ADDR))   // IEEE1284_DATA is 0!
        {
          parport_io_mode |= IEEE1284_ADDR;
          ioctl (parport_io_fd, PPSETMODE, &parport_io_mode);
        }
      write2 (parport_io_fd, &byte, 1);
      break;
    case 4:                                     // EPP data
      if (parport_io_mode & IEEE1284_ADDR)
        {
          parport_io_mode &= ~IEEE1284_ADDR;    // IEEE1284_DATA is 0
          ioctl (parport_io_fd, PPSETMODE, &parport_io_mode);
        }
      write2 (parport_io_fd, &byte, 1);
      break;
    default:
      fprintf (stderr,
               "ERROR: outportb() tried to write to an unsupported port (0x%x)\n",
               port);
      exit (1);
    }
#elif   defined __BEOS__
  st_ioport_t temp;

  temp.port = port;
  temp.data8 = byte;
  ioctl (parport_io_fd, 'w', &temp, 0);
#elif   defined AMIGA
  (void) port;                                  // warning remover
  ULONG wait_mask;

  parport_io_req->io_Length = 1;
  parport_io_req->io_Data = byte;
  parport_io_req->io_Command = CMD_WRITE;

  if (DoIO ((struct IORequest *) parport_io_req))
    {
      fprintf (stderr, "ERROR: Could not communicate with parallel port (%s, %u)\n",
               ucon64.parport_dev, ucon64.parport);
      exit (1);
    }
#elif   defined _WIN32 || defined __CYGWIN__
  output_byte (port, byte);
#elif   defined __i386__ || defined __x86_64__
  i386_output_byte (port, byte);
#elif   defined HAVE_SYS_IO_H
  outb (byte, port);
#endif
}


void
outportw (unsigned short port, unsigned short word)
{
#ifdef  USE_PPDEV
  int ppreg = port - ucon64.parport;
  unsigned char buf[2];

  // words are written in little endian format
  buf[0] = word;
  buf[1] = word >> 8;
  switch (ppreg)
    {
    case 3:                                     // EPP address
      if (!(parport_io_mode & IEEE1284_ADDR))   // IEEE1284_DATA is 0!
        {
          parport_io_mode |= IEEE1284_ADDR;
          ioctl (parport_io_fd, PPSETMODE, &parport_io_mode);
        }
      write2 (parport_io_fd, buf, 2);
      break;
    case 4:                                     // EPP data
      if (parport_io_mode & IEEE1284_ADDR)
        {
          parport_io_mode &= ~IEEE1284_ADDR;    // IEEE1284_DATA is 0
          ioctl (parport_io_fd, PPSETMODE, &parport_io_mode);
        }
      write2 (parport_io_fd, buf, 2);
      break;
    default:
      fprintf (stderr,
               "ERROR: outportw() tried to write to an unsupported port (0x%x)\n",
               port);
      exit (1);
    }
#elif   defined __BEOS__
  st_ioport_t temp;

  temp.port = port;
  temp.data16 = word;
  ioctl (parport_io_fd, 'w16', &temp, 0);
#elif   defined AMIGA
  (void) port;                                  // warning remover
  ULONG wait_mask;

  parport_io_req->io_Length = 2;
  parport_io_req->io_Data = word;
  parport_io_req->io_Command = CMD_WRITE;

  if (DoIO ((struct IORequest *) parport_io_req))
    {
      fprintf (stderr, "ERROR: Could not communicate with parallel port (%s, %u)\n",
               ucon64.parport_dev, ucon64.parport);
      exit (1);
    }
#elif   defined _WIN32 || defined __CYGWIN__
  output_word (port, word);
#elif   defined __i386__ || defined __x86_64__
  i386_output_word (port, word);
#elif   defined HAVE_SYS_IO_H
  outw (word, port);
#endif
}


#if     (defined __i386__ || defined __x86_64__ || defined _WIN32) && !defined USE_PPDEV
#define DETECT_MAX_CNT 1000
static int
parport_probe (unsigned short port)
{
  int i = 0;

  outportb (port, 0xaa);
  for (i = 0; i < DETECT_MAX_CNT; i++)
    if (inportb (port) == 0xaa)
      break;

  if (i < DETECT_MAX_CNT)
    {
      outportb (port, 0x55);
      for (i = 0; i < DETECT_MAX_CNT; i++)
        if (inportb (port) == 0x55)
          break;
    }

  if (i >= DETECT_MAX_CNT)
    return 0;

  return 1;
}
#endif


#ifdef  _WIN32
static LONG WINAPI
new_exception_filter (LPEXCEPTION_POINTERS exception_pointers)
{
  if (exception_pointers->ExceptionRecord->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION)
    {
      fputs (NODRIVER_MSG, stderr);
      exit (1);
    }
  return EXCEPTION_CONTINUE_SEARCH;
}
#elif   defined __CYGWIN__
static void
new_signal_handler (int signum, siginfo_t *info, void *ctx)
{
  (void) ctx;
  if (signum == SIGILL && info->si_code == ILL_PRVOPC)
    {
      // We are not interrupting a standard library function, so we do not have
      //  to limit ourselves to async-signal-safe functions.
      fputs (NODRIVER_MSG, stderr);
      exit (1); // Not necessary on 32-bit Cygwin, but it is on 64-bit Cygwin.
    }
}
#endif


#if     !(defined __i386__ || defined __x86_64__) && !defined USE_PPDEV && defined __linux__
static void
new_signal_handler (int signum)
{
  if (signum == SIGSEGV /* && info->si_code == SEGV_MAPERR */)
    {
      fputs ("ERROR: I/O caused a segmentation fault. Try configuring with --enable-ppdev\n", stderr);
      exit (1);
    }
}
#endif


unsigned short
parport_open (unsigned short port)
{
#ifdef  USE_PPDEV
  struct timeval t;
  int x;

  if (port == PARPORT_UNKNOWN)
    port = 0;

  parport_io_fd = open (ucon64.parport_dev, O_RDWR);
  if (parport_io_fd == -1)
    {
      fprintf (stderr, "ERROR: Could not open parallel port device (%s)\n"
                       "       Check if you have the required privileges\n",
               ucon64.parport_dev);
      exit (1);
    }

  ioctl (parport_io_fd, PPEXCL);                // disable sharing
  if (ioctl (parport_io_fd, PPCLAIM) == -1)     // claim the device
    {
      fprintf (stderr, "ERROR: Could not get exclusive access to parallel port device (%s)\n"
                       "       Check if another module (like lp) uses the module parport\n",
               ucon64.parport_dev);
      exit (1);
    }

  t.tv_sec = 0;
  t.tv_usec = 500 * 1000;                       // set time-out to 500 milliseconds
  ioctl (parport_io_fd, PPSETTIME, &t);
/*
  ioctl (parport_io_fd, PPGETTIME, &t);
  printf ("Current time-out value: %ld microseconds\n", t.tv_usec);
//*/

  x = PP_FASTREAD | PP_FASTWRITE;               // enable 16-bit transfers
  ioctl (parport_io_fd, PPSETFLAGS, &x);

  parport_io_direction = FORWARD;               // set forward direction as default
  ioctl (parport_io_fd, PPDATADIR, &parport_io_direction);
#elif   defined __BEOS__
  parport_io_fd = open ("/dev/misc/ioport", O_RDWR);
  if (parport_io_fd == -1)
    {
      fputs ("ERROR: Could not open I/O port device (no driver)\n"
             "       You can download the latest ioport driver from\n"
             "       https://ucon64.sourceforge.io\n",
             stderr);
      exit (1);
    }
#elif   defined AMIGA
  int x;

  parport = CreatePort (NULL, 0);
  if (parport == NULL)
    {
      fputs ("ERROR: Could not create the MsgPort\n", stderr);
      exit (1);
    }
  parport_io_req = CreateExtIO (parport, sizeof (struct IOExtPar));
  if (parport_io_req == NULL)
    {
      fputs ("ERROR: Could not create the I/O request structure\n", stderr);
      DeletePort (parport);
      exit (1);
    }

  // Is it possible to probe for the correct port?
  if (port == PARPORT_UNKNOWN)
    port = 0;

  x = OpenDevice (ucon64.parport_dev, port, (struct IORequest *) parport_io_req,
                  (ULONG) 0);
  if (x != 0)
    {
      fprintf (stderr, "ERROR: Could not open parallel port (%s, %u)\n",
               ucon64.parport_dev, port);
      DeleteExtIO ((struct IOExtPar *) parport_io_req);
      DeletePort (parport);
      exit (1);
    }
#elif   defined __FreeBSD__
  parport_io_fd = open ("/dev/io", O_RDWR);
  if (parport_io_fd == -1)
    {
      fputs ("ERROR: Could not open I/O port device (/dev/io)\n"
             "       uCON64 needs root privileges for the requested action\n",
             stderr);
      exit (1);
    }
#endif

#if     ((defined __linux__ && !defined USE_PPDEV) || \
         defined __OpenBSD__ || defined __NetBSD__) && \
        (defined __i386__ || defined __x86_64__)
  /*
    Some code needs us to switch to the real uid and gid. However, other code
    needs access to I/O ports other than the standard printer port registers.
    We just do an iopl(3) and all code should be happy. Using iopl(3) enables
    users to run all code without being root (of course with the uCON64
    executable setuid root).
    Another reason to use iopl() and not ioperm() is that the former enables
    access to all I/O ports, while the latter enables access to ports up to
    0x3ff. For the standard parallel port hardware addresses this is not a
    problem. It *is* a problem for add-on parallel port cards which can be
    mapped to I/O addresses above 0x3ff.
  */
#ifdef  __linux__
  if (iopl (3) == -1)
#elif   defined __i386__ // OpenBSD or NetBSD
  if (i386_iopl (3) == -1)
#else
#ifdef  __OpenBSD__
  if (amd64_iopl (3) == -1)
#else // __NetBSD__
  if (x86_64_iopl (3) == -1)
#endif
#endif
    {
      fputs ("ERROR: Could not set the I/O privilege level to 3\n"
             "       uCON64 needs root privileges for the requested action\n",
             stderr);
      exit (1);                                 // don't return, if iopl() fails port access
    }                                           //  causes core dump
#endif

#ifndef USE_PPDEV

#if     defined __i386__ || defined __x86_64__ || defined _WIN32

#if     defined _WIN32 || defined __CYGWIN__
  /*
    We support the I/O port drivers inpout32.dll, io.dll and dlportio.dll,
    because using them is way easier than using GiveIO. The drivers are also
    more reliable and seem to enable access to all I/O ports (dlportio.dll
    enables access to ports >= 0x100).
  */
  char fname[FILENAME_MAX];
  int driver_found = 0;
  u_func_ptr_t sym;

  io_driver = NULL;

  snprintf (fname, FILENAME_MAX, "%s" DIR_SEPARATOR_S "%s", ucon64.configdir,
            "dlportio.dll");
  fname[FILENAME_MAX - 1] = '\0';
#if 0 // we must not do this for Cygwin or access() won't "find" the file
  change_mem (fname, strlen (fname), "/", 1, 0, 0, "\\", 1, 0);
#endif
  if (access (fname, F_OK) == 0)
    {
      io_driver = open_module (fname);

      driver_found = 1;
      printf ("Using %s\n\n", fname);

      sym.void_ptr = get_symbol (io_driver, "DlPortReadPortUchar");
      DlPortReadPortUchar = (unsigned char (__stdcall *) (unsigned short)) sym.func_ptr;
      sym.void_ptr = get_symbol (io_driver, "DlPortReadPortUshort");
      DlPortReadPortUshort = (unsigned short (__stdcall *) (unsigned short)) sym.func_ptr;
      sym.void_ptr = get_symbol (io_driver, "DlPortWritePortUchar");
      DlPortWritePortUchar = (void (__stdcall *) (unsigned short, unsigned char)) sym.func_ptr;
      sym.void_ptr = get_symbol (io_driver, "DlPortWritePortUshort");
      DlPortWritePortUshort = (void (__stdcall *) (unsigned short, unsigned short)) sym.func_ptr;

      input_byte = dlportio_input_byte;
      input_word = dlportio_input_word;
      output_byte = dlportio_output_byte;
      output_word = dlportio_output_word;
    }

  if (!driver_found)
    {
      snprintf (fname, FILENAME_MAX, "%s" DIR_SEPARATOR_S "%s", ucon64.configdir,
                "io.dll");
      fname[FILENAME_MAX - 1] = '\0';
      if (access (fname, F_OK) == 0)
        {
          io_driver = open_module (fname);

          sym.void_ptr = get_symbol (io_driver, "IsDriverInstalled");
          IsDriverInstalled = (short int (WINAPI *) (void)) sym.func_ptr;
          if (IsDriverInstalled ())
            {
              driver_found = 1;
              printf ("Using %s\n\n", fname);

              sym.void_ptr = get_symbol (io_driver, "PortIn");
              PortIn = (char (WINAPI *) (short int)) sym.func_ptr;
              sym.void_ptr = get_symbol (io_driver, "PortWordIn");
              PortWordIn = (short int (WINAPI *) (short int)) sym.func_ptr;
              sym.void_ptr = get_symbol (io_driver, "PortOut");
              PortOut = (void (WINAPI *) (short int, char)) sym.func_ptr;
              sym.void_ptr = get_symbol (io_driver, "PortWordOut");
              PortWordOut = (void (WINAPI *) (short int, short int)) sym.func_ptr;

              input_byte = io_input_byte;
              input_word = io_input_word;
              output_byte = io_output_byte;
              output_word = io_output_word;
            }
        }
    }

  if (!driver_found)
    {
      snprintf (fname, FILENAME_MAX, "%s" DIR_SEPARATOR_S "%s", ucon64.configdir,
                "inpout32.dll");
      fname[FILENAME_MAX - 1] = '\0';
      if (access (fname, F_OK) == 0)
        {
          io_driver = open_module (fname);

          printf ("Using %s\n\n", fname);

          /*
            Newer ports of inpout32.dll also contain the API provided by
            dlportio.dll. Since the API of dlportio.dll does not have the flaws
            of inpout32.dll (*signed* short return value and arguments), we
            prefer it if it is present.
          */
          sym.void_ptr = try_get_symbol (io_driver, "DlPortReadPortUchar");
          DlPortReadPortUchar = (unsigned char (__stdcall *) (unsigned short)) sym.func_ptr;
          if (DlPortReadPortUchar != (void *) -1)
            input_byte = dlportio_input_byte;
          else
            {
              sym.void_ptr = get_symbol (io_driver, "Inp32");
              Inp32 = (short (__stdcall *) (short)) sym.func_ptr;

              input_byte = inpout32_input_byte;
            }

          sym.void_ptr = try_get_symbol (io_driver, "DlPortWritePortUchar");
          DlPortWritePortUchar = (void (__stdcall *) (unsigned short, unsigned char)) sym.func_ptr;
          if (DlPortWritePortUchar != (void *) -1)
            output_byte = dlportio_output_byte;
          else
            {
              sym.void_ptr = get_symbol (io_driver, "Out32");
              Outp32 = (void (__stdcall *) (short, short)) sym.func_ptr;

              output_byte = inpout32_output_byte;
            }

          sym.void_ptr = try_get_symbol (io_driver, "DlPortReadPortUshort");
          DlPortReadPortUshort = (unsigned short (__stdcall *) (unsigned short)) sym.func_ptr;
          if (DlPortReadPortUshort != (void *) -1)
            input_word = dlportio_input_word;

          sym.void_ptr = try_get_symbol (io_driver, "DlPortWritePortUshort");
          DlPortWritePortUshort = (void (__stdcall *) (unsigned short, unsigned short)) sym.func_ptr;
          if (DlPortWritePortUshort != (void *) -1)
            output_word = dlportio_output_word;
        }
    }

  {
#ifdef  _WIN32                                  // MinGW & Visual C++
    LPTOP_LEVEL_EXCEPTION_FILTER org_exception_filter =
      SetUnhandledExceptionFilter (new_exception_filter);
    input_byte (0x378 + 0x402); // 0x378 + 0x402 is okay (don't use "port")
    // if we get here accessing the I/O port did not cause an exception
    SetUnhandledExceptionFilter (org_exception_filter);
#else                                           // Cygwin
    struct sigaction org_sigaction, new_sigaction;

    memset (&new_sigaction, 0, sizeof new_sigaction);
    new_sigaction.sa_flags = SA_SIGINFO | SA_RESETHAND;
    new_sigaction.sa_sigaction = new_signal_handler;
    sigaction (SIGILL, &new_sigaction, &org_sigaction);
    input_byte (0x378 + 0x402); // 0x378 + 0x402 is okay (don't use "port")
    // if we get here accessing the I/O port did not cause an exception
    sigaction (SIGILL, &org_sigaction, NULL);
#endif
  }
#endif // _WIN32 || __CYGWIN__

  if (port == PARPORT_UNKNOWN)                  // no port specified or forced?
    {
      unsigned short parport_addresses[] = { 0x3bc, 0x378, 0x278 };
      int x, found = 0;

      for (x = 0; x < 3; x++)
        if ((found = parport_probe (parport_addresses[x])) == 1)
          {
            port = parport_addresses[x];
            break;
          }

      if (found != 1)
        {
          fputs ("ERROR: Could not find a parallel port on your system\n"
                 "       Try to specify it on the command line or in the configuration file\n",
                 stderr);
          exit (1);
        }
    }

  if (port < 0x100)
    {
      printf ("WARNING: Specified port (0x%hx) < 0x100. Using 0x278\n", port);
      port = 0x278;
    }
#elif   defined __linux__ // __i386__ || __x86_64__ || _WIN32
  struct sigaction org_sigaction, new_sigaction;

  memset (&new_sigaction, 0, sizeof new_sigaction);
  new_sigaction.sa_flags = SA_RESETHAND;
  new_sigaction.sa_handler = new_signal_handler;
  sigaction (SIGSEGV, &new_sigaction, &org_sigaction);
  inportb (0x378 + 0x402); // probably any port is OK
  // if we get here accessing the I/O port did not generate a signal
  sigaction (SIGSEGV, &org_sigaction, NULL);
#endif

#endif // !USE_PPDEV

  return port;
}


parport_mode_t
parport_setup (unsigned short port, parport_mode_t mode)
{
  if (mode != PPMODE_SPP && mode != PPMODE_SPP_BIDIR && mode != PPMODE_EPP)
    {
      fputs ("ERROR: Mode must be PPMODE_SPP, PPMODE_SPP_BIDIR or PPMODE_EPP\n", stderr);
      exit (1);
    }
#ifdef  USE_PPDEV
  (void) port;
  if (mode == PPMODE_SPP)
    parport_io_mode = IEEE1284_MODE_COMPAT | IEEE1284_DATA;
  else if (mode == PPMODE_SPP_BIDIR)
    parport_io_mode = IEEE1284_MODE_BYTE | IEEE1284_DATA;
  else // if (mode == PPMODE_EPP)
    {
      int capabilities = 0;

      ioctl (parport_io_fd, PPGETMODES, &capabilities);
//      printf ("Capabilities: %x\n", capabilities);
      if (capabilities & PARPORT_MODE_EPP)
        parport_io_mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      else
        {
          puts ("WARNING: EPP mode was requested, but not available");
          mode = PPMODE_SPP_BIDIR;
          parport_io_mode = IEEE1284_MODE_BYTE | IEEE1284_DATA;
        }
    }
  // set mode for read() and write(); IEEE1284_DATA is 0...
  ioctl (parport_io_fd, PPSETMODE, &parport_io_mode);
#elif   defined __i386__ || defined __x86_64__ || defined _WIN32
  {
    const char *p = get_property (ucon64.configfile, "ecr_offset", PROPERTY_MODE_TEXT);

    if (p)
      sscanf (p, "%hx", &ucon64.ecr_offset);
    else
      ucon64.ecr_offset = 0x402;
  }
  if ((uint16_t) (port + ucon64.ecr_offset) < 0x100)
    ucon64.ecr_offset = 0xffff - port;
/*
  printf ("ecr_offset: 0x%hx, ECR address: 0x%hx\n", ucon64.ecr_offset,
          port + ucon64.ecr_offset);
//*/

  /*
    "If you are using SPP, then set the port to Standard Mode as the first
    thing you do. Don't assume that the port will already be in Standard (SPP)
    mode.

    Under some of the modes, the *SPP registers may disappear or not work
    correctly*." - "Interfacing the Standard Parallel Port", page 17.
  */
  if (port != 0x3bc) // The ECP registers are not available if port is 0x3bc.
    {
      unsigned char ecr = inportb (port + ucon64.ecr_offset) & 0x1f;

      if (mode == PPMODE_SPP)
        outportb (port + ucon64.ecr_offset, ecr);
      else if (mode == PPMODE_SPP_BIDIR)
        outportb (port + ucon64.ecr_offset, ecr | 0x20);
      else // if (mode == PPMODE_EPP)
        outportb (port + ucon64.ecr_offset, ecr | 0x80);
    }
  else if (mode == PPMODE_EPP)
    mode = PPMODE_SPP_BIDIR;
/*
  printf ("Parallel port mode: %s\n", mode == PPMODE_EPP ?
            "EPP" : mode == PPMODE_SPP_BIDIR ?
              "SPP bidirectional" : mode == PPMODE_SPP ?
                "SPP" : "unknown");
//*/
  /*
    "In the idle state, an EPP port should have it's nAddress Strobe,
    nDataStrobe, nWrite and nReset lines inactive, high." - "Interfacing the
    Enhanced Parallel Port", page 6.
    The SPP names of those pins are respectively nSelect Printer/nSelect In,
    nAuto Linefeed, nStrobe and nInitialize. Strobe, Auto Linefeed and Select
    Printer/Select In are hardware inverted, so we have to write a 0 to bring
    the associated pins in a high state.

    "On some cards, if the Parallel Port is placed in reverse mode, a EPP Write
    cycle cannot be performed. Therefore it is also wise to place the Parallel
    Port in forward mode before using EPP." - "Interfacing the Enhanced
    Parallel Port", page 6.

    The bits in the Control register are (starting with bit 0) Strobe, Auto
    Linefeed, Initialize (Printer), Select Printer, Enable IRQ Via Ack Line,
    Enable Bidirectional Port. Bits 6 and 7 are reserved.
  */
  outportb (port + PARPORT_CONTROL, 0x04);

  // On some chipsets EPP Time-out will even block SPP cycles.
  parport_reset_timeout (port);
#else
  (void) port;
  (void) mode;
#endif
  return mode;
}


void
parport_reset_timeout (unsigned short port)
// reset EPP Time-out bit
{
#if     (defined __i386__ || defined __x86_64__ || defined _WIN32) && !defined USE_PPDEV
  if ((inportb (port + PARPORT_STATUS) & 0x01) != 0)
    { // some reset by reading twice, others by writing 1, others by writing 0
      unsigned char status = inportb (port + PARPORT_STATUS);
      outportb (port + PARPORT_STATUS, status | 0x01);
      outportb (port + PARPORT_STATUS, status & 0xfe);
    }
#else
  (void) port;
#endif
}


void
parport_close (void)
{
#ifdef  USE_PPDEV
  parport_io_mode = IEEE1284_MODE_COMPAT;
  // We really don't want to perform IEEE 1284 negotiation, but if we don't do
  //  it ppdev will do it for us...
  ioctl (parport_io_fd, PPNEGOT, &parport_io_mode);
  ioctl (parport_io_fd, PPRELEASE);
  close (parport_io_fd);
#elif   defined __BEOS__ || defined __FreeBSD__
  close (parport_io_fd);
#elif   defined AMIGA
  CloseDevice ((struct IORequest *) parport_io_req);
  DeleteExtIO ((struct IOExtPar *) parport_io_req);
  DeletePort (parport);
  parport_io_req = NULL;
#elif   defined _WIN32 || defined __CYGWIN__
  if (io_driver)
    {
      close_module (io_driver);
      io_driver = NULL;
    }
#if     defined __CYGWIN__ || defined __MINGW32__
  input_byte = i386_input_byte;
  input_word = i386_input_word;
  output_byte = i386_output_byte;
  output_word = i386_output_word;
#elif   defined _WIN32
  input_byte = inp_func;
  input_word = inpw_func;
  output_byte = outp_func;
  output_word = outpw_func;
#endif
#endif // _WIN32 || __CYGWIN__
}


void
parport_print_info (void)
{
#ifdef  USE_PPDEV
  printf ("Using parallel port device: %s\n", ucon64.parport_dev);
#elif   defined AMIGA
  printf ("Using parallel port device: %s, port %u\n", ucon64.parport_dev, ucon64.parport);
#else
  printf ("Using I/O port base: 0x%hx; I/O port Extended Control register: 0x%hx\n",
          ucon64.parport, (unsigned short) (ucon64.parport + ucon64.ecr_offset));
#endif
}

#endif // USE_PARALLEL
