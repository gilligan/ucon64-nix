/*
ucon64_dat.c - support for DAT files as known from RomCenter, GoodXXXX, etc.

Copyright (c) 1999 - 2004              NoisyB
Copyright (c) 2002 - 2005, 2015 - 2021 dbjh


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
#ifdef  HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#include <io.h>
#pragma warning(pop)
#endif
#include <stdlib.h>
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
#include <windows.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#endif
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
#include <sys/stat.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include "misc/archive.h"
#include "misc/file.h"
#include "misc/misc.h"
#include "misc/property.h"
#include "misc/string.h"
#include "ucon64_dat.h"
#include "ucon64_misc.h"
#include "console/atari.h"
#include "console/coleco.h"
#include "console/console.h"
#include "console/dc.h"
#include "console/gb.h"
#include "console/gba.h"
#include "console/genesis.h"
#include "console/jaguar.h"
#include "console/lynx.h"
#include "console/n64.h"
#include "console/neogeo.h"
#include "console/nes.h"
#include "console/ngp.h"
#include "console/pce.h"
#include "console/sms.h"
#include "console/snes.h"
#include "console/swan.h"
#include "console/vboy.h"
#include "backup/backup.h"


#define MAX_FIELDS_IN_DAT 32
#define DAT_FIELD_SEPARATOR (0xac)
#define DAT_FIELD_SEPARATOR_S "\xac"
#define MAX_GAMES_FOR_CONSOLE 50000             // TODO?: dynamic size

#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
typedef struct
{
  const char *id;                               // string to detect console from datfile name
  int (*compare) (const void *a, const void *b); // the function which compares the id with the filename
                                                // compare() == 0 means success
  int8_t console;                               // UCON64_SNES, UCON64_NES, etc.
  const st_getopt2_t *console_usage;
} st_console_t;
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

typedef struct
{
  uint32_t crc32;
  long filepos;
} st_idx_entry_t;

#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#endif
typedef struct
{
  uint32_t crc32;
  char *fname;
} st_mkdat_entry_t;
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

#ifndef _WIN32
static DIR *ddat = NULL;
#else
static HANDLE ddat = NULL;
#endif
static FILE *fdat = NULL;
static int ucon64_n_files = 0, filepos_line = 0;
static FILE *ucon64_datfile;
static char ucon64_dat_fname[FILENAME_MAX];
static st_mkdat_entry_t *ucon64_mkdat_entries = NULL;


static st_ucon64_obj_t ucon64_dat_obj[] =
  {
    {0, WF_STOP | WF_NO_ROM},
    {0, WF_INIT | WF_NO_SPLIT},
    {0, WF_INIT | WF_PROBE},
    {0, WF_NO_ARCHIVE},
    {0, WF_INIT | WF_PROBE | WF_NO_SPLIT}
  };

const st_getopt2_t ucon64_dat_usage[] =
  {
    {
      NULL, 0, 0, 0,
      NULL, "DATabase (support for DAT files)",
      NULL
    },
    {
      "db", 0, 0, UCON64_DB,
      NULL, "DATabase statistics",
      &ucon64_dat_obj[0]
    },
    {
      "dbv", 0, 0, UCON64_DBV,
      NULL, "like " OPTION_LONG_S "db but more verbose",
      &ucon64_dat_obj[0]
    },
    {
      "dbs", 1, 0, UCON64_DBS,
      "CRC32", "search ROM with CRC32 value in DATabase",
      &ucon64_dat_obj[0]
    },
    {
      "scan", 0, 0, UCON64_SCAN,
      NULL, "generate ROM list for all ROMs using DATabase\n"
      "like: GoodXXXX scan ...",
      &ucon64_dat_obj[4]
    },
    {
      "lsd", 0, 0, UCON64_LSD,
      NULL, "same as " OPTION_LONG_S "scan",
      &ucon64_dat_obj[2]
    },
    {
      "mkdat", 1, 0, UCON64_MKDAT,
      "DATFILE", "create DAT file; use " OPTION_S "o to specify an output directory",
      &ucon64_dat_obj[2]
    },
    {
      "rdat", 0, 0, UCON64_RDAT,
      NULL, "rename ROMs to their DATabase names",
      &ucon64_dat_obj[4]
    },
    {
      "rrom", 0, 0, UCON64_RROM,
      NULL, "rename ROMs to their internal names (if any)",
      &ucon64_dat_obj[4]
    },
    {
      "r83", 0, 0, UCON64_R83,
      NULL, "rename to 8.3 filenames",
      &ucon64_dat_obj[3]
    },
    {
      "rjoliet", 0, 0, UCON64_RJOLIET,
      NULL, "rename to Joliet compatible filenames",
      &ucon64_dat_obj[3]
    },
    {
      "rl", 0, 0, UCON64_RL,
      NULL, "rename to lowercase",
      &ucon64_dat_obj[3]
    },
    {
      "ru", 0, 0, UCON64_RU,
      NULL, "rename to uppercase",
      &ucon64_dat_obj[3]
    },
#if 0
    {
      "good", 0, 0, UCON64_GOOD,
      NULL, "used with " OPTION_LONG_S "rrom and " OPTION_LONG_S "rr83 ROMs will be renamed using\n"
      "the DATabase",
      NULL
    },
#endif
    {
      NULL, 0, 0, 0,
      NULL, NULL,
      NULL
    }
  };


static void
closedir_ddat (void)
{
  if (ddat)
#ifndef _WIN32
    closedir (ddat);
#else
    FindClose (ddat);
#endif
  ddat = NULL;
}


static void
fclose_fdat (void)
{
  if (fdat)
    fclose (fdat);
  fdat = NULL;
}


static int
custom_stristr (const void *a, const void *b)
{
  return !stristr ((const char *) a, (const char *) b);
}


static int
custom_strnicmp (const void *a, const void *b)
{
  return strnicmp ((const char *) a, (const char *) b,
                   MIN (strlen ((const char *) a), strlen ((const char *) b)));
}


#if 0
static int
custom_stricmp (const void *a, const void *b)
{
  return stricmp ((const char *) a, (const char *) b);
}
#endif


static char *
get_next_file (char *fname)
{
#ifndef _WIN32
  struct dirent *ep;

  if (!ddat && !(ddat = opendir(ucon64.datdir)))
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], ucon64.datdir);
      return NULL;
    }
  while ((ep = readdir (ddat)) != NULL)
    if (!stricmp (get_suffix (ep->d_name), ".dat"))
      {
        snprintf (fname, FILENAME_MAX, "%s" DIR_SEPARATOR_S "%s",
                  ucon64.datdir, ep->d_name);
        fname[FILENAME_MAX - 1] = '\0';
        return fname;
      }
#else
  WIN32_FIND_DATA find_data;

  if (!ddat)
    {
      char search_pattern[FILENAME_MAX];

      // NOTE: FindFirstFile() & FindNextFile() are case insensitive.
      snprintf (search_pattern, FILENAME_MAX, "%s" DIR_SEPARATOR_S "*.dat",
                ucon64.datdir);
      search_pattern[FILENAME_MAX - 1] = '\0';
      if ((ddat = FindFirstFile (search_pattern, &find_data)) == INVALID_HANDLE_VALUE)
        {
          // Not being able to find a DAT file is not a real error.
          if (GetLastError () != ERROR_FILE_NOT_FOUND)
            {
              fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], ucon64.datdir);
              return NULL;
            }
        }
      else
        {
          snprintf (fname, FILENAME_MAX, "%s" DIR_SEPARATOR_S "%s",
                    ucon64.datdir, find_data.cFileName);
          fname[FILENAME_MAX - 1] = '\0';
          return fname;
        }
    }
  else if (FindNextFile (ddat, &find_data))
    {
      snprintf (fname, FILENAME_MAX, "%s" DIR_SEPARATOR_S "%s", ucon64.datdir,
                find_data.cFileName);
      fname[FILENAME_MAX - 1] = '\0';
      return fname;
    }
#endif
  closedir_ddat ();
  return NULL;
}


static st_ucon64_dat_t *
get_dat_header (char *fname, st_ucon64_dat_t *dat)
{
  const char *p;
  size_t len;

  p = get_property (fname, "author", PROPERTY_MODE_TEXT);
  if (!p)
    p = "Unknown";
  len = strnlen (p, sizeof dat->author - 1);
  strncpy (dat->author, p, len)[len] = '\0';

  p = get_property (fname, "version", PROPERTY_MODE_TEXT);
  if (!p)
    p = "?";
  len = strnlen (p, sizeof dat->version - 1);
  strncpy (dat->version, p, len)[len] = '\0';

  p = get_property (fname, "refname", PROPERTY_MODE_TEXT);
  if (!p)
    p = "";
  len = strnlen (p, sizeof dat->refname - 1);
  strncpy (dat->refname, p, len)[len] = '\0';

  p = get_property (fname, "comment", PROPERTY_MODE_TEXT);
  if (!p)
    p = "";
  len = strnlen (p, sizeof dat->comment - 1);
  strncpy (dat->comment, p, len)[len] = '\0';

  p = get_property (fname, "date", PROPERTY_MODE_TEXT);
  if (!p)
    p = "?";
  len = strnlen (p, sizeof dat->date - 1);
  strncpy (dat->date, p, len)[len] = '\0';

  return dat;
}


static int
fname_to_console (const char *fname, st_ucon64_dat_t *dat, int warning)
{
  int pos;
  // We use the filename to find out for what console a DAT file is meant.
  //  The field "refname" seems too unreliable.
  static const st_console_t console_type[] =
    {
      {"GoodSNES", custom_strnicmp, UCON64_SNES, snes_usage},
      {"SNES", custom_strnicmp, UCON64_SNES, snes_usage},
      {"GoodNES", custom_strnicmp, UCON64_NES, nes_usage},
      {"NES", custom_strnicmp, UCON64_NES, nes_usage},
      {"FDS", custom_stristr, UCON64_NES, nes_usage},
      {"GoodGBA", custom_strnicmp, UCON64_GBA, gba_usage},
      {"GBA", custom_strnicmp, UCON64_GBA, gba_usage},
      {"GoodGBX", custom_strnicmp, UCON64_GB, gb_usage},
      {"GBX", custom_strnicmp, UCON64_GB, gb_usage},
      {"GoodGEN", custom_strnicmp, UCON64_GEN, genesis_usage},
      {"GEN", custom_strnicmp, UCON64_GEN, genesis_usage},
      {"GoodGG", custom_strnicmp, UCON64_SMS, sms_usage},
      {"GG", custom_strnicmp, UCON64_SMS, sms_usage},
      {"GoodSMS", custom_strnicmp, UCON64_SMS, sms_usage},
      {"SMS", custom_strnicmp, UCON64_SMS, sms_usage},
      {"GoodJAG", custom_strnicmp, UCON64_JAG, jaguar_usage},
      {"JAG", custom_strnicmp, UCON64_JAG, jaguar_usage},
      {"GoodLynx", custom_strnicmp, UCON64_LYNX, lynx_usage},
      {"Lynx", custom_strnicmp, UCON64_LYNX, lynx_usage},
      {"GoodN64", custom_strnicmp, UCON64_N64, n64_usage},
      {"N64", custom_strnicmp, UCON64_N64, n64_usage},
      {"GoodPCE", custom_strnicmp, UCON64_PCE, pce_usage},
      {"PCE", custom_strnicmp, UCON64_PCE, pce_usage},
      {"Good2600", custom_strnicmp, UCON64_ATA, atari_usage},
      {"2600", custom_strnicmp, UCON64_ATA, atari_usage},
      {"Good5200", custom_strnicmp, UCON64_ATA, atari_usage},
      {"5200", custom_strnicmp, UCON64_ATA, atari_usage},
      {"Good7800", custom_strnicmp, UCON64_ATA, atari_usage},
      {"7800", custom_strnicmp, UCON64_ATA, atari_usage},
      {"GoodVECT", custom_strnicmp, UCON64_VEC, vectrex_usage},
      {"Vectrex", custom_stristr, UCON64_VEC, vectrex_usage},
      {"GoodWSX", custom_strnicmp, UCON64_SWAN, swan_usage},
      {"swan", custom_stristr, UCON64_SWAN, swan_usage},
      {"GoodCOL", custom_strnicmp, UCON64_COLECO, coleco_usage},
      {"Coleco", custom_stristr, UCON64_COLECO, coleco_usage},
      {"GoodINTV", custom_strnicmp, UCON64_INTELLI, intelli_usage},
      {"Intelli", custom_stristr, UCON64_INTELLI, intelli_usage},
      {"GoodNGPX", custom_strnicmp, UCON64_NGP, ngp_usage},
      {"NGP", custom_strnicmp, UCON64_NGP, ngp_usage},
      {"GoodVBOY", custom_strnicmp, UCON64_VBOY, vboy_usage},
      {"VBOY", custom_strnicmp, UCON64_VBOY, vboy_usage},
      {"Neo-Geo", custom_strnicmp, UCON64_NG, neogeo_usage},
      {"MAME", custom_stristr, UCON64_ARCADE, arcade_usage},
      {"Dreamcast", custom_stristr, UCON64_DC, dc_usage},
      {"Saturn", custom_stristr, UCON64_SAT, sat_usage},
      {"3do", custom_stristr, UCON64_3DO, real3do_usage},
      {"CDi", custom_stristr, UCON64_CDI, cdi_usage},
      {"Xbox", custom_stristr, UCON64_XBOX, xbox_usage},
      {"CD32", custom_stristr, UCON64_CD32, cd32_usage},
/* TODO:
      {"psx", custom_stristr, UCON64_PSX, psx_usage},
      {"ps1", custom_stristr, UCON64_PSX, psx_usage},
      {"psone", custom_stristr, UCON64_PSX, psx_usage},
      {"ps2", custom_stristr, UCON64_PS2, ps2_usage},
      {"dc", custom_stristr, UCON64_DC, dc_usage},
      {"system", custom_stristr, UCON64_S16, s16_usage},
      {"pocket", custom_stristr, UCON64_NGP, ngp_usage},
      {"virtual", custom_stristr, UCON64_VBOY, vboy_usage},
      {"", custom_stristr, 0, channelf_usage},
      {"", custom_stristr, 0, gamecom_usage},
      {"", custom_stristr, 0, gc_usage},
      {"", custom_stristr, 0, gp32_usage},
      {"", custom_stristr, 0, odyssey2_usage},
      {"", custom_stristr, 0, odyssey_usage},
      {"", custom_stristr, 0, s16_usage},
      {"", custom_stristr, 0, sat_usage},
      {"", custom_stristr, 0, vc4000_usage},
*/
      {0, 0, 0, 0}
    };

  for (pos = 0; console_type[pos].id; pos++)
    if (!console_type[pos].compare (fname, console_type[pos].id))
      {
        dat->console = console_type[pos].console;
        dat->console_usage = (console_type[pos].console_usage[0].help);
        break;
      }

  if (console_type[pos].id == 0)
    {
      if (warning)
        printf ("WARNING: \"%s\" is meant for a console unknown to uCON64\n"
                "         Please see the FAQ, question 56\n\n", fname);
      dat->console = UCON64_UNKNOWN;
      dat->console_usage = NULL;
    }

  return dat->console;
}


