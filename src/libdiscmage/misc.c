/*
misc.c - miscellaneous functions

Copyright (c) 1999 - 2008              NoisyB
Copyright (c) 2001 - 2005, 2015 - 2021 dbjh
Copyright (c) 2002 - 2005              Jan-Erik Karlsson (Amiga code)


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
#include "config.h"                             // USE_ZLIB
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif
#include <ctype.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#include <direct.h>
#endif
#include <errno.h>
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#include <io.h>
#pragma warning(pop)
#endif
#include <stdarg.h>                             // va_arg()
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#ifdef  HAVE_UNISTD_H
#include <unistd.h>                             // usleep(), microseconds
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <time.h>
#include <sys/stat.h>                           // for S_IFLNK
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

#ifdef  __MSDOS__
#include <dos.h>                                // delay(), milliseconds
#elif   defined __BEOS__
#include <OS.h>                                 // snooze(), microseconds
// Include OS.h before misc.h, because OS.h includes StorageDefs.h which
//  includes param.h which unconditionally defines MIN and MAX.
#elif   defined AMIGA
#include <unistd.h>
#include <fcntl.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <dos/dostags.h>
#include <libraries/lowlevel.h>                 // GetKey()
#include <proto/dos.h>
#include <proto/lowlevel.h>
#elif   defined _WIN32 || defined __CYGWIN__
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4255) // 'function' : no function prototype given: converting '()' to '(void)'
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <windows.h>                            // Sleep(), milliseconds
#ifdef  __CYGWIN__
#undef  _WIN32
#endif
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#endif

#ifdef  USE_ZLIB
#include "misc_z.h"
#endif
#include "misc.h"

#if     (defined __unix__ && !defined __MSDOS__) || defined __BEOS__ || \
        defined __APPLE__                       // Mac OS X actually
#include <termios.h>
#ifndef __BEOS__                                // select() in BeOS only works on sockets
#include <sys/select.h>
#endif

typedef struct termios tty_t;
#endif


#ifdef  DJGPP
#include <dpmi.h>                               // needed for __dpmi_int() by ansi_init()
#ifdef  DLL
#include "dxedll_priv.h"
#endif
#endif


typedef struct st_func_node
{
  void (*func) (void);
  struct st_func_node *next;
} st_func_node_t;

int cm_verbose = 0;
static st_func_node_t func_list = { NULL, NULL };
static int func_list_locked = 0;
static int misc_ansi_color = 0;


#ifndef USE_ZLIB
off_t
q_fsize (const char *filename)
{
  struct stat fstate;

  errno = 0;
  if (!stat (filename, &fstate))
    return fstate.st_size;

  return -1;
}
#endif


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

  misc_ansi_color = result;

  return result;
}


#if 0                                           // currently not used
char *
ansi_strip (char *str)
{
  int ansi = 0;
  char *p = str, *s = str;

  for (; *p; p++)
    switch (*p)
      {
      case '\x1b':                              // escape
        ansi = 1;
        break;

      case 'm':
        if (ansi)
          {
            ansi = 0;
            break;
          }

      default:
        if (!ansi)
          {
            *s = *p;
            s++;
          }
        break;
      }
  *s = '\0';

  return str;
}
#endif


int
isfname (int c)
{
  // characters that are allowed in filenames
  return isalnum (c) || (c &&
#ifndef __MSDOS__
           strchr (" !#$%&'()-@^_`{}~+,;=[].", c));
#else
           // WinDOS can handle the above, but at this point I do not want to
           //  add run-time checks whether we are running in Windows. - dbjh
           strchr ("!#$%&'()-@^_`{}~", c));
#endif
}


int
isprint2 (int c)
{
  if (isprint (c))
    return TRUE;

  // characters that also work with printf()
  if (c == '\x1b')
    return misc_ansi_color ? TRUE : FALSE;

  return strchr ("\t\n\r", c) ? TRUE : FALSE;
}


int
tofname (int c)
{
  return isfname (c) ? c : '_';
}


int
toprint2 (int c)
{
  return isprint2 (c) ? c : '.';
}


int
isfunc (char *s, size_t size, int (*func) (int))
{
  char *p = s;

  /*
    Casting to unsigned char * is necessary to avoid differences between the
    different compilers' run-time environments. At least for isprint(). Without
    the cast the isprint() of (older versions of) DJGPP, MinGW, Cygwin and
    Visual C++ returns nonzero values for ASCII characters > 126.
  */
  for (; size > 0; p++, size--)
    if (!func (*(unsigned char *) p))
      return FALSE;

  return TRUE;
}


char *
tofunc (char *s, size_t size, int (*func) (int))
{
  char *p = s;

  for (; size > 0; p++, size--)
    *p = (char) func (*p);

  return s;
}


char *
strcasestr2 (const char *str, const char *search)
{
  char *p = (char *) str;
  size_t len = strlen (search);

  if (!len)
    return p;

  for (; *p; p++)
    if (!strnicmp (p, search, len))
      return p;

  return NULL;
}


char *
strncpy2 (char *dest, const char *src, size_t size)
{
  if (dest)
    strncpy (dest, src ? src : "", size)[size] = '\0';
  return dest;
}


int
isupper2 (int c)
{
  return isupper (c);
}


char *
set_suffix (char *filename, const char *suffix)
{
  size_t len;
  char *p, suffix2[FILENAME_MAX];

  if (filename == NULL || suffix == NULL)
    return filename;
  len = strnlen (suffix, FILENAME_MAX - 1);
  strncpy (suffix2, suffix, len)[len] = '\0';
  // the next statement is repeating work that get_suffix() does, but that is
  //  better than repeating the implementation of get_suffix() here
  if ((p = basename2 (filename)) == NULL)
    p = filename;

  {
    size_t len2;
    int basename_isupper = isfunc (p, strlen (p), isupper2);

    p = (char *) get_suffix (p);
    len2 = strlen (filename) - strlen (p);
    if (len2 < FILENAME_MAX - 1)
      {
        if (len + len2 >= FILENAME_MAX)
          len = FILENAME_MAX - 1 - len2;
        strncpy (p, basename_isupper ? strupr (suffix2) : strlwr (suffix2), len)
          [len] = '\0';
      }
  }

  return filename;
}


char *
set_suffix_i (char *filename, const char *suffix)
{
  const char *p;
  size_t len2;

  if (filename == NULL || suffix == NULL)
    return filename;
  p = get_suffix (filename);
  len2 = strlen (filename) - strlen (p);
  if (len2 < FILENAME_MAX - 1)
    {
      size_t len = strnlen (suffix, FILENAME_MAX - 1 - len2);

      strncpy ((char *) p, suffix, len)[len] = '\0';
    }

  return filename;
}


const char *
get_suffix (const char *filename)
// Note that get_suffix() does not return NULL unless filename is NULL. Other
//  code relies on that!
{
  const char *p, *s;

  if (filename == NULL)
    return NULL;
  if ((p = basename2 (filename)) == NULL)
    p = filename;
  if ((s = strrchr (p, '.')) == NULL || s == p) // files can start with '.'
    s = strchr (p, '\0');                       // strchr(p, '\0') and NOT "" is the
                                                //  suffix of a file without suffix
  return s;
}


static size_t
strtrimr (char *str)
/*
  Removes all trailing blanks from a string.
  Blanks are defined with isspace (blank, tab, newline, return, formfeed,
  vertical tab = 0x09 - 0x0D + 0x20)
*/
{
  size_t i, j;

  if (str == NULL || str[0] == '\0')
    return 0;

  j = i = strlen (str) - 1;
  while (i <= j && isspace ((int) str[i]))
    str[i--] = '\0';

  return j - i;
}


static size_t
strtriml (char *str)
/*
  Removes all leading blanks from a string.
  Blanks are defined with isspace (blank, tab, newline, return, formfeed,
  vertical tab = 0x09 - 0x0D + 0x20)
*/
{
  size_t i = 0, j;

  if (str == NULL || str[0] == '\0')
    return 0;

  j = strlen (str) - 1;
  while (i <= j && isspace ((int) str[i]))
    i++;

  if (i > 0)
    memmove (str, &str[i], j + 2 - i);

  return i;
}


char *
strtrim (char *str)
/*
  Removes all leading and trailing blanks in a string.
  Blanks are defined with isspace (blank, tab, newline, return, formfeed,
  vertical tab = 0x09 - 0x0D + 0x20)
*/
{
  strtrimr (str);
  strtriml (str);

  return str;
}


int
memwcmp (const void *buffer, const void *search, size_t searchlen, int wildcard)
{
  size_t n;

  for (n = 0; n < searchlen; n++)
    if (((uint8_t *) search)[n] != wildcard &&
        ((uint8_t *) buffer)[n] != ((uint8_t *) search)[n])
      return -1;

  return 0;
}


void *
mem_search (const void *buffer, size_t buflen,
            const void *search, size_t searchlen)
{
  if (buflen >= searchlen)
    {
      size_t n;

      for (n = 0; n <= buflen - searchlen; n++)
        if (memcmp ((uint8_t *) buffer + n, search, searchlen) == 0)
          return (uint8_t *) buffer + n;
    }

  return NULL;
}


#ifndef HAVE_STRNLEN
size_t
strnlen (const char *str, size_t maxlen)
{
  size_t n;

  for (n = 0; n < maxlen && str[n]; n++)
    ;
  return n;
}
#endif


void *
mem_swap_b (void *buffer, size_t n)
{
  uint8_t *a = (uint8_t *) buffer;

  for (; (size_t) (a - (uint8_t *) buffer) < n; a += 2)
    {
      uint8_t byte = *a;
      *a = *(a + 1);
      *(a + 1) = byte;
    }

  return buffer;
}


void *
mem_swap_w (void *buffer, size_t n)
{
  uint16_t *a = (uint16_t *) buffer;

  n >>= 1;                                      // # words = # bytes / 2
  for (; (size_t) (a - (uint16_t *) buffer) < n; a += 2)
    {
      uint16_t word = *a;
      *a = *(a + 1);
      *(a + 1) = word;
    }

  return buffer;
}


