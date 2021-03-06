/*
format.c - support of different image formats for libdiscmage

Copyright (c) 2004 NoisyB


This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#if     defined _MSC_VER && _MSC_VER >= 1900
#pragma warning(push)
#pragma warning(disable: 4464) // relative include path contains '..'
#endif
#ifdef  HAVE_CONFIG_H
#include "../config.h"
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif
#include <ctype.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#include <io.h>
#pragma warning(pop)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <time.h>
#include <sys/stat.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#ifdef  DEBUG
#ifdef  __GNUC__
#warning DEBUG active
#else
#pragma message ("DEBUG active")
#endif
#endif
#include "../misc.h"
#include "../libdiscmage.h"
#include "../libdm_misc.h"
#include "format.h"
#include "cdi.h"
#include "cue.h"
#include "nero.h"
#include "other.h"
#include "toc.h"
#ifdef  DJGPP                                   // DXEs are specific to DJGPP
#include "../dxedll_priv.h"
#endif
#if     defined _MSC_VER && _MSC_VER >= 1900
#pragma warning(pop)
#endif


/*
  callibrate()        a brute force function that tries to find a iso header
                      or anything else that could identify a file as an
                      image (can be very slow)
*/
#if 0
static FILE *
callibrate (const char *s, int len, FILE *fh)
// brute force callibration
{
  int32_t pos = ftell (fh);
  char buf[MAXBUFSIZE];
//   malloc ((len + 1) * sizeof (char));
  int size = 0;
  int tries = 0; //TODO: make this an arg

  fseek (fh, 0, SEEK_END);
  size = ftell (fh);
  fseek (fh, pos, SEEK_SET);

  for (; pos < size - len && tries < 32768; pos++, tries++)
    {
      fseek (fh, pos, SEEK_SET);
      fread (&buf, len, 1, fh);
#ifdef  DEBUG
  mem_hexdump (buf, len, ftell (fh) - len);
  mem_hexdump (s, len, ftell (fh) - len);
#endif
      if (!memcmp (s, buf, len))
        {
          fseek (fh, -len, SEEK_CUR);
          return fh;
        }
    }

  return NULL;
}
#endif


int
dm_track_init (dm_track_t *track, FILE *fh)
{
  int x = 0, identified = 0;
  const char sync_data[] = { 0, 0xff, 0xff,
                                0xff, 0xff,
                                0xff, 0xff,
                                0xff, 0xff,
                                0xff, 0xff, 0 };
  char value_s[32];

  fseek (fh, track->track_start, SEEK_SET);
#if 1
  if (fread_checked2 (value_s, 1, 16, fh) != 0)
    return -1;
#else
// callibrate
  fseek (fh, -15, SEEK_CUR);
  for (x = 0; x < 64; x++)
    {
      if (fread (&value_s, 1, 16, fh) != 16)
        return -1;
      fseek (fh, -16, SEEK_CUR);
      if (!memcmp (sync_data, value_s, 12))
        break;
      fseek (fh, 1, SEEK_CUR);
    }
#endif

  if (!memcmp (sync_data, value_s, 12))
    {
      uint8_t value8 = (uint8_t) value_s[15];

      for (x = 0; track_probe[x].sector_size; x++)
        if (track_probe[x].mode == value8)
          {
            // search for valid PVD in sector 16 of source image
            int pos = (track_probe[x].sector_size * 16) +
                      track_probe[x].seek_header + track->track_start;
            fseek (fh, pos, SEEK_SET);
            if (fread_checked2 (value_s, 1, 16, fh) != 0)
              return -1;
            if (!memcmp (pvd_magic, &value_s, 8) ||
                !memcmp (svd_magic, &value_s, 8) ||
                !memcmp (vdt_magic, &value_s, 8))
              {
                identified = 1;
                break;
              }
          }
    }

  // no sync_data found? probably MODE1/2048
  if (!identified)
    {
      x = 0;
      if (track_probe[x].sector_size != 2048)
        fputs ("ERROR: dm_track_init()\n", stderr);

      fseek (fh, (track_probe[x].sector_size * 16) +
             track_probe[x].seek_header + track->track_start, SEEK_SET);
      if (fread_checked2 (value_s, 1, 16, fh) != 0)
        return -1;

      if (!memcmp (pvd_magic, &value_s, 8) ||
          !memcmp (svd_magic, &value_s, 8) ||
          !memcmp (vdt_magic, &value_s, 8))
        identified = 1;
    }

  if (!identified)
    {
      fputs ("ERROR: could not find iso header of current track\n", stderr);
      return -1;
    }

  track->sector_size = (uint16_t) track_probe[x].sector_size;
  track->mode = (int8_t) track_probe[x].mode;
  track->seek_header = (int16_t) track_probe[x].seek_header;
  track->seek_ecc = (int16_t) track_probe[x].seek_ecc;
  track->iso_header_start = (track_probe[x].sector_size * 16) + track_probe[x].seek_header;
  track->id = dm_get_track_mode_id (track->mode, track->sector_size);

  return 0;
}