static void
reset_dat (st_ucon64_dat_t *dat)
{
  dat->crc32 = 0;
  dat->console = 0;
  dat->name[0] = '\0';
  dat->maker = NULL;
  dat->country = NULL;
  dat->misc[0] = '\0';
  dat->fname[0] = '\0';
  dat->fsize = 0;
  dat->datfile[0] = '\0';
  dat->author[0] = '\0';
  dat->version[0] = '\0';
  dat->date[0] = '\0';
  dat->comment[0] = '\0';
  dat->refname[0] = '\0';
  dat->console_usage = NULL;
  dat->backup_usage = NULL;
}


static st_ucon64_dat_t *
line_to_dat (const char *fname, const char *dat_entry, st_ucon64_dat_t *dat)
// parse a dat entry into st_ucon64_dat_t
{
  static const char *dat_country[28][2] =
    {
      {"(FC)", "French Canada"},
      {"(FN)", "Finland"},
      {"(G)", "Germany"},
      {"(GR)", "Greece"},
      {"(H)", "Holland"},               // other (incorrect) name for The Netherlands
      {"(HK)", "Hong Kong"},
      {"(I)", "Italy"},
      {"(J)", "Japan"},
      {"(JE)", "Japan & Europe"},
      {"(JU)", "Japan & U.S.A."},
      {"(JUE)", "Japan, U.S.A. & Europe"},
      {"(K)", "Korea"},
      {"(NL)", "The Netherlands"},
      {"(PD)", "Public Domain"},
      {"(S)", "Spain"},
      {"(SW)", "Sweden"},
      {"(U)", "U.S.A."},
      {"(UE)", "U.S.A. & Europe"},
      {"(UK)", "England"},
      {"(Unk)", "Unknown country"},
      /*
        At least (A), (B), (C), (E) and (F) have to come after the other
        countries, because some games have (A), (B) etc. in their name (the
        non-country part). For example, the SNES games
        "SD Gundam Generations (A) 1 Nen Sensouki (J) (ST)" or
        "SD Gundam Generations (B) Guripus Senki (J) (ST)".
      */
      {"(1)", "Japan & Korea"},
      {"(4)", "U.S.A. & Brazil NTSC"},
      {"(A)", "Australia"},
      {"(B)", "non U.S.A. (Genesis)"},
      {"(C)", "China"},
      {"(E)", "Europe"},
      {"(F)", "France"},
      {NULL, NULL}
    };
  char *dat_field[MAX_FIELDS_IN_DAT + 2] = { NULL }, buf[MAXBUFSIZE], *p = NULL;

  if ((unsigned char) dat_entry[0] != DAT_FIELD_SEPARATOR)
    return NULL;

  strcpy (buf, dat_entry);
  strarg (dat_field, buf, DAT_FIELD_SEPARATOR_S, MAX_FIELDS_IN_DAT);
  reset_dat (dat);
  strcpy (dat->datfile, basename2 (fname));

  if (dat_field[3])
    {
      size_t len = strnlen (dat_field[3], sizeof dat->name - 1);

      strncpy (dat->name, dat_field[3], len)[len] = '\0';
    }

  if (dat_field[4])
    {
      size_t len = strnlen (dat_field[4], sizeof dat->fname - 1);

      strncpy (dat->fname, dat_field[4], len)[len] = '\0';
    }

  if (dat_field[5])
    sscanf (dat_field[5], "%x", &dat->crc32);

  if (dat_field[6][0] == 'N' && dat_field[7][0] == 'O')
    // e.g. GoodSNES bad crc & Nintendo FDS DAT
    sscanf (dat_field[8], "%d", (int *) &dat->fsize);
  else
    sscanf (dat_field[6], "%d", (int *) &dat->fsize);

  *buf = '\0';
  {
    static const char *dat_flags[][2] =
      {
        // Often flags contain numbers, so don't search for the closing bracket
        {"[a", "Alternate"},
        {"[p", "Pirate"},
        {"[b", "Bad dump"},
        {"[t", "Trained"},
        {"[f", "Fixed"},
        {"[T", "Translation"},
        {"[h", "Hack"},
        {"[x", "Bad checksum"},
        {"[o", "Overdump"},
        {"[!]", "Verified good dump"}, // [!] is OK
        {NULL, NULL}
      };
    int x = 0;

    for (x = 0, p = buf; dat_flags[x][0]; x++, p += strlen (p))
      if (strstr (dat->name, dat_flags[x][0]))
        sprintf (p, "%s, ", dat_flags[x][1]);
  }
  if (buf[0])
    {
      if ((p = strrchr (buf, ',')) != NULL)
        *p = '\0';
      snprintf (dat->misc, sizeof dat->misc, "Flags: %s", buf);
      dat->misc[sizeof dat->misc - 1] = '\0';
    }

  p = dat->name;
  dat->country = NULL;
  {
    uint32_t pos = 0;

    for (pos = 0; dat_country[pos][0]; pos++)
      if (stristr (p, dat_country[pos][0]))
        {
          dat->country = dat_country[pos][1];
          break;
        }
  }

  fname_to_console (dat->datfile, dat, 0);
  dat->backup_usage = unknown_backup_usage[0].help;

  return dat;
}