#ifdef  DEBUG
static void
mem_hexdump_code (const void *buffer, size_t n, size_t virtual_start)
// hexdump something into C code (for development)
{
  size_t pos;
  const unsigned char *p = (const unsigned char *) buffer;

  for (pos = 0; pos < n; pos++, p++)
    {
      printf ("0x%02x, ", *p);

      if (!((pos + 1) & 7))
        fprintf (stdout, "// 0x%x (%d)\n",
                 (unsigned int) (pos + virtual_start + 1),
                 (unsigned int) (pos + virtual_start + 1));
    }
}
#endif


void
mem_hexdump (const void *buffer, size_t n, size_t virtual_start)
// hexdump something
{
#ifdef  DEBUG
  mem_hexdump_code (buffer, n, virtual_start);
#else
  size_t pos;
  char buf[17];
  const unsigned char *p = (const unsigned char *) buffer;

  buf[16] = '\0';
  for (pos = 0; pos < n; pos++, p++)
    {
      if (!(pos & 15))
        printf ("%08x  ", (unsigned int) (pos + virtual_start));
      printf ((pos + 1) & 3 ? "%02x " : "%02x  ", *p);

      *(buf + (pos & 15)) = isprint (*p) ? *p : '.';
      if (!((pos + 1) & 15))
        {
          fflush (stdout);
          puts (buf);
        }
    }
  if (pos & 15)
    {
      fflush (stdout);
      *(buf + (pos & 15)) = '\0';
      puts (buf);
    }
#endif
}


#if 0                                           // currently not used
int
mkdir2 (const char *name)
// create a directory and check its permissions
{
  struct stat *st = NULL;

  if (stat (name, st) == -1)
    {
      if (errno != ENOENT)
        {
          fprintf (stderr, "stat %s", name);
          return -1;
        }
      if (mkdir (name, 0700) == -1)
        {
          fprintf (stderr, "mkdir %s", name);
          return -1;
        }
      if (stat (name, st) == -1)
        {
          fprintf (stderr, "stat %s", name);
          return -1;
        }
    }

  if (!S_ISDIR (st->st_mode))
    {
      fprintf (stderr, "%s is not a directory\n", name);
      return -1;
    }
  if (st->st_uid != getuid ())
    {
      fprintf (stderr, "%s is not owned by you\n", name);
      return -1;
    }
  if (st->st_mode & 077)
    {
      fprintf (stderr, "%s must not be accessible by other users\n", name);
      return -1;
    }

  return 0;
}
#endif


char *
basename2 (const char *path)
// basename() clone (differs from GNU/Linux's basename())
{
  char *p1;
#if     defined DJGPP || defined __CYGWIN__ || defined __MINGW32__
  char *p2;
#endif

  if (path == NULL)
    return NULL;

#if     defined DJGPP || defined __CYGWIN__ || defined __MINGW32__
  // yes, DJGPP, not __MSDOS__, because DJGPP's basename() behaves the same
  // Cygwin has no basename()
  p1 = strrchr (path, '/');
  p2 = strrchr (path, '\\');
  if (p2 > p1)                                  // use the last separator in path
    p1 = p2;
#else
  p1 = strrchr (path, DIR_SEPARATOR);
#endif
#if     defined DJGPP || defined __CYGWIN__ || defined _WIN32
  if (p1 == NULL)                               // no slash, perhaps a drive?
    p1 = strrchr (path, ':');
#endif

  return p1 ? p1 + 1 : (char *) path;
}


char *
dirname2 (const char *path)
// dirname() clone (differs from GNU/Linux's dirname())
{
  size_t len;
  char *p1, *dir;
#if     defined DJGPP || defined __CYGWIN__ || defined __MINGW32__
  char *p2;
#endif

  if (path == NULL)
    return NULL;
  // real dirname() uses malloc() so we do too
  // +2: +1 for string terminator +1 if path is "<drive>:"
  len = strnlen (path, FILENAME_MAX - 1);
  if ((dir = (char *) malloc (len + 2)) == NULL)
    return NULL;

  strncpy (dir, path, len)[len] = '\0';
#if     defined DJGPP || defined __CYGWIN__ || defined __MINGW32__
  // yes, DJGPP, not __MSDOS__, because DJGPP's dirname() behaves the same
  // Cygwin has no dirname()
  p1 = strrchr (dir, '/');
  p2 = strrchr (dir, '\\');
  if (p2 > p1)                                  // use the last separator in path
    p1 = p2;
#else
  p1 = strrchr (dir, DIR_SEPARATOR);
#endif

#if     defined DJGPP || defined __CYGWIN__ || defined _WIN32
  if (p1 == NULL && (p1 = strrchr (dir, ':')) != NULL) // no (back)slash, perhaps a drive?
    {
      p1[1] = '.';
      p1 += 2;
    }
#endif

  while (p1 > dir &&                            // find first of last separators (we have to strip trailing ones)
#if     defined DJGPP || defined __CYGWIN__ || defined __MINGW32__
         (*(p1 - 1) == '/' || *(p1 - 1) == '\\') && (*p1 == '/' || *p1 == '\\'))
#else
         *(p1 - 1) == DIR_SEPARATOR && *p1 == DIR_SEPARATOR)
#endif
    p1--;

  if (p1 == dir)
    p1++;                                       // don't overwrite single separator (root dir)
#if     defined DJGPP || defined __CYGWIN__ || defined _WIN32
  else if (p1 > dir && *(p1 - 1) == ':')
    p1++;                                       // we must not overwrite the last separator if
#endif                                          //  it was directly preceded by a drive letter

  if (p1)
    *p1 = '\0';                                 // terminate string (overwrite the separator)
  else
    {
      dir[0] = '.';
      dir[1] = '\0';
    }

  return dir;
}


#if     defined DJGPP || defined __MINGW32__
static inline void
strchrreplace (char *str, char find, char replace)
{
  char *p;

  for (p = str; *p; p++)
    if (*p == find)
      *p = replace;
}
#endif


#ifndef HAVE_REALPATH
#undef  realpath
char *
realpath (const char *path, char *full_path)
{
#if     defined __unix__ || defined __BEOS__ || defined __MSDOS__
// Keep the "defined _WIN32"s in this code in case GetFullPathName() turns out
//  to have some unexpected problems.
#define MAX_READLINKS 256
  char copy_path[FILENAME_MAX], got_path[FILENAME_MAX], *new_path = got_path,
       *max_path = copy_path + FILENAME_MAX - 1;
#if     defined __MSDOS__ || defined _WIN32 || defined __CYGWIN__
  char c;
#endif
#ifdef  S_IFLNK
  int readlinks = 0;
#endif

  {
    size_t len = strlen (path);

    if (len >= FILENAME_MAX)
      {
        errno = ENAMETOOLONG;
        return NULL;
      }
    else if (len == 0)
      {
        errno = ENOENT;
        return NULL;
      }
  }

  // make a copy of the source path since we may need to modify it
  strcpy (copy_path, path);
#if     defined DJGPP || defined __MINGW32__
  // with DJGPP path can contain (forward) slashes
  strchrreplace (copy_path, '/', DIR_SEPARATOR);
#endif
  path = copy_path;

#if     defined __MSDOS__ || defined _WIN32 || defined __CYGWIN__
  c = (char) toupper (*path);
  if (c >= 'A' && c <= 'Z' && path[1] == ':')
    {
      *new_path++ = *path++;
      *new_path++ = *path++;
      if (*path == DIR_SEPARATOR)
        *new_path++ = *path++;
      else
        {
          char org_wd[FILENAME_MAX], drive[3] = ".:";

          getcwd (org_wd, FILENAME_MAX);

          drive[0] = c;
          chdir (drive);
          new_path -= 2;
          getcwd (new_path, FILENAME_MAX);
#if     defined DJGPP || defined __MINGW32__
          // DJGPP's getcwd() returns a path with forward slashes
          strchrreplace (new_path, '/', DIR_SEPARATOR);
#endif
          new_path += strlen (new_path);
          if (*(new_path - 1) != DIR_SEPARATOR)
            *new_path++ = DIR_SEPARATOR;

          chdir (org_wd);
        }
    }
  else
#endif
  if (*path != DIR_SEPARATOR)
    {
      getcwd (new_path, FILENAME_MAX);
#if     defined DJGPP || defined __MINGW32__
      // DJGPP's getcwd() returns a path with forward slashes
      strchrreplace (new_path, '/', DIR_SEPARATOR);
#endif
      new_path += strlen (new_path);
      if (*(new_path - 1) != DIR_SEPARATOR)
        *new_path++ = DIR_SEPARATOR;
    }
  else
    {
      *new_path++ = DIR_SEPARATOR;
      path++;
    }

  // expand each (back)slash-separated pathname component
  while (*path != '\0')
    {
#ifdef  S_IFLNK
      char link_path[FILENAME_MAX];
      ssize_t n;
#endif

      // ignore stray DIR_SEPARATOR
      if (*path == DIR_SEPARATOR)
        {
          path++;
          continue;
        }
      if (*path == '.')
        {
          // ignore "."
          if (path[1] == '\0' || path[1] == DIR_SEPARATOR)
            {
              path++;
              continue;
            }
          if (path[1] == '.' && (path[2] == '\0' || path[2] == DIR_SEPARATOR))
            {
              path += 2;
              // ignore ".." at root
              if (new_path == got_path + 1)
                continue;
              // handle ".." by backing up
              while (*((--new_path) - 1) != DIR_SEPARATOR)
                ;
              continue;
            }
        }
      // safely copy the next pathname component
      while (*path != '\0' && *path != DIR_SEPARATOR)
        {
          if (path > max_path)
            {
              errno = ENAMETOOLONG;
              return NULL;
            }
          *new_path++ = *path++;
        }
#ifdef  S_IFLNK
      // protect against infinite loops
      if (readlinks++ > MAX_READLINKS)
        {
          errno = ELOOP;
          return NULL;
        }

      // see if latest pathname component is a symlink
      *new_path = '\0';
      n = readlink (got_path, link_path, FILENAME_MAX - 1);
      if (n < 0)
        {
          // EINVAL means the file exists but isn't a symlink
          if (errno != EINVAL && errno != ENOENT
#ifdef  __BEOS__
              // make this function work for a mounted ext2 fs ("/:")
              && errno != B_NAME_TOO_LONG
#endif
             )
            {
              // make sure it's null terminated
              *new_path = '\0';
              if (full_path)
                strcpy (full_path, got_path);
              return NULL;
            }
        }
      else
        {
          // NOTE: readlink() doesn't add the null byte
          link_path[n] = '\0';
          if (*link_path == DIR_SEPARATOR)
            // start over for an absolute symlink
            new_path = got_path;
          else
            // otherwise back up over this component
            while (*(--new_path) != DIR_SEPARATOR)
              ;
          if (strlen (path) + n >= FILENAME_MAX)
            {
              errno = ENAMETOOLONG;
              return NULL;
            }
          // insert symlink contents into path
          strcpy (link_path + n, path);
          strcpy (copy_path, link_path);
          path = copy_path;
        }
#endif // S_IFLNK
      *new_path++ = DIR_SEPARATOR;
    }
  // delete trailing slash but don't whomp a lone slash
  if (new_path != got_path + 1 && *(new_path - 1) == DIR_SEPARATOR)
    {
#if     defined __MSDOS__ || defined _WIN32 || defined __CYGWIN__
      if (new_path >= got_path + 3)
        {
          if (*(new_path - 2) == ':')
            {
              c = (char) toupper (*(new_path - 3));
              if (!(c >= 'A' && c <= 'Z'))
                new_path--;
            }
          else
            new_path--;
        }
      else
        new_path--;
#else
      new_path--;
#endif
    }
  // make sure it's null terminated
  *new_path = '\0';
  if (!full_path && (full_path = malloc (strlen (got_path) + 1)) == NULL)
    {
      errno = ENOMEM;
      return NULL;
    }
  strcpy (full_path, got_path);

  return full_path;
#elif   defined _WIN32
  char *full_path2 = full_path, c;
  size_t n, len = strlen (path);
  DWORD result;

  if (len >= FILENAME_MAX)
    {
      errno = ENAMETOOLONG;
      return NULL;
    }
  else if (len == 0)
    {
      errno = ENOENT;
      return NULL;
    }

  if (!full_path && (full_path2 = malloc (FILENAME_MAX)) == NULL)
    {
      errno = ENOMEM;
      return NULL;
    }
  result = GetFullPathName (path, FILENAME_MAX, full_path2, NULL);
  if (result >= FILENAME_MAX)
    {
      if (!full_path)
        free (full_path2);
      errno = ENAMETOOLONG;
      return NULL;
    }
  else if (result == 0)
    {
      if (!full_path)
        free (full_path2);
      return NULL;
    }

  c = (char) toupper (full_path2[0]);
  n = strlen (full_path2) - 1;
  // remove trailing separator if full_path is not the root dir of a drive,
  //  because Visual C++'s run-time system is *really* stupid
  if ((full_path2[n] == DIR_SEPARATOR
#ifdef  __MINGW32__
       || full_path2[n] == '/'
#endif
      ) &&
      !(c >= 'A' && c <= 'Z' && full_path2[1] == ':' && full_path2[3] == '\0'))
    full_path2[n] = '\0';

  return full_path2;
#elif   defined AMIGA
  size_t len = strlen (path);

  if (len >= FILENAME_MAX)
    {
      errno = ENAMETOOLONG;
      return NULL;
    }
  else if (len == 0)
    {
      errno = ENOENT;
      return NULL;
    }

  if (!full_path && (full_path = malloc (FILENAME_MAX)) == NULL)
    {
      errno = ENOMEM;
      return NULL;
    }
  strcpy (full_path, path);
  return full_path;
#endif
}
#endif


