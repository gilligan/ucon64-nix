/*
archive.c - g(un)zip and unzip support

Copyright (c) 2001 - 2004, 2015 - 2021 dbjh


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

#ifdef  USE_ZLIB

#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif
#include <errno.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include <stdlib.h>
#include <string.h>
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <sys/stat.h>
#include <zlib.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include "misc/archive.h"
#include "misc/map.h"


#ifdef  MAXBUFSIZE
#undef  MAXBUFSIZE
#endif  // MAXBUFSIZE
#define MAXBUFSIZE 32768


uint64_t
fsizeof (const char *filename)
// If USE_ZLIB is defined this function is very slow. Please avoid to use
//  it much.
{
  FILE *file;
  unsigned char magic[4] = { 0 };

  errno = 0;
#undef  fopen
#undef  fread
#undef  fclose
  if ((file = fopen (filename, "rb")) != NULL)
    {
      size_t nbytes = fread (magic, 1, sizeof magic, file);
      (void) nbytes;
      fclose (file);
    }
#define fopen   fopen2
#define fclose  fclose2
#define fread   fread2

  if (magic[0] == 0x1f && magic[1] == 0x8b && magic[2] == 0x08)
    {                                   // ID1, ID2 and CM. gzip uses Compression Method 8
      z_off64_t size;

      // shouldn't fail because we could open it with fopen()
      if ((file = (FILE *) gzopen (filename, "rb")) == NULL)
        return (uint64_t) -1;
#if 1
      // this is not much faster than the method below
      while (!gzeof ((gzFile) file))
        {
#ifdef  Z_LARGE64
          gzseek64 ((gzFile) file, 1024 * 1024, SEEK_CUR);
#else
          gzseek ((gzFile) file, 1024 * 1024, SEEK_CUR);
#endif
          gzgetc ((gzFile) file);       // necessary in order to set EOF (zlib 1.2.8)
        }
#else
      // Is there a more efficient way to determine the uncompressed size?
      unsigned char buf[MAXBUFSIZE];
      while (gzread ((gzFile) file, buf, MAXBUFSIZE) > 0)
        ;
#endif
#ifdef  Z_LARGE64
      size = gztell64 ((gzFile) file);
#else
      size = gztell ((gzFile) file);
#endif
      gzclose ((gzFile) file);
      {
        uint64_t size2 = 0;
        size_t n;

        // avoid sign bit extension (z_off64_t is a signed type)
        for (n = 0; n < sizeof size / 4; ++n)
          size2 |= size & (0xffffffffULL << (n * 32));
        return size2;
      }
    }
  else if (magic[0] == 'P' && magic[1] == 'K' && magic[2] == 0x03 && magic[3] == 0x04)
    {
      unz_file_info64 info;

      if ((file = (FILE *) unzOpen (filename)) == NULL)
        {
          errno = ENOENT;               // unzOpen() does not set errno
          return (uint64_t) -1;
        }
      if (unzip_goto_file (file, unzip_current_file_nr) != UNZ_OK)
        {
          unzClose (file);
          errno = ENOENT;
          return (uint64_t) -1;
        }
      unzGetCurrentFileInfo64 (file, &info, NULL, 0, NULL, 0, NULL, 0);
      unzClose (file);
      return info.uncompressed_size;
    }
  else
    {
#ifdef  _WIN32
      struct _stati64 fstate;
      if (!_stati64 (filename, &fstate)) // _stat64() is not available on Windows 98
        return fstate.st_size;
#else
      struct stat fstate;
      if (!stat (filename, &fstate))
        {
          uint64_t size = 0;
          size_t n;

          // avoid sign bit extension if sizeof fstate.st_size < sizeof (uint64_t)
          for (n = 0; n < sizeof fstate.st_size / 4; ++n)
            size |= fstate.st_size & (0xffffffffULL << (n * 32));
          return size;
        }
#endif
      return (uint64_t) -1;
    }
}


/*
  The zlib functions gzwrite() and gzputc() write compressed data if the file
  was opened with a "w" in the mode string, but we want to control whether we
  write compressed data. Note that for the mode string "w+", zlib ignores the
  '+'. zlib does the same for "r+".
*/
static st_map_t *fh_map = NULL;                 // associative array: file handle -> file mode