uint32_t
line_to_crc (const char *dat_entry)
// get CRC32 value of current line
{
  char *dat_field[MAX_FIELDS_IN_DAT + 2] = { NULL }, buf[MAXBUFSIZE];
  uint32_t crc32 = 0;

  if ((unsigned char) dat_entry[0] != DAT_FIELD_SEPARATOR)
    return 0;

  strcpy (buf, dat_entry);

  strarg (dat_field, buf, DAT_FIELD_SEPARATOR_S, MAX_FIELDS_IN_DAT);

  if (dat_field[5])
    sscanf (dat_field[5], "%x", &crc32);

  return crc32;
}


static st_ucon64_dat_t *
get_dat_entry (char *fname, st_ucon64_dat_t *dat, uint32_t crc32, long start)
{
  char buf[MAXBUFSIZE];

  if (!fdat && (fdat = fopen (fname, "rb")) == NULL)
    {
      fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], fname);
#if     defined _WIN32 || defined __CYGWIN__ || defined __MSDOS__
      if (!stricmp (basename2 (fname), "ntuser.dat"))
        fputs ("       Please see the FAQ, question 47 & 36\n", stderr);
        //     "ERROR: "
#endif
      return NULL;
    }

  if (start >= 0)
    fseek (fdat, start, SEEK_SET);

  filepos_line = ftell (fdat);
  while (fgets (buf, MAXBUFSIZE, fdat) != NULL)
    {
      if ((unsigned char) buf[0] == DAT_FIELD_SEPARATOR &&
          (!crc32 || line_to_crc (buf) == crc32) &&
          line_to_dat (fname, buf, dat))
        return dat;
      filepos_line = ftell (fdat);
    }

  fclose_fdat ();
  return NULL;
}