char *
realpath2 (const char *path, char *full_path)
// enhanced realpath() which returns the absolute path of a file
{
  char path1[FILENAME_MAX] = "";
  const char *path2;

  if (path[0] == '~')
    {
      if (path[1] == DIR_SEPARATOR
#ifdef  __CYGWIN__
          || path[1] == '\\'
#elif   defined DJGPP || defined __MINGW32__
          || path[1] == '/'
#endif
         )
        {
          snprintf (path1, sizeof path1, "%s" DIR_SEPARATOR_S "%s",
                    getenv2 ("HOME"), &path[2]);
          path1[sizeof path1 - 1] = '\0';
          path2 = "";
        }
      else
        path2 = getenv2 ("HOME");
    }
  else
    path2 = path;

  if (path1[0] == '\0')
    {
      size_t len = strnlen (path2, sizeof path1 - 1);

#if     defined __GNUC__ && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
      strncpy (path1, path2, len)[len] = '\0';
#if     defined __GNUC__ && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif
    }
  path2 = path1;

  if (access (path2, F_OK) == 0)
    return realpath (path2, full_path);
  else
    /*
      According to "The Open Group Base Specifications Issue 7" realpath() is
      supposed to fail if path refers to a file that does not exist. uCON64
      however, expects the behavior of realpath() on GNU/Linux (which sets
      full_path to a reasonable path for a nonexisting file).
    */
    {
      if (full_path)
        {
          strcpy (full_path, path2);
#if     defined DJGPP || defined __MINGW32__
          // with DJGPP full_path may contain (forward) slashes (DJGPP's
          //  getcwd() returns a path with forward slashes)
          strchrreplace (full_path, '/', DIR_SEPARATOR);
#endif
        }
      errno = ENOENT;
      return NULL;
    }
}


int
one_file (const char *filename1, const char *filename2)
// returns 1 if filename1 and filename2 refer to one file, 0 if not (or error)
{
#ifndef _WIN32
  struct stat finfo1, finfo2;

  /*
    Not the name, but the combination inode & device identify a file.
    Note that stat() doesn't need any access rights except search rights for
    the directories in the path to the file.
  */
  if (stat (filename1, &finfo1) != 0)
    return 0;
  if (stat (filename2, &finfo2) != 0)
    return 0;
  return finfo1.st_dev == finfo2.st_dev && finfo1.st_ino == finfo2.st_ino ?
           1 : 0;
#else
  HANDLE file1, file2;
  BY_HANDLE_FILE_INFORMATION finfo1, finfo2;

  file1 = CreateFile (filename1, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);
  if (file1 == INVALID_HANDLE_VALUE)
    return 0;
  file2 = CreateFile (filename2, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);
  if (file2 == INVALID_HANDLE_VALUE)
    {
      CloseHandle (file1);
      return 0;
    }
  GetFileInformationByHandle (file1, &finfo1);
  GetFileInformationByHandle (file2, &finfo2);
  CloseHandle (file1);
  CloseHandle (file2);
  return finfo1.dwVolumeSerialNumber == finfo2.dwVolumeSerialNumber &&
           (finfo1.nFileIndexHigh << 16 | finfo1.nFileIndexLow) ==
           (finfo2.nFileIndexHigh << 16 | finfo2.nFileIndexLow) ? 1 : 0;
#endif
}


int
one_filesystem (const char *filename1, const char *filename2)
// returns 1 if filename1 and filename2 reside on one file system, 0 if not
//  (or an error occurred)
{
#ifndef _WIN32
  struct stat finfo1, finfo2;

  if (stat (filename1, &finfo1) != 0)
    return 0;
  if (stat (filename2, &finfo2) != 0)
    return 0;
  return finfo1.st_dev == finfo2.st_dev ? 1 : 0;
#else
  DWORD fattrib1, fattrib2;
  HANDLE file1, file2;
  BY_HANDLE_FILE_INFORMATION finfo1, finfo2;

  if ((fattrib1 = GetFileAttributes (filename1)) == (DWORD) -1)
    return 0;
  if ((fattrib2 = GetFileAttributes (filename2)) == (DWORD) -1)
    return 0;
  if (fattrib1 & FILE_ATTRIBUTE_DIRECTORY || fattrib2 & FILE_ATTRIBUTE_DIRECTORY)
    /*
      We special-case directories, because we can't use
      FILE_FLAG_BACKUP_SEMANTICS as argument to CreateFile() on Windows 9x/ME.
      There seems to be no Win32 function other than CreateFile() to obtain a
      handle to a directory.
    */
    {
      char path1[FILENAME_MAX], path2[FILENAME_MAX], d1, d2;
      DWORD result;

      result = GetFullPathName (filename1, FILENAME_MAX, path1, NULL);
      if (result == 0 || result >= FILENAME_MAX)
        return 0;
      result = GetFullPathName (filename2, FILENAME_MAX, path2, NULL);
      if (result == 0 || result >= FILENAME_MAX)
        return 0;
      d1 = (char) toupper (path1[0]);
      d2 = (char) toupper (path2[0]);
      return d1 == d2 && d1 >= 'A' && d1 <= 'Z' &&
               // we don't handle unique volume names
               path1[1] == ':' && path2[1] == ':' ? 1 : 0;
    }

  file1 = CreateFile (filename1, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);
  if (file1 == INVALID_HANDLE_VALUE)
    return 0;
  file2 = CreateFile (filename2, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);
  if (file2 == INVALID_HANDLE_VALUE)
    {
      CloseHandle (file1);
      return 0;
    }
  GetFileInformationByHandle (file1, &finfo1);
  GetFileInformationByHandle (file2, &finfo2);
  CloseHandle (file1);
  CloseHandle (file2);
  return finfo1.dwVolumeSerialNumber == finfo2.dwVolumeSerialNumber ? 1 : 0;
#endif
}