typedef enum { FM_NORMAL, FM_GZIP, FM_ZIP, FM_UNDEF } fmode2_t;

typedef struct st_finfo
{
  fmode2_t fmode;
  int compressed;
} st_finfo_t;

static st_finfo_t finfo_list[6] = { {FM_NORMAL, 0},
                                    {FM_NORMAL, 1},     // should never be used
                                    {FM_GZIP, 0},
                                    {FM_GZIP, 1},
                                    {FM_ZIP, 0},        // should never be used
                                    {FM_ZIP, 1} };

ZPOS64_T unzip_current_file_nr = 0;


static void
init_fh_map (void)
{
  fh_map = map_create (20);                     // 20 simultaneous open files
  map_put (fh_map, stdin, &finfo_list[0]);      //  should be enough to start with
  map_put (fh_map, stdout, &finfo_list[0]);
  map_put (fh_map, stderr, &finfo_list[0]);
}


static st_finfo_t *
get_finfo (FILE *file)
{
  st_finfo_t *finfo;

  if (fh_map == NULL)
    init_fh_map ();
  if ((finfo = (st_finfo_t *) map_get (fh_map, file)) == NULL)
    {
      fprintf (stderr, "\nINTERNAL ERROR: File pointer was not present in map (%p)\n", file);
      map_dump (fh_map);
      exit (1);
    }
  return finfo;
}


ZPOS64_T
unzip_get_number_entries (const char *filename)
{
  FILE *file;
  unsigned char magic[4] = { 0 };
  size_t nbytes;

#undef  fopen
#undef  fread
#undef  fclose
  if ((file = fopen (filename, "rb")) == NULL)
    return (ZPOS64_T) -1;
  nbytes = fread (magic, 1, sizeof magic, file);
  (void) nbytes;
  fclose (file);
#define fopen   fopen2
#define fclose  fclose2
#define fread   fread2

  if (magic[0] == 'P' && magic[1] == 'K' && magic[2] == 0x03 && magic[3] == 0x04)
    {
      unz_global_info64 info;

      file = (FILE *) unzOpen (filename);
      if (unzGetGlobalInfo64 (file, &info) != UNZ_OK)
        return (ZPOS64_T) -1;
      unzClose (file);
      return info.number_entry;
    }
  else
    return (ZPOS64_T) -1;
}


int
unzip_goto_file (unzFile file, ZPOS64_T file_index)
{
  int retval = unzGoToFirstFile (file);

  if (file_index > 0 && retval == UNZ_OK)
    {
      ZPOS64_T n = 0;
      while (n < file_index && retval == UNZ_OK) // stop if UNZ_END_OF_LIST_OF_FILE
        {
          retval = unzGoToNextFile (file);
          n++;
        }
    }
  return retval;
}


static int
unzip_seek_helper (FILE *file, ZPOS64_T offset)
{
  char buffer[MAXBUFSIZE];
  ZPOS64_T n, pos = unztell64 (file);           // returns file position of the "current file"

  if (pos == offset)
    return 0;
  else if (pos > offset)
    {
      unzCloseCurrentFile (file);
      if (unzip_goto_file (file, unzip_current_file_nr) != UNZ_OK ||
          unzOpenCurrentFile (file) != UNZ_OK)
        return -1;
      pos = 0;
    }
  n = offset - pos;
  while (n > 0 && !unzeof (file))
    {
      int tmp = unzReadCurrentFile (file, buffer,
                                    n > MAXBUFSIZE ? MAXBUFSIZE : (unsigned int) n);
      if (tmp < 0)
        return -1;
      n -= (ZPOS64_T) tmp;
    }
  return n > 0 ? -1 : 0;
}