static void
update_fname_field (st_ucon64_dat_t *dat)
{
  /*
    The DAT files are not consistent. Some include the file suffix, but others
    don't. We want to display the canonical filename only if it really differs
    from the canonical game name (usually filename without suffix).
  */
  char *p = (char *) get_suffix (dat->fname);

  if (strlen (p) < 5)
    {
      int suffix = 0;

      switch (dat->console)
        {
        case UCON64_SNES:
          suffix = !(stricmp (p, ".smc") &&     // SNES
                     stricmp (p, ".sfc") &&     // SNES
                     stricmp (p, ".bin"));      // enhancement chips
          break;
        case UCON64_NES:
          suffix = !(stricmp (p, ".fds") &&     // NES FDS
                     stricmp (p, ".nes") &&     // NES
                     stricmp (p, ".bin"));      // BIOS dumps
          break;
        case UCON64_GBA:
          suffix = !stricmp (p, ".gba");        // Game Boy Advance
          break;
        case UCON64_GB:
          suffix = !(stricmp (p, ".gb") &&      // Game Boy
                     stricmp (p, ".gbc"));      // Game Boy Color
          break;
        case UCON64_GEN:
          suffix = !(stricmp (p, ".smd") &&     // Genesis
                     stricmp (p, ".md"));       // Genesis
          break;
        case UCON64_SMS:
          suffix = !(stricmp (p, ".sc") &&      // Sega Master System
                     stricmp (p, ".sg") &&      // Sega Master System
                     stricmp (p, ".sms") &&     // Sega Master System
                     stricmp (p, ".gg"));       // Game Gear
          break;
        case UCON64_JAG:
          suffix = !stricmp (p, ".j64");        // Jaguar
          break;
        case UCON64_LYNX:
          suffix = !stricmp (p, ".lnx");        // Lynx
          break;
        case UCON64_N64:
          suffix = !(stricmp (p, ".v64") &&     // Nintendo 64
                     stricmp (p, ".z64") &&     // Nintendo 64
                     stricmp (p, ".ndd"));      // Nintendo 64DD
          break;
        case UCON64_PCE:
          suffix = !stricmp (p, ".pce");        // PC-Engine / TurboGrafx-16
          break;
        case UCON64_ATA:
          suffix = !(stricmp (p, ".a26") &&     // Atari 2600
                     stricmp (p, ".a52") &&     // Atari 5200
                     stricmp (p, ".a78"));      // Atari 7800
          break;
        case UCON64_VEC:
          suffix = !(stricmp (p, ".vec") &&     // Vectrex
                     stricmp (p, ".bin"));      // BIOS dumps
          break;
        case UCON64_SWAN:
          suffix = !(stricmp (p, ".ws") &&      // WonderSwan
                     stricmp (p, ".wsc"));      // WonderSwan Color
          break;
        case UCON64_COLECO:
          suffix = !stricmp (p, ".col");        // ColecoVision
          break;
        case UCON64_VBOY:
          suffix = !stricmp (p, ".vb");         // Virtual Boy
          break;
        case UCON64_INTELLI:
        case UCON64_NGP:
        case UCON64_NG:
        case UCON64_ARCADE:
        case UCON64_DC:
        case UCON64_SAT:
        case UCON64_3DO:
        case UCON64_CDI:
        case UCON64_XBOX:
        case UCON64_CD32:
        default:
          break;
        }
      if (suffix)
        *p = '\0';
    }
}