int
rename2 (const char *oldname, const char *newname)
{
  int retval;
  char *dir1 = dirname2 (oldname), *dir2 = dirname2 (newname);
  struct stat fstate;

  // we should use dirname{2}() in case oldname or newname doesn't exist yet
  if (one_filesystem (dir1, dir2))
    {
      if (access (newname, F_OK) == 0)
        {
          stat (newname, &fstate);
          chmod (newname, fstate.st_mode | S_IWUSR);
          remove (newname);                     // *try* to remove or rename() will fail
        }
      retval = rename (oldname, newname);
    }
  else
    {
      retval = q_rfcpy (oldname, newname);
      // don't remove unless the file can be copied
      if (retval == 0)
        {
          stat (oldname, &fstate);
          chmod (oldname, fstate.st_mode | S_IWUSR);
          remove (oldname);
        }
    }

  free (dir1);
  free (dir2);
  return retval;
}


int
truncate2 (const char *filename, off_t new_size)
{
  int result;
  off_t size = q_fsize (filename);
  struct stat fstate;

  stat (filename, &fstate);
  if (chmod (filename, fstate.st_mode | S_IWUSR))
    return -1;

  if (size < new_size)
    {
      FILE *file;
      unsigned char padbuffer[MAXBUFSIZE];

      if ((file = fopen (filename, "ab")) == NULL)
        return -1;

      memset (padbuffer, 0, MAXBUFSIZE);

      while (size < new_size)
        {
          int n_bytes = new_size - size > MAXBUFSIZE ?
                          MAXBUFSIZE : new_size - size;

          fwrite (padbuffer, 1, n_bytes, file);
          size += n_bytes;
        }

      fclose (file);
      result = 0;                               // success
    }
  else
    result = truncate (filename, new_size);

  return result;
}


int
change_mem (char *buf, size_t bufsize, char *searchstr, size_t strsize,
            char wc, char esc, char *newstr, size_t newsize, int offset, ...)
// convenience wrapper for change_mem2()
{
  va_list argptr;
  size_t i, n_esc = 0;
  unsigned int retval;
  st_cm_set_t *sets = NULL;

  for (i = 0; i < strsize; i++)
    if (searchstr[i] == esc)
      n_esc++;
  if (n_esc &&
      (sets = (st_cm_set_t *) malloc (n_esc * sizeof (st_cm_set_t))) == NULL)
    {
      fprintf (stderr, "ERROR: Not enough memory for buffer (%u bytes)\n",
               (unsigned int) (n_esc * sizeof (st_cm_set_t)));
      return -1;
    }
  va_start (argptr, offset);
  for (i = 0; i < n_esc; i++)
    {
      sets[i].data = va_arg (argptr, char *);   // get next set of characters
      sets[i].size = va_arg (argptr, int);      // get set size
    }
  va_end (argptr);
  retval = change_mem2 (buf, bufsize, searchstr, strsize, wc, esc, newstr,
                        newsize, offset, sets);
  free (sets);
  return retval;
}


static int
match_found (char *buf, size_t bufsize, size_t strsize, char *newstr,
             size_t newsize, int offset, size_t bufpos, size_t strpos)
{
  if ((int) bufpos + offset >= 0 && bufpos + offset + newsize <= bufsize)
    {
      if (cm_verbose > 0)
        {
          printf ("Match, patching at pattern offset %d/0x%08x / buffer[%u/0x%08x]\n",
                  offset, offset, (unsigned) (bufpos + offset),
                  (unsigned) (bufpos + offset));
          mem_hexdump (buf + bufpos - strpos, strsize, bufpos - strpos);
        }
      memcpy (buf + bufpos + offset, newstr, newsize);
      return 1;
    }
  else
    {
      printf ("WARNING: The combination of buffer position (%u), offset (%d) and\n"
              "         replacement size (%u) would cause a buffer overflow -- ignoring\n"
              "         match\n", (unsigned) bufpos, offset, (unsigned) newsize);
      return 0;
    }
}


int
change_mem2 (char *buf, size_t bufsize, char *searchstr, size_t strsize,
             char wc, char esc, char *newstr, size_t newsize, int offset,
             st_cm_set_t *sets)
/*
  Search for all occurrences of string searchstr in buf and replace newsize
  bytes in buf by copying string newstr to the end of the found search string
  in buf plus offset.
  If searchstr contains wildcard characters wc, then n wildcard characters in
  searchstr match any n characters in buf.
  If searchstr contains escape characters esc, sets must point to an array of
  sets. sets must contain as many elements as there are escape characters in
  searchstr. searchstr matches for an escape character if one of the characters
  in sets[i]->data matches.
  Note that searchstr is not necessarily a C string; it may contain one or more
  zero bytes as strsize indicates the length.
  offset is the relative offset from the last character in searchstring and may
  have a negative value.
  The return value is the number of times a match was found.
  This function was written to patch SNES ROM dumps. It does basically the same
  as the old uCON does, with one exception, the line with:
    bufpos -= n_wc;

  As stated in the comment, this causes the search to restart at the first
  wildcard character of the sequence of wildcards that was most recently
  skipped if the current character in buf didn't match the current character
  in searchstr. This makes change_mem() behave a bit more intuitive. For
  example
    char str[] = "f foobar means...";
    change_mem2 (str, strlen (str), "f**bar", 6, '*', '!', "XXXXXXXX", 8, 2, NULL);
  finds and changes "foobar means..." into "foobar XXXXXXXX", while with uCON's
  algorithm it would not (but does the job good enough for patching SNES ROMs).

  One example of using sets:
    char str[] = "fu-bar     is the same as foobar    ";
    st_cm_set_t sets[] = {{"uo", 2}, {"o-", 2}};
    change_mem2 (str, strlen (str), "f!!", 3, '*', '!', "fighter", 7, 1, sets);
  This changes str into "fu-fighter is the same as foofighter".
*/
{
  char *set;
  size_t bufpos, strpos = 0, pos_1st_esc = (size_t) -1;
  unsigned int n_wc, n_matches = 0, setindex = 0;

  for (bufpos = 0; bufpos < bufsize; bufpos++)
    {
      if (strpos == 0 && searchstr[0] != esc && searchstr[0] != wc)
        while (bufpos < bufsize && searchstr[0] != buf[bufpos])
          bufpos++;

      // handle escape character in searchstr
      while (searchstr[strpos] == esc && bufpos < bufsize)
        {
          unsigned int setsize, i = 0;

          if (strpos == pos_1st_esc)
            setindex = 0;                       // reset argument pointer
          if (pos_1st_esc == (size_t) -1)
            pos_1st_esc = strpos;

          if (sets == NULL)
            {
              // this would be an internal error when called by change_mem()
              fprintf (stderr,
                       "ERROR: Encountered escape character (0x%02x), but no set was specified\n",
                       (unsigned char) esc);
              exit (1);
            }
          set = sets[setindex].data;            // get next set of characters
          setsize = sets[setindex].size;        // get set size
          setindex++;
          // see if buf[bufpos] matches with any character in current set
          while (i < setsize && buf[bufpos] != set[i])
            i++;
          if (i == setsize)
            break;                              // buf[bufpos] didn't match with any char

          if (strpos == strsize - 1)            // check if we are at the end of searchstr
            {
              n_matches += match_found (buf, bufsize, strsize, newstr, newsize,
                                        offset, bufpos, strpos);
              break;
            }

          strpos++;
          bufpos++;
        }
      if (searchstr[strpos] == esc)
        {
          strpos = 0;
          continue;
        }

      // skip wildcards in searchstr
      n_wc = 0;
      while (searchstr[strpos] == wc && bufpos < bufsize)
        {
          if (strpos == strsize - 1)            // check if at end of searchstr
            {
              n_matches += match_found (buf, bufsize, strsize, newstr, newsize,
                                        offset, bufpos, strpos);
              break;
            }

          strpos++;
          bufpos++;
          n_wc++;
        }
      if (bufpos == bufsize)
        break;
      if (searchstr[strpos] == wc)
        {
          strpos = 0;
          continue;
        }

      if (searchstr[strpos] == esc)
        {
          bufpos--;                             // current char has to be checked, but "for"
          continue;                             //  increments bufpos
        }

      // no escape char, no wildcard => normal character
      if (searchstr[strpos] == buf[bufpos])
        {
          if (strpos == strsize - 1)            // check if at end of searchstr
            {
              n_matches += match_found (buf, bufsize, strsize, newstr, newsize,
                                        offset, bufpos, strpos);
              strpos = 0;
            }
          else
            strpos++;
        }
      else
        {
          bufpos -= n_wc;                       // scan the most recent wildcards too if
          if (strpos > 0)                       //  the character didn't match
            {
              bufpos--;                         // current char has to be checked, but "for"
              strpos = 0;                       //  increments bufpos
            }
        }
    }

  return n_matches;
}