dm_image_t *
dm_reopen (const char *fname, uint32_t flags, dm_image_t *image)
// recurses through all <image_type>_init functions to find correct image type
{
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
  typedef struct
    {
      int type;
      int (*init) (dm_image_t *);
      int (*track_init) (dm_track_t *, FILE *);
    } st_probe_t;
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

  static st_probe_t probe[] =
    {
      {DM_CDI, cdi_init, cdi_track_init},
      {DM_NRG, nrg_init, nrg_track_init},
//      {DM_CCD, ccd_init, ccd_track_init},
      {DM_CUE, cue_init, dm_track_init},
      {DM_TOC, toc_init, dm_track_init},
      {DM_OTHER, other_init, dm_track_init},
      {0, NULL, NULL}
    };
  int x, identified = 0;
//  static dm_image_t image2;
  FILE *fh = NULL;

#ifdef  DEBUG
  printf ("sizeof (dm_track_t) == %d\n", sizeof (dm_track_t));
  printf ("sizeof (dm_image_t) == %d\n", sizeof (dm_image_t));
  fflush (stdout);
#endif

  if (image)
    {
      dm_close (image);
      image = NULL;
    }

  if (access (fname, F_OK) != 0)
    return NULL;

  if (!image)
#if 1
    {
      image = (dm_image_t *) malloc (sizeof (dm_image_t));
      if (!image)
        return NULL;
    }
#else
    image = (dm_image_t *) &image2;
#endif

  memset (image, 0, sizeof (dm_image_t));

  image->desc = ""; // deprecated

  for (x = 0; probe[x].type; x++)
    if (probe[x].init)
      {
        dm_clean (image);
        image->flags = flags;
        strcpy (image->fname, fname);

        if (!probe[x].init (image))
          {
            identified = 1;
            break;
          }
      }

  if (!identified) // unknown image
    return NULL;

  image->type = probe[x].type;

  if ((fh = fopen (image->fname, "rb")) == NULL)
    return image;

  // verify header or sheet informations
  for (x = 0; x < image->tracks; x++)
    {
      dm_track_t *track = (dm_track_t *) &image->track[x];

      if (track->mode != 0) // AUDIO/2352 has no iso header
        track->iso_header_start = track->track_start + (track->sector_size * (16 + track->pregap_len)) + track->seek_header;

#ifdef  DEBUG
      printf ("iso header offset: %d\n\n", (int) track->iso_header_start);
      fflush (stdout);
#endif

      track->id = dm_get_track_mode_id (track->mode, track->sector_size);
    }

  fclose (fh);

  return image;
}


dm_image_t *
dm_open (const char *fname, uint32_t flags)
{
  return dm_reopen (fname, flags, NULL);
}


int
dm_close (dm_image_t *image)
{
#if 1
  free (image);
#else
  memset (image, 0, sizeof (dm_image_t));
#endif
  return 0;
}


int
dm_read (char *buffer, int track_num, int sector, const dm_image_t *image)
{
  dm_track_t *track = (dm_track_t *) &image->track[track_num];
  FILE *fh;

  if ((fh = fopen (image->fname, "rb")) == NULL)
    return 0;

  if (fseek (fh, track->track_start + (track->sector_size * sector), SEEK_SET) != 0)
    {
      fclose (fh);
      return 0;
    }

  if (fread (buffer, track->sector_size, 1, fh) != track->sector_size)
    {
      fclose (fh);
      return 0;
    }

  fclose (fh);
  return track->sector_size;
}


int
dm_write (const char *buffer, int track_num, int sector, const dm_image_t *image)
{
  (void) buffer;
  (void) track_num;
  (void) sector;
  (void) image;
  return 0;
}


int
dm_disc_read (const dm_image_t *image)
{
  (void) image;
  fprintf (stderr, dm_msg[DEPRECATED], "dm_disc_read()");
  fflush (stderr);
  return 0;
}


int
dm_disc_write (const dm_image_t *image)
{
  (void) image;
  fprintf (stderr, dm_msg[DEPRECATED], "dm_disc_write()");
  fflush (stderr);
  return 0;
}


int
dm_seek (dm_image_t *image, int track_num, int sector)
{
  (void) image;                                 // warning remover
  (void) track_num;                             // warning remover
  (void) sector;                                // warning remover
  return 0;
}
