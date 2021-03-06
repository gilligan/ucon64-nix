/*
aps.c - Advanced Patch System support for uCON64

Copyright (c) 1998                                 Silo/BlackBag
Copyright (c) 1999 - 2001                          NoisyB
Copyright (c) 2002 - 2005, 2015, 2017, 2019 - 2021 dbjh


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
#include <stdlib.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include <string.h>
#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "misc/archive.h"
#include "misc/bswap.h"
#include "misc/file.h"
#include "ucon64.h"
#include "ucon64_misc.h"
#include "patch/aps.h"


#define N64APS_DESCRIPTION_LEN 50
#define N64APS_BUFFERSIZE 255
#define N64APS_MAGICLENGTH 5


static st_ucon64_obj_t aps_obj[] =
  {
    {0, WF_STOP}
  };

const st_getopt2_t aps_usage[] =
  {
    {
      "a", 0, 0, UCON64_A,
      NULL, "apply APS PATCH to ROM (APS<=v1.2)",
      &aps_obj[0]
    },
    {
      "mka", 1, 0, UCON64_MKA,
      "ORG_ROM", "create APS patch; ROM should be the modified ROM",
      &aps_obj[0]
    },
    {
      "na", 1, 0, UCON64_NA,
      "DESC", "change APS single line DESCRIPTION",
      NULL
    },
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };

static char n64aps_magic[] = "APS10";
static unsigned char n64aps_patchtype = 1, n64aps_encodingmethod = 0;
static FILE *n64aps_apsfile, *n64aps_orgfile, *n64aps_modfile;
static int n64aps_changefound;


static void
readstdheader (void)
{
  char magic[N64APS_MAGICLENGTH], description[N64APS_DESCRIPTION_LEN + 1];

  fread_checked (magic, 1, N64APS_MAGICLENGTH, n64aps_apsfile);
  if (memcmp (magic, n64aps_magic, N64APS_MAGICLENGTH) != 0)
    {
      fputs ("ERROR: Not a valid APS file\n", stderr);
      fclose (n64aps_modfile);
      fclose (n64aps_apsfile);
      exit (1);
    }
  n64aps_patchtype = (unsigned char) fgetc (n64aps_apsfile);
  if (n64aps_patchtype != 1)                    // N64 patch
    {
      fputs ("ERROR: Could not process patch file\n", stderr);
      fclose (n64aps_modfile);
      fclose (n64aps_apsfile);
      exit (1);
    }
  n64aps_encodingmethod = (unsigned char) fgetc (n64aps_apsfile);
  if (n64aps_encodingmethod != 0)               // simple encoding
    {
      fputs ("ERROR: Unknown or new encoding method\n", stderr);
      fclose (n64aps_modfile);
      fclose (n64aps_apsfile);
      exit (1);
    }

  memset (description, ' ', N64APS_DESCRIPTION_LEN);
  fread_checked (description, 1, N64APS_DESCRIPTION_LEN, n64aps_apsfile);
  description[N64APS_DESCRIPTION_LEN] = '\0';
  printf ("Description: %s\n", description);
}


static void
readN64header (void)
{
  unsigned int n64aps_magictest;
  unsigned char buffer[8], APSbuffer[8], cartid[2], teritory, APSteritory;

  fseek (n64aps_modfile, 0, SEEK_SET);
  fread_checked (&n64aps_magictest, 4, 1, n64aps_modfile);
#ifdef  WORDS_BIGENDIAN
  n64aps_magictest = bswap_32 (n64aps_magictest);
#endif
  buffer[0] = (unsigned char) fgetc (n64aps_apsfile); // APS format
  if (((n64aps_magictest == 0x12408037) && (buffer[0] == 1)) ||
      ((n64aps_magictest != 0x12408037 && (buffer[0] == 0))))
      // 0 for Doctor format, 1 for everything else
    {
      fputs ("ERROR: Image is in the wrong format\n", stderr);
      fclose (n64aps_modfile);
      fclose (n64aps_apsfile);
      exit (1);
    }

  fseek (n64aps_modfile, 60, SEEK_SET);         // cart id
  fread_checked (cartid, 1, 2, n64aps_modfile);
  fread_checked (buffer, 1, 2, n64aps_apsfile);
  if (n64aps_magictest == 0x12408037)
    {
      unsigned char temp = cartid[0];
      cartid[0] = cartid[1];
      cartid[1] = temp;
    }
  if ((buffer[0] != cartid[0]) || (buffer[1] != cartid[1]))
    {
      fputs ("ERROR: This patch does not belong to this image\n", stderr);
      fclose (n64aps_modfile);
      fclose (n64aps_apsfile);
      exit (1);
    }

  if (n64aps_magictest == 0x12408037)
    fseek (n64aps_modfile, 63, SEEK_SET);       // teritory
  else
    fseek (n64aps_modfile, 62, SEEK_SET);
  teritory = (unsigned char) fgetc (n64aps_modfile);
  APSteritory = (unsigned char) fgetc (n64aps_apsfile);
  if (teritory != APSteritory)
    {
      puts ("WARNING: Wrong country");
#if 0
      if (!force)
        {
          fclose (n64aps_modfile);
          fclose (n64aps_apsfile);
          exit (1);
        }
#endif
    }

  fseek (n64aps_modfile, 16, SEEK_SET);         // CRC header position
  fread_checked (buffer, 1, 8, n64aps_modfile);
  fread_checked (APSbuffer, 1, 8, n64aps_apsfile);
  if (n64aps_magictest == 0x12408037)
    ucon64_bswap16_n (buffer, 8);
  if (memcmp (APSbuffer, buffer, 8))
    {
      puts ("WARNING: Incorrect image");
#if 0
      if (!force)
        {
          fclose (n64aps_modfile);
          fclose (n64aps_apsfile);
          exit (1);
        }
#endif
    }

  fseek (n64aps_apsfile, 5, SEEK_CUR);
  fseek (n64aps_modfile, 0, SEEK_SET);
}


static void
readsizeheader (int modsize, const char *modname)
{
  int orgsize;

  fread_checked (&orgsize, 4, 1, n64aps_apsfile);
#ifdef  WORDS_BIGENDIAN
  orgsize = bswap_32 (orgsize);
#endif
  if (modsize != orgsize)                       // resize file
    {
      if (orgsize < modsize)
        {
          fclose (n64aps_modfile);
          if (truncate (modname, orgsize) != 0)
            fputs ("ERROR: Truncate failed\n", stderr);
          if ((n64aps_modfile = fopen (modname, "rb")) == NULL)
            {
              fprintf (stderr, "ERROR: Could not open %s after truncation\n", modname);
              exit (1);
            }
        }
      else
        {
          int i;

          fseek (n64aps_modfile, 0, SEEK_END);
          for (i = 0; i < (orgsize - modsize); i++)
            fputc (0, n64aps_modfile);
        }
    }
//  fseek (n64aps_modfile, 0, SEEK_SET);
}


static void
readpatch (void)
{
  int offset;
  unsigned char buffer[N64APS_BUFFERSIZE];

  while (fread (&offset, 1, 4, n64aps_apsfile) != 0)
    {
      unsigned char size;

#ifdef  WORDS_BIGENDIAN
      offset = bswap_32 (offset);
#endif
      if ((size = (unsigned char) fgetc (n64aps_apsfile)) != 0)
        {
          fread_checked (buffer, 1, size, n64aps_apsfile);
          if ((fseek (n64aps_modfile, offset, SEEK_SET)) != 0)
            {
              fputs ("ERROR: Seek failed\n", stderr);
              exit (1);
            }
          fwrite (buffer, 1, size, n64aps_modfile);
        }
      else                                      // apply an RLE block
        {
          unsigned char data, len;
          int i;

          data = (unsigned char) fgetc (n64aps_apsfile),
          len = (unsigned char) fgetc (n64aps_apsfile);

          if ((fseek (n64aps_modfile, offset, SEEK_SET)) != 0)
            {
              fputs ("ERROR: Seek failed\n", stderr);
              exit (1);
            }
          for (i = 0; i < len; i++)
            fputc (data, n64aps_modfile);
        }
    }
}


// based on source code (version 1.2 981217) by Silo / BlackBag
int
aps_apply (const char *mod, const char *apsname)
{
  char modname[FILENAME_MAX];
  int size = (int) fsizeof (mod);

  strcpy (modname, mod);
  ucon64_file_handler (modname, NULL, 0);
  fcopy (mod, 0, size, modname, "wb");          // no copy if one file

  if ((n64aps_modfile = fopen (modname, "r+b")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], modname);
      exit (1);
    }
  if ((n64aps_apsfile = fopen (apsname, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], apsname);
      exit (1);
    }

  readstdheader ();
  readN64header ();
  readsizeheader (size, modname);

  readpatch ();

  fclose (n64aps_modfile);
  fclose (n64aps_apsfile);

  printf (ucon64_msg[WROTE], modname);
  return 0;
}


static int
n64caps_checkfile (FILE *file, const char *filename)
{
  unsigned int n64aps_magictest;

  fread_checked (&n64aps_magictest, 4, 1, file);
#ifdef  WORDS_BIGENDIAN
  n64aps_magictest = bswap_32 (n64aps_magictest);
#endif
  fseek (file, 0, SEEK_SET);

  if (n64aps_magictest != 0x12408037 && n64aps_magictest != 0x40123780)
    {
      fprintf (stderr, "ERROR: %s is an invalid N64 image\n", filename);
      return FALSE;
    }
  else
    return TRUE;
}


static void
writestdheader (void)
{
  char description[N64APS_DESCRIPTION_LEN];

  fwrite (n64aps_magic, 1, N64APS_MAGICLENGTH, n64aps_apsfile);
  fputc (n64aps_patchtype, n64aps_apsfile);
  fputc (n64aps_encodingmethod, n64aps_apsfile);

  memset (description, ' ', N64APS_DESCRIPTION_LEN);
  fwrite (description, 1, N64APS_DESCRIPTION_LEN, n64aps_apsfile);
}


static void
writeN64header (void)
{
  unsigned int n64aps_magictest;
  unsigned char buffer[8], teritory, cartid[2];

  fread_checked (&n64aps_magictest, 4, 1, n64aps_orgfile);
#ifdef  WORDS_BIGENDIAN
  n64aps_magictest = bswap_32 (n64aps_magictest);
#endif

  if (n64aps_magictest == 0x12408037)           // 0 for Doctor format, 1 for everything else
    fputc (0, n64aps_apsfile);
  else
    fputc (1, n64aps_apsfile);

  fseek (n64aps_orgfile, 60, SEEK_SET);
  fread_checked (cartid, 1, 2, n64aps_orgfile);
  if (n64aps_magictest == 0x12408037)
    {
      unsigned char temp;

      temp = cartid[0];
      cartid[0] = cartid[1];
      cartid[1] = temp;
    }
  fwrite (cartid, 1, 2, n64aps_apsfile);

  if (n64aps_magictest == 0x12408037)
    fseek (n64aps_orgfile, 63, SEEK_SET);
  else
    fseek (n64aps_orgfile, 62, SEEK_SET);
  teritory = (unsigned char) fgetc (n64aps_orgfile);
  fputc (teritory, n64aps_apsfile);

  fseek (n64aps_orgfile, 0x10, SEEK_SET);       // CRC header position
  fread_checked (buffer, 1, 8, n64aps_orgfile);
  if (n64aps_magictest == 0x12408037)
    ucon64_bswap16_n (buffer, 8);

  fwrite (buffer, 1, 8, n64aps_apsfile);
  memset (buffer, 0, 5);
  fwrite (buffer, 1, 5, n64aps_apsfile);        // pad

  fseek (n64aps_orgfile, 0, SEEK_SET);
}


static void
writesizeheader (int orgsize, int newsize)
{
  if (orgsize != newsize)
    n64aps_changefound = TRUE;

#ifdef  WORDS_BIGENDIAN
  newsize = bswap_32 (newsize);
#endif
  fwrite (&newsize, 4, 1, n64aps_apsfile);
}


static void
writepatch (void)
// currently RLE is not supported
{
  size_t newreadlen, filepos, changedstart = 0, changedoffset = 0, i;
  int changefound = 0;
  unsigned char changedlen = 0, orgbuffer[N64APS_BUFFERSIZE], newbuffer[N64APS_BUFFERSIZE];

  fseek (n64aps_orgfile, 0, SEEK_SET);
  fseek (n64aps_modfile, 0, SEEK_SET);
  filepos = 0;
  while ((newreadlen = fread (newbuffer, 1, N64APS_BUFFERSIZE, n64aps_modfile)) != 0)
    {
      size_t orgreadlen = fread (orgbuffer, 1, N64APS_BUFFERSIZE, n64aps_orgfile);
      for (i = orgreadlen; i < newreadlen; i++)
        orgbuffer[i] = 0;

      for (i = 0; i < newreadlen; i++)
        {
          if (newbuffer[i] != orgbuffer[i])
            {
              if (!changefound)
                {
                  changedstart = filepos + i;
                  changedoffset = i;
                  changedlen = 0;
                  changefound = TRUE;
                  n64aps_changefound = TRUE;
                }
              changedlen++;
            }
          else if (changefound)
            {
#ifdef  WORDS_BIGENDIAN
              changedstart = bswap_32 (changedstart);
#endif
              fwrite (&changedstart, 4, 1, n64aps_apsfile);
              fputc (changedlen, n64aps_apsfile);
              fwrite (newbuffer + changedoffset, 1, changedlen, n64aps_apsfile);
              changefound = FALSE;
            }
        }

      if (changefound)
        {
#ifdef  WORDS_BIGENDIAN
          changedstart = bswap_32 (changedstart);
#endif
          fwrite (&changedstart, 4, 1, n64aps_apsfile);
          fputc (changedlen, n64aps_apsfile);
          fwrite (newbuffer + changedoffset, 1, changedlen, n64aps_apsfile);
          changefound = FALSE;
        }

      filepos += newreadlen;
    }
}


// based on source code (version 1.2 981217) by Silo / BlackBag
int
aps_create (const char *orgname, const char *modname)
{
  char apsname[FILENAME_MAX];

  if ((n64aps_orgfile = fopen (orgname, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], orgname);
      exit (1);
    }
  if ((n64aps_modfile = fopen (modname, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], modname);
      exit (1);
    }
  strcpy (apsname, modname);
  set_suffix (apsname, ".aps");
  ucon64_file_handler (apsname, NULL, 0);
  if ((n64aps_apsfile = fopen (apsname, "wb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], apsname);
      exit (1);
    }

  if (!n64caps_checkfile (n64aps_orgfile, orgname) ||
      !n64caps_checkfile (n64aps_modfile, modname))
    exit (1);                                   // n64caps_checkfile() already
                                                //  displayed an error message
  n64aps_changefound = FALSE;

  writestdheader ();
  writeN64header ();
  writesizeheader ((int) fsizeof (orgname), (int) fsizeof (modname));

  fputs ("Searching differences...", stdout);
  fflush (stdout);
  writepatch ();
  puts (" done\n");

  fclose (n64aps_modfile);
  fclose (n64aps_orgfile);
  fclose (n64aps_apsfile);

  if (!n64aps_changefound)
    {
      printf ("%s and %s are identical\n"
              "Removing %s\n", orgname, modname, apsname);
      remove (apsname);
      return -1;
    }
  else
    printf (ucon64_msg[WROTE], apsname);

  return 0;
}


int
aps_set_desc (const char *aps, const char *description)
{
  char desc[N64APS_DESCRIPTION_LEN], apsname[FILENAME_MAX];
  size_t len = strnlen (description, sizeof desc);

  strcpy (apsname, aps);
  if (len < sizeof desc)                        // warning remover
    memset (desc + len, ' ', sizeof desc - len);
  strncpy (desc, description, len);
  ucon64_file_handler (apsname, NULL, 0);
  fcopy (aps, 0, fsizeof (aps), apsname, "wb"); // no copy if one file
  ucon64_fwrite (desc, 7, sizeof desc, apsname, "r+b");

  printf (ucon64_msg[WROTE], apsname);
  return 0;
}
