/*
cdi.c - DiscJuggler/CDI image support for libdiscmage

Copyright (c) 2002 NoisyB
based on specs and code by Dext


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
#include <stdio.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include <stdlib.h>
#include <string.h>
#include "../misc.h"
#include "../libdiscmage.h"
#include "../libdm_misc.h"
#include "format.h"
#ifdef  DJGPP
#include "../dxedll_priv.h"
#endif
#if     defined _MSC_VER && _MSC_VER >= 1900
#pragma warning(pop)
#endif


// cdi images are little-endian

// header magic
#define CDI_V2  0x80000004
#define CDI_V3  0x80000005
#define CDI_V35 0x80000006
#define CDI_V4  (CDI_V35)

static uint32_t header_start = 0, version = 0, position = 0;

int
cdi_track_init (dm_track_t *track, FILE *fh)
// based on the DiscJuggler 4.0 header specs by Dext (see comments below)
{
  int cdi_track_modes[] = {2048, 2336, 2352, 0}, x = 0;
  uint32_t value32;
//  uint16_t value16;
  uint8_t value8;
  char value_s[256];
  const char track_header_magic[] = { 0, 0, 0x01, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF};

#ifdef  DEBUG
  printf ("%lx\n", ftell(fh));
  fflush (stdout);
#endif

#if 0
  if (fread_checked2 (&value32, 4, 1, fh) != 0)    // [   8    unkown data (not NULL)      3.00.780 only  (may not be present)]
    return -1;
  if (le2me_32 (value32))
    fseek (fh, 2, SEEK_CUR);   //          skip
#else
// callibrate
  fseek (fh, -9, SEEK_CUR);
  for (x = 0; x < 64; x++)
    {
      if (fread (&value_s, 1, 10, fh) != 10)
        return -1;
      fseek (fh, -10, SEEK_CUR);
      if (!memcmp (track_header_magic, value_s, 10))
        break;
      fseek (fh, 1, SEEK_CUR);
    }
#endif
#ifdef  DEBUG
  printf ("new offset: %lx\n", ftell (fh));
  fflush (stdout);
#endif

  for (x = 0; x < 2; x++)      //    20    00 00 01 00  00 00 FF FF  FF FF
    {                          //          00 00 01 00  00 00 FF FF  FF FF    track start mark?
      if (fread_checked2 (&value_s, 1, 10, fh) != 0)
        return -1;
      if (memcmp (track_header_magic, value_s, 10))
        {
          fprintf (stderr, "ERROR: could not locate the track start mark (pos: %08lx)\n", ftell (fh));
          return -1;
        }
    }

  fseek (fh, 4, SEEK_CUR);     //     4    discjuggler settings        no idea of internal bit fields
  if (fread_checked2 (&value8, 1, 1, fh) != 0) //     1    filename_length
    return -1;
  if (fread_checked2 (&value_s, 1, value8, fh) != 0) //  [fl]    [filename]
    return -1;
#ifdef  DEBUG
  value_s[value8] = '\0';
  puts (value_s);
  fflush (stdout);
#endif
  fseek (fh, 19, SEEK_CUR);    //     1    NULL
                               //    10    NULL  (ISRC?)
                               //     4    2   (always?)
                               //     4    NULL
  if (fread_checked2 (&value32, 4, 1, fh) != 0) // [   4    0x80000000 (4.x only)]
    return -1;

//  fseek (fh, 4, SEEK_CUR);     //     4    max_cd_length = 0x514C8 (333000 dec) or 0x57E40 (360000 dec)
  if (le2me_32 (value32) == 0x80000000)
    fseek (fh, 4, SEEK_CUR);   // [   4    0x980000 (4.x only)]
  fseek (fh, 2, SEEK_CUR);     //     2    2   (always?)
  if (fread_checked2 (&value32, 4, 1, fh) != 0) //     4    track->pregap_len = 0x96 (150 dec) in sectors
    return -1;
  track->pregap_len = (int16_t) le2me_32 (value32);
#ifdef  DEBUG
  printf ("pregap: %x\n", track->pregap_len);
  fflush (stdout);
#endif
  if (fread_checked2 (&value32, 4, 1, fh) != 0) //     4    track_length (in sectors)
    return -1;
  track->track_len = le2me_32 (value32);
#ifdef  DEBUG
  printf ("track len: %d\n", track->track_len);
  fflush (stdout);
#endif
  fseek (fh, 6, SEEK_CUR);     // NULL
  if (fread_checked2 (&value32, 4, 1, fh) != 0) //     4    track_mode (0 = audio, 1 = mode1, 2 = mode2)
    return -1;
  track->mode = (int8_t) le2me_32 (value32);
#ifdef  DEBUG
  printf ("track mode: %d\n", track->mode);
  fflush (stdout);
#endif
  fseek (fh, 12, SEEK_CUR);    //     4    NULL
                               //     4    session_number (starting at 0)
                               //     4    track_number (in current session, starting at 0)
  if (fread_checked2 (&value32, 4, 1, fh) != 0) //     4    start_lba
    return -1;
  track->start_lba = (int16_t) le2me_32 (value32);
  if (fread_checked2 (&value32, 4, 1, fh) != 0) //     4    total_length (pregap+track), less if truncated
    return -1;
  track->total_len = le2me_32 (value32);
#if 1
  fseek (fh, 16, SEEK_CUR);    //    16    NULL
  if (fread_checked2 (&value32, 4, 1, fh) != 0) //     4    sector_size (0 = 2048, 1 = 2336, 2 = 2352)
    return -1;
  value32 = le2me_32 (value32);
  if (/* value8 < 0 || */ value32 > 2)
    {
      fprintf (stderr, "ERROR: unsupported sector size (%u)\n", value32);
      return -1;
    }
  track->sector_size = (uint16_t) cdi_track_modes[value32];