FILE *
fopen2 (const char *filename, const char *mode)
{
#undef  fopen
  const char *p = mode;
  int read = 0, compressed = 0;
  fmode2_t fmode = FM_UNDEF;
  st_finfo_t *finfo;
  FILE *file = NULL;

//  printf ("opening %s", filename);
  if (fh_map == NULL)
    init_fh_map ();

  for (; *p; p++)
    {
      switch (*p)
        {
        case 'r':
          read = 1;
          break;
        case 'f':
        case 'h':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          fmode = FM_GZIP;
          break;
        case 'w':
        case 'a':
          fmode = FM_NORMAL;
          break;
        case '+':
          if (fmode == FM_UNDEF)
            fmode = FM_NORMAL;
          break;
        }
      }

  if (read)
    {
#undef  fread
#undef  fclose
      if ((file = fopen (filename, "rb")) != NULL)
        {
          unsigned char magic[4] = { 0 };
          size_t nbytes = fread (magic, 1, sizeof magic, file);
          (void) nbytes;
          if (magic[0] == 0x1f && magic[1] == 0x8b && magic[2] == 0x08)
            {                           // ID1, ID2 and CM. gzip uses Compression Method 8
              fmode = FM_GZIP;
              compressed = 1;
            }
          else if (magic[0] == 'P' && magic[1] == 'K' &&
                   magic[2] == 0x03 && magic[3] == 0x04)
            {
              fmode = FM_ZIP;
              compressed = 1;
            }
          else
            /*
              Files that are opened with mode "r+" will probably be written to.
              zlib doesn't support mode "r+", so we have to use FM_NORMAL.
              Mode "r" doesn't require FM_NORMAL and FM_GZIP works, but we
              shouldn't introduce needless overhead.
            */
            fmode = FM_NORMAL;
          fclose (file);
        }
#define fread   fread2
#define fclose  fclose2
    }

  if (fmode == FM_GZIP)
    file = (FILE *) gzopen (filename, mode);
  else if (fmode == FM_ZIP)
    {
      file = (FILE *) unzOpen (filename);
      if (file != NULL &&
          (unzip_goto_file (file, unzip_current_file_nr) != UNZ_OK ||
           unzOpenCurrentFile (file) != UNZ_OK))
        {
          unzClose (file);
          return NULL;
        }
    }
  else // if (fmode == FM_NORMAL)
    file = fopen (filename, mode);

  if (file == NULL)
    return NULL;

  finfo = &finfo_list[fmode * 2 + compressed];
  fh_map = map_put (fh_map, file, finfo);

/*
  printf (", ptr = %p, mode = %s, fmode = %s\n", file, mode,
          fmode == FM_NORMAL ? "FM_NORMAL" :
            (fmode == FM_GZIP ? "FM_GZIP" :
              (fmode == FM_ZIP ? "FM_ZIP" : "FM_UNDEF")));
  map_dump (fh_map);
//*/
  return file;
#define fopen   fopen2
}


int
fclose2 (FILE *file)
{
#undef  fclose
  fmode2_t fmode = get_finfo (file)->fmode;

  map_del (fh_map, file);
  if (fmode == FM_NORMAL)
    return fclose (file);
  else if (fmode == FM_GZIP)
    return gzclose ((gzFile) file);
  else if (fmode == FM_ZIP)
    {
      unzCloseCurrentFile (file);
      return unzClose (file);
    }
  else
    return EOF;
#define fclose  fclose2
}


int
fseek2 (FILE *file, long offset, int mode)
{
  return fseeko2 (file, offset, mode);
}