int
build_cm_patterns (st_cm_pattern_t **patterns, const char *filename)
/*
  This function goes a bit over the top what memory allocation technique
  concerns, but at least it's stable.
  Note the important difference between (*patterns)[0].n_sets and
  patterns[0]->n_sets (not especially that member).
*/
{
  char src_name[FILENAME_MAX], line[MAXBUFSIZE], buffer[MAXBUFSIZE],
       *token, *last;
  unsigned int line_num = 0, n, currentsize1, requiredsize1, currentsize2,
               requiredsize2, currentsize3, requiredsize3;
  int n_codes = 0;
  FILE *srcfile;

  strcpy (src_name, filename);
  if (access (src_name, F_OK | R_OK))
    return -1;                                  // NOT an error, it's optional

  if ((srcfile = fopen (src_name, "r")) == NULL) // open in text mode
    {
      fprintf (stderr, "ERROR: Cannot open \"%s\" for reading\n", src_name);
      return -1;
    }

  *patterns = NULL;
  currentsize1 = requiredsize1 = 0;
  while (fgets (line, sizeof line, srcfile) != NULL)
    {
      char *ptr = line + strspn (line, "\t ");
      unsigned int n_sets = 0;

      line_num++;

      if (*ptr == '#' || *ptr == '\n' || *ptr == '\r')
        continue;
      if ((ptr = strpbrk (line, "\n\r#")) != NULL) // text after # is comment
        *ptr = '\0';

      requiredsize1 += sizeof (st_cm_pattern_t);
      if (requiredsize1 > currentsize1)
        {
          st_cm_pattern_t *old_patterns = *patterns;
          currentsize1 = requiredsize1 + 10 * sizeof (st_cm_pattern_t);
          if ((*patterns = (st_cm_pattern_t *) realloc (old_patterns, currentsize1)) == NULL)
            {
              fprintf (stderr, "ERROR: Not enough memory for buffer (%u bytes)\n", currentsize1);
              free (old_patterns);
              return -1;
            }
        }

      (*patterns)[n_codes].search = NULL;
      currentsize2 = 0;
      requiredsize2 = 1;                        // for string terminator
      n = 0;
      strcpy (buffer, line);
      token = strtok (buffer, ":");
      token = strtok (token, " ");
//      printf ("token: \"%s\"\n", token);
      last = token;
      do
        {
          requiredsize2++;
          if (requiredsize2 > currentsize2)
            {
              char *old_search = (*patterns)[n_codes].search;
              currentsize2 = requiredsize2 + 10;
              if (((*patterns)[n_codes].search =
                   (char *) realloc (old_search, currentsize2)) == NULL)
                {
                  fprintf (stderr, "ERROR: Not enough memory for buffer (%u bytes)\n", currentsize2);
                  free (old_search);
                  free (*patterns);
                  *patterns = NULL;
                  return -1;
                }
            }
          (*patterns)[n_codes].search[n] = (unsigned char) strtol (token, NULL, 16);
          n++;
        }
      while ((token = strtok (NULL, " ")) != NULL);
      (*patterns)[n_codes].search_size = n;     // size in bytes

      strcpy (buffer, line);
      strtok (last, ":");
      token = strtok (NULL, ":");
      token = strtok (token, " ");
      last = token;
      if (!token)
        {
          printf ("WARNING: Line %u is invalid, no wildcard value is specified\n", line_num);
          continue;
        }
      (*patterns)[n_codes].wildcard = (char) strtol (token, NULL, 16);

      strcpy (buffer, line);
      strtok (last, ":");
      token = strtok (NULL, ":");
      token = strtok (token, " ");
      last = token;
      if (!token)
        {
          printf ("WARNING: Line %u is invalid, no escape value is specified\n", line_num);
          continue;
        }
      (*patterns)[n_codes].escape = (char) strtol (token, NULL, 16);

      strcpy (buffer, line);
      strtok (last, ":");
      token = strtok (NULL, ":");
      token = strtok (token, " ");
      last = token;
      if (!token)
        {
          printf ("WARNING: Line %u is invalid, no replacement is specified\n", line_num);
          continue;
        }
      (*patterns)[n_codes].replace = NULL;
      currentsize2 = 0;
      requiredsize2 = 1;                        // for string terminator
      n = 0;
      do
        {
          requiredsize2++;
          if (requiredsize2 > currentsize2)
            {
              char *old_replace = (*patterns)[n_codes].replace;
              currentsize2 = requiredsize2 + 10;
              if (((*patterns)[n_codes].replace =
                   (char *) realloc (old_replace, currentsize2)) == NULL)
                {
                  fprintf (stderr, "ERROR: Not enough memory for buffer (%u bytes)\n", currentsize2);
                  free (old_replace);
                  free ((*patterns)[n_codes].search);
                  free (*patterns);
                  *patterns = NULL;
                  return -1;
                }
            }
          (*patterns)[n_codes].replace[n] = (unsigned char) strtol (token, NULL, 16);
          n++;
        }
      while ((token = strtok (NULL, " ")) != NULL);
      (*patterns)[n_codes].replace_size = n;    // size in bytes

      strcpy (buffer, line);
      strtok (last, ":");
      token = strtok (NULL, ":");
      token = strtok (token, " ");
      last = token;
      if (!token)
        {
          printf ("WARNING: Line %u is invalid, no offset is specified\n", line_num);
          continue;
        }
      (*patterns)[n_codes].offset = strtol (token, NULL, 10); // yes, offset is decimal

      if (cm_verbose)
        {
          printf ("pattern:      %d\n"
                  "line:         %u\n"
                  "searchstring: ",
                  n_codes + 1, line_num);
          for (n = 0; n < (*patterns)[n_codes].search_size; n++)
            printf ("%02x ", (unsigned char) (*patterns)[n_codes].search[n]);
          printf ("(%u)\n"
                  "wildcard:     %02x\n"
                  "escape:       %02x\n"
                  "replacement:  ",
                  (unsigned) (*patterns)[n_codes].search_size,
                  (unsigned char) (*patterns)[n_codes].wildcard,
                  (unsigned char) (*patterns)[n_codes].escape);
          for (n = 0; n < (*patterns)[n_codes].replace_size; n++)
            printf ("%02x ", (unsigned char) (*patterns)[n_codes].replace[n]);
          printf ("(%u)\n"
                  "offset:       %d\n",
                  (unsigned) (*patterns)[n_codes].replace_size,
                  (*patterns)[n_codes].offset);
        }

      (*patterns)[n_codes].sets = NULL;
      currentsize2 = 0;
      requiredsize2 = 1;                        // for string terminator
      strcpy (buffer, line);
      strtok (last, ":");
      token = strtok (NULL, ":");
      last = token;
      while (token)
        {
          requiredsize2 += sizeof (st_cm_set_t);
          if (requiredsize2 > currentsize2)
            {
              st_cm_set_t *old_sets = (*patterns)[n_codes].sets;
              currentsize2 = requiredsize2 + 10 * sizeof (st_cm_set_t);
              if (((*patterns)[n_codes].sets =
                   (st_cm_set_t *) realloc (old_sets, currentsize2)) == NULL)
                {
                  fprintf (stderr, "ERROR: Not enough memory for buffer (%u bytes)\n", currentsize2);
                  free (old_sets);
                  free ((*patterns)[n_codes].replace);
                  free ((*patterns)[n_codes].search);
                  free (*patterns);
                  *patterns = NULL;
                  return -1;
                }
            }

          (*patterns)[n_codes].sets[n_sets].data = NULL;
          currentsize3 = 0;
          requiredsize3 = 1;                    // for string terminator
          n = 0;
          token = strtok (token, " ");
          do
            {
              requiredsize3++;
              if (requiredsize3 > currentsize3)
                {
                  char *old_data = (*patterns)[n_codes].sets[n_sets].data;
                  currentsize3 = requiredsize3 + 10;
                  if (((*patterns)[n_codes].sets[n_sets].data =
                       (char *) realloc (old_data, currentsize3)) == NULL)
                    {
                      fprintf (stderr, "ERROR: Not enough memory for buffer (%u bytes)\n", currentsize3);
                      free (old_data);
                      free ((*patterns)[n_codes].sets);
                      free ((*patterns)[n_codes].replace);
                      free ((*patterns)[n_codes].search);
                      free (*patterns);
                      *patterns = NULL;
                      return -1;
                    }
                }
              (*patterns)[n_codes].sets[n_sets].data[n] =
                (unsigned char) strtol (token, NULL, 16);
              n++;
            }
          while ((token = strtok (NULL, " ")) != NULL);
          (*patterns)[n_codes].sets[n_sets].size = n;

          if (cm_verbose)
            {
              fputs ("set:          ", stdout);
              for (n = 0; n < (*patterns)[n_codes].sets[n_sets].size; n++)
                printf ("%02x ", (unsigned char) (*patterns)[n_codes].sets[n_sets].data[n]);
              printf ("(%u)\n", (*patterns)[n_codes].sets[n_sets].size);
            }

          strcpy (buffer, line);
          strtok (last, ":");
          token = strtok (NULL, ":");
          last = token;

          n_sets++;
        }
      (*patterns)[n_codes].n_sets = n_sets;

      n_codes++;

      if (cm_verbose)
        fputc ('\n', stdout);
    }
  fclose (srcfile);
  return n_codes;
}


void
cleanup_cm_patterns (st_cm_pattern_t **patterns, int n_patterns)
{
  int n;

  for (n = 0; n < n_patterns; n++)
    {
      unsigned int m;

      free ((*patterns)[n].search);
      (*patterns)[n].search = NULL;
      free ((*patterns)[n].replace);
      (*patterns)[n].replace = NULL;
      for (m = 0; m < (*patterns)[n].n_sets; m++)
        {
          free ((*patterns)[n].sets[m].data);
          (*patterns)[n].sets[m].data = NULL;
        }
      free ((*patterns)[n].sets);
      (*patterns)[n].sets = NULL;
    }
  free (*patterns);
  *patterns = NULL;
}


int
gauge (time_t init_time, size_t pos, size_t size)
{
#define GAUGE_LENGTH ((uint64_t) 24)

  unsigned int curr, bps, left, percentage;
  size_t p;
  char progress[MAXBUFSIZE];

  if (pos > size || !size)
    return -1;

  if ((curr = (unsigned int) difftime (time (NULL), init_time)) == 0)
    curr = 1;                                   // "round up" to at least 1 sec (no division
                                                //  by zero below)
  bps = (unsigned int) (pos / curr);            // # bytes/second (average transfer speed)
  left = (unsigned int) (size - pos);
  left /= bps ? bps : 1;

  p = (size_t) ((GAUGE_LENGTH * pos) / size);
  *progress = '\0';
  strncat (progress, "========================", p);

  if (misc_ansi_color)
    {
      progress[p] = '\0';
      if (p < GAUGE_LENGTH)
        strcat (progress, "\x1b[31;41m");
    }

  strncat (&progress[p], "------------------------", (size_t) (GAUGE_LENGTH - p));

  percentage = (unsigned int) ((((uint64_t) 100) * pos) / size);

  printf (
    misc_ansi_color ? "\r%10u Bytes [\x1b[32;42m%s\x1b[0m] %u%%, BPS=%u, " :
      "\r%10u Bytes [%s] %u%%, BPS=%u, ", (unsigned) pos, progress, percentage, bps);

  if (pos == size)
    printf ("TOTAL=%03u:%02u", curr / 60, curr % 60); // DON'T print a newline
  else                                                //  -> gauge can be cleared
    printf ("ETA=%03u:%02u  ", left / 60, left % 60);

  fflush (stdout);

  return 0;
}


