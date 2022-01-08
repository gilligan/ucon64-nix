/*
archive.h - g(un)zip and unzip support

Copyright (c) 2001 - 2004, 2015 - 2017, 2019 - 2021 dbjh


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
#ifndef MISC_ARCHIVE_H
#define MISC_ARCHIVE_H

#ifdef  HAVE_CONFIG_H
#include "config.h"                             // USE_ZLIB
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef  USE_ZLIB
// make sure ZLIB support is enabled everywhere
//#warning USE_ZLIB is defined

#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif
#include <stdio.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include "misc/itypes.h"                        // int64_t, uint64_t
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include "misc/unzip.h"
#ifdef  _MSC_VER
#pragma warning(pop)
#endif


extern FILE *fopen2 (const char *filename, const char *mode);
extern int fclose2 (FILE *file);
extern int fseek2 (FILE *file, long offset, int mode);
extern int fseeko2 (FILE *file, int64_t offset, int mode);
extern size_t fread2 (void *buffer, size_t size, size_t number, FILE *file);
extern int fgetc2 (FILE *file);
extern char *fgets2 (char *buffer, int maxlength, FILE *file);
extern int feof2 (FILE *file);
extern size_t fwrite2 (const void *buffer, size_t size, size_t number, FILE *file);
extern int fputc2 (int character, FILE *file);
extern long ftell2 (FILE *file);
extern int64_t ftello2 (FILE *file);
extern void rewind2 (FILE *file);
extern FILE *popen2 (const char *command, const char *mode);
extern int pclose2 (FILE *stream);

extern uint64_t fsizeof (const char *filename);

#define fopen(FILE, MODE) fopen2(FILE, MODE)
#define fclose(FILE) fclose2(FILE)
#define fseek(FILE, OFFSET, MODE) fseek2(FILE, OFFSET, MODE)
#define fread(BUF, SIZE, NUM, FILE) fread2(BUF, SIZE, NUM, FILE)
#define fgetc(FILE) fgetc2(FILE)
#undef  fgets                                   // necessary on (at least) NetBSD
#define fgets(BUF, MAXLEN, FILE) fgets2(BUF, MAXLEN, FILE)
#undef  feof                                    // necessary for (at least) Cygwin
#define feof(FILE) feof2(FILE)
#define fwrite(BUF, SIZE, NUM, FILE) fwrite2(BUF, SIZE, NUM, FILE)
#define fputc(CHAR, FILE) fputc2(CHAR, FILE)
#define ftell(FILE) ftell2(FILE)
#define rewind(FILE) rewind2(FILE)
#undef  popen
#define popen(COMMAND, MODE) popen2(COMMAND, MODE)
#undef  pclose
#define pclose(FILE) pclose2(FILE)

// Returns the number of files in the "central dir of this disk" or -1 if
//  filename is not a ZIP file or an error occured.
extern ZPOS64_T unzip_get_number_entries (const char *filename);
extern int unzip_goto_file (unzFile file, ZPOS64_T file_index);
extern ZPOS64_T unzip_current_file_nr;
#endif // USE_ZLIB

#ifdef  __cplusplus
}
#endif
#endif // MISC_ARCHIVE_H