int
fseeko2 (FILE *file, int64_t offset, int mode)
{
  fmode2_t fmode = get_finfo (file)->fmode;

/*
//  if (fmode != FM_NORMAL)
  printf ("fmode = %s\n", fmode == FM_NORMAL ? "FM_NORMAL" :
                            (fmode == FM_GZIP ? "FM_GZIP" :
                              (fmode == FM_ZIP ? "FM_ZIP" : "FM_UNDEF")));
*/

  if (fmode == FM_NORMAL)
    {
#if     defined __MINGW32__ && !defined __MINGW64__
      return fseeko64 (file, offset, mode);
#elif   defined _WIN32
      return _fseeki64 (file, offset, mode);
#else // MinGW-w64 has this too (as well as the previous 2)
      return fseeko (file, offset, mode);
#endif
    }
  else if (fmode == FM_GZIP)
    {
      if (mode == SEEK_END)                     // zlib doesn't support SEEK_END
        {
          // note that this is _slow_...
          unsigned char buf[MAXBUFSIZE];
          while (gzread ((gzFile) file, buf, MAXBUFSIZE) > 0)
            ;
#ifdef  Z_LARGE64
          offset += gztell64 ((gzFile) file);
#else
          offset += gztell ((gzFile) file);
#endif
          mode = SEEK_SET;
        }
#ifdef  Z_LARGE64
      return gzseek64 ((gzFile) file, offset, mode) == -1 ? -1 : 0;
#elif   defined _MSC_VER
      return gzseek ((gzFile) file, (long) offset, mode) == -1 ? -1 : 0;
#else
      return gzseek ((gzFile) file, offset, mode) == -1 ? -1 : 0;
#endif
    }
  else if (fmode == FM_ZIP)
    {
      ZPOS64_T base;
      if (mode != SEEK_SET && mode != SEEK_CUR && mode != SEEK_END)
        {
          errno = EINVAL;
          return -1;
        }
      if (mode == SEEK_SET)
        base = 0;
      else if (mode == SEEK_CUR)
        base = unztell64 (file);
      else // mode == SEEK_END
        {
          unz_file_info64 info;

          if (unzip_goto_file (file, unzip_current_file_nr) != UNZ_OK)
            return -1;
          unzGetCurrentFileInfo64 (file, &info, NULL, 0, NULL, 0, NULL, 0);
          base = info.uncompressed_size;
        }
      return unzip_seek_helper (file, base + offset);
    }
  return -1;
}


size_t
fread2 (void *buffer, size_t size, size_t number, FILE *file)
{
#undef  fread
  fmode2_t fmode = get_finfo (file)->fmode;

  if (size == 0 || number == 0)
    return 0;

  if (fmode == FM_NORMAL)
    return fread (buffer, size, number, file);
  else if (fmode == FM_GZIP)
    {
      int n = gzread ((gzFile) file, buffer, (unsigned int) (number * size));
      if (n < 0)
        n = 0;
      return n / size;
    }
  else if (fmode == FM_ZIP)
    {
      int n = unzReadCurrentFile (file, buffer, (unsigned int) (number * size));
      if (n < 0)
        n = 0;
      return n / size;
    }
  return 0;
#define fread   fread2
}


int
fgetc2 (FILE *file)
{
#undef  fgetc
  fmode2_t fmode = get_finfo (file)->fmode;

  if (fmode == FM_NORMAL)
    return fgetc (file);
  else if (fmode == FM_GZIP)
    return gzgetc ((gzFile) file);
  else if (fmode == FM_ZIP)
    {
      char c;
      return unzReadCurrentFile (file, &c, 1) <= 0 ?
               EOF : c & 0xff;                  // avoid sign bit extension
    }
  else
    return EOF;
#define fgetc   fgetc2
}