int
ucon64_dat_view (int console, int verbose)
{
  char fname_dat[FILENAME_MAX];
  int n_entries_sum = 0, n_datfiles = 0;

  while (get_next_file (fname_dat))
    {
      char fname_index[FILENAME_MAX];
      unsigned char *p;
      st_ucon64_dat_t dat;
      size_t fsize;
      int n_entries;
      const char *fname = basename2 (fname_dat);

      if (console != UCON64_UNKNOWN &&
          fname_to_console (fname, &dat, 0) != console)
        continue;

      get_dat_header (fname_dat, &dat);
      strcpy (fname_index, fname_dat);
      set_suffix (fname_index, ".idx");
      if (access (fname_index, F_OK) != 0)      // for a "bad" DAT file
        continue;
      fsize = (size_t) fsizeof (fname_index);

      n_entries = (int) (fsize / sizeof (st_idx_entry_t));
      n_entries_sum += n_entries;
      n_datfiles++;

      printf ("DAT info:\n"
              "  %s\n"
//              "  Console: %s\n"
              "  Version: %s (%s%s%s)\n"
              "  Author: %s\n"
              "  Comment: %s\n"
              "  Entries: %d\n\n",
              fname,
//              dat.console_usage[0],
              dat.version,
              dat.date,
              dat.date[0] ? ", " : "",
              dat.refname,
              dat.author,
              dat.comment,
              n_entries);

      if ((p = (unsigned char *) malloc (fsize)) == NULL)
        {
          fprintf (stderr, ucon64_msg[BUFFER_ERROR], fsize);
          continue;
        }

      if (ucon64_fread (p, 0, fsize, fname_index) != fsize)
        {
          fprintf (stderr, ucon64_msg[READ_ERROR], fname_index);
          free (p);
          continue;
        }

      if (verbose)
        {
          int n;

          // display all DAT entries
          for (n = 0; n < n_entries; n++)
            {
              st_idx_entry_t *idx_entry = &((st_idx_entry_t *) p)[n];
              printf ("Checksum (CRC32): 0x%08x\n", idx_entry->crc32);
              if (get_dat_entry (fname_dat, &dat, idx_entry->crc32, idx_entry->filepos))
                {
                  update_fname_field (&dat);
                  ucon64_dat_nfo (&dat, 0);
                }
              fputc ('\n', stdout);
            }
          fclose_fdat ();
        }
      free (p);
    }

  printf ("DAT files: %d; entries: %d; total entries: %u\n",
          n_datfiles, n_entries_sum, ucon64_dat_total_entries ());

  return 0;
}


uint32_t
ucon64_dat_total_entries (void)
{
  uint32_t entries = 0;
  char fname[FILENAME_MAX];

  if (!ucon64.dat_enabled)
    return 0;

  while (get_next_file (fname))
    {
      int64_t fsize;

      set_suffix (fname, ".idx");
      fsize = fsizeof (fname);
      entries += (uint32_t) (fsize < 0 ? 0 : fsize / sizeof (st_idx_entry_t));
    }

  return entries;
}


static int
idx_compare (const void *key, const void *found)
{
  /*
    The return statement looks overly complicated, but is really necessary.
    This construct:
      return ((st_idx_entry_t *) key)->crc32 - ((st_idx_entry_t *) found)->crc32;
    does *not* work correctly for all cases.
  */
  return (int) (((int64_t) ((st_idx_entry_t *) key)->crc32 -
                 (int64_t) ((st_idx_entry_t *) found)->crc32) / 2);
}