char *
getenv2 (const char *variable)
/*
  getenv() suitable for enviroments w/o HOME, TMP or TEMP variables.
  The caller should copy the returned string to its own memory, because this
  function will overwrite that memory on the next call.
  Note that this function never returns NULL.
*/
{
  char *tmp;
  static char value[MAXBUFSIZE];
  size_t len;
#ifdef  __MSDOS__
/*
  On DOS and Windows the environment variables are not stored in a case
  sensitive manner. The run-time system of DJGPP acts as if they are stored in
  upper case. Its getenv() however *is* case sensitive. We fix this by changing
  all characters of the search string (variable) to upper case.

  Note that in Cygwin's Bash environment variables *are* stored in a case
  sensitive manner.
*/
  char tmp2[MAXBUFSIZE];

  len = strnlen (variable, sizeof tmp2 - 1);
  strncpy (tmp2, variable, len)[len] = '\0';
  variable = strupr (tmp2);                     // DON'T copy the string into variable
#endif                                          //  (variable itself is local)

  *value = '\0';

  if ((tmp = getenv (variable)) != NULL)
    {
      len = strnlen (tmp, sizeof value - 1);
      strncpy (value, tmp, len)[len] = '\0';
    }
  else
    {
      if (!strcmp (variable, "HOME"))
        {
          if ((tmp = getenv ("USERPROFILE")) != NULL)
            {
              len = strnlen (tmp, sizeof value - 1);
              strncpy (value, tmp, len)[len] = '\0';
            }
          else if ((tmp = getenv ("HOMEDRIVE")) != NULL)
            {
              char *p = getenv ("HOMEPATH");

              if (!p)
                p = DIR_SEPARATOR_S;
              len = strnlen (tmp, sizeof value - 1) + strnlen (p, sizeof value - 1);
              if (len >= sizeof value)
                len = sizeof value - 1;
              snprintf (value, len + 1, "%s%s", tmp, p);
              value[len] = '\0';
            }
          else
            /*
              Don't just use C:\\ on DOS, the user might not have write access
              there (Windows NT DOS-box). Besides, it would make uCON64 behave
              differently on DOS than on the other platforms.
              Returning the current directory when none of the above environment
              variables are set can be seen as a feature. A frontend could execute
              uCON64 with an environment without any of the environment variables
              set, so that the directory from where uCON64 starts will be used.
            */
            {
              if (getcwd (value, FILENAME_MAX) != NULL)
                {
                  char c = (char) toupper ((int) *value);
                  // if current dir is root dir strip problematic ending slash (DJGPP)
                  if (c >= 'A' && c <= 'Z' &&
                      value[1] == ':' && value[2] == '/' && value[3] == '\0')
                    value[2] = '\0';
                }
              else
                *value = '\0';
            }
        }

      if (!strcmp (variable, "TEMP") || !strcmp (variable, "TMP"))
        {
#if     defined __MSDOS__ || defined __CYGWIN__
          /*
            DJGPP and (yet another) Cygwin quirck
            A trailing backslash is used to check for a directory. Normally
            DJGPP's run-time system is able to handle forward slashes in paths,
            but access() won't differentiate between files and dirs if a
            forward slash is used. Cygwin's run-time system seems to handle
            paths with forward slashes quite different from paths with
            backslashes. This trick seems to work only if a backslash is used.
          */
          if (access ("\\tmp\\", R_OK | W_OK) == 0)
#else
          // trailing file separator to force it to be a directory
          if (access (DIR_SEPARATOR_S "tmp" DIR_SEPARATOR_S, R_OK | W_OK) == 0)
#endif
            strcpy (value, DIR_SEPARATOR_S "tmp");
          else
            if (getcwd (value, FILENAME_MAX) == NULL)
              *value = '\0';
        }
    }

#ifdef  __CYGWIN__
  /*
    Under certain circumstances Cygwin's run-time system returns "/" as value
    of HOME while that var has not been set. To specify a root dir a path like
    /cygdrive/<drive letter> or simply a drive letter should be used.
  */
  if (!strcmp (variable, "HOME") && !strcmp (value, "/") &&
      getcwd (value, FILENAME_MAX) == NULL)
    *value = '\0';
#endif

  return value;
}


char *
get_property (const char *filename, const char *propname, char *buffer,
              size_t bufsize, const char *def)
{
  char *p = NULL;
  FILE *fh;
  int prop_found = 0;

  if ((fh = fopen (filename, "r")) != NULL)     // opening the file in text mode
    {                                           //  avoids trouble on DOS
      char line[MAXBUFSIZE];

      while (fgets (line, sizeof line, fh) != NULL)
        {
          size_t i, whitespace_len = strspn (line, "\t ");

          p = line + whitespace_len;            // ignore leading whitespace
          if (*p == '#' || *p == '\n' || *p == '\r')
            continue;                           // text after # is comment
          if ((p = strpbrk (line, "#\r\n")) != NULL) // strip *any* returns
            *p = '\0';

          p = strchr (line, PROPERTY_SEPARATOR);
          // if no divider was found the propname must be a bool config entry
          //  (present or not present)
          if (p)
            *p = '\0';                          // note that this "cuts" _line_
          // strip trailing whitespace from property name part of line
          {
            size_t len = strlen (line);

            for (i = len - 1;
                 i <= len - 1 && (line[i] == '\t' || line[i] == ' ');
                 i--)
              ;
          }
          line[i + 1] = '\0';

          if (!stricmp (line + whitespace_len, propname))
            {
              if (p)
                {
                  char *p2;
                  size_t len;

                  p++;
                  p2 = p + strspn (p, "\t ");
                  len = strnlen (p2, bufsize - 1);
                  // strip leading whitespace from value
                  strncpy (buffer, p2, len)[len] = '\0';
                  // strip trailing whitespace from value
                  for (i = len - 1;
                       i <= len - 1 && (buffer[i] == '\t' || buffer[i] == ' ');
                       i--)
                    ;
                  buffer[i + 1] = '\0';
                }
              prop_found = 1;
              break;                            // an environment variable
            }                                   //  might override this
        }
      fclose (fh);
    }

  p = getenv2 (propname);
  if (*p == '\0')                               // getenv2() never returns NULL
    {
      if (!prop_found)
        {
          if (def)
            {
              size_t len = strnlen (def, bufsize - 1);

              strncpy (buffer, def, len)[len] = '\0';
            }
          else
            buffer = NULL;                      // buffer won't be changed
        }                                       //  after this func (=OK)
    }
  else
    {
      size_t len = strnlen (p, bufsize - 1);

      strncpy (buffer, p, len)[len] = '\0';
    }
  return buffer;
}


int32_t
get_property_int (const char *filename, const char *propname)
{
  char buf[160];                                // 159 is enough for a *very* large number
  int32_t value = 0;

  get_property (filename, propname, buf, sizeof buf, NULL);

  if (buf[0])
    switch (tolower ((int) buf[0]))
      {
      case '0':                                 // 0
      case 'n':                                 // [Nn]o
        return 0;
      }

  value = strtol (buf, NULL, 10);
  return value ? value : 1;                     // if buf was only text like 'Yes'
}                                               //  we'll return at least 1


char *
get_property_fname (const char *filename, const char *propname, char *buffer,
                    const char *def)
// get a filename from file with name filename and expand it
{
  char tmp[FILENAME_MAX] = "";

  get_property (filename, propname, tmp, sizeof tmp, def);
  return realpath2 (tmp, buffer);
}


int
set_property (const char *filename, const char *propname, const char *value,
              const char *comment)
{
  int found = 0, file_size = 0;
  size_t result = 0;
  char line[MAXBUFSIZE], *str = NULL, *p = NULL;
  FILE *fh;
  struct stat fstate;

  if (stat (filename, &fstate) != 0)
    file_size = fstate.st_size;

  if ((str = (char *) malloc ((file_size + MAXBUFSIZE))) == NULL)
    {
      errno = ENOMEM;
      return -1;
    }
  *str = '\0';

  if ((fh = fopen (filename, "r")) != NULL)     // opening the file in text mode
    {                                           //  avoids trouble on DOS
      size_t i;
      char line2[MAXBUFSIZE];

      while (fgets (line, sizeof line, fh) != NULL)
        {
          strcpy (line2, line);
          if ((p = strpbrk (line2, PROPERTY_SEPARATOR_S "#\r\n")) != NULL)
            *p = '\0';                          // note that this "cuts" _line2_
          {
            size_t len = strlen (line2);

            for (i = len - 1;
                 i <= len - 1 && (line2[i] == '\t' || line2[i] == ' ');
                 i--)
              ;
          }
          line2[i + 1] = '\0';

          if (!stricmp (line2 + strspn (line2, "\t "), propname))
            {
              found = 1;
              if (value == NULL)
                continue;

              sprintf (line, "%s" PROPERTY_SEPARATOR_S "%s\n", propname, value);
            }
          strcat (str, line);
        }
      fclose (fh);
    }

  if (!found && value)
    {
      if (comment)
        {
          strcat (str, PROPERTY_COMMENT_S "\n" PROPERTY_COMMENT_S " ");

          for (; *comment; comment++)
            switch (*comment)
              {
              case '\r':
                break;
              case '\n':
                strcat (str, "\n" PROPERTY_COMMENT_S " ");
                break;
              default:
                p = strchr (str, '\0');
                *p = *comment;
                *(++p) = '\0';
                break;
              }

          strcat (str, "\n" PROPERTY_COMMENT_S "\n");
        }

      sprintf (line, "%s" PROPERTY_SEPARATOR_S "%s\n", propname, value);
      strcat (str, line);
    }

  if ((fh = fopen (filename, "w")) == NULL)     // open in text mode
    {
      free (str);
      return -1;
    }
  result = fwrite (str, 1, strlen (str), fh);
  fclose (fh);
  free (str);

  return (int) result;
}


