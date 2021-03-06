/*
mgd.c - Multi Game Doctor 2 support for uCON64

Copyright (c) 1999 - 2001                          NoisyB
Copyright (c) 2001 - 2004, 2015, 2017, 2020 - 2021 dbjh


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
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif
#include <stdlib.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include "misc/archive.h"
#include "misc/file.h"
#include "misc/string.h"
#include "ucon64.h"
#include "ucon64_misc.h"
#include "backup/mgd.h"


const st_getopt2_t mgd_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "Multi Game Doctor 2/MGD2/Multi Game Hunter/MGH"
      /*"19XX Bung Enterprises Ltd http://www.bung.com.hk\n" "Makko Toys Co., Ltd.?" "Venus Corp. (MGH)"*/,
      NULL
    },
#if 0
    {
      "xmgd", 0, 0, UCON64_XMGD,
      NULL, "(TODO) send/receive ROM to/from Multi Game Doctor 2/MGD2; " OPTION_LONG_S "port" OPTARG_S "PORT\n"
      "receives automatically when " OPTION_LONG_S "rom does not exist",
      NULL
    },
#endif
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };


// the following four functions are used by non-transfer code in genesis.c
void
mgd_interleave (unsigned char **buffer, size_t size)
{
  size_t n;
  unsigned char *src = *buffer;

  if ((*buffer = (unsigned char *) malloc (size)) == NULL)
    {
      fprintf (stderr, ucon64_msg[BUFFER_ERROR], size);
      exit (1);
    }
  for (n = 0; n < size / 2; n++)
    {
      (*buffer)[n] = src[n * 2 + 1];
      (*buffer)[size / 2 + n] = src[n * 2];
    }
  free (src);
}


void
mgd_deinterleave (unsigned char **buffer, size_t data_size, size_t buffer_size)
{
  int n = 0;
  size_t offset;
  unsigned char *src = *buffer;

  if ((*buffer = (unsigned char *) malloc (buffer_size)) == NULL)
    {
      fprintf (stderr, ucon64_msg[BUFFER_ERROR], buffer_size);
      exit (1);
    }
  for (offset = 0; offset < data_size / 2; offset++)
    {
      (*buffer)[n++] = src[data_size / 2 + offset];
      (*buffer)[n++] = src[offset];
    }
  free (src);
}


size_t
fread_mgd (void *buffer, size_t size, size_t number, FILE *fh)
/*
  This function is used to handle a Genesis MGD file as if it wasn't
  interleaved, without the overhead of reading the entire file into memory.
  This is important for genesis_init(). When the file turns out to be a Genesis
  dump in MGD format it is much more efficient for compressed files to read the
  entire file into memory and then deinterleave it (as load_rom() does).
  In order to speed this function up a bit ucon64.fsize is used. That means it
  can't be used for an arbitrary file.
*/
{
  unsigned int fpos_org, fsize = (unsigned int) ucon64.fsize; // fsizeof (filename)
  size_t n = 0, bpos = 0, fpos, bytesread = 0, len = number * size;
  unsigned char tmp1[MAXBUFSIZE], tmp2[MAXBUFSIZE];

  fpos = fpos_org = ftell (fh);
  if (fpos >= fsize)
    return 0;

  if (len == 0)
    return 0;
  else if (len == 1)
    {
      if (fpos_org & 1)
        {
          fseek (fh, (long) (fpos / 2), SEEK_SET);
          *((unsigned char *) buffer) = (unsigned char) fgetc (fh);
        }
      else
        {
          fseek (fh, (long) (fpos / 2 + fsize / 2), SEEK_SET);
          *((unsigned char *) buffer) = (unsigned char) fgetc (fh);
        }
      fseek (fh, fpos_org + 1, SEEK_SET);
      return 1;
    }

  while (len > 0 && !feof (fh))
    {
      size_t block_size = len > MAXBUFSIZE ? MAXBUFSIZE : len;

      fseek (fh, (long) (fpos / 2), SEEK_SET);
      bytesread += fread (tmp1, 1, block_size / 2, fh); // read odd bytes
      fseek (fh, (long) ((fpos + 1) / 2 + fsize / 2), SEEK_SET);
      bytesread += fread (tmp2, 1, block_size / 2, fh); // read even bytes

      if (fpos_org & 1)
        for (n = 0; n < block_size / 2; n++)
          {
            ((unsigned char *) buffer)[bpos + n * 2] = tmp1[n];
            ((unsigned char *) buffer)[bpos + n * 2 + 1] = tmp2[n];
          }
      else
        for (n = 0; n < block_size / 2; n++)
          {
            ((unsigned char *) buffer)[bpos + n * 2] = tmp2[n];
            ((unsigned char *) buffer)[bpos + n * 2 + 1] = tmp1[n];
          }
      fpos += block_size;
      bpos += block_size;
      len -= block_size;
    }
  fseek (fh, (long) (fpos_org + bytesread), SEEK_SET);
  return bytesread / size;
}