st_ucon64_dat_t *
ucon64_dat_search (uint32_t crc32)
{
  char fname_dat[FILENAME_MAX];
  static st_ucon64_dat_t dat;

  if (!crc32)
    return NULL;

  reset_dat (&dat);

  while (get_next_file (fname_dat))
    {
      char fname_index[FILENAME_MAX];
      unsigned char *p;
      size_t fsize;
      st_idx_entry_t *idx_entry, key;
      const char *fname = basename2 (fname_dat);

      if (ucon64.console != UCON64_UNKNOWN &&
          fname_to_console (fname, &dat, 0) != ucon64.console)
        continue;

      strcpy (fname_index, fname_dat);
      set_suffix (fname_index, ".idx");
      if (access (fname_index, F_OK) != 0)      // for a "bad" DAT file
        continue;
      fsize = (size_t) fsizeof (fname_index);

      if ((p = (unsigned char *) malloc (fsize)) == NULL)
        {
          fprintf (stderr, ucon64_msg[BUFFER_ERROR], fsize);
          closedir_ddat ();
          return NULL;
        }

      // load the index for the current dat file
      if (ucon64_fread (p, 0, fsize, fname_index) != fsize)
        {
          fprintf (stderr, ucon64_msg[READ_ERROR], fname_index);
          closedir_ddat ();
          free (p);
          return NULL;
        }

      // search index for CRC32 value
      key.crc32 = crc32;
      idx_entry = (st_idx_entry_t *) bsearch (&key, p, fsize / sizeof (st_idx_entry_t),
                                              sizeof (st_idx_entry_t), idx_compare);
      if (idx_entry)                            // CRC32 value found
        {
          // open dat file and read entry
          if (get_dat_entry (fname_dat, &dat, crc32, idx_entry->filepos) &&
              crc32 == dat.crc32)
            {
              strcpy (dat.datfile, basename2 (fname_dat));
              get_dat_header (fname_dat, &dat);
              closedir_ddat ();
              fclose_fdat ();
              free (p);
              update_fname_field (&dat);
              return &dat;
            }
          fclose_fdat ();
        }
      free (p);
    }

  return NULL;
}


int
ucon64_dat_indexer (void)
// create or update index of DAT file
{
  char fname_dat[FILENAME_MAX];
  st_idx_entry_t *idx_entries;

  if ((idx_entries = (st_idx_entry_t *)
         malloc (MAX_GAMES_FOR_CONSOLE * sizeof (st_idx_entry_t))) == NULL)
    {
      fprintf (stderr, ucon64_msg[BUFFER_ERROR],
               MAX_GAMES_FOR_CONSOLE * sizeof (st_idx_entry_t));
      exit (1);
    }

  while (get_next_file (fname_dat))
    {
      char fname_index[FILENAME_MAX], errorfname[FILENAME_MAX];
      struct stat fstate_dat, fstate_index;
      st_ucon64_dat_t dat;
      FILE *errorfile = NULL;
      time_t start_time;
      int update = 0, n_duplicates = 0, pos = 0;
      size_t fsize;
      const char *fname = basename2 (fname_dat);

      if (fname_to_console (fname, &dat, 1) == UCON64_UNKNOWN)
        continue;

      strcpy (fname_index, fname_dat);
      set_suffix (fname_index, ".idx");

      if (!stat (fname_dat, &fstate_dat) && !stat (fname_index, &fstate_index))
        {
          if (fstate_dat.st_mtime < fstate_index.st_mtime)
            continue;                   // index file seems to be present and up-to-date
          update = 1;
        }

      start_time = time (NULL);
      fsize = (size_t) fsizeof (fname_dat);

      printf ("%s: %s\n", (update ? "Update" : "Create"), basename2 (fname_index));
      while (get_dat_entry (fname_dat, &dat, 0, -1))
        {
          int n;
          st_idx_entry_t *idx_entry = NULL;

          if (pos == MAX_GAMES_FOR_CONSOLE)
            {
              fprintf (stderr,
                       "\n"
                       "INTERNAL ERROR: MAX_GAMES_FOR_CONSOLE is too small (%d)\n",
                       MAX_GAMES_FOR_CONSOLE);
              break;
            }

          /*
            Doing a linear search removes the need of using the slow qsort()
            function inside the loop. Doing a binary search doesn't improve the
            speed much, but is much more efficient of course. Using qsort()
            inside the loop slows it down with a factor greater than 10.
          */
          for (n = 0; n < pos; n++)
            if (idx_entries[n].crc32 == dat.crc32)
              idx_entry = &idx_entries[n];
          if (idx_entry)
            {
              // This really makes one lose trust in the DAT files...
              char current_name[2 * 80];
              long current_filepos = ftell (fdat);

              if (!errorfile)
                {
                  strcpy (errorfname, fname_index);
                  set_suffix (errorfname, ".err");
                  if ((errorfile = fopen (errorfname, "w")) == NULL) // text file for WinDOS
                    {
                      fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], errorfname);
                      continue;
                    }
                }

              strcpy (current_name, dat.name);
              get_dat_entry (fname_dat, &dat, 0, idx_entry->filepos);
              fprintf (errorfile,
                       "\n"
                       "WARNING: DAT file contains a duplicate CRC32 value (0x%x)!\n"
                       "  First game with this CRC32 value: \"%s\"\n"
                       "  Ignoring game:                    \"%s\"\n",
                       dat.crc32, dat.name, current_name);

              n_duplicates++;
              fseek (fdat, current_filepos, SEEK_SET);
              continue;
            }

          idx_entries[pos].crc32 = dat.crc32;
          idx_entries[pos].filepos = filepos_line;

          if (!(pos % 20))
            ucon64_gauge (start_time, ftell (fdat), fsize);
          pos++;
        }
      fclose_fdat ();

      if (pos > 0)
        {
          qsort (idx_entries, pos, sizeof (st_idx_entry_t), idx_compare);
          if (ucon64_fwrite (idx_entries, 0, pos * sizeof (st_idx_entry_t), fname_index, "wb")
                != pos * sizeof (st_idx_entry_t))
            {
              fputc ('\n', stderr);
              fprintf (stderr, ucon64_msg[WRITE_ERROR], fname_index);
            }
          ucon64_gauge (start_time, fsize, fsize);
        }

      if (n_duplicates > 0)
        printf ("\n"
                "\n"
                "WARNING: DAT file contains %d duplicate CRC32 value%s\n"
                "         Warnings have been written to %s",
                n_duplicates, n_duplicates != 1 ? "s" : "", errorfname);
      if (errorfile)
        {
          fclose (errorfile);
          errorfile = NULL;
        }
      fputs ("\n\n", stdout);
    }
  free (idx_entries);

  return 0;
}


