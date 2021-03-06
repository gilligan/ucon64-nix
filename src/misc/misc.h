/*
misc.h - miscellaneous functions

Copyright (c) 1999 - 2008              NoisyB
Copyright (c) 2001 - 2005, 2015 - 2021 dbjh
Copyright (c) 2002 - 2004              Jan-Erik Karlsson (Amiga code)


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
#ifndef MISC_H
#define MISC_H

#ifdef  HAVE_CONFIG_H
#include "config.h"                             // USE_ZLIB, USE_ANSI_COLOR support
#endif
#ifdef  __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <string.h>
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <time.h>                               // bytes_per_second() requires time()
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include "misc/itypes.h"                        // uint64_t


#ifdef  __sun
#ifdef  __SVR4
#define __solaris__
#endif
#endif

#ifdef  WORDS_BIGENDIAN
#undef  WORDS_BIGENDIAN
#endif

#if     defined _LIBC || defined __GLIBC__
  #include <endian.h>
  #if     __BYTE_ORDER == __BIG_ENDIAN
    #define WORDS_BIGENDIAN 1
  #endif
#elif   defined __BYTE_ORDER__ && defined __ORDER_BIG_ENDIAN__
  #if     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define WORDS_BIGENDIAN 1
  #endif
#elif   defined AMIGA || defined __sparc__ || defined __BIG_ENDIAN__ || \
        defined __APPLE__
  #define WORDS_BIGENDIAN 1
#endif

#ifdef  __MSDOS__                               // __MSDOS__ must come before __unix__,
  #define CURRENT_OS_S "MS-DOS"                 //  because DJGPP defines both
#elif   defined __unix__
  #ifdef  __CYGWIN__
    #ifdef  __x86_64__                          // _WIN64 is defined in a header
      #define CURRENT_OS_S "Win64 (Cygwin)"     //  file (include windows.h)
    #else
      #define CURRENT_OS_S "Win32 (Cygwin)"
    #endif
  #elif   defined __FreeBSD__
    #define CURRENT_OS_S "UNIX (FreeBSD)"
  #elif   defined __OpenBSD__
    #define CURRENT_OS_S "UNIX (OpenBSD)"
  #elif   defined __NetBSD__
    #define CURRENT_OS_S "UNIX (NetBSD)"
  #elif   defined __linux__
    #define CURRENT_OS_S "UNIX (Linux)"
  #elif   defined __solaris__
    #ifdef  __sparc__
      #define CURRENT_OS_S "UNIX (Solaris/Sparc)"
    #else
      #define CURRENT_OS_S "UNIX (Solaris/i386)"
    #endif
  #else
    #define CURRENT_OS_S "UNIX"
  #endif
#elif   defined _WIN32
  #ifdef  __MINGW64__
    #define CURRENT_OS_S "Win64 (MinGW-w64)"
  #elif   defined __MINGW32__
    #define CURRENT_OS_S "Win32 (MinGW)"
  #elif   defined _MSC_VER
    #ifdef  _WIN64
      #define CURRENT_OS_S "Win64 (Visual C++)"
    #else
      #define CURRENT_OS_S "Win32 (Visual C++)"
    #endif
  #else
    #define CURRENT_OS_S "Win32"
  #endif
#elif   defined __APPLE__
  #if     defined __POWERPC__ || defined __ppc__
    #define CURRENT_OS_S "Mac OS X (PPC)"
  #else
    #define CURRENT_OS_S "macOS"                // first Mac OS X, then OS X,
  #endif                                        //  now macOS
#elif   defined __BEOS__
  #define CURRENT_OS_S "BeOS"
#elif   defined AMIGA
  #if     defined __PPC__
    #define CURRENT_OS_S "Amiga (PPC)"
  #else
    #define CURRENT_OS_S "Amiga (68K)"
  #endif
#else
  #define CURRENT_OS_S "?"
#endif

/*
  dumper()    dump n bytes of buffer
                you can use here a virtual_start for the displayed counter
                DUMPER_HEX
                dump in hex (base: 16) (default)
                DUMPER_BIT
                dump in bits (base: 2)
                DUMPER_CODE
                dump as C code
                DUMPER_PRINT
                printf() buffer after chars passed isprint() and isspace()
                other chars will be printed as dots '.'
                do NOT use DUMPER_PRINT in uCON64 code - dbjh
                DUMPER_HEX_COUNT
                show position as hex value (default)
                DUMPER_DEC_COUNT
                show position as decimal value
                DUMPER_HEX_UPPER
                show hex values in uppercase (default: lowercase)
*/
#define DUMPER_HEX       (0)
#define DUMPER_HEX_COUNT (0)
#define DUMPER_BIT       (1)
#define DUMPER_CODE      (1 << 1)
#define DUMPER_PRINT     (1 << 2)
#define DUMPER_DEC       (1 << 3)
#define DUMPER_DEC_COUNT (1 << 4)
#define DUMPER_DEFAULT   (DUMPER_HEX_COUNT|DUMPER_HEX)
//#define DUMPER_HEX_UPPER (1 << 5)
extern void dumper (FILE *output, const void *buffer, size_t bufferlen,
                    uint64_t virtual_start, unsigned int flags);