int
q_fread_mgd (void *buffer, size_t start, size_t len, const char *filename)
{
  int result;
  FILE *fh;

  if ((fh = fopen (filename, "rb")) == NULL)
    return -1;
  fseek (fh, (long) start, SEEK_SET);
  result = (int) fread_mgd (buffer, 1, len, fh);
  fclose (fh);

  return result;
}


static void
remove_mgd_id (char *name, const char *id)
{
  char *p = name;
  while ((p = strstr (p, id)) != NULL)
    {
      *p = 'X';
      *(p + 1) = 'X';
      p += 2;
    }
}


void
mgd_make_name (const char *filename, int console, unsigned int size, char *name)
// these characters are also valid in MGD file names: !@#$%^&_
{
  char *prefix = NULL, *suffix = NULL;
  const char *fname;
  unsigned int size_mbit = 0, n;

  switch (console)
    {
    default:                                    // falling through
    case UCON64_SNES:
      prefix = "SF";
      suffix = ".048";
      if (size <= 1 * MBIT)
        size_mbit = 1;
      else if (size <= 2 * MBIT)
        size_mbit = 2;
      else if (size <= 4 * MBIT)
        size_mbit = 4;
      else if (size <= 8 * MBIT)
        {
          size_mbit = 8;
          suffix = ".058";
        }
      else
        {
          suffix = ".078";
          if (size <= 10 * MBIT)
            size_mbit = 10;
          else if (size <= 12 * MBIT)
            size_mbit = 12;
          else if (size <= 16 * MBIT)
            size_mbit = 16;
          else if (size <= 20 * MBIT)
            size_mbit = 20;
          else if (size <= 24 * MBIT)
            size_mbit = 24;
          else // MGD supports SNES games with sizes up to 32 Mbit
            size_mbit = 32;
        }
      break;
    case UCON64_GEN:
      prefix = "MD";
      suffix = ".000";
      if (size <= 1 * MBIT)
        size_mbit = 1;
      else if (size <= 2 * MBIT)
        size_mbit = 2;
      else if (size <= 4 * MBIT)
        size_mbit = 4;
      else
        {
          if (size <= 8 * MBIT)
            {
              size_mbit = 8;
              suffix = ".008";
            }
          else if (size <= 16 * MBIT)
            {
              size_mbit = 16;
              suffix = ".018";
            }
          else
            {
              suffix = ".038";
              if (size <= 20 * MBIT)
                size_mbit = 20;
              else if (size <= 24 * MBIT)
                size_mbit = 24;
              else // MGD supports Genesis games with sizes up to 32 Mbit
                size_mbit = 32;
            }
        }
      break;
    case UCON64_PCE:
      prefix = "PC";
      suffix = ".040";
      if (size <= 1 * MBIT)
        size_mbit = 1;
      else if (size <= 2 * MBIT)
        size_mbit = 2;
      else if (size <= 3 * MBIT)
        {
          size_mbit = 3;
          suffix = ".030";
        }
      else if (size <= 4 * MBIT)
        {
          size_mbit = 4;
          suffix = ".048";
        }
      else
        {
          suffix = ".058";
          if (size <= 6 * MBIT)
            size_mbit = 6;
          else // MGD supports PC-Engine games with sizes up to 8 Mbit
            size_mbit = 8;
        }
      break;
    case UCON64_SMS:
      prefix = "GG";
      suffix = ".060";
      if (size < 1 * MBIT)
        size_mbit = 0;
      else if (size == 1 * MBIT)
        size_mbit = 1;
      else if (size <= 2 * MBIT)
        size_mbit = 2;
      else
        {
          suffix = ".078";
          if (size <= 3 * MBIT)
            size_mbit = 3;
          else if (size <= 4 * MBIT)
            size_mbit = 4;
          else if (size <= 6 * MBIT)
            size_mbit = 6;
          else // MGD supports Sega Master System games with sizes up to 8 Mbit
            size_mbit = 8;
        }
      break;
    case UCON64_GAMEGEAR:
      prefix = "GG";
      suffix = ".040";
      if (size < 1 * MBIT)
        size_mbit = 0;
      else if (size == 1 * MBIT)
        size_mbit = 1;
      else if (size <= 2 * MBIT)
        size_mbit = 2;
      else
        {
          suffix = ".048";
          if (size <= 3 * MBIT)
            size_mbit = 3;
          else if (size <= 4 * MBIT)
            size_mbit = 4;
          else
            {
              suffix = ".078";
              if (size <= 6 * MBIT)
                size_mbit = 6;
              else // MGD supports Game Gear games with sizes up to 8 Mbit
                size_mbit = 8;
            }
        }
      break;
    case UCON64_GB:
      prefix = "GB";
      /*
        What is the maximum game size the MGD2 supports for GB (color) games?
        At least one 64 Mbit game exists, Densha De Go! 2 (J) [C][!].
      */
      suffix = ".040";
      if (size < 1 * MBIT)
        size_mbit = 0;
      else if (size == 1 * MBIT)
        size_mbit = 1;
      else if (size <= 2 * MBIT)
        size_mbit = 2;
      else if (size <= 3 * MBIT)
        {
          size_mbit = 3;
          suffix = ".030";
        }
      else if (size <= 4 * MBIT)
        {
          size_mbit = 4;
          suffix = ".048";
        }
      else
        {
          suffix = ".058";
          if (size <= 6 * MBIT)
            size_mbit = 6;
          else
            size_mbit = 8;
        }
      break;
    }

  fname = basename2 (filename);
  // Do NOT mess with prefix (strupr()/strlwr()). See below (remove_mgd_id()).
  snprintf (name, 8, "%s%u%.3sXX", prefix, size_mbit, fname);
  name[7] = '\0';
  if (!strnicmp (name, fname, size < 10 * MBIT ? 3 : 4))
    snprintf (name, 8, "%sXXX", fname);
  n = size < 10 * MBIT ? 6 : 7;
  name[n] = '0';                                // last character must be a number
  name[n + 1] = '\0';
  for (n--; n >= 3; n--)                        // skip prefix and first digit
    if (name[n] == ' ' || name[n] == '.')
      name[n] = 'X';

  /*
    The transfer program "pclink" contains a bug in that it looks at the entire
    filename for an ID string (it should look only at the first 2 characters).
  */
  strupr (name);
  remove_mgd_id (name + 3, "SF");
  remove_mgd_id (name + 3, "MD");
  remove_mgd_id (name + 3, "PC");
  remove_mgd_id (name + 3, "GG");
  remove_mgd_id (name + 3, "GB");

  set_suffix (name, suffix);
}


void
mgd_write_index_file (void *ptr, int n_names)
{
  char buf[100 * 10], name[9], dest_name[FILENAME_MAX];
  // one line in the index file takes 10 bytes at max (name (8) + "\r\n" (2)),
  //  so buf is large enough for 44 files of 1/4 Mbit (max for 1 diskette)
  int n = 0;
  size_t offset = 0;

  if (n_names <= 0)
    return;
  for (; n < n_names; n++)
    {
      char *p;

      strncpy (name, n_names > 1 ? ((char **) ptr)[n] : (char *) ptr, 8)[8] =
        '\0';
      if ((p = strrchr (name, '.')) != NULL)
        *p = '\0';
      sprintf (buf + offset, "%s\r\n", name);   // DOS text file format
      offset += strlen (name) + 2;              // + 2 for "\r\n"
    }

  strcpy (dest_name, "MULTI-GD");
  ucon64_file_handler (dest_name, NULL, OF_FORCE_BASENAME);
  ucon64_fwrite (buf, 0, strlen (buf), dest_name, "wb");
  printf (ucon64_msg[WROTE], dest_name);
}