char *
tmpnam2 (char *tmpname, const char *basedir)
{
  static time_t init = 0;
  const char *p = basedir ? basedir : getenv2 ("TEMP");

  if (!init)
    {
      init = time (NULL);
      srand ((unsigned) init);
    }

  *tmpname = '\0';
  do
    {
      snprintf (tmpname, FILENAME_MAX, "%s" DIR_SEPARATOR_S "%08x.tmp", p,
                (int) time (NULL) * rand ());
      tmpname[FILENAME_MAX - 1] = '\0';
    }
  while (!access (tmpname, F_OK));              // must work for files AND dirs

  return tmpname;
}


void
fread_checked (void *buffer, size_t size, size_t number, FILE *file)
{
  if (fread_checked2 (buffer, size, number, file) != 0)
    exit (1);
}


int
fread_checked2 (void *buffer, size_t size, size_t number, FILE *file)
{
  size_t result = fread (buffer, size, number, file);
  if (result != number)
    {
#if     defined _MSC_VER && defined _M_IX86
#pragma warning(push)
#pragma warning(disable: 4777)
/*
  In this case Visual Studio Community 2015 warns with: 'fprintf' : format
  string '%u' requires an argument of type 'unsigned int', but variadic
  argument 1 has type 'size_t'
  This is a bug in Visual Studio Community 2015.
*/
#endif
      fprintf (stderr,
               "ERROR: Could read only %u of %u bytes from file handle %p\n",
               (unsigned) (result * size), (unsigned) (number * size), file);
#if     defined _MSC_VER && defined _M_IX86
#pragma warning(pop)
#endif
      if (feof (file))
        fputs ("       (end of file)\n", stderr);
      else if (ferror (file))
        fputs ("       (I/O error)\n", stderr);
      else
        fputs ("       (unknown error)\n", stderr);
      return -1;
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

  if (register_func (deinit_conio) == -1)
    {
      fputs ("ERROR: Could not register function with register_func()\n", stderr);
      exit (102);
    }

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


#if     defined __unix__ && !defined __MSDOS__
int
drop_privileges (void)
{
  uid_t uid;
  gid_t gid;

  gid = getgid ();
  if (setgid (gid) == -1)
    {
      fprintf (stderr, "ERROR: Could not set group ID to %u\n", gid);
      return -1;
    }

  uid = getuid ();
  if (setuid (uid) == -1)
    {
      fprintf (stderr, "ERROR: Could not set user ID to %u\n", uid);
      return -1;
    }

  return 0;
}
#endif


int
register_func (void (*func) (void))
{
  st_func_node_t *new_node;

  if ((new_node = (st_func_node_t *) malloc (sizeof (st_func_node_t))) == NULL)
    return -1;

  new_node->func = func;
  new_node->next = func_list.next;
  func_list.next = new_node;
  return 0;
}


int
unregister_func (void (*func) (void))
{
  st_func_node_t *func_node = &func_list, *prev_node = &func_list;

  while (func_node->next != NULL && func_node->func != func)
    {
      prev_node = func_node;
      func_node = func_node->next;
    }
  if (func_node->func != func)
    return -1;

  if (!func_list_locked)
    {
      prev_node->next = func_node->next;
      free (func_node);
      return 0;
    }
  else
    return -1;
}


void
handle_registered_funcs (void)
{
  st_func_node_t *func_node = &func_list;

  func_list_locked = 1;
  while (func_node->next != NULL)
    {
      func_node = func_node->next;              // first node's func is NULL
      if (func_node->func != NULL)
        func_node->func ();
    }
  func_list_locked = 0;
}


#ifndef HAVE_BYTESWAP_H
uint16_t
bswap_16 (uint16_t x)
{
#if 1
  uint8_t *ptr = (uint8_t *) &x, tmp;
  tmp = ptr[0];
  ptr[0] = ptr[1];
  ptr[1] = tmp;
  return x;
#else
  return (((x) & 0x00ff) << 8 | ((x) & 0xff00) >> 8);
#endif
}


uint32_t
bswap_32 (uint32_t x)
{
#if 1
  uint8_t *ptr = (uint8_t *) &x, tmp;
  tmp = ptr[0];
  ptr[0] = ptr[3];
  ptr[3] = tmp;
  tmp = ptr[1];
  ptr[1] = ptr[2];
  ptr[2] = tmp;
  return x;
#else
  return ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |
          (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24));
#endif
}


uint64_t
bswap_64 (uint64_t x)
{
  uint8_t *ptr = (uint8_t *) &x, tmp;
  tmp = ptr[0];
  ptr[0] = ptr[7];
  ptr[7] = tmp;
  tmp = ptr[1];
  ptr[1] = ptr[6];
  ptr[6] = tmp;
  tmp = ptr[2];
  ptr[2] = ptr[5];
  ptr[5] = tmp;
  tmp = ptr[3];
  ptr[3] = ptr[4];
  ptr[4] = tmp;
  return x;
}
#endif // #ifndef HAVE_BYTESWAP_H


void
wait2 (unsigned int nmillis)
{
#ifdef  __MSDOS__
  delay (nmillis);
#elif   defined _WIN32
  Sleep (nmillis);
#elif   defined HAVE_CLOCK_NANOSLEEP            // see comment about usleep()
#if 0 // very inaccurate, but sleeping implementation
  struct timespec end;

  clock_gettime (CLOCK_MONOTONIC, &end);
  end.tv_sec += nmillis / 1000;
  end.tv_nsec += (nmillis % 1000) * 1000000;
  if (end.tv_nsec > 999999999)
    {
      end.tv_sec++;
      end.tv_nsec -= 1000000000;
    }

  while (clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &end, NULL) == EINTR)
    ;
#else // busy-waiting, but much more accurate
  struct timespec end, current;

  clock_gettime (CLOCK_MONOTONIC, &end);
  end.tv_sec += nmillis / 1000;
  end.tv_nsec += (nmillis % 1000) * 1000000;
  if (end.tv_nsec > 999999999)
    {
      end.tv_sec++;
      end.tv_nsec -= 1000000000;
    }

  do
    {
      clock_gettime (CLOCK_MONOTONIC, &current);
      current.tv_sec -= end.tv_sec;
      current.tv_nsec -= end.tv_nsec;
      if (current.tv_nsec < 0)
        {
          current.tv_sec--;
          current.tv_nsec += 1000000000;
        }
    }
  while (current.tv_sec < 0);
#endif
#elif   defined __unix__ || defined __APPLE__   // Mac OS X actually
  // NOTE: Apparently usleep() was deprecated in POSIX.1-2001 and removed from
  //       POSIX.1-2008. clock_nanosleep() will likely give better results.
  usleep (nmillis * 1000);
#elif   defined __BEOS__
  snooze (nmillis * 1000);
#elif   defined AMIGA
  Delay (nmillis > 20 ? nmillis / 20 : 1);      // 50Hz clock, so interval is 20ms
#else
#ifdef  __GNUC__
#warning Please provide a wait2() implementation
#else
#pragma message ("Please provide a wait2() implementation")
#endif
  volatile int n;
  for (n = 0; n < nmillis * 65536; n++)
    ;
#endif
}


char *
q_fbackup (const char *filename, int mode)
{
  static char buf[FILENAME_MAX];
  size_t len;

  if (access (filename, R_OK) != 0)
    return (char *) filename;

  len = strnlen (filename, sizeof buf - 1);
  strncpy (buf, filename, len)[len] = '\0';
  set_suffix (buf, ".BAK");
  if (stricmp (filename, buf) != 0)
    remove (buf);                               // *try* to remove or rename() will fail
  else
    {                                           // handle the case where filename has the suffix ".BAK"
      char *dir = dirname2 (filename), buf2[FILENAME_MAX];

      if (dir == NULL)
        {
          fputs ("INTERNAL ERROR: dirname2() returned NULL\n", stderr);
          exit (1);
        }
      strcpy (buf2, dir);
      free (dir);
      tmpnam2 (buf, buf2);
    }
  if (rename (filename, buf))                   // keep file attributes like date, etc.
    {
      fprintf (stderr, "ERROR: Cannot rename \"%s\" to \"%s\"\n", filename, buf);
      exit (1);
    }

  switch (mode)
    {
    case BAK_MOVE:
      return buf;

    case BAK_DUPE:
    default:
      if (q_fcpy (buf, 0, q_fsize (buf), filename, "wb"))
        {
          fprintf (stderr, "ERROR: Cannot open \"%s\" for writing\n", filename);
          exit (1);
        }
      return buf;
    }
}


int
q_fcpy (const char *src, size_t start, size_t len, const char *dest, const char *mode)
{
  size_t seg_len;
  char buf[MAXBUFSIZE];
  FILE *fh, *fh2;

  if (one_file (dest, src))                     // other code depends on this
    return -1;                                  //  behavior!

  if ((fh = fopen (src, "rb")) == NULL)
    {
      errno = ENOENT;
      return -1;
    }
  if ((fh2 = fopen (dest, mode)) == NULL)
    {
      errno = ENOENT;
      fclose (fh);
      return -1;
    }

  fseek (fh, (long) start, SEEK_SET);
  fseek (fh2, 0, SEEK_END);

  for (; len > 0; len -= seg_len)
    {
      if ((seg_len = fread (buf, 1, MIN (len, MAXBUFSIZE), fh)) == 0)
        break;
      fwrite (buf, 1, seg_len, fh2);
    }

  fclose (fh);
  fclose (fh2);
  return 0;
}