char *
fgets2 (char *buffer, int maxlength, FILE *file)
{
#undef  fgets
  fmode2_t fmode = get_finfo (file)->fmode;

  if (fmode == FM_NORMAL)
    return fgets (buffer, maxlength, file);
  else if (fmode == FM_GZIP)
    {
      char *retval = gzgets ((gzFile) file, buffer, maxlength);
      return retval == Z_NULL ? NULL : retval;
    }
  else if (fmode == FM_ZIP)
    {
      int n = 0, c = 0;
      while (n < maxlength - 1 && (c = fgetc (file)) != EOF)
        {
          buffer[n] = (char) c;                 // '\n' must also be stored in buffer
          n++;
          if (c == '\n')
            {
              buffer[n] = '\0';
              break;
            }
        }
      if (n >= maxlength - 1 || c == EOF)
        buffer[n] = '\0';
      return n > 0 ? buffer : NULL;
    }
  else
    return NULL;
#define fgets   fgets2
}


int
feof2 (FILE *file)
{
#undef  feof
  fmode2_t fmode = get_finfo (file)->fmode;

  if (fmode == FM_NORMAL)
    return feof (file);
  else if (fmode == FM_GZIP)
    return gzeof ((gzFile) file);
  else if (fmode == FM_ZIP)
    return unzeof (file);
  else
    return -1;
#define feof    feof2
}


size_t
fwrite2 (const void *buffer, size_t size, size_t number, FILE *file)
{
#undef  fwrite
  fmode2_t fmode = get_finfo (file)->fmode;

  if (size == 0 || number == 0)
    return 0;

  if (fmode == FM_NORMAL)
    return fwrite (buffer, size, number, file);
  else if (fmode == FM_GZIP)
    {
      int n = gzwrite ((gzFile) file, (void *) buffer, (unsigned int) (number * size));
      return n / size;
    }
  else
    return 0;                                   // writing to zip files is not supported
#define fwrite  fwrite2
}


int
fputc2 (int character, FILE *file)
{
#undef  fputc
  fmode2_t fmode = get_finfo (file)->fmode;

  if (fmode == FM_NORMAL)
    return fputc (character, file);
  else if (fmode == FM_GZIP)
    return gzputc ((gzFile) file, character);
  else
    return EOF;                                 // writing to zip files is not supported
#define fputc   fputc2
}


long
ftell2 (FILE *file)
{
  return (long) ftello2 (file);
}


int64_t
ftello2 (FILE *file)
{
  fmode2_t fmode = get_finfo (file)->fmode;

  if (fmode == FM_NORMAL)
    {
#if     defined __MINGW32__ && !defined __MINGW64__
      return ftello64 (file);
#elif   defined _WIN32
      return _ftelli64 (file);
#else // MinGW-w64 has this too (as well as the previous 2)
      return ftello (file);
#endif
    }
  else if (fmode == FM_GZIP)
#ifdef  Z_LARGE64
    return gztell64 ((gzFile) file);
#else
    return gztell ((gzFile) file);
#endif
  else if (fmode == FM_ZIP)
    return unztell64 (file);                    // returns file position of the "current file"
  else
    return -1;
}


void
rewind2 (FILE *file)
{
  fseek2 (file, 0, SEEK_SET);
}


FILE *
popen2 (const char *command, const char *mode)
{
#undef  popen
#if     defined _WIN32 || defined AMIGA
#define popen _popen
#endif
  int compressed = 0;
  fmode2_t fmode = FM_NORMAL;
  st_finfo_t *finfo;
  FILE *file;

  if (fh_map == NULL)
    init_fh_map ();

  if ((file = popen (command, mode)) == NULL)
    return NULL;

  finfo = &finfo_list[fmode * 2 + compressed];
  fh_map = map_put (fh_map, file, finfo);

  return file;
#undef  popen
#define popen   popen2
}


int
pclose2 (FILE *stream)
{
#undef  pclose
#if     defined _WIN32 || defined AMIGA
#define pclose _pclose
#endif
  fmode2_t fmode = get_finfo (stream)->fmode;

  map_del (fh_map, stream);
  if (fmode == FM_NORMAL)
    return pclose (stream);
  else if (fmode == FM_GZIP || fmode == FM_ZIP)
    return -1;
  else
    return -1;
#undef  pclose
#define pclose  pclose2
}

#endif // USE_ZLIB
