/*
file.c - miscellaneous file functions

Copyright (c) 1999 - 2004                    NoisyB
Copyright (c) 2001 - 2005, 2015, 2017 - 2021 dbjh
Copyright (c) 2002 - 2004                    Jan-Erik Karlsson (Amiga)


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
#include <errno.h>
#include <stdlib.h>
#ifdef  HAVE_UNISTD_H
#include <unistd.h>                             // access()
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <sys/stat.h>                           // for S_IFLNK
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

#ifdef  _WIN32
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4255) // 'function' : no function prototype given: converting '()' to '(void)'
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <io.h>                                 // access()
#include <windows.h>                            // GetFullPathName()
#ifdef  _MSC_VER
#pragma warning(pop)

#define F_OK 00
#endif
#elif   defined AMIGA
#error Include directives are missing. Please add them and let us know.
#endif

#include "misc/archive.h"
#include "misc/file.h"
#include "misc/misc.h"                          // getenv2()
#include "misc/string.h"

#ifndef MAXBUFSIZE
#define MAXBUFSIZE 32768
#endif // MAXBUFSIZE

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
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
tofname (int c)
{
  return isfname (c) ? c : '_';
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
    return NULL;
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
    return NULL;
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
    return NULL;
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

      strncpy (path1, path2, len)[len] = '\0';
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
      return NULL;
    }
}


char *
dirname2 (const char *path, char *dir)
{
  size_t len;
  char *p1;
#if     defined DJGPP || defined __CYGWIN__ || defined __MINGW32__
  char *p2;
#endif

  if (path == NULL || dir == NULL)
    return NULL;

  len = strnlen (path, FILENAME_MAX - 1);
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


const char *
basename2 (const char *path)
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

  return p1 ? p1 + 1 : path;
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


char *
set_suffix (char *filename, const char *suffix)
{
  const char *p;
  size_t len2;

  if (filename == NULL || suffix == NULL)
    return filename;
  // always use set_suffix() and NEVER the code below
  p = get_suffix (filename);
  len2 = strlen (filename) - strlen (p);
  if (len2 < FILENAME_MAX - 1)
    {
      size_t len = strnlen (suffix, FILENAME_MAX - 1 - len2);

      strncpy ((char *) p, suffix, len)[len] = '\0';
    }

  return filename;
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
  char dir1[FILENAME_MAX], dir2[FILENAME_MAX];
  struct stat fstate;

  dirname2 (oldname, dir1);
  dirname2 (newname, dir2);

  // we should use dirname{2}() in case oldname or newname doesn't exist yet
  if (one_filesystem (dir1, dir2))
    {
      if (access (newname, F_OK) == 0 && !one_file (oldname, newname))
        {
          stat (newname, &fstate);
          chmod (newname, fstate.st_mode | S_IWUSR);
          remove (newname);                     // *try* to remove or rename() will fail
        }
      retval = rename (oldname, newname);
    }
  else
    {
      retval = fcopy_raw (oldname, newname);
      // don't remove unless the file can be copied
      if (retval == 0)
        {
          stat (oldname, &fstate);
          chmod (oldname, fstate.st_mode | S_IWUSR);
          remove (oldname);
        }
    }

  return retval;
}


int
truncate2 (const char *filename, uint64_t new_size)
{
  int result;
  uint64_t size = fsizeof (filename);
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
          size_t n_bytes = new_size - size > MAXBUFSIZE ?
                             MAXBUFSIZE : (size_t) (new_size - size);

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


char *
mkbak (const char *filename, backup_t type)
{
  static char buf[FILENAME_MAX];
  size_t len;

  if (access (filename, R_OK) != 0)
    return (char *) filename;

  len = strnlen (filename, sizeof buf - 1);
  strncpy (buf, filename, len)[len] = '\0';
  set_suffix (buf, ".bak");
  if (stricmp (filename, buf) != 0)
    remove (buf);                               // *try* to remove or rename() will fail
  else
    {                                           // handle the case where filename has the suffix ".bak"
      char buf2[FILENAME_MAX];

      if (!dirname2 (filename, buf2))
        {
          fputs ("INTERNAL ERROR: dirname2() returned NULL\n", stderr);
          exit (1);
        }
      tmpnam2 (buf, buf2);
    }
  if (rename (filename, buf))                   // keep file attributes like date, etc.
    {
      fprintf (stderr, "ERROR: Cannot rename \"%s\" to \"%s\"\n", filename, buf);
      exit (1);
    }

  switch (type)
    {
    case BAK_MOVE:
      return buf;

    case BAK_DUPE:
    default:
      if (fcopy (buf, 0, fsizeof (buf), filename, "wb"))
        {
          fprintf (stderr, "ERROR: Cannot open \"%s\" for writing\n", filename);
          exit (1);
        }
      return buf;
    }
}


static inline size_t
fcopy_func (void *buffer, size_t n, void *object)
{
  return fwrite (buffer, 1, n, (FILE *) object);
}


int
fcopy (const char *src, uint64_t start, uint64_t len, const char *dest,
       const char *mode)
{
  FILE *output;

  if (one_file (dest, src))                     // other code depends on this
    return -1;                                  //  behavior!

  if ((output = fopen (dest, mode)) == NULL)
    return -1;

  fseeko2 (output, 0, SEEK_END);

  quick_io_func (fcopy_func, MAXBUFSIZE, output, start, len, src, "rb");

  fclose (output);
  return 0;
}


int
fcopy_raw (const char *src, const char *dest)
// Raw file copy function. Raw, because it will copy the file data as it is,
//  unlike fcopy(). Don't merge fcopy_raw() with fcopy(). They both have their
//  uses.
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
  while ((seg_len = fread (buf, 1, MAXBUFSIZE, fh)) > 0)
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


#ifndef USE_ZLIB
uint64_t
fsizeof (const char *filename)
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


int
fseeko2 (FILE *file, int64_t offset, int mode)
{
#if     defined __MINGW32__ && !defined __MINGW64__
  return fseeko64 (file, offset, mode);
#elif   defined _WIN32
  return _fseeki64 (file, offset, mode);
#else // MinGW-w64 has this too (as well as the previous 2)
  return fseeko (file, offset, mode);
#endif
}


int64_t
ftello2 (FILE *file)
{
#if     defined __MINGW32__ && !defined __MINGW64__
  return ftello64 (file);
#elif   defined _WIN32
  return _ftelli64 (file);
#else // MinGW-w64 has this too (as well as the previous 2)
  return ftello (file);
#endif
}
#endif


static FILE *
quick_io_open (const char *filename, const char *mode)
{
  FILE *fh = NULL;

  if ((*mode == 'w' || *mode == 'a' || mode[1] == '+') && // will we write to it?
      !access (filename, F_OK))                 // exists?
    {
#ifdef  _WIN32
      struct _stati64 fstate;
      _stati64 (filename, &fstate);
#else
      struct stat fstate;
      stat (filename, &fstate);
#endif
      // first (try to) change the file mode or we won't be able to write to
      //  it if it's a read-only file
      if (chmod (filename, fstate.st_mode | S_IWUSR))
        return NULL;
    }

  if ((fh = fopen (filename, mode)) == NULL)
    {
#ifdef  DEBUG
      fprintf (stderr, "ERROR: Could not open \"%s\" in mode \"%s\"\n"
                       "CAUSE: %s\n", filename, mode, strerror (errno));
#endif
      return NULL;
    }

#ifdef  DEBUG
  fprintf (stderr, "\"%s\": \"%s\"\n", filename, (char *) mode);
#endif

  return fh;
}


int
quick_io_c (int value, uint64_t pos, const char *filename, const char *mode)
{
  int result;
  FILE *fh;

  if ((fh = quick_io_open (filename, mode)) == NULL)
    return -1;

  if (fseeko2 (fh, pos, SEEK_SET))
    {
      fclose (fh);
      return -1;
    }

  if (*mode == 'r' && mode[1] != '+')           // "r+b" always writes
    result = fgetc (fh);
  else
    result = fputc (value, fh);

  fclose (fh);
  return result;
}


size_t
quick_io (void *buffer, uint64_t start, size_t len, const char *filename,
          const char *mode)
{
  size_t result;
  FILE *fh;

  if ((fh = quick_io_open (filename, mode)) == NULL)
    return 0;

  if (fseeko2 (fh, start, SEEK_SET))
    {
      fclose (fh);
      return 0;
    }

  // Note the order of arguments of fread() and fwrite(). Now quick_io()
  //  returns the number of characters read or written. Some code relies on
  //  this behavior!
  if (*mode == 'r' && mode[1] != '+')           // "r+b" always writes
    result = fread (buffer, 1, len, fh);
  else
    result = fwrite (buffer, 1, len, fh);

  fclose (fh);
  return result;
}


static inline size_t
quick_io_func_inline (size_t (*func) (void *, size_t, void *), size_t func_maxlen,
                      void *object, void *buffer, size_t buffer_len)
{
  size_t i = 0, func_size = MIN (func_maxlen, buffer_len);

  while (i < buffer_len)
    {
      size_t func_result;

      func_size = MIN (func_size, buffer_len - i);
      func_result = func ((char *) buffer + i, func_size, object);
      i += func_result;
      if (func_result < func_size)
        break;
    }

  return i;
}


uint64_t
quick_io_func (size_t (*func) (void *, size_t, void *), size_t func_maxlen, void *object,
               uint64_t start, uint64_t len, const char *filename, const char *mode)
// func() takes buffer, length and object (optional), func_maxlen is maximum
//  length passed to func()
{
  void *buffer;
  uint64_t len_done;
  size_t buffer_len;
  FILE *fh;

  errno = 0;

  if ((buffer = malloc (func_maxlen)) == NULL)
    return 0;
  if ((fh = quick_io_open (filename, mode)) == NULL)
    {
      free (buffer);
      return 0;
    }

  if (fseeko2 (fh, start, SEEK_SET))
    {
      fclose (fh);
      free (buffer);
      return 0;
    }

  for (len_done = 0; len_done < len; len_done += buffer_len)
    {
      size_t func_len;

      if (len_done + func_maxlen > len)
        func_maxlen = (size_t) (len - len_done);
      if ((buffer_len = fread (buffer, 1, func_maxlen, fh)) == 0)
        break;

      func_len = quick_io_func_inline (func, func_maxlen, object, buffer, buffer_len);
      if (func_len < buffer_len)        // less than buffer_len? this must be
        {                               //  the end or a problem (if write mode)
          len_done += func_len;
          break;
        }

      if (*mode == 'w' || *mode == 'a' || mode[1] == '+')
        {
          fseeko2 (fh, -(int64_t) buffer_len, SEEK_CUR);
          fwrite (buffer, 1, buffer_len, fh);
          /*
            This appears to be a bug in DJGPP and Solaris (for example, when
            called from ucon64_fbswap16()). Without an extra call to fseek() a
            part of the file won't be written (DJGPP: after 8 MB, Solaris: after
            12 MB).
          */
          fseeko2 (fh, 0, SEEK_CUR);
        }
    }

  fclose (fh);
  free (buffer);
  // return total number of bytes processed
  return len_done;
}


#ifdef  _WIN32
int
truncate (const char *path, uint64_t size)
{
  int retval;
  HANDLE file = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return -1;

  SetFilePointer (file, (LONG) size, ((PLONG) &size) + 1, FILE_BEGIN);
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
truncate (const char *path, uint64_t size)
{
  BPTR fh;
  ULONG newsize;

  if (!(fh = Open (path, MODE_OLDFILE)))
    return -1;

  newsize = SetFileSize (fh, (ULONG) size, OFFSET_BEGINNING);
  Close (fh);

  return newsize == (ULONG) size ? 0 : -1;      // truncate() returns zero on success
}


void
sync (void)
{
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


int
readlink (const char *path, char *buf, int bufsize)
{
  (void) path;                                  // warning remover
  (void) buf;                                   // idem
  (void) bufsize;                               // idem
  // always return -1 as if anything passed to it isn't a soft link
  return -1;
}
#endif                                          // AMIGA