void
ucon64_dat_nfo (const st_ucon64_dat_t *dat, int display_dat_file_line)
{
  char *p = NULL;

  if (!dat)
    {
      printf (ucon64_msg[DAT_NOT_FOUND], ucon64.crc32);
      return;
    }

  fputs ("DAT info:\n", stdout);
  // console type?
  if (dat->console_usage != NULL)
    {
      char buf[MAXBUFSIZE];

      strcpy (buf, dat->console_usage);
      // fix ugly multi-line console "usages" (PC-Engine)
      if ((p = strchr (buf, '\n')) != NULL)
        *p = '\0';
      printf ("  %s\n", buf);
    }

  printf ("  %s\n", dat->name);

  if (dat->country)
    printf ("  %s\n", dat->country);

  if (stricmp (dat->name, dat->fname) != 0)
    printf ("  Filename: %s\n", dat->fname);

  printf ("  %d Bytes (%.4f Mb)\n", (int) dat->fsize, TOMBIT_F (dat->fsize));

  if (dat->misc[0])
    printf ("  %s\n", dat->misc);

  if (display_dat_file_line)
    {
      char format[80];
      int display_version = stristr (dat->datfile, dat->version) ? 0 : 1;

      snprintf (format, 80, "  %%s (%s%s%s%s%%s)\n",
                display_version ? dat->version : "", display_version ? ", " : "",
                dat->date, dat->date[0] ? ", " : "");
      format[80 - 1] = '\0';
      printf (format, dat->datfile, dat->refname);
    }
}


static void
ucon64_close_datfile (void)
{
  if (ucon64_datfile)
    {
      int n;

      fclose (ucon64_datfile);
      printf (ucon64_msg[WROTE], ucon64_dat_fname);
      ucon64_datfile = NULL;

      for (n = 0; n < ucon64_n_files; n++)
        {
          free (ucon64_mkdat_entries[n].fname);
          ucon64_mkdat_entries[n].fname = NULL;
        }
      ucon64_n_files = 0;
    }
}