int
q_rfcpy (const char *src, const char *dest)
// Raw file copy function. Raw, because it will copy the file data as it is,
//  unlike q_fcpy().
{
#ifdef  USE_ZLIB
#undef  fopen
#undef  fread
#undef  fwrite
#undef  fclose
#endif
  FILE *fh, *fh2;
  size_t seg_len;
  char buf[MAXBUFSIZE];

  if (one_file (dest, src))
    return -1;

  if ((fh = fopen (src, "rb")) == NULL)
    return -1;
  if ((fh2 = fopen (dest, "wb")) == NULL)
    {
      fclose (fh);
      return -1;
    }
  while ((seg_len = fread (buf, 1, MAXBUFSIZE, fh)) != 0)
    fwrite (buf, 1, seg_len, fh2);

  fclose (fh);
  fclose (fh2);
  return 0;
#ifdef  USE_ZLIB
#define fopen   fopen2
#define fread   fread2
#define fwrite  fwrite2
#define fclose  fclose2
#endif
}


int
q_fswap (const char *filename, size_t start, size_t len, swap_t type)
{
  size_t seg_len;
  FILE *fh;
  char buf[MAXBUFSIZE];
  struct stat fstate;

  // first (try to) change the file mode or we won't be able to write to it if
  //  it's a read-only file
  stat (filename, &fstate);
  if (chmod (filename, fstate.st_mode | S_IWUSR))
    {
      errno = EACCES;
      return -1;
    }

  if ((fh = fopen (filename, "r+b")) == NULL)
    {
      errno = ENOENT;
      return -1;
    }

  fseek (fh, (long) start, SEEK_SET);

  for (; len > 0; len -= seg_len)
    {
      if ((seg_len = fread (buf, 1, MIN (len, MAXBUFSIZE), fh)) == 0)
        break;
      if (type == SWAP_BYTE)
        mem_swap_b (buf, seg_len);
      else                                      // SWAP_WORD
        mem_swap_w (buf, seg_len);
      fseek (fh, -(long) seg_len, SEEK_CUR);
      fwrite (buf, 1, seg_len, fh);
      /*
        This appears to be a bug in DJGPP and Solaris. Without an extra call to
        fseek() a part of the file won't be swapped (DJGPP: after 8 MB, Solaris:
        after 12 MB).
      */
      fseek (fh, 0, SEEK_CUR);
    }

  fclose (fh);

  return 0;
}


int
q_fncmp (const char *filename, size_t start, size_t len, const char *search,
         size_t searchlen, int wildcard)
{
#define BUFSIZE 8192
  char buf[BUFSIZE];
  FILE *fh;
  size_t seg_len, searchpos, filepos = 0, matchlen = 0;

  if ((fh = fopen (filename, "rb")) == NULL)
    {
      errno = ENOENT;
      return -1;
    }
  fseek (fh, (long) start, SEEK_SET);
  filepos = start;

  while ((seg_len = fread (buf, 1, BUFSIZE + filepos > start + len ?
                             start + len - filepos : BUFSIZE, fh)) != 0)
    {
      size_t maxsearchlen = searchlen - matchlen;
      for (searchpos = 0; searchpos <= seg_len; searchpos++)
        {
          if (searchpos + maxsearchlen >= seg_len)
            maxsearchlen = seg_len - searchpos;
          if (!memwcmp (buf + searchpos, search + matchlen, maxsearchlen, wildcard))
            {
              if (matchlen + maxsearchlen < searchlen)
                {
                  matchlen += maxsearchlen;
                  break;
                }
              else
                {
                  fclose (fh);
                  return (int) (filepos + searchpos - matchlen);
                }
            }
          else
            matchlen = 0;
        }
      filepos += seg_len;
    }

  fclose (fh);
  return -1;
}


int
quick_io (void *buffer, size_t start, size_t len, const char *filename,
          const char *mode)
{
  size_t result;
  FILE *fh;

  if ((fh = fopen (filename, mode)) == NULL)
    {
#ifdef  DEBUG
      fprintf (stderr, "ERROR: Could not open \"%s\" in mode \"%s\"\n"
                       "CAUSE: %s\n", filename, mode, strerror (errno));
#endif
      return -1;
    }

#ifdef DEBUG
  fprintf (stderr, "\"%s\": \"%s\"\n", filename, (char *) mode);
#endif

  fseek (fh, (long) start, SEEK_SET);

  // Note the order of arguments of fread() and fwrite(). Now quick_io()
  //  returns the number of characters read or written. Some code relies on
  //  this behavior!
  if (*mode == 'r' && mode[1] != '+')           // "r+b" always writes
    result = fread (buffer, 1, len, fh);
  else
    result = fwrite (buffer, 1, len, fh);

  fclose (fh);
  return (int) result;
}


int
quick_io_c (int value, size_t start, const char *filename, const char *mode)
{
  int result;
  FILE *fh;

  if ((fh = fopen (filename, mode)) == NULL)
    {
#ifdef  DEBUG
      fprintf (stderr, "ERROR: Could not open \"%s\" in mode \"%s\"\n"
                       "CAUSE: %s\n", filename, mode, strerror (errno));
#endif
      return -1;
    }

#ifdef  DEBUG
  fprintf (stderr, "\"%s\": \"%s\"\n", filename, (char *) mode);
#endif

  fseek (fh, (long) start, SEEK_SET);

  if (*mode == 'r' && mode[1] != '+')           // "r+b" always writes
    result = fgetc (fh);
  else
    result = fputc (value, fh);

  fclose (fh);
  return result;
}


#if 0
int
process_file (const char *src, size_t start, size_t len, const char *dest,
              const char *mode, int (*func) (char *, size_t))
{
  size_t seg_len;
  char buf[MAXBUFSIZE];
  FILE *fh, *fh2;

  if (one_file (dest, src))
    return -1;

  if (!(fh = fopen (src, "rb")))
    {
      errno = ENOENT;
      return -1;
    }
  if (!(fh2 = fopen (dest, mode)))
    {
      errno = ENOENT;
      fclose (fh);
      return -1;
    }

  fseek (fh, (long) start, SEEK_SET);
  fseek (fh2, 0, SEEK_END);

  for (; len > 0; len -= seg_len)
    {
      if (!(seg_len = fread (buf, 1, MIN (len, MAXBUFSIZE), fh)))
        break;
      func (buf, seg_len);
      fwrite (buf, 1, seg_len, fh2);
    }

  fclose (fh);
  fclose (fh2);

  return 0;
}
#endif


int
strarg (char **argv, char *str, const char *separator_s, int max_args)
{
#ifdef  DEBUG
  int pos = 0;
#endif
  int argc = 0;

  if (str && *str)
    for (; argc < max_args - 1 &&
           (argv[argc] = (char *) strtok (!argc ? str : NULL, separator_s)) != NULL;
         argc++)
      ;

#ifdef  DEBUG
  fprintf (stderr, "argc:     %d\n", argc);
  for (pos = 0; pos < argc; pos++)
    fprintf (stderr, "argv[%d]:  %s\n", pos, argv[pos]);

  fflush (stderr);
#endif

  return argc;
}


#ifdef  _WIN32
int
truncate (const char *path, off_t size)
{
  int retval;
  HANDLE file = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return -1;

  SetFilePointer (file, size, NULL, FILE_BEGIN);
  retval = SetEndOfFile (file);                 // returns nonzero on success
  CloseHandle (file);

  return retval ? 0 : -1;                       // truncate() returns zero on success
}


int
sync (void)
{
  _commit (fileno (stdout));
  _commit (fileno (stderr));
  fflush (NULL);                                // flushes all streams opened for output
  return 0;
}


#elif   defined AMIGA                           // _WIN32
int
truncate (const char *path, off_t size)
{
  BPTR fh;
  ULONG newsize;

  if (!(fh = Open (path, MODE_OLDFILE)))
    return -1;

  newsize = SetFileSize (fh, size, OFFSET_BEGINNING);
  Close (fh);

  return newsize == (ULONG) size ? 0 : -1;      // truncate() returns zero on success
}


int
chmod (const char *path, mode_t mode)
{
  if (!SetProtection ((STRPTR) path,
                      ((mode & S_IRUSR ? 0 : FIBF_READ) |
                       (mode & S_IWUSR ? 0 : FIBF_WRITE | FIBF_DELETE) |
                       (mode & S_IXUSR ? 0 : FIBF_EXECUTE) |
                       (mode & S_IRGRP ? FIBF_GRP_READ : 0) |
                       (mode & S_IWGRP ? FIBF_GRP_WRITE | FIBF_GRP_DELETE : 0) |
                       (mode & S_IXGRP ? FIBF_GRP_EXECUTE : 0) |
                       (mode & S_IROTH ? FIBF_OTR_READ : 0) |
                       (mode & S_IWOTH ? FIBF_OTR_WRITE | FIBF_OTR_DELETE : 0) |
                       (mode & S_IXOTH ? FIBF_OTR_EXECUTE : 0))))
    return -1;
  else
    return 0;
}


void
sync (void)
{
}


int
readlink (const char *path, char *buf, int bufsize)
{
  (void) path;                                  // warning remover
  (void) buf;                                   // idem
  (void) bufsize;                               // idem
  // always return -1 as if anything passed to it isn't a soft link
  return -1;
}


// custom _popen() and _pclose(), because the standard ones (named popen() and
//  pclose()) are buggy
FILE *
_popen (const char *path, const char *mode)
{
  int fd;
  BPTR fh;
  long fhflags;
  char *apipe = malloc (strlen (path) + 7);
  if (!apipe)
    return NULL;

  sprintf (apipe, "PIPE:%08lx.%08lx", (ULONG) FindTask (NULL), (ULONG) time (NULL));

  if (*mode == 'w')
    fhflags = MODE_NEWFILE;
  else
    fhflags = MODE_OLDFILE;

  if (fh = Open (apipe, fhflags))
    {
      switch (SystemTags(path, SYS_Input, Input(), SYS_Output, fh, SYS_Asynch,
                TRUE, SYS_UserShell, TRUE, NP_CloseInput, FALSE, TAG_END))
        {
        case 0:
          return fopen (apipe, mode);
          break;
        case -1:
          Close (fh);
          return NULL;
          break;
        default:
          return NULL;
          break;
        }
    }
  return NULL;
}


int
_pclose (FILE *stream)
{
  if (stream)
    return fclose (stream);
  else
    return -1;
}
#endif                                          // AMIGA
