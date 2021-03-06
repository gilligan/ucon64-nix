/*
term.c - terminal functions

Copyright (c) 1999 - 2006                    NoisyB
Copyright (c) 2001 - 2005, 2015 - 2017, 2020 dbjh
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
// gcc term.c -o term -ltermcap -DUSE_TERMCAP
// st_term_t and term_*() functions taken and modified from LAME frontend/console.c
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif
#include <stdio.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include <string.h>
#include <stdlib.h>
#ifdef  USE_TERMCAP
#include <termcap.h>
#endif
#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef  _WIN32
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4255) // 'function' : no function prototype given: converting '()' to '(void)'
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <io.h>                                 // isatty() (MinGW)
#include <windows.h>                            // HANDLE, SetConsoleTextAttribute()
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#elif   defined AMIGA // _WIN32
#error Include directives are missing. Please add them and let us know.
#include <libraries/lowlevel.h>                 // GetKey()
#endif
#ifdef  DJGPP
#include <dpmi.h>                               // needed for __dpmi_int() by ansi_init()
#endif
#include "misc/term.h"
#include "ucon64_defines.h"


#if     (defined __unix__ && !defined __MSDOS__) || defined __BEOS__ || \
        defined __APPLE__                       // Mac OS X actually
#include <termios.h>
#ifndef __BEOS__                                // select() in BeOS only works on sockets
#include <sys/select.h>
#endif

typedef struct termios tty_t;
#endif


typedef struct
{
#if     defined _WIN32 && !defined __CYGWIN__
  HANDLE Console_Handle;
#endif
  int w;                                        // display width
  int h;                                        // display height
  char up[10];
  char clreoln[10];
  char emph[10];
  char norm[10];
} st_term_t;


static st_term_t term;


int
term_open (void)
{
  st_term_t *t = &term;
#ifdef  USE_TERMCAP
  char *tp;
  char tc[10];
  int val;
#endif

  // defaults
  memset (t, 0, sizeof (st_term_t));
  t->w = 80;
  t->h = 25;
#if     defined _WIN32 && !defined __CYGWIN__
  t->Console_Handle = GetStdHandle (STD_ERROR_HANDLE);
#endif
  strcpy (t->up, "\033[A");

#ifdef  USE_TERMCAP
  // try to catch additional information about special termsole sequences
#if 0
  if (!(term_name = getenv ("TERM")))
    return -1;
  if (tgetent (term_buff, term_name) != 1)
    return -1;
#endif

  val = tgetnum ("co");
  if (val >= 40 && val <= 512)
    t->w = val;

  val = tgetnum ("li");
  if (val >= 16 && val <= 256)
    t->h = val;

  *(tp = tc) = 0;
  tp = tgetstr ("up", &tp);
  if (tp)
    strcpy (t->up, tp);

  *(tp = tc) = 0;
  tp = tgetstr ("ce", &tp);
  if (tp)
    strcpy (t->clreoln, tp);

  *(tp = tc) = 0;
  tp = tgetstr ("md", &tp);
  if (tp)
    strcpy (t->emph, tp);

  *(tp = tc) = 0;
  tp = tgetstr ("me", &tp);
  if (tp)
    strcpy (t->norm, tp);
#endif // USE_TERMCAP

  return 0;
}


int
term_close (void)
{
  memset (&term, 0, sizeof term);
  return 0;
}


int
term_w (void)
{
  return term.w;
}


int
term_h (void)
{
  return term.h;
}


const char *
term_up (void)
{
  return term.up;
}


const char *
term_clreoln (void)
{
  return term.clreoln;
}


const char *
term_emph (void)
{
  return term.emph;
}


const char *
term_norm (void)
{
  return term.norm;
}


#if     defined _WIN32 && defined USE_ANSI_COLOR
int
vprintf2 (const char *format, va_list argptr)
// Cheap hack to get the Visual C++ and MinGW ports support "ANSI colors".
//  Cheap, because it only supports the ANSI escape sequences uCON64 uses.
{
#undef  fprintf
  int n_chars = 0;
  char output[MAXBUFSIZE], *ptr, *ptr2;
  CONSOLE_SCREEN_BUFFER_INFO info;

  n_chars = _vsnprintf (output, MAXBUFSIZE, format, argptr);
  if (n_chars == -1)
    {
      fprintf (stderr, "INTERNAL ERROR: Output buffer in vprintf2() is too small (%d bytes).\n"
                       "                Please send a bug report\n", MAXBUFSIZE);
      exit (1);
    }
  output[MAXBUFSIZE - 1] = '\0';

  if ((ptr = strchr (output, 0x1b)) == NULL)
    fputs (output, stdout);
  else
    {
      int done = 0;
      HANDLE stdout_handle;
      WORD org_attr;

      stdout_handle = GetStdHandle (STD_OUTPUT_HANDLE);
      GetConsoleScreenBufferInfo (stdout_handle, &info);
      org_attr = info.wAttributes;

      if (ptr > output)
        {
          *ptr = '\0';
          fputs (output, stdout);
          *ptr = 0x1b;
        }
      while (!done)
        {
          int n_ctrl = 0;
          size_t n_print;
          WORD new_attr = 0;

          if (memcmp (ptr, "\x1b[0m", 4) == 0)
            {
              new_attr = org_attr;
              n_ctrl = 4;
            }
          else if (memcmp (ptr, "\x1b[01;31m", 8) == 0)
            {
              new_attr = FOREGROUND_INTENSITY | FOREGROUND_RED;
              n_ctrl = 8;
            }
          else if (memcmp (ptr, "\x1b[01;32m", 8) == 0)
            {
              new_attr = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
              n_ctrl = 8;
            }
          else if (memcmp (ptr, "\x1b[01;33m", 8) == 0) // bright yellow
            {
              new_attr = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN;
              n_ctrl = 8;
            }
          else if (memcmp (ptr, "\x1b[31;41m", 8) == 0)
            {
              new_attr = FOREGROUND_RED | BACKGROUND_RED;
              n_ctrl = 8;
            }
          else if (memcmp (ptr, "\x1b[32;42m", 8) == 0)
            {
              new_attr = FOREGROUND_GREEN | BACKGROUND_GREEN;
              n_ctrl = 8;
            }
          else if (memcmp (ptr, "\x1b[30;41m", 8) == 0) // 30 = foreground black
            {
              new_attr = BACKGROUND_RED;
              n_ctrl = 8;
            }
          else if (memcmp (ptr, "\x1b[30;42m", 8) == 0)
            {
              new_attr = BACKGROUND_GREEN;
              n_ctrl = 8;
            }
          else if (*ptr == 0x1b)
            {
              new_attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
              SetConsoleTextAttribute (stdout_handle, new_attr);
              puts ("\n"
                    "INTERNAL WARNING: vprintf2() encountered an unsupported ANSI escape sequence\n"
                    "                  Please send a bug report");
              n_ctrl = 0;
            }
          SetConsoleTextAttribute (stdout_handle, new_attr);

          ptr2 = strchr (ptr + 1, 0x1b);
          if (ptr2)
            n_print = ptr2 - ptr;
          else
            {
              n_print = strlen (ptr);
              done = 1;
            }

          ptr[n_print] = '\0';
          ptr += n_ctrl;
          fputs (ptr, stdout);
          (ptr - n_ctrl)[n_print] = 0x1b;
          ptr = ptr2;
        }
    }
  return n_chars;
#define fprintf fprintf2
}


int
printf2 (const char *format, ...)
{
  va_list argptr;
  int n_chars;

  va_start (argptr, format);
  n_chars = vprintf2 (format, argptr);
  va_end (argptr);
  return n_chars;
}


int
fprintf2 (FILE *file, const char *format, ...)
{
  va_list argptr;
  int n_chars;

  va_start (argptr, format);
  if (file != stdout)
    n_chars = vfprintf (file, format, argptr);
  else
    n_chars = vprintf2 (format, argptr);
  va_end (argptr);
  return n_chars;
}
#endif // defined _WIN32 && defined USE_ANSI_COLOR


void
clear_line (void)
/*
  This function is used to fix a problem when using the MinGW or Visual C++ port
  on Windows 98 (probably Windows 95 too) while ANSI.SYS is not loaded.
  If a line contains colors, printed with printf() or fprintf() (actually
  printf2() or fprintf2()), it cannot be cleared by printing spaces on the same
  line. A solution is using SetConsoleTextAttribute(). The problem doesn't occur
  if ANSI.SYS is loaded. It also doesn't occur on Windows XP, even if ANSI.SYS
  isn't loaded. We print 79 spaces (not 80), because in command.com the cursor
  advances to the next line if we print something on the 80th column (in 80
  column mode). This doesn't happen in xterm.
*/
{
#if     !defined _WIN32 || !defined USE_ANSI_COLOR
  fputs ("\r                                                                               \r", stdout);
#else
  WORD org_attr;
  CONSOLE_SCREEN_BUFFER_INFO info;
  HANDLE stdout_handle = GetStdHandle (STD_OUTPUT_HANDLE);
  GetConsoleScreenBufferInfo (stdout_handle, &info);
  org_attr = info.wAttributes;
  SetConsoleTextAttribute (stdout_handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
  fputs ("\r                                                                               \r", stdout);
  SetConsoleTextAttribute (stdout_handle, org_attr);
#endif
}


int
ansi_init (void)
{
  int result = isatty (STDOUT_FILENO);

// disabled ANSI.SYS installation check, because it "fails" on DOSBox
#if     defined DJGPP && 0
  if (result)
    {
      // don't use __MSDOS__, because __dpmi_regs and __dpmi_int are DJGPP specific
      __dpmi_regs reg;

      reg.x.ax = 0x1a00;                        // DOS 4.0+ ANSI.SYS installation check
      __dpmi_int (0x2f, &reg);
      if (reg.h.al != 0xff)                     // AL == 0xff if installed
        result = 0;
    }
#endif

  return result;
}


int
gauge (unsigned int percent, unsigned int width, char char1, char char2,
       int color1, int color2)
{
  unsigned int x;
  char buf[1024 + 32];                          // 32 == ANSI code buffer

  if (!width || percent > 100)
    return -1;

  if (width > 1024)
    width = 1024;

  x = (width * percent) / 100;

  memset (buf, char1, x);
  buf[x] = '\0';

  if (x < width)                                // percent < 100
    {
      if (color1 != -1 && color2 != -1)
        sprintf (&buf[x], "\x1b[3%d;4%dm", color1, color1);

      memset (strchr (buf, 0), char2, width - x);
    }

  if (color1 != -1 && color2 != -1)             // ANSI?
    {
      buf[width + 8] = '\0';                    // 8 == ANSI code length
      fprintf (stdout, "\x1b[3%d;4%dm%s\x1b[0m", color2, color2, buf);
    }
  else // no ANSI
    {
      buf[width] = '\0';
      fputs (buf, stdout);
    }

  return 0;
}


#if     (defined __unix__ && !defined __MSDOS__) || defined __BEOS__ || \
        defined __APPLE__                       // Mac OS X actually
static int oldtty_set = 0, stdin_tty = 1;       // 1 => stdin is a TTY, 0 => it's not
static tty_t oldtty, newtty;


static void
set_tty (tty_t *param)
{
  if (stdin_tty && tcsetattr (STDIN_FILENO, TCSANOW, param) == -1)
    {
      fputs ("ERROR: Could not set TTY parameters\n", stderr);
      exit (100);
    }
}


/*
  This code compiles with DJGPP, but is not neccesary. Our kbhit() conflicts
  with DJGPP's one, so it won't be used for that function. Perhaps it works
  for making getchar() behave like getch(), but that's a bit pointless.
*/
void
init_conio (void)
{
  if (!isatty (STDIN_FILENO))
    {
      stdin_tty = 0;
      return;                                   // rest is nonsense if not a TTY
    }

  if (tcgetattr (STDIN_FILENO, &oldtty) == -1)
    {
      fputs ("ERROR: Could not get TTY parameters\n", stderr);
      exit (101);
    }
  oldtty_set = 1;

#if 0
  if (register_func (deinit_conio) == -1)
    {
      fputs ("ERROR: Could not register function with register_func()\n", stderr);
      exit (102);
    }
#endif

  newtty = oldtty;
  newtty.c_lflag &= ~(ICANON | ECHO);
  newtty.c_lflag |= ISIG;
  newtty.c_cc[VMIN] = 1;                        // if VMIN != 0, read() calls
  newtty.c_cc[VTIME] = 0;                       //  block (wait for input)

  set_tty (&newtty);
}


void
deinit_conio (void)
{
  if (oldtty_set)
    {
      tcsetattr (STDIN_FILENO, TCSAFLUSH, &oldtty);
      oldtty_set = 0;
    }
}


// this kbhit() conflicts with DJGPP's one
int
kbhit (void)
{
#ifdef  __BEOS__
  // select() in BeOS only works on sockets
  tty_t tmptty = newtty;
  int ch, key_pressed;

  tmptty.c_cc[VMIN] = 0;                        // doesn't work as expected on
  set_tty (&tmptty);                            //  Mac OS X

  if ((ch = fgetc (stdin)) != EOF)
    {
      key_pressed = 1;
      ungetc (ch, stdin);
    }
  else
    key_pressed = 0;

  set_tty (&newtty);

  return key_pressed;
#else
  struct timeval timeout = { 0L, 0L };
  fd_set fds;

  FD_ZERO (&fds);
  FD_SET (STDIN_FILENO, &fds);

  return select (1, &fds, NULL, NULL, &timeout) > 0;
#endif
}


#elif   defined AMIGA                           // (__unix__ && !__MSDOS__) ||
int                                             //  __BEOS__ || __APPLE__
kbhit (void)
{
  return GetKey () != 0xff ? 1 : 0;
}


int
getch (void)
{
  BPTR con_fileh;
  int temp;

  con_fileh = Input ();
  // put the console into RAW mode which makes getchar() behave like getch()?
  if (con_fileh)
    SetMode (con_fileh, 1);
  temp = getchar ();
  // put the console out of RAW mode (might make sense)
  if (con_fileh)
    SetMode (con_fileh, 0);

  return temp;
}
#endif                                          // AMIGA


//#define TEST
#ifdef  TEST
int
main (int argc, char **argv)
{
  term_open ();
  printf ("%s%s%s%s%s%s", term_up (), term_up (), term_up (), term_up (), term_up (), term_up ());
}
#endif // TEST