int
ucon64_create_dat (const char *dat_file_name, const char *filename,
                   int backup_header_len)
{
  static int first_file = 1, console;
  int n;
  static char *console_name;
  char fname[FILENAME_MAX], *ptr;
  time_t time_t_val;

  if (first_file)
    {
      struct tm *t;
      char *plugin = "";

      first_file = 0;
      console = ucon64.console;
      switch (ucon64.console)
        {
        case UCON64_3DO:
          console_name = "3DO";
          break;
        case UCON64_ATA:
          console_name = "NES";
          break;
        case UCON64_CD32:
          console_name = "CD32";
          break;
        case UCON64_CDI:
          console_name = "CD-i";
          break;
        case UCON64_COLECO:
          console_name = "Coleco";
          break;
        case UCON64_DC:
          console_name = "Dreamcast";
          break;
        case UCON64_GB:
          console_name = "Game Boy";
          break;
        case UCON64_GBA:
          console_name = "Game Boy Advance";
          break;
        case UCON64_GC:
          console_name = "Game Cube";
          break;
        case UCON64_GEN:
          console_name = "Genesis/Mega Drive";
          plugin = "genesis.dll";
          break;
        case UCON64_INTELLI:
          console_name = "Intellivision";
          break;
        case UCON64_JAG:
          console_name = "Jaguar";
          break;
        case UCON64_LYNX:
          console_name = "Lynx";
          break;
        case UCON64_ARCADE:
          console_name = "M.A.M.E.";
          plugin = "arcade.dll";
          break;
        case UCON64_N64:
          console_name = "Nintendo 64";
          plugin = "n64.dll";
          break;
        case UCON64_NES:
          console_name = "NES";
          plugin = "nes.dll";
          break;
        case UCON64_NG:
          console_name = "Neo Geo";
          plugin = "arcade.dll";
          break;
        case UCON64_NGP:
          console_name = "Neo Geo Pocket";
          break;
        case UCON64_PCE:
          console_name = "PC-Engine";
          break;
        case UCON64_PS2:
          console_name = "Playstation 2";
          break;
        case UCON64_PSX:
          console_name = "Playstation";
          break;
        case UCON64_S16:
          console_name = "S16";
          break;
        case UCON64_SAT:
          console_name = "Saturn";
          break;
        case UCON64_SMS:
          console_name = "SMS/Game Gear";
          plugin = "sms.dll";           // change this to gg.dll (in DAT file) for GG ROMs
          break;
        case UCON64_SNES:
          console_name = "SNES";
          plugin = "snes.dll";          // be sure to use the new SNES plug-in (RC 2.62)
          break;
        case UCON64_SWAN:
          console_name = "Wonderswan";
          break;
        case UCON64_VBOY:
          console_name = "Virtual Boy";
          break;
        case UCON64_VEC:
          console_name = "Vectrex";
          break;
        case UCON64_XBOX:
          console_name = "Xbox";
          break;
        default:
          fputs (ucon64_msg[CONSOLE_WARNING], stderr);
          exit (1);
          break;
        }

      if ((ucon64_mkdat_entries = (st_mkdat_entry_t *)
            malloc (MAX_GAMES_FOR_CONSOLE * sizeof (st_mkdat_entry_t))) == NULL)
        {
          fprintf (stderr, ucon64_msg[BUFFER_ERROR],
                   MAX_GAMES_FOR_CONSOLE * sizeof (st_mkdat_entry_t));
          exit (1);
        }

      strcpy (ucon64_dat_fname, dat_file_name);
      ucon64_file_handler (ucon64_dat_fname, NULL, OF_FORCE_BASENAME);
      if ((ucon64_datfile = fopen (ucon64_dat_fname, "wb")) == NULL)
        {
          fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], ucon64_dat_fname);
          exit (1);
        }
      register_func (ucon64_close_datfile);

      time_t_val = time (NULL);
      t = localtime (&time_t_val);
      // RomCenter uses files in DOS text format, so we generate a file in that format
      fprintf (ucon64_datfile, "[CREDITS]\r\n"
                               "author=uCON64\r\n"
                               "email=ucon64-main@lists.sourceforge.net\r\n"
                               "homepage=uCON64 homepage\r\n"
                               "url=ucon64.sf.net\r\n"
                               "version=%s-%s\r\n"
                               "date=%d/%d/%d\r\n"
                               "comment=%s DAT file generated by uCON64\r\n"
                               "[DAT]\r\n"
                               "version=2.50\r\n" // required by RomCenter!
                               "plugin=%s\r\n"
                               "[EMULATOR]\r\n"
                               "refname=%s\r\n"
                               "version=\r\n"
                               "[GAMES]\r\n",
                               UCON64_VERSION_S, console_name,
                               t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
                               console_name,
                               plugin,
                               console_name);
    } // first_file

  if (ucon64_n_files == MAX_GAMES_FOR_CONSOLE)
    {
      fprintf (stderr,
               "INTERNAL ERROR: MAX_GAMES_FOR_CONSOLE is too small (%d)\n",
               MAX_GAMES_FOR_CONSOLE);
      exit (1);
    }
  strcpy (fname, basename2 (filename));

  // Check the console type
  n = 0;
  if (ucon64.console != console)
    {
      if (ucon64.quiet < 0)
        printf ("WARNING: Skipping (!%s) ", console_name);
      else
        return -1;
    }
  else
    {
      // Check if the CRC32 value is unique. We don't want to be as stupid as
      //  the tool used to create the GoodDAT files.
      // Yes, a plain and simple linear search. Analyzing the files is orders
      //  of magnitude slower than this search.
      for (; n < ucon64_n_files; n++)
        if (ucon64_mkdat_entries[n].crc32 == ucon64.crc32)
          break;
      if (n != ucon64_n_files)
        {
          if (ucon64.quiet < 1)                 // better print this by default
            fputs ("WARNING: Skipping (duplicate) ", stdout);
          else
            return -1;
        }
    }

  fputs (filename, stdout);
  if (ucon64.quiet < 0 && ucon64.fname_arch[0]) // -v was specified
    printf (" (%s)", ucon64.fname_arch);
  fputc ('\n', stdout);

  if (ucon64.console != console)                // ucon64.quiet < 0 (-1)
    return -1;
  if (n != ucon64_n_files)
    {
      if (ucon64.quiet < 1)                     // better print this by default
        printf ("         First file with this CRC32 value (0x%x) is:\n"
                "         \"%s\"\n", ucon64.crc32, ucon64_mkdat_entries[n].fname);
      return -1;
    }

  // store the CRC32 value to check if a file is unique
  ucon64_mkdat_entries[ucon64_n_files].crc32 = ucon64.crc32;
  /*
    Also store the name of the file to display a helpful error message if a
    file is not unique (a duplicate). We store the current filename inside the
    archive as well, to be even more helpful :-)
  */
  {
    size_t size = strlen (fname) + (ucon64.fname_arch[0] ? strlen (ucon64.fname_arch) + 4 : 1);

    if ((ucon64_mkdat_entries[ucon64_n_files].fname = (char *) malloc (size)) == NULL)
      {                                                   // + 3 for " ()"
        fprintf (stderr, ucon64_msg[BUFFER_ERROR], size); //  + 1 for ASCII-z
        exit (1);
      }
  }
  sprintf (ucon64_mkdat_entries[ucon64_n_files].fname, "%s%s%s%s",
           fname,
           ucon64.fname_arch[0] ? " (" : "",
           ucon64.fname_arch[0] ? ucon64.fname_arch : "",
           ucon64.fname_arch[0] ? ")" : "");

  ptr = (char *) get_suffix (fname);
  if (*ptr)
    *ptr = '\0';
  fprintf (ucon64_datfile, DAT_FIELD_SEPARATOR_S "%s" // set filename
                           DAT_FIELD_SEPARATOR_S "%s" // set full name
                           DAT_FIELD_SEPARATOR_S "%s" // clone filename
                           DAT_FIELD_SEPARATOR_S "%s" // clone full name
                           DAT_FIELD_SEPARATOR_S "%s" // ROM filename
                           DAT_FIELD_SEPARATOR_S "%08x" // RC quirck: leading zeroes are required
                           DAT_FIELD_SEPARATOR_S "%u"
                           DAT_FIELD_SEPARATOR_S // merged clone name
                           DAT_FIELD_SEPARATOR_S // merged ROM name
                           DAT_FIELD_SEPARATOR_S "\r\n",
                           fname,
                           fname,
                           fname,
                           fname,
                           fname,
                           ucon64.crc32,
                           (unsigned int) ucon64.fsize - backup_header_len);
  ucon64_n_files++;
  return 0;
}