/*
  Misc stuff

  change_mem{2}() see header of implementation for usage
  build_cm_patterns() helper function for change_mem2() to read search patterns
                  from a file
  cleanup_cm_patterns() helper function for build_cm_patterns() to free all
                  memory allocated for a (list of) st_pattern_t structure(s)
  bytes_per_second() returns bytes per second (useful in combination with
                  gauge())
  misc_percent()  returns percentage of progress (useful in combination with
                  gauge())
  drop_privileges() switch to the real user and group ID (leave "root mode")
  drop_privileges_temp() change only effective user and group ID to the real
                  user and group ID respectively, thus retaining the option to
                  regain privileges (if any)
  regain_privileges() regain root privileges after drop_privileges_temp() if
                  setuid root
  register_func() atexit() replacement
                  returns -1 if it fails, 0 if it was successful
  unregister_func() unregisters a previously registered function
                  returns -1 if it fails, 0 if it was successful
  handle_registered_funcs() calls all the registered functions
  wait2()         wait (sleep) a specified number of milliseconds
  microwait()     wait (sleep) a specified number of microseconds
  getenv2()       getenv() clone for enviroments w/o HOME, TMP or TEMP variables
*/
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
typedef struct st_cm_set
{
  char *data;
  unsigned int size;
} st_cm_set_t;

typedef struct st_cm_pattern
{
  char *search, wildcard, escape, *replace;
  size_t search_size, replace_size;
  unsigned int n_sets;
  int offset;
  st_cm_set_t *sets;
} st_cm_pattern_t;
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

extern int change_mem (char *buf, size_t bufsize, const char *searchstr,
                       size_t strsize, char wc, char esc, const char *newstr,
                       size_t newsize, int offset, ...);
extern int change_mem2 (char *buf, size_t bufsize, const char *searchstr,
                        size_t strsize, char wc, char esc, const char *newstr,
                        size_t newsize, int offset, const st_cm_set_t *sets);
extern int build_cm_patterns (st_cm_pattern_t **patterns, const char *filename);
extern void cleanup_cm_patterns (st_cm_pattern_t **patterns, int n_patterns);
extern int cm_verbose;

extern unsigned int bytes_per_second (time_t start_time, size_t nbytes);
extern unsigned int misc_percent (size_t pos, size_t len);
#if     (defined __unix__ && !defined __MSDOS__) || defined __APPLE__
extern int drop_privileges (void);
extern int drop_privileges_temp (void);
extern int regain_privileges (void);
#endif
extern int register_func (void (*func) (void));
extern int unregister_func (void (*func) (void));
extern void handle_registered_funcs (void);
extern void wait2 (unsigned int nmillis);
#if     (defined __unix__ && !defined __MSDOS__) || defined __APPLE__ || \
        defined __BEOS__ || defined _WIN32
extern void microwait (unsigned int nmicros);
#endif
extern char *getenv2 (const char *variable);
extern int misc_digits (unsigned long value);


/*
  Portability and Fixes

  popen()
  pclose()
*/
#ifdef  _WIN32
// Note that _WIN32 is defined by cl.exe while the other constants (like WIN32)
//  are defined in header files. MinGW's gcc.exe defines all constants.

// For MinGW popen() and pclose() are unavailable for DLLs. For DLLs _popen()
//  and _pclose() should be used. Visual C++ only has the latter two.
#ifndef pclose                                  // archive.h's definition gets higher precedence
#define pclose _pclose
#endif
#ifndef popen                                   // idem
#define popen _popen
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#include <sys/stat.h>
#pragma warning(pop)

#define S_IWUSR _S_IWRITE
#define S_IRUSR _S_IREAD
#define S_ISDIR(mode) ((mode) & _S_IFDIR ? 1 : 0)
#define S_ISREG(mode) ((mode) & _S_IFREG ? 1 : 0)

#define F_OK 00
#define W_OK 02
#define R_OK 04
#define X_OK R_OK                               // this is correct for dirs, but not for exes

#define STDIN_FILENO (fileno (stdin))
#define STDOUT_FILENO (fileno (stdout))
#define STDERR_FILENO (fileno (stderr))

#else
#ifdef  DLL
#define access _access
#define chdir _chdir
#define chmod _chmod
#define getcwd _getcwd
#define isatty _isatty
#ifndef USE_ZLIB
#define snprintf _snprintf
#endif
#define stat _stat
#define strdup _strdup
#define stricmp _stricmp
#define strnicmp _strnicmp
#endif // DLL

#endif // _MSC_VER
#define snprintf _snprintf

#endif // _WIN32

#ifdef  AMIGA
// The compiler used by Jan-Erik doesn't have snprintf().
#include "misc/snprintf.h"

// custom _popen() and _pclose(), because the standard ones (named popen() and
//  pclose()) are buggy
#ifndef pclose                                  // archive.h's definition gets higher precedence
#define pclose _pclose
#endif
#ifndef popen                                   // idem
#define popen _popen
#endif

extern FILE *_popen (const char *path, const char *mode);
extern int _pclose (FILE *stream);
#endif


#ifdef  __cplusplus
}
#endif

#endif // MISC_H