#else
  fseek (fh, 19, SEEK_CUR);    //    19    NULL
  if (fread_checked2 (&value8, 1, 1, fh) != 0) //     1    sector_size (0 = 2048, 1 = 2336, 2 = 2352)
    return -1;
//  value32 = le2me_32 (value32);
  if (/* value8 < 0 || */ value8 > 2)
    {
      fprintf (stderr, "ERROR: unsupported sector size (%d)\n", value8);
      return -1;
    }
  track->sector_size = cdi_track_modes[value8];
#endif
#ifdef  DEBUG
  printf ("sector size: %d\n\n", track->sector_size);
  fflush (stdout);
#endif
  fseek (fh, 29, SEEK_CUR);    //     4    0 = audio, 4 = data
                               //     1    NULL
                               //     4    total_length (again?)
                               //    20    NULL
  if (version != CDI_V2)
    {
      fseek (fh, 5, SEEK_CUR);
      if (fread_checked2 (&value32, 4, 1, fh) != 0) //     9    unknown data           3.0 only (build 780+: 00FFFFFFFFFFFFFFFF)
        return -1;
      if (le2me_32 (value32) == 0xffffffff)
        fseek (fh, 78, SEEK_CUR); //    78    unknown data      3.00.780 only (not NULL)
    }

  fseek (fh, (version == CDI_V2 ? 12 : 13), SEEK_CUR); // skip session

#ifdef  DEBUG
  printf ("%lx\n", ftell(fh));
  fflush (stdout);
#endif

  track->track_start = position; // position inside the image
  position += (track->total_len * track->sector_size);

#if 0
  pos = ftell (fh);
  fseek (fh, track->track_start, SEEK_SET);
  dm_track_init (track, fh); // try to get more (precise) info from the track itself
  fseek (fh, pos, SEEK_SET);
#endif

  return 0;
}


int
cdi_init (dm_image_t *image)
{
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
  typedef struct
  {
    uint32_t version;
    char *version_s;
  } st_probe_t;
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

  static const st_probe_t probe[] = {
      {CDI_V2, "DiscJuggler/CDI image (v2.x)"},
      {CDI_V3, "DiscJuggler/CDI image (v3.x)"},
      {CDI_V35, "DiscJuggler/CDI image (v3.5)"},
      {CDI_V4, "DiscJuggler/CDI image (v4.x)"},
      {0, NULL}
    };
  int s = 0, t = 0, x = 0, size = q_fsize (image->fname);
  FILE *fh;
  uint16_t value_16;
  uint32_t value_32;
#ifdef  DEBUG
  int result;
  unsigned char buf[MAXBUFSIZE];
#endif

  header_start =
  version =
  position = 0;

  if (size < 8)
    return -1; // image file is too small

  if ((fh = fopen (image->fname, "rb")) == NULL)
    return -1;

  fseek (fh, size - 8, SEEK_SET);
  if (fread_checked2 (&value_32, 1, 4, fh) != 0)
    return -1;
  image->version = version = value_32;
  if (fread_checked2 (&value_32, 1, 4, fh) != 0)
    return -1;
  image->header_start = header_start = value_32;

  if (!image->header_start)
    {
      fclose (fh);
      return -1; // bad image
    }

  // a supported DiscJuggler version?
  for (x = 0; probe[x].version; x++)
    if (image->version == probe[x].version)
      break;

  if (image->version != probe[x].version)
    {
      fclose (fh);
      return -1;
    }

  image->desc = probe[x].version_s;

  image->header_start =
                        image->version == CDI_V35
#if     CDI_V35 != CDI_V4
                        || image->version == CDI_V4
#endif
                        ? size - image->header_start : image->header_start;

//TODO: get the correct header size when != (image->version == CDI_V35 || image->version == CDI_V4)
//  image->header_len = size - image->header_start;

#ifdef  DEBUG
  fseek (fh, image->header_start, SEEK_SET);
  memset (&buf, 0, MAXBUFSIZE);
  result = fread (buf, 1, MAXBUFSIZE, fh);
  mem_hexdump (buf, result, image->header_start);
#endif

  fseek (fh, image->header_start, SEEK_SET);

  if (fread_checked2 (&value_16, 2, 1, fh) != 0) // how many sessions?
    return -1;
  image->sessions = value_16;

  if (!image->sessions)
    {
      fclose (fh);
      return -1;
    }

  image->tracks = 0;
  for (s = 0; s < image->sessions; s++)
    {
#ifdef  DEBUG
      printf ("track# offset: %lx\n", ftell (fh));
      fflush (stdout);
#endif
      if (fread_checked2 (&value_16, 1, 2, fh) != 0) // how many tracks in this session?
        return -1;

      for (t = 0; t < value_16; t++)
        if (!cdi_track_init (&image->track[image->tracks], fh))
          {
            image->tracks++; // total # of tracks
            image->session[s]++; // # of tracks for this session
          }
        else
          {
            fclose (fh);
            return !image->tracks ? (-1) : 0;
          }
    }

  fclose (fh);
  return 0;
}
