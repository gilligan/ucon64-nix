/*
ucon64_opts.c - switches for all uCON64 options

Copyright (c) 2002 - 2005              NoisyB
Copyright (c) 2002 - 2005, 2015 - 2021 dbjh
Copyright (c) 2005                     Jan-Erik Karlsson (Amiga)


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
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4820) // 'bytes' bytes padding added after construct 'member_name'
#include <io.h>
#pragma warning(pop)
#endif
#include <stdlib.h>
#ifdef  HAVE_UNISTD_H
#include <unistd.h>
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
#include "misc/parallel.h"
#include "misc/string.h"
#include "misc/term.h" // to avoid MinGW warning about using ll prefix in fprintf()
#include "ucon64_dat.h"
#include "ucon64_misc.h"
#include "ucon64_opts.h"
#include "console/dc.h"
#include "console/gb.h"
#include "console/gba.h"
#include "console/genesis.h"
#include "console/lynx.h"
#include "console/n64.h"
#include "console/nds.h"
#include "console/neogeo.h"
#include "console/nes.h"
#include "console/pce.h"
#include "console/sms.h"
#include "console/snes.h"
#include "console/swan.h"
#include "backup/backup.h"
#include "backup/cd64.h"
#include "backup/cmc.h"
#include "backup/doctor64.h"
#include "backup/doctor64jr.h"
#include "backup/f2a.h"
#include "backup/fal.h"
#include "backup/gbx.h"
#include "backup/gd.h"
#include "backup/lynxit.h"
#include "backup/mccl.h"
#include "backup/mcd.h"
#include "backup/md-pro.h"
#include "backup/msg.h"
#include "backup/pce-pro.h"
#include "backup/pl.h"
#include "backup/quickdev16.h"
#include "backup/sflash.h"
#include "backup/smc.h"
#include "backup/smcic2.h"
#include "backup/smd.h"
#include "backup/smsgg-pro.h"
#include "backup/swc.h"
#include "backup/ufosd.h"
#include "patch/aps.h"
#include "patch/bsl.h"
#include "patch/gg.h"
#include "patch/ips.h"
#include "patch/ppf.h"


static int
strtoint (const char *str, int base, void *var, size_t var_size)
{
  char *endptr;
  int64_t value = 0;

  if (var_size > sizeof value)
    var_size = sizeof value;
  errno = 0;
  value = strtoll (str, &endptr, base);
  if (errno || *endptr)
    return -1;
  else
#ifndef WORDS_BIGENDIAN
    memcpy (var, &value, var_size);
#else
    memcpy (var, ((uint8_t *) &value) + sizeof value - var_size, var_size);
#endif

  return 0;
}


int
ucon64_switches (st_ucon64_t *p)
{
  int x = 0;
  const char *option_arg = p->optarg;

  /*
    Handle options or switches that cause other _options_ to be ignored except
    other options of the same class (so the order in which they were specified
    matters).
    We have to do this here (not in ucon64_options()) or else other options
    might be executed before these.
  */
  switch (p->option)
    {
    /*
      Many tools ignore other options if --help has been specified. We do the
      same (compare with GNU tools).
    */
    case UCON64_HELP:
      x = USAGE_VIEW_LONG;
      if (option_arg)
        {
          if (!strcmp (option_arg, "pad"))
            x = USAGE_VIEW_PAD;
          else if (!strcmp (option_arg, "dat"))
            x = USAGE_VIEW_DAT;
          else if (!strcmp (option_arg, "patch"))
            x = USAGE_VIEW_PATCH;
          else if (!strcmp (option_arg, "backup"))
            x = USAGE_VIEW_BACKUP;
          else if (!strcmp (option_arg, "disc"))
            x = USAGE_VIEW_DISC;
        }
      ucon64_usage (ucon64.argc, ucon64.argv, x);
      exit (0);

    /*
      It's also common to exit after displaying version information.
      On some configurations printf is a macro (Red Hat Linux 6.2 + GCC 3.2),
      so we can't use preprocessor directives in the argument list.
    */
    case UCON64_VER:
      printf ("version:                           %s (%s)\n"
              "platform:                          %s\n",
              UCON64_VERSION_S, __DATE__, CURRENT_OS_S);

#ifdef  USE_PARALLEL
#ifdef  USE_PPDEV
      puts ("parallel port backup unit support: yes (ppdev)");
#else
      puts ("parallel port backup unit support: yes");
#endif // USE_PPDEV
#else
      puts ("parallel port backup unit support: no");
#endif

#ifdef  USE_USB
      puts ("USB port backup unit support:      yes");
#else
      puts ("USB port backup unit support:      no");
#endif


#ifdef  USE_ANSI_COLOR
      puts ("ANSI colors enabled:               yes");
#else
      puts ("ANSI colors enabled:               no");
#endif

#ifdef  USE_ZLIB
      printf ("gzip and zip support:              yes, zlib %s\n",
              ZLIB_VERSION);
#else
      puts ("gzip and zip support:              no");
#endif

      printf ("configuration file %s  %s\n",
              // display the existence only for the config file (really helps solving problems)
              access (ucon64.configfile, F_OK) ? "(not present):" : "(present):    ",
              ucon64.configfile);

#ifdef  USE_DISCMAGE
      fputs ("discmage DLL:                      ", stdout);

#ifdef  DLOPEN
      puts (ucon64.discmage_path);
#else
#if     defined __MSDOS__
      fputs ("discmage.dxe", stdout);
#elif   defined __CYGWIN__ || defined _WIN32
      fputs ("discmage.dll", stdout);
#elif   defined __APPLE__                       // Mac OS X actually
      fputs ("libdiscmage.dylib", stdout);
#elif   defined __unix__ || defined __BEOS__
      fputs ("libdiscmage.so", stdout);
#else
      fputs ("unknown", stdout);
#endif
      puts (", dynamically linked");
#endif // DLOPEN

      if (ucon64.discmage_enabled)
        {
          x = dm_get_version ();
          printf ("discmage enabled:                  yes, %d.%d.%d (%s)\n",
                  (x >> 16) & 0xff, (x >> 8) & 0xff, x & 0xff,
                  dm_get_version_s ());
        }
      else
        puts ("discmage enabled:                  no");
#endif // USE_DISCMAGE

      printf ("configuration directory:           %s\n"
              "DAT file directory:                %s\n"
              "entries in DATabase:               %u\n"
              "DATabase enabled:                  %s\n",
              ucon64.configdir,
              ucon64.datdir,
              ucon64_dat_total_entries (),
              ucon64.dat_enabled ? "yes" : "no");
      exit (0);
      break;

    case UCON64_FRONTEND:
      ucon64.frontend = 1;                      // used by (for example) ucon64_gauge()
      break;

    case UCON64_NBAK:
      ucon64.backup = 0;
      break;

    case UCON64_R:
      ucon64.recursive = 1;
      break;

#ifdef  USE_ANSI_COLOR
    case UCON64_NCOL:
      ucon64.ansi_color = 0;
      break;
#endif

    case UCON64_RIP:
    case UCON64_MKTOC:
    case UCON64_MKCUE:
    case UCON64_MKSHEET:
    case UCON64_BIN2ISO:
    case UCON64_ISOFIX:
    case UCON64_XCDRW:
    case UCON64_DISC:
    case UCON64_CDMAGE:
      ucon64.force_disc = 1;
      break;

    case UCON64_NS:
      ucon64.org_split = ucon64.split = 0;
      break;

    case UCON64_HD:
      ucon64.backup_header_len = UNKNOWN_BACKUP_HEADER_LEN;
      break;

    case UCON64_HDN:
      ucon64.backup_header_len = strtol (option_arg, NULL, 10);
      break;

    case UCON64_NHD:
      ucon64.backup_header_len = 0;
      break;

    case UCON64_SWP:                            // deprecated
    case UCON64_INT:
      ucon64.interleaved = 1;
      break;

    case UCON64_INT2:
      ucon64.interleaved = 2;
      break;

    case UCON64_NSWP:                           // deprecated
    case UCON64_NINT:
      ucon64.interleaved = 0;
      break;

    case UCON64_PORT:
#ifdef  USE_USB
      if (!strnicmp (option_arg, "usb", 3))
        {
          if (strlen (option_arg) >= 4)
            ucon64.usbport = strtol (option_arg + 3, NULL, 10) + 1; // usb0 => ucon64.usbport = 1
          else                                  // we automatically detect the
            ucon64.usbport = 1;                 //  USB port in the F2A & Quickdev16 code

          /*
            We don't want to make uCON64 behave differently if --port=USB{n} is
            specified *after* a transfer option (instead of before one), so we
            have to reset ucon64.parport_needed here.
          */
          ucon64.parport_needed = 0;
        }
      else
#endif
        ucon64.parport = (uint16_t) strtol (option_arg, NULL, 16);
      break;

#ifdef  USE_PARALLEL
    /*
      We detect the presence of these options here so that we can drop
      privileges ASAP.
      Note that the libcd64 options are not listed here. We cannot drop
      privileges before libcd64 is initialised (after cd64_t.devopen() has been
      called).
    */
    case UCON64_XFIG:
    case UCON64_XFIGC:
    case UCON64_XFIGS:
    case UCON64_XIC2:
    case UCON64_XLIT:
    case UCON64_XMSG:
    case UCON64_XRESET:
    case UCON64_XSMC:
    case UCON64_XSMCR:
    case UCON64_XSMD:
    case UCON64_XSMDS:
    case UCON64_XSWC:
    case UCON64_XSWC2:
    case UCON64_XSWCC:
    case UCON64_XSWCR:
    case UCON64_XSWCS:
    case UCON64_XV64:
      if (!UCON64_ISSET2 (ucon64.parport_mode, parport_mode_t))
        ucon64.parport_mode = PPMODE_SPP;
    case UCON64_XCMC:
    case UCON64_XCMCT:
    case UCON64_XGD3:
    case UCON64_XGD3R:
    case UCON64_XGD3S:
    case UCON64_XGD6:
    case UCON64_XGD6R:
    case UCON64_XGD6S:
    case UCON64_XMCCL:
    case UCON64_XMCD:
      if (!UCON64_ISSET2 (ucon64.parport_mode, parport_mode_t))
        ucon64.parport_mode = PPMODE_SPP_BIDIR;
    case UCON64_XDJR:
    case UCON64_XF2A:                           // could be for USB version
    case UCON64_XF2AMULTI:                      // idem
    case UCON64_XF2AB:                          // idem
    case UCON64_XF2AC:                          // idem
    case UCON64_XF2AS:                          // idem
    case UCON64_XFAL:
    case UCON64_XFALMULTI:
    case UCON64_XFALB:
    case UCON64_XFALC:
    case UCON64_XFALS:
    case UCON64_XGBX:
    case UCON64_XGBXB:
    case UCON64_XGBXS:
    case UCON64_XGG:
    case UCON64_XGGB:
    case UCON64_XGGS:
    case UCON64_XMD:
    case UCON64_XMDB:
    case UCON64_XMDS:
    case UCON64_XPCE:
    case UCON64_XPL:
    case UCON64_XPLI:
    case UCON64_XSF:
    case UCON64_XSFS:
      if (!UCON64_ISSET2 (ucon64.parport_mode, parport_mode_t))
        ucon64.parport_mode = PPMODE_EPP;
#ifdef  USE_USB
      if (!ucon64.usbport)                      // no pport I/O if F2A option and USB F2A
#endif
      ucon64.parport_needed = 1;
      break;
#endif // USE_PARALLEL

#ifdef  USE_LIBCD64
    case UCON64_XCD64:
    case UCON64_XCD64B:
    case UCON64_XCD64C:
    case UCON64_XCD64E:
    case UCON64_XCD64F:
    case UCON64_XCD64M:
    case UCON64_XCD64S:
#ifdef  USE_PARALLEL
      if (!UCON64_ISSET2 (ucon64.parport_mode, parport_mode_t))
        ucon64.parport_mode = PPMODE_SPP_BIDIR;
#endif
      // We don't really need the parallel port. We just have to make sure that
      //  privileges aren't dropped.
      ucon64.parport_needed = 2;
      break;

    case UCON64_XCD64P:
      ucon64.io_mode = strtol (option_arg, NULL, 10);
      break;
#endif // USE_LIBCD64

#ifdef  USE_PARALLEL
    case UCON64_XCMCM:
      ucon64.io_mode = strtol (option_arg, NULL, 10);
      break;

    case UCON64_XFALM:
    case UCON64_XGBXM:
    case UCON64_XPLM:
      ucon64.parport_mode = PPMODE_SPP_BIDIR;
      break;

    case UCON64_XSWC_IO:
      ucon64.io_mode = strtol (option_arg, NULL, 16);

      if (ucon64.io_mode & SWC_IO_ALT_ROM_SIZE)
        puts ("WARNING: I/O mode not yet implemented");
#if 0 // all these constants are defined by default
      if (ucon64.io_mode & (SWC_IO_SPC7110 | SWC_IO_SDD1 | SWC_IO_SA1 | SWC_IO_MMX2))
        puts ("WARNING: Be sure to compile swc.c with the appropriate constants defined");
#endif

      if (ucon64.io_mode > SWC_IO_MAX)
        {
          printf ("WARNING: Invalid value for MODE (0x%x), using 0\n", ucon64.io_mode);
          ucon64.io_mode = 0;
        }
      else
        {
          printf ("I/O mode: 0x%03x", ucon64.io_mode);
          if (ucon64.io_mode)
            {
              char flagstr[160];

              flagstr[0] = '\0';
              if (ucon64.io_mode & SWC_IO_FORCE_32MBIT)
                strcat (flagstr, "force 32 Mbit dump, ");
              if (ucon64.io_mode & SWC_IO_ALT_ROM_SIZE)
                strcat (flagstr, "alternative ROM size method, ");
              if (ucon64.io_mode & SWC_IO_SUPER_FX)
                strcat (flagstr, "Super FX, ");
              if (ucon64.io_mode & SWC_IO_SDD1)
                strcat (flagstr, "S-DD1, ");
              if (ucon64.io_mode & SWC_IO_SA1)
                strcat (flagstr, "SA-1, ");
              if (ucon64.io_mode & SWC_IO_SPC7110)
                strcat (flagstr, "SPC7110, ");
              if (ucon64.io_mode & SWC_IO_DX2_TRICK)
                strcat (flagstr, "DX2 trick, ");
              if (ucon64.io_mode & SWC_IO_MMX2)
                strcat (flagstr, "Mega Man X 2, ");
              if (ucon64.io_mode & SWC_IO_DUMP_BIOS)
                strcat (flagstr, "dump BIOS, ");

              if (flagstr[0])
                flagstr[strlen (flagstr) - 2] = '\0';
              printf (" (%s)", flagstr);
            }
          fputc ('\n', stdout);
        }
      break;
#endif // USE_PARALLEL

#ifdef  USE_USB
    case UCON64_XQD16:
    case UCON64_XUFOSD:
      /*
        It is possible to perform USB I/O without being root. However, without
        any configuration root privileges are required. By default uCON64 will
        be installed setuid root (on UNIX), so we just have to make sure
        privileges will not be dropped. One way to do that is assigning 2 to
        ucon64.parport_needed. A more appropriate way for USB devices is using
        ucon64.usbport.
      */
      if (!ucon64.usbport)
        ucon64.usbport = 1;
      break;
#endif // USE_USB

    case UCON64_PATCH: // --patch and --file are the same
    case UCON64_FILE:
      ucon64.file = option_arg;
      break;

    case UCON64_I:
    case UCON64_B:
    case UCON64_A:
    case UCON64_NA:
    case UCON64_PPF:
    case UCON64_NPPF:
    case UCON64_IDPPF:
      if (!ucon64.file || !ucon64.file[0])
        ucon64.file = ucon64.argv[ucon64.argc - 1];
      break;

#if 0
    case UCON64_ROM:
      if (option_arg)
        ucon64.fname = option_arg;
      break;
#endif

    case UCON64_O:
      {
        char path[FILENAME_MAX];
        int dir = 0;

        if (realpath2 (option_arg, path))
          {
            char dir_name[FILENAME_MAX];
            struct stat fstate;

            /*
              MinGW's stat() does not accept a trailing slash or backslash, but
              dirname2() only recognizes a path as directory if it ends with a
              slash or backslash.
            */
            strcat (path, DIR_SEPARATOR_S);
            dirname2 (path, dir_name);
            if (!stat (dir_name, &fstate) && S_ISDIR (fstate.st_mode))
              {
                // dirname2() strips trailing slashes and/or backslashes (if not
                //  the root directory of a drive).
                snprintf (ucon64.output_path, FILENAME_MAX, "%s%s", dir_name,
                          dir_name[strlen (dir_name) - 1] != DIR_SEPARATOR ?
                            DIR_SEPARATOR_S : "");
                ucon64.output_path[FILENAME_MAX - 1] = '\0';
                dir = 1;
              }
          }

        if (!dir)
          puts ("WARNING: Argument for " OPTION_S "o must be a directory. Using current directory instead");
      }
      break;

    case UCON64_NHI:
      ucon64.snes_hirom = 0;
      break;

    case UCON64_HI:
      ucon64.snes_hirom = SNES_HIROM;
      break;

    case UCON64_EROM:
      ucon64.snes_header_base = SNES_EROM;
      break;

    case UCON64_BS:
      ucon64.bs_dump = 1;
      break;

    case UCON64_NBS:
      ucon64.bs_dump = 0;
      break;

    case UCON64_CTRL:
      if (UCON64_ISSET (ucon64.controller))
        ucon64.controller |= 1 << strtol (option_arg, NULL, 10);
      else
        ucon64.controller = 1 << strtol (option_arg, NULL, 10);
      break;

    case UCON64_CTRL2:
      if (UCON64_ISSET (ucon64.controller2))
        ucon64.controller2 |= 1 << strtol (option_arg, NULL, 10);
      else
        ucon64.controller2 = 1 << strtol (option_arg, NULL, 10);
      break;

    case UCON64_NTSC:
      if (!UCON64_ISSET (ucon64.tv_standard))
        ucon64.tv_standard = 0;
      else if (ucon64.tv_standard == 1)
        ucon64.tv_standard = 2;                 // code for NTSC/PAL (NES UNIF/iNES)
      break;

    case UCON64_PAL:
      if (!UCON64_ISSET (ucon64.tv_standard))
        ucon64.tv_standard = 1;
      else if (ucon64.tv_standard == 0)
        ucon64.tv_standard = 2;                 // code for NTSC/PAL (NES UNIF/iNES)
      break;

    case UCON64_BAT:
      ucon64.battery = 1;
      break;

    case UCON64_NBAT:
      ucon64.battery = 0;
      break;

    case UCON64_VRAM:
      ucon64.vram = 1;
      break;

    case UCON64_NVRAM:
      ucon64.vram = 0;
      break;

    case UCON64_MIRR:
      ucon64.mirror = strtol (option_arg, NULL, 10);
      break;

    case UCON64_MAPR:
      ucon64.mapr = option_arg;                 // pass the _string_, it can be a
      break;                                    //  board name

    case UCON64_CMNT:
      ucon64.comment = option_arg;
      break;

    case UCON64_DUMPINFO:
      ucon64.use_dump_info = 1;
      ucon64.dump_info = option_arg;
      break;

    case UCON64_RANGE:
      {
        char buf[80], *token;
        size_t len = strnlen (option_arg, sizeof buf - 1);
        unsigned int offset1, offset2;

        strncpy (buf, option_arg, len)[len] = '\0';

        token = strtok (buf, ":");
        if (!token)
          {
            fputs ("ERROR: No O1 (offset 1) specified. Specify one before a colon\n", stderr);
            break;
          }
        if (strtoint (token, 16, &offset1, sizeof offset1))
          {
            fprintf (stderr, "ERROR: Invalid O1 (offset 1): %s\n", token);
            break;
          }

        token = strtok (NULL, "");
        if (!token)
          {
            fputs ("ERROR: No O2 (offset 2) specified. Specify one after a colon\n", stderr);
            break;
          }
        if (strtoint (token, 16, &offset2, sizeof offset2))
          {
            fprintf (stderr, "ERROR: Invalid O2 (offset 2): %s\n", token);
            break;
          }

        if (offset1 >= offset2)
          {
            fputs ("ERROR: When specifying a range O2 (offset 2) must be larger than O1 (offset 1)\n",
                   stderr);
            break;
          }

        ucon64.range_start = offset1;
        ucon64.range_length = offset2 - offset1 + 1;
      }
      break;

    case UCON64_Q:
    case UCON64_QQ:                             // for now -qq is equivalent to -q
      ucon64.quiet = 1;
      cm_verbose = 0;
      break;

    case UCON64_V:
      ucon64.quiet = -1;
      cm_verbose = 1;
      break;

    case UCON64_SSIZE:
      ucon64.part_size = strtol (option_arg, NULL, 10) * MBIT;
      break;

    case UCON64_ID:
      ucon64.id = -2;                           // just a value other than
      break;                                    //  UCON64_UNKNOWN and smaller than 0

    case UCON64_IDNUM:
      ucon64.id = strtol (option_arg, NULL, 10);
      if (ucon64.id < 0)
        ucon64.id = 0;
      else if (ucon64.id > 999)
        {
          fputs ("ERROR: NUM must be smaller than or equal to 999\n", stderr);
          exit (1);
        }
      break;

    case UCON64_REGION:
      if (option_arg[1] == '\0' && toupper ((int) option_arg[0]) == 'X') // be insensitive to case
        ucon64.region = 256;
      else
        ucon64.region = strtol (option_arg, NULL, 10);
      break;

    default:
      break;
    }

  return 0;
}


static inline char *
tofunc (char *s, size_t len, int (*func) (int))
{
  char *p = s;

  for (; len > 0; p++, len--)
    *p = (char) func (*p);

  return s;
}


static inline int
ls_toprint (int c)
{
  return isprint (c) ? c : '.';
}


static int
hexstring_to_ints (char *src, int wildcard)
{
  int retval = 0, i = 0;
  char *dest = src;
  uint8_t value = 0;

  for (; *src; ++src)
    {
      uint8_t digit;

      if (*src == ' ' || *src == '\t')
        continue;
      else if (*src >= '0' && *src <= '9')
        digit = *src - '0';
      else if (*src >= 'A' && *src <= 'F')
        digit = *src - 'A' + 10;
      else if (*src >= 'a' && *src <= 'f')
        digit = *src - 'a' + 10;
      else if (wildcard && *src == '?')
        {
          *dest++ = *src;
          i += i & 1 ? 1 : 2;
          continue;
        }
      else
        {
          retval = -1;
          break;
        }
      value = value << 4 | digit;
      if (i & 1)
        *dest++ = value;
      i++;
    }
  *dest = '\0';

  if (!retval)
    retval = i / 2;

  return retval;
}


static int64_t
strtoll2 (const char *str)
{
  char *endptr;
  int64_t value = 0;

  errno = 0;
  value = strtoll (str, &endptr, 10);
  if (errno || *endptr)
    value = strtoll (str, NULL, 16);

  return value;
}


static void
set_dump_args (const char *str, uint64_t *start, uint64_t *len)
{
  char buf[80], *token;
  size_t l;
  uint64_t end;

  if (!str)
    {
      *start = 0;
      *len = ucon64.fsize;
      return;
    }

  l = strnlen (str, sizeof buf - 1);
  strncpy (buf, str, l)[l] = '\0';

  token = strtok (buf, ":");
  if (!token)
    {
      *start = 0;
      *len = ucon64.fsize;
      return;
    }
  *start = strtoll2 (token);
  if (*start >= ucon64.fsize)
    {
      fprintf (stderr,
               "ERROR: O1 (start) must be smaller than the file size (0x%llx)\n",
               (long long unsigned int) ucon64.fsize);
      exit (1);
    }

  token = strtok (NULL, "");
  if (!token)
    {
      *len = ucon64.fsize - *start;
      return;
    }
  end = strtoll2 (token);
  if (end >= ucon64.fsize)
    {
      fprintf (stderr,
               "ERROR: O2 (end) must be smaller than the file size (0x%llx)\n",
               (long long unsigned int) ucon64.fsize);
      exit (1);
    }
  if (*start > end)
    {
      fputs ("ERROR: O2 (end) must be larger than or equal to O1 (start)\n", stderr);
      exit (1);
    }
  *len = end - *start + 1;
}


int
ucon64_options (st_ucon64_t *p)
{
#ifdef  USE_PARALLEL
  unsigned short enableRTS = (unsigned short) -1; // for UCON64_XSWC & UCON64_XSWC2
#endif
  int value = 0, x = 0;
  unsigned int checksum;
  char buf[MAXBUFSIZE], src_name[FILENAME_MAX], dest_name[FILENAME_MAX],
       *ptr = NULL, *values[UCON64_MAX_ARGS];
  const char *option_arg = p->optarg;

  strcpy (src_name, ucon64.fname);
  strcpy (dest_name, ucon64.fname);

  switch (p->option)
    {
    case UCON64_CRCHD:                          // deprecated
      value = UNKNOWN_BACKUP_HEADER_LEN;
    case UCON64_CRC:
      if (!value)
        value = ucon64.nfo ? ucon64.nfo->backup_header_len :
                  UCON64_ISSET2 (ucon64.backup_header_len, unsigned int) ?
                    ucon64.backup_header_len : 0;
      fputs (ucon64.fname, stdout);
      if (ucon64.fname_arch[0])
        printf (" (%s)\n", ucon64.fname_arch);
      else
        fputc ('\n', stdout);
      checksum = 0;
      ucon64_chksum (NULL, NULL, &checksum, ucon64.fname, ucon64.fsize, value);
      printf ("Checksum (CRC32): 0x%08x\n", checksum);
      break;

    case UCON64_SHA1:
      value = ucon64.nfo ? ucon64.nfo->backup_header_len :
                UCON64_ISSET2 (ucon64.backup_header_len, unsigned int) ?
                  ucon64.backup_header_len : 0;
      fputs (ucon64.fname, stdout);
      if (ucon64.fname_arch[0])
        printf (" (%s)\n", ucon64.fname_arch);
      else
        fputc ('\n', stdout);
      ucon64_chksum (buf, NULL, NULL, ucon64.fname, ucon64.fsize, value);
      printf ("Checksum (SHA1): 0x%s\n", buf);
      break;

    case UCON64_MD5:
      value = ucon64.nfo ? ucon64.nfo->backup_header_len :
                UCON64_ISSET2 (ucon64.backup_header_len, unsigned int) ?
                  ucon64.backup_header_len : 0;
      fputs (ucon64.fname, stdout);
      if (ucon64.fname_arch[0])
        printf (" (%s)\n", ucon64.fname_arch);
      else
        fputc ('\n', stdout);
      ucon64_chksum (NULL, buf, NULL, ucon64.fname, ucon64.fsize, value);
      printf ("Checksum (MD5): 0x%s\n", buf);
      break;

    case UCON64_HEX:
      {
        uint64_t start, len;
        set_dump_args (option_arg, &start, &len);
        ucon64_dump (stdout, ucon64.fname, start, len, DUMPER_HEX);
      }
      break;

    case UCON64_BIT:
      {
        uint64_t start, len;
        set_dump_args (option_arg, &start, &len);
        ucon64_dump (stdout, ucon64.fname, start, len, DUMPER_BIT);
      }
      break;

    case UCON64_CODE:
      {
        uint64_t start, len;
        set_dump_args (option_arg, &start, &len);
        ucon64_dump (stdout, ucon64.fname, start, len, DUMPER_CODE);
      }
      break;

    case UCON64_PRINT:
      {
        uint64_t start, len;
        set_dump_args (option_arg, &start, &len);
        ucon64_dump (stdout, ucon64.fname, start, len, DUMPER_PRINT);
      }
      break;

    case UCON64_C:
      ucon64_filefile (option_arg, 0, 0, FALSE);
      break;

    case UCON64_CS:
      ucon64_filefile (option_arg, 0, 0, TRUE);
      break;

    case UCON64_FIND:
      ucon64_find (ucon64.fname, 0, ucon64.fsize, option_arg,
                   strlen (option_arg), MEMCMP2_WCARD ('?'));
      break;

    case UCON64_FINDR:
      ucon64_find (ucon64.fname, 0, ucon64.fsize, option_arg,
                   strlen (option_arg), MEMCMP2_REL);
      break;

    case UCON64_FINDI:
      ucon64_find (ucon64.fname, 0, ucon64.fsize, option_arg,
                   strlen (option_arg), MEMCMP2_WCARD ('?') | MEMCMP2_CASE);
      break;

    case UCON64_HFIND:
      strcpy (buf, option_arg);
      value = hexstring_to_ints (buf, 1);
      if (value < 0)
        {
          fprintf (stderr, "ERROR: Invalid search string: %s\n", option_arg);
          break;
        }
      ucon64_find (ucon64.fname, 0, ucon64.fsize, buf, value, MEMCMP2_WCARD ('?'));
      break;

    case UCON64_HFINDR:
      strcpy (buf, option_arg);
      value = hexstring_to_ints (buf, 0);
      if (value < 0)
        {
          fprintf (stderr, "ERROR: Invalid search string: %s\n", option_arg);
          break;
        }
      ucon64_find (ucon64.fname, 0, ucon64.fsize, buf, value, MEMCMP2_REL);
      break;

    case UCON64_DFIND:
      strcpy (buf, option_arg);
      value = strarg (values, buf, " ", UCON64_MAX_ARGS);
      for (x = 0; x < value; x++)
        buf[x] = values[x][0] == '?' && values[x][1] == '\0' ?
                   '?' : (char) strtol (values[x], NULL, 10);
      buf[x] = '\0';
      ucon64_find (ucon64.fname, 0, ucon64.fsize, buf, value, MEMCMP2_WCARD ('?'));
      break;

    case UCON64_DFINDR:
      strcpy (buf, option_arg);
      value = strarg (values, buf, " ", UCON64_MAX_ARGS);
      for (x = 0; x < value; x++)
        buf[x] = (char) strtol (values[x], NULL, 10);
      buf[x] = '\0';
      ucon64_find (ucon64.fname, 0, ucon64.fsize, buf, value, MEMCMP2_REL);
      break;

    case UCON64_HREPLACE:
      {
        size_t len = strnlen (option_arg, sizeof buf - 1);
        int searchlen, replacelen;
        char *token, *search, *replace;

        strncpy (buf, option_arg, len)[len] = '\0';

        token = strtok (buf, ":");
        if (!token)
          {
            fputs ("ERROR: No search string specified. Specify one before a colon\n",
                   stderr);
            break;
          }
        if ((search = strdup (token)) == NULL)
          {
            fprintf (stderr, ucon64_msg[BUFFER_ERROR], strlen (token) + 1);
            break;
          }
        searchlen = hexstring_to_ints (search, 1);
        if (searchlen < 0)
          {
            fprintf (stderr, "ERROR: S (search string) is invalid: %s\n", token);
            free (search);
            break;
          }

        token = strtok (NULL, "");
        if (!token)
          {
            fputs ("ERROR: No replacement string specified. Specify one after a colon\n",
                   stderr);
            free (search);
            break;
          }
        if ((replace = strdup (token)) == NULL)
          {
            fprintf (stderr, ucon64_msg[BUFFER_ERROR], strlen (token) + 1);
            free (search);
            break;
          }
        replacelen = hexstring_to_ints (replace, 0);
        if (replacelen < 0)
          {
            fprintf (stderr, "ERROR: R (replacement string) is invalid: %s\n", token);
            free (search);
            free (replace);
            break;
          }

        ucon64_replace (ucon64.fname, 0, ucon64.fsize, search, searchlen,
                        replace, replacelen, MEMCMP2_WCARD ('?'));

        free (search);
        free (replace);
      }
      break;

    case UCON64_PADHD:                          // deprecated
      value = UNKNOWN_BACKUP_HEADER_LEN;
    case UCON64_P:
    case UCON64_PAD:
      if (!value && ucon64.nfo)
        value = ucon64.nfo->backup_header_len;
      ucon64_file_handler (dest_name, src_name, 0);

      fcopy (src_name, 0, ucon64.fsize, dest_name, "wb");
      if (truncate2 (dest_name, ucon64.fsize +
                       (MBIT - ((ucon64.fsize - value) % MBIT))) == -1)
        {
          fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], dest_name); // msg is not a typo
          exit (1);
        }

      printf (ucon64_msg[WROTE], dest_name);
      remove_temp_file ();
      break;

    case UCON64_PADN:
      ucon64_file_handler (dest_name, src_name, 0);

      fcopy (src_name, 0, ucon64.fsize, dest_name, "wb");
      if (truncate2 (dest_name, strtoll (option_arg, NULL, 10) +
                       (ucon64.nfo ? ucon64.nfo->backup_header_len : 0)) == -1)
        {
          fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], dest_name); // msg is not a typo
          exit (1);
        }

      printf (ucon64_msg[WROTE], dest_name);
      remove_temp_file ();
      break;

    case UCON64_ISPAD:
      {
        int64_t padded = ucon64_testpad (ucon64.fname);

        if (padded != -1)
          {
            if (!padded)
              puts ("Padded: No");
            else
              printf ("Padded: Maybe, %lld Byte%s (%.4f Mb)\n",
                      (long long int) padded, padded != 1 ? "s" : "",
                      TOMBIT_F (padded));
          }
      }
      break;

    case UCON64_STRIP:
      ucon64_file_handler (dest_name, src_name, 0);
      fcopy (src_name, 0, ucon64.fsize - strtoll (option_arg, NULL, 10),
             dest_name, "wb");
      printf (ucon64_msg[WROTE], dest_name);
      remove_temp_file ();
      break;

    case UCON64_STP:
      ucon64_file_handler (dest_name, src_name, 0);
      fcopy (src_name, 512, ucon64.fsize, dest_name, "wb");
      printf (ucon64_msg[WROTE], dest_name);
      remove_temp_file ();
      break;

    case UCON64_STPN:
      ucon64_file_handler (dest_name, src_name, 0);
      fcopy (src_name, strtoll (option_arg, NULL, 10), ucon64.fsize,
             dest_name, "wb");
      printf (ucon64_msg[WROTE], dest_name);
      remove_temp_file ();
      break;

    case UCON64_INS:
      ucon64_file_handler (dest_name, src_name, 0);
      memset (buf, 0, 512);
      ucon64_fwrite (buf, 0, 512, dest_name, "wb");
      fcopy (src_name, 0, ucon64.fsize, dest_name, "ab");
      printf (ucon64_msg[WROTE], dest_name);
      remove_temp_file ();
      break;

    case UCON64_INSN:
      ucon64_file_handler (dest_name, src_name, 0);
      {
        FILE *file;
        uint64_t bytesleft = strtoll (option_arg, NULL, 10);

        if ((file = fopen (dest_name, "wb")) == NULL)
          {
            fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], dest_name);
            break;
          }
        memset (buf, 0, (size_t) MIN (bytesleft, MAXBUFSIZE));
        while (bytesleft > 0)
          bytesleft -= fwrite (buf, 1, (size_t) MIN (bytesleft, MAXBUFSIZE), file);
        fclose (file);
      }
      fcopy (src_name, 0, ucon64.fsize, dest_name, "ab");
      printf (ucon64_msg[WROTE], dest_name);
      remove_temp_file ();
      break;

    case UCON64_SPLIT:
      ucon64_split (strtoll (option_arg, NULL, 10));
      break;

    case UCON64_A:
      aps_apply (ucon64.fname, ucon64.file);
      break;

    case UCON64_B:
      bsl_apply (ucon64.fname, ucon64.file);
      break;

    case UCON64_I:
      ips_apply (ucon64.fname, ucon64.file);
      break;

    case UCON64_PPF:
      ppf_apply (ucon64.fname, ucon64.file);
      break;

    case UCON64_MKA:
      aps_create (option_arg, ucon64.fname);    // original, modified
      break;

    case UCON64_MKI:
      ips_create (option_arg, ucon64.fname);    // original, modified
      break;

    case UCON64_MKPPF:
      ppf_create (option_arg, ucon64.fname);    // original, modified
      break;

    case UCON64_NA:
      aps_set_desc (ucon64.fname, option_arg);
      break;

    case UCON64_NPPF:
      ppf_set_desc (ucon64.fname, option_arg);
      break;

    case UCON64_IDPPF:
      ppf_set_fid (ucon64.fname, option_arg);
      break;

    case UCON64_SCAN:
    case UCON64_LSD:
      if (ucon64.dat_enabled)
        {
          if (ucon64.crc32)
            {
              fputs (ucon64.fname, stdout);
              if (ucon64.fname_arch[0])
                printf (" (%s)\n", ucon64.fname_arch);
              else
                fputc ('\n', stdout);
              // use ucon64.fcrc32 for SNES & Genesis interleaved/N64 non-interleaved
              printf ("Checksum (CRC32): 0x%08x\n", ucon64.fcrc32 ?
                        ucon64.fcrc32 : ucon64.crc32);
              ucon64_dat_nfo ((st_ucon64_dat_t *) ucon64.dat, 1);
            }
        }
      else
        fputs (ucon64_msg[DAT_NOT_ENABLED], stdout);
      break;

    case UCON64_LSV:
      if (ucon64.nfo)
        ucon64_nfo ();
      break;

    case UCON64_LS:
      ucon64.newline_before_rom = 0;

      if (ucon64.nfo)
        ptr = ucon64.nfo->name;

      if (ucon64.dat)
        {
          if (!ptr)
            ptr = ((st_ucon64_dat_t *) ucon64.dat)->name;
          else if (!ptr[0])
            ptr = ((st_ucon64_dat_t *) ucon64.dat)->name;
        }

      if (ptr && ptr[0])
        {
          struct stat fstate;

          if (stat (ucon64.fname, &fstate))
            break;
          strftime (buf, 13, "%b %d %Y", localtime (&fstate.st_mtime));
          printf ("%-31.31s %10u %s %s", tofunc (ptr, strlen (ptr), ls_toprint),
                  (unsigned int) ucon64.fsize, buf, basename2 (ucon64.fname));
          if (ucon64.fname_arch[0])
            printf (" (%s)\n", ucon64.fname_arch);
          else
            fputc ('\n', stdout);
        }
      break;

    case UCON64_RDAT:
      ucon64.newline_before_rom = 0;
      ucon64_rename (UCON64_RDAT);
      break;

    case UCON64_RROM:
      ucon64.newline_before_rom = 0;
      ucon64_rename (UCON64_RROM);
      break;

    case UCON64_R83:
      ucon64.newline_before_rom = 0;
      ucon64_rename (UCON64_R83);
      break;

    case UCON64_RJOLIET:
      ucon64.newline_before_rom = 0;
      ucon64_rename (UCON64_RJOLIET);
      break;

    case UCON64_RL:
      ucon64.newline_before_rom = 0;
      ucon64_rename (UCON64_RL);
      break;

    case UCON64_RU:
      ucon64.newline_before_rom = 0;
      ucon64_rename (UCON64_RU);
      break;

#ifdef  USE_DISCMAGE
    case UCON64_BIN2ISO:
    case UCON64_ISOFIX:
    case UCON64_RIP:
    case UCON64_CDMAGE:
      if (ucon64.discmage_enabled)
        {
          uint32_t flags = 0;

          switch (p->option)
            {
            case UCON64_BIN2ISO:
              flags |= DM_2048; // DM_2048 read sectors and convert to 2048 Bytes
              break;

            case UCON64_ISOFIX:
              flags |= DM_FIX; // DM_FIX read sectors and fix (if needed/possible)
              break;

            case UCON64_CDMAGE:
//              flags |= DM_CDMAGE;
              break;
            }

          ucon64.image = dm_reopen (ucon64.fname, 0, (dm_image_t *) ucon64.image);
          if (ucon64.image)
            {
              int track = strtol (option_arg, NULL, 10);
              if (track < 1)
                track = 1;
              track--; // decrement for dm_rip()

              printf ("Writing track: %d\n\n", track + 1);

              dm_set_gauge (&discmage_gauge);
              dm_rip ((dm_image_t *) ucon64.image, track, flags);
              fputc ('\n', stdout);
            }
        }
      else
        printf (ucon64_msg[NO_LIB], ucon64.discmage_path);
      break;

    case UCON64_MKTOC:
    case UCON64_MKCUE:
    case UCON64_MKSHEET:
      if (ucon64.discmage_enabled && ucon64.image)
        {
          char fname[FILENAME_MAX];
          strcpy (fname, ((dm_image_t *) ucon64.image)->fname);

          if (p->option == UCON64_MKTOC || p->option == UCON64_MKSHEET)
            {
              set_suffix (fname, ".toc");
              ucon64_file_handler (fname, NULL, 0);

              if (!dm_toc_write ((dm_image_t *) ucon64.image))
                printf (ucon64_msg[WROTE], basename2 (fname));
              else
                fputs ("ERROR: Could not generate toc sheet\n", stderr);
            }

          if (p->option == UCON64_MKCUE || p->option == UCON64_MKSHEET)
            {
              set_suffix (fname, ".cue");
              ucon64_file_handler (fname, NULL, 0);

              if (!dm_cue_write ((dm_image_t *) ucon64.image))
                printf (ucon64_msg[WROTE], basename2 (fname));
              else
                fputs ("ERROR: Could not generate cue sheet\n", stderr);
            }
        }
      else
        printf (ucon64_msg[NO_LIB], ucon64.discmage_path);
      break;

    case UCON64_XCDRW:
      if (ucon64.discmage_enabled)
        {
//          dm_set_gauge (&discmage_gauge);
          if (!access (ucon64.fname, F_OK))
            dm_disc_write ((dm_image_t *) ucon64.image);
          else
            dm_disc_read ((dm_image_t *) ucon64.image);
        }
      else
        printf (ucon64_msg[NO_LIB], ucon64.discmage_path);
      break;
#endif // USE_DISCMAGE

    case UCON64_DB:
      if (ucon64.quiet > -1)
        {
          if (ucon64.dat_enabled)
            {
              ucon64_dat_view (ucon64.console, 0);
              printf ("TIP: %s " OPTION_LONG_S "db " OPTION_LONG_S "nes"
                      " would show only information about known NES ROMs\n",
                      basename2 (ucon64.argv[0]));
            }
          else
            fputs (ucon64_msg[DAT_NOT_ENABLED], stdout);
          break;
        }

    case UCON64_DBV:
      if (ucon64.dat_enabled)
        {
          ucon64_dat_view (ucon64.console, 1);
          printf ("TIP: %s " OPTION_LONG_S "dbv " OPTION_LONG_S "nes"
                  " would show only information about known NES ROMs\n",
                  basename2 (ucon64.argv[0]));
        }
      else
        fputs (ucon64_msg[DAT_NOT_ENABLED], stdout);
      break;

    case UCON64_DBS:
      if (ucon64.dat_enabled)
        {
          ucon64.crc32 = 0;
          sscanf (option_arg, "%x", &ucon64.crc32);

          if ((ucon64.dat = ucon64_dat_search (ucon64.crc32)) == NULL)
            {
              printf (ucon64_msg[DAT_NOT_FOUND], ucon64.crc32);
              printf ("TIP: Be sure to install the right DAT files in %s\n",
                      ucon64.datdir);
            }
          else
            {
              ucon64_dat_nfo ((st_ucon64_dat_t *) ucon64.dat, 1);
              printf ("\n"
                      "TIP: %s " OPTION_LONG_S "dbs" OPTARG_S "0x%08x " OPTION_LONG_S
                      "nes would search only for a NES ROM\n",
                      basename2 (ucon64.argv[0]), ucon64.crc32);
            }
        }
      else
        fputs (ucon64_msg[DAT_NOT_ENABLED], stdout);
      break;

    case UCON64_MKDAT:
      ucon64.newline_before_rom = 0;
      ucon64_create_dat (option_arg, ucon64.fname,
                         ucon64.nfo ? ucon64.nfo->backup_header_len : 0);
      break;

    case UCON64_MULTI:
      switch (ucon64.console)
        {
        case UCON64_GBA:
          gba_multi (strtol (option_arg, NULL, 10) * MBIT, NULL);
          break;
        case UCON64_GEN:
          genesis_multi (strtol (option_arg, NULL, 10) * MBIT);
          break;
        case UCON64_PCE:
          pce_multi (strtol (option_arg, NULL, 10) * MBIT);
          break;
        case UCON64_SMS:                        // Sega Master System *and* Game Gear
          sms_multi (strtol (option_arg, NULL, 10) * MBIT);
          break;
        case UCON64_SNES:
          snes_multi (strtol (option_arg, NULL, 10) * MBIT);
          break;
        default:
          return -1;
        }
      break;

    case UCON64_E:
      ucon64_e ();
      break;

    case UCON64_1991:
      genesis_1991 (ucon64.nfo);
      break;

    case UCON64_B0:
      lynx_b0 (ucon64.nfo, option_arg);
      break;

    case UCON64_B1:
      lynx_b1 (ucon64.nfo, option_arg);
      break;

    case UCON64_BIN:
      genesis_bin (ucon64.nfo);
      break;

    case UCON64_BIOS:
      neogeo_bios (option_arg);
      break;

    case UCON64_BOT:
      n64_bot (ucon64.nfo, option_arg);
      break;

    case UCON64_CHK:
      switch (ucon64.console)
        {
        case UCON64_GB:
          gb_chk (ucon64.nfo);
          break;
        case UCON64_GBA:
          gba_chk (ucon64.nfo);
          break;
        case UCON64_GEN:
          genesis_chk (ucon64.nfo);
          break;
        case UCON64_N64:
          n64_chk (ucon64.nfo);
          break;
        case UCON64_SMS:
          sms_chk (ucon64.nfo);
          break;
        case UCON64_SNES:
          snes_chk (ucon64.nfo);
          break;
        case UCON64_SWAN:
          swan_chk (ucon64.nfo);
          break;
        case UCON64_NDS:
          nds_chk (ucon64.nfo);
          break;
        default:
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_COL:
      snes_col (option_arg);
      break;

    case UCON64_CRP:
      gba_crp (ucon64.nfo, option_arg);
      break;

    case UCON64_DBUH:
      snes_backup_header_info (ucon64.nfo);
      break;

    case UCON64_SWAP:
    case UCON64_DINT:
      switch (ucon64.console)
        {
        case UCON64_NES:
          nes_dint ();
          break;
        case UCON64_PCE:
          pce_swap (ucon64.nfo);
          break;
        case UCON64_SNES:
          snes_dint (ucon64.nfo);
          break;
        default:                                // Nintendo 64
          puts ("Converting file...");
          ucon64_file_handler (dest_name, NULL, 0);
          fcopy (src_name, 0, ucon64.fsize, dest_name, "wb");
          ucon64_fbswap16 (dest_name, 0, ucon64.fsize);
          printf (ucon64_msg[WROTE], dest_name);
          break;
        }
      break;

    case UCON64_SWAP2:
      // --swap2 is currently used only for Nintendo 64
      puts ("Converting file...");
      ucon64_file_handler (dest_name, NULL, 0);
      fcopy (src_name, 0, ucon64.fsize, dest_name, "wb");
      ucon64_fwswap32 (dest_name, 0, ucon64.fsize);
      printf (ucon64_msg[WROTE], dest_name);
      break;

    case UCON64_DMIRR:
      snes_demirror (ucon64.nfo);
      break;

    case UCON64_DNSRT:
      snes_densrt (ucon64.nfo);
      break;

    case UCON64_F:
      switch (ucon64.console)
        {
        case UCON64_GEN:
          genesis_f (ucon64.nfo);
          break;
        case UCON64_N64:
          n64_f (ucon64.nfo);
          break;
        case UCON64_PCE:
          pce_f (ucon64.nfo);
          break;
        case UCON64_SNES:
          snes_f (ucon64.nfo);
          break;
        default:
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_FDS:
      nes_fds ();
      break;

    case UCON64_FDSL:
      nes_fdsl (ucon64.nfo, NULL);
      break;

    case UCON64_FFE:
      nes_ffe (ucon64.nfo);
      break;

    case UCON64_FIG:
      snes_fig (ucon64.nfo);
      break;

    case UCON64_FIGS:
      snes_figs (ucon64.nfo);
      break;

    case UCON64_GBX:
      gb_gbx (ucon64.nfo);
      break;

    case UCON64_GD3:
      snes_gd3 (ucon64.nfo);
      break;

    case UCON64_GD3S:
      snes_gd3s (ucon64.nfo);
      break;

    case UCON64_GG:
      switch (ucon64.console)
        {
        case UCON64_GB:
        case UCON64_GEN:
        case UCON64_NES:
        case UCON64_SMS:
        case UCON64_SNES:
          // ROM images for SNES (GD3) and Genesis (SMD) can be interleaved
          if (ucon64.nfo->interleaved)
            fputs ("ERROR: This ROM seems to be interleaved, but uCON64 will only apply a Game\n"
                   "       Genie patch to non-interleaved ROMs. Convert to a non-interleaved\n"
                   "       format\n", stderr);
          else if (ucon64.console == UCON64_NES &&
                   nes_get_file_type () != INES && nes_get_file_type () != FFE)
            fputs ("ERROR: This NES ROM is in a format that uCON64 cannot apply a Game Genie patch\n"
                   "       to. Convert to iNES\n", stderr);
          else
            gg_apply (ucon64.nfo, option_arg);
          break;
        default:
          fputs ("ERROR: Cannot apply Game Genie code for this ROM/console\n", stderr);
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_GGD:
      gg_display (ucon64.nfo, option_arg);
      break;

    case UCON64_GGE:
      gg_display (ucon64.nfo, option_arg);
      break;

    case UCON64_GP2BMP:
      gb_gp2bmp ();
      break;

    case UCON64_IC2:
      snes_ic2 (ucon64.nfo);
      break;

    case UCON64_INES:
      nes_ines ();
      break;

    case UCON64_INESHD:
      nes_ineshd (ucon64.nfo);
      break;

    case UCON64_PARSE:
      dc_parse (option_arg);
      break;

    case UCON64_MKIP:
      dc_mkip ();
      break;

    case UCON64_J:
      switch (ucon64.console)
        {
        case UCON64_GEN:
          genesis_j (ucon64.nfo);
          break;
        case UCON64_NES:
          nes_j (NULL);
          break;
        case UCON64_SNES:
          snes_j (ucon64.nfo);
          break;
        default:
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_K:
      snes_k (ucon64.nfo);
      break;

    case UCON64_L:
      snes_l (ucon64.nfo);
      break;

    case UCON64_LNX:
      lynx_lnx (ucon64.nfo);
      break;

    case UCON64_LOGO:
      switch (ucon64.console)
        {
        case UCON64_GB:
          gb_logo (ucon64.nfo);
          break;
        case UCON64_GBA:
          gba_logo (ucon64.nfo);
          break;
        case UCON64_NDS:
          nds_logo (ucon64.nfo);
          break;
        default:
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_LSRAM:
      n64_sram (ucon64.nfo, option_arg);
      break;

    case UCON64_LYX:
      lynx_lyx (ucon64.nfo);
      break;

    case UCON64_MGD:
      switch (ucon64.console)
        {
        case UCON64_GB:
          gb_mgd (ucon64.nfo);
          break;
        case UCON64_GEN:
          genesis_mgd (ucon64.nfo);
          break;
        case UCON64_NG:
          neogeo_mgd ();
          break;
        case UCON64_PCE:
          pce_mgd (ucon64.nfo);
          break;
        case UCON64_SMS:
          sms_mgd (ucon64.nfo, UCON64_SMS);
          break;
        case UCON64_SNES:
          snes_mgd (ucon64.nfo);
          break;
        default:
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_MGDGG:
      sms_mgd (ucon64.nfo, UCON64_GAMEGEAR);
      break;

    case UCON64_MGH:
      switch (ucon64.console)
        {
        case UCON64_GEN:
          genesis_mgh (ucon64.nfo);
          break;
        case UCON64_SNES:
          snes_mgh (ucon64.nfo);
          break;
        default:
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_MKSRM:
      snes_create_sram ();
      break;

    case UCON64_MSG:
      pce_msg (ucon64.nfo);
      break;

    case UCON64_N:
      switch (ucon64.console)
        {
        case UCON64_GB:
          gb_n (ucon64.nfo, option_arg);
          break;
        case UCON64_GBA:
          gba_n (ucon64.nfo, option_arg);
          break;
        case UCON64_GEN:
          genesis_n (ucon64.nfo, option_arg);
          break;
        case UCON64_LYNX:
          lynx_n (ucon64.nfo, option_arg);
          break;
        case UCON64_N64:
          n64_n (ucon64.nfo, option_arg);
          break;
        case UCON64_NES:
          nes_n (option_arg);
          break;
        case UCON64_SNES:
          snes_n (ucon64.nfo, option_arg);
          break;
        case UCON64_NDS:
          nds_n (ucon64.nfo, option_arg);
          break;
        default:
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_N2:
      genesis_n2 (ucon64.nfo, option_arg);
      break;

    case UCON64_N2GB:
      gb_n2gb (ucon64.nfo, option_arg);
      break;

    case UCON64_NROT:
      lynx_nrot (ucon64.nfo);
      break;

    case UCON64_PASOFAMI:
      nes_pasofami ();
      break;

    case UCON64_PATTERN:
      ucon64_pattern (option_arg);
      break;

    case UCON64_POKE:
      {
        size_t len = strnlen (option_arg, sizeof buf - 1);
        char *token;
        uint64_t offset;

        strncpy (buf, option_arg, len)[len] = '\0';

        token = strtok (buf, ":");
        if (!token)
          {
            fputs ("ERROR: No offset specified. Specify one before a colon\n", stderr);
            break;
          }
        if (strtoint (token, 16, &offset, sizeof offset))
          {
            fprintf (stderr, "ERROR: Invalid offset: %s\n", token);
            break;
          }

        token = strtok (NULL, "");
        if (!token)
          {
            fputs ("ERROR: No value specified. Specify one after a colon\n", stderr);
            break;
          }
        if (strtoint (token, 16, &value, sizeof value) ||
            (unsigned int) value > 255)
          {
            fprintf (stderr, "ERROR: Invalid value: %s\n", token);
            break;
          }

        if (offset >= ucon64.fsize)
          {
            fprintf (stderr, "ERROR: Offset 0x%016llx is too large for file size\n",
                     (long long unsigned int) offset);
            break;
          }

        ucon64_file_handler (dest_name, src_name, 0);
        fcopy (src_name, 0, ucon64.fsize, dest_name, "wb");

        fputc ('\n', stdout);
        buf[0] = (char) ucon64_fgetc (dest_name, offset);
        dumper (stdout, buf, 1, offset, DUMPER_HEX);

        ucon64_fputc (dest_name, offset, value, "r+b");

        buf[0] = (char) value;
        dumper (stdout, buf, 1, offset, DUMPER_HEX);
        fputc ('\n', stdout);

        printf (ucon64_msg[WROTE], dest_name);
        remove_temp_file ();
      }
      break;

    case UCON64_ROTL:
      lynx_rotl (ucon64.nfo);
      break;

    case UCON64_ROTR:
      lynx_rotr (ucon64.nfo);
      break;

    case UCON64_S:
      switch (ucon64.console)
        {
        case UCON64_GEN:
          genesis_s (ucon64.nfo);
          break;
        case UCON64_NES:
          nes_s ();
          break;
        case UCON64_NG:
          neogeo_s ();
          break;
        case UCON64_SNES:
          snes_s (ucon64.nfo);
          break;
        default:
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_SAM:
      neogeo_sam (option_arg);
      break;

    case UCON64_SCR:
      dc_scramble ();
      break;

    case UCON64_SGB:
      gb_sgb (ucon64.nfo);
      break;

    case UCON64_SMC:
      snes_smc (ucon64.nfo);
      break;

    case UCON64_SMD:
      switch (ucon64.console)
        {
        case UCON64_GEN:
          genesis_smd (ucon64.nfo);
          break;
        case UCON64_SMS:
          sms_smd (ucon64.nfo);
          break;
        default:
          return -1;
        }
      break;

    case UCON64_SMDS:
      switch (ucon64.console)
        {
        case UCON64_GEN:
          genesis_smds ();
          break;
        case UCON64_SMS:
          sms_smds ();
          break;
        default:
          return -1;
        }
      break;

    case UCON64_SMGH:
      switch (ucon64.console)
        {
        case UCON64_GEN:
          genesis_smgh (ucon64.nfo);
          break;
        case UCON64_SNES:
          snes_smgh (ucon64.nfo);
          break;
        default:
// the next msg has already been printed
//          fputs (ucon64_msg[CONSOLE_ERROR], stderr);
          return -1;
        }
      break;

    case UCON64_SMINI2SRM:
      snes_smini2srm ();
      break;

#ifdef  USE_ZLIB
    case UCON64_SMINIS:
      snes_sminis (ucon64.nfo, option_arg);
      break;
#endif

    case UCON64_SRAM:
      gba_sram ();
      break;

    case UCON64_SC:
      gba_sc ();
      break;

    case UCON64_SSC:
      gb_ssc (ucon64.nfo);
      break;

    case UCON64_SWC:
      snes_swc (ucon64.nfo);
      break;

    case UCON64_SWCS:
      snes_swcs (ucon64.nfo);
      break;

    case UCON64_UFO:
      snes_ufo (ucon64.nfo);
      break;

    case UCON64_UFOS:
      snes_ufos (ucon64.nfo);
      break;

    case UCON64_UFOSD:
      snes_ufosd (ucon64.nfo);
      break;

    case UCON64_UFOSDS:
      snes_ufosds (ucon64.nfo);
      break;

    case UCON64_UNIF:
      nes_unif ();
      break;

    case UCON64_UNSCR:
      dc_unscramble ();
      break;

    case UCON64_USMS:
      n64_usms (ucon64.nfo, option_arg);
      break;

    case UCON64_V64:
      n64_v64 (ucon64.nfo);
      break;

    /*
      It doesn't make sense to continue after executing a (send) backup option
      ("multizip"). Don't return, but use break instead. ucon64_execute_options()
      checks if an option was used that should stop uCON64.
    */
#ifdef  USE_LIBCD64
    case UCON64_XCD64:
      if (access (ucon64.fname, F_OK) != 0)
        cd64_read_rom (ucon64.fname, 64);
      else
        cd64_write_rom (ucon64.fname);
      fputc ('\n', stdout);
      break;

    case UCON64_XCD64C:
      if (!access (ucon64.fname, F_OK) && ucon64.backup)
        printf ("Wrote backup to %s\n", mkbak (ucon64.fname, BAK_MOVE));
      cd64_read_rom (ucon64.fname, strtol (option_arg, NULL, 10));
      fputc ('\n', stdout);
      break;

    case UCON64_XCD64B:
      cd64_write_bootemu (ucon64.fname);
      fputc ('\n', stdout);
      break;

    case UCON64_XCD64S:
      if (access (ucon64.fname, F_OK) != 0)
        cd64_read_sram (ucon64.fname);
      else
        cd64_write_sram (ucon64.fname);
      fputc ('\n', stdout);
      break;

    case UCON64_XCD64F:
      if (access (ucon64.fname, F_OK) != 0)
        cd64_read_flashram (ucon64.fname);
      else
        cd64_write_flashram (ucon64.fname);
      fputc ('\n', stdout);
      break;

    case UCON64_XCD64E:
      if (access (ucon64.fname, F_OK) != 0)
        cd64_read_eeprom (ucon64.fname);
      else
        cd64_write_eeprom (ucon64.fname);
      fputc ('\n', stdout);
      break;

    case UCON64_XCD64M:
      if (access (ucon64.fname, F_OK) != 0)
        cd64_read_mempack (ucon64.fname, strtol (option_arg, NULL, 10));
      else
        cd64_write_mempack (ucon64.fname, strtol (option_arg, NULL, 10));
      fputc ('\n', stdout);
      break;
#endif // USE_LIBCD64

#ifdef  USE_PARALLEL
    case UCON64_XRESET:
      parport_print_info ();
      fputs ("Resetting parallel port...", stdout);
      outportb (ucon64.parport + PARPORT_DATA, 0);
      // Strobe, Auto Linefeed and Select Printer are hardware inverted, so we
      //  have to write a 1 to bring the associated pins in a low state (0V).
      outportb (ucon64.parport + PARPORT_CONTROL,
                (inportb (ucon64.parport + PARPORT_CONTROL) & 0xf0) | 0x0b);
      puts ("done");
      break;

    case UCON64_XCMC:
      if (!access (ucon64.fname, F_OK) && ucon64.backup)
        printf ("Wrote backup to %s\n", mkbak (ucon64.fname, BAK_MOVE));
      cmc_read_rom (ucon64.fname, ucon64.parport, ucon64.io_mode); // ucon64.io_mode contains speed value
      fputc ('\n', stdout);
      break;

    case UCON64_XCMCT:
      cmc_test (strtol (option_arg, NULL, 10), ucon64.parport, ucon64.io_mode);
      fputc ('\n', stdout);
      break;

    case UCON64_XDJR:
      if (access (ucon64.fname, F_OK) != 0)
        {
          doctor64jr_read (ucon64.fname, ucon64.parport);
          fputc ('\n', stdout);
        }
      else
        {
          if (!ucon64.nfo->interleaved)
            fputs ("ERROR: This ROM does not seem to be interleaved, but the Doctor V64 Junior only\n"
                   "       supports interleaved ROMs. Convert to a Doctor V64 compatible format\n",
                   stderr);
          else
            {
              doctor64jr_write (ucon64.fname, ucon64.parport);
              fputc ('\n', stdout);
            }
        }
      break;

    case UCON64_XFAL:
      if (access (ucon64.fname, F_OK) != 0)
        fal_read_rom (ucon64.fname, ucon64.parport, 32);
      else
        fal_write_rom (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XFALMULTI:
      tmpnam2 (src_name, NULL);
      ucon64.temp_file = src_name;
      register_func (remove_temp_file);
      // gba_multi() calls ucon64_file_handler() so the directory part will be
      //  stripped from src_name. The directory should be used though.
      if (!ucon64.output_path[0])
        {
          dirname2 (src_name, ucon64.output_path);
          if (ucon64.output_path[strlen (ucon64.output_path) - 1] != DIR_SEPARATOR)
            strcat (ucon64.output_path, DIR_SEPARATOR_S);
        }
      if (gba_multi (strtol (option_arg, NULL, 10) * MBIT, src_name) == 0)
        { // don't try to start a transfer if there was a problem
          fputc ('\n', stdout);
          ucon64.fsize = fsizeof (src_name);
          fal_write_rom (src_name, ucon64.parport);
        }

      unregister_func (remove_temp_file);
      remove_temp_file ();
      fputc ('\n', stdout);
      break;

    case UCON64_XFALC:
      if (!access (ucon64.fname, F_OK) && ucon64.backup)
        printf ("Wrote backup to %s\n", mkbak (ucon64.fname, BAK_MOVE));
      fal_read_rom (ucon64.fname, ucon64.parport,
                    strtol (option_arg, NULL, 10));
      fputc ('\n', stdout);
      break;

    case UCON64_XFALS:
      if (access (ucon64.fname, F_OK) != 0)
        fal_read_sram (ucon64.fname, ucon64.parport, UCON64_UNKNOWN);
      else
        fal_write_sram (ucon64.fname, ucon64.parport, UCON64_UNKNOWN);
      fputc ('\n', stdout);
      break;

    case UCON64_XFALB:
      if (access (ucon64.fname, F_OK) != 0)
        fal_read_sram (ucon64.fname, ucon64.parport,
                       strtol (option_arg, NULL, 10));
      else
        fal_write_sram (ucon64.fname, ucon64.parport,
                        strtol (option_arg, NULL, 10));
      fputc ('\n', stdout);
      break;

    case UCON64_XFIG:
      if (access (ucon64.fname, F_OK) != 0)       // file does not exist => dump cartridge
        {
          fig_read_rom (ucon64.fname, ucon64.parport);
          fputc ('\n', stdout);
        }
      else
        {
          if (!ucon64.nfo->backup_header_len)
            fputs ("ERROR: This ROM has no header. Convert to a FIG compatible format\n",
                   stderr);
          else if (ucon64.nfo->interleaved)
            fputs ("ERROR: This ROM seems to be interleaved, but the FIG does not support\n"
                   "       interleaved ROMs. Convert to a FIG compatible format\n",
                   stderr);
          else // file exists => send it to the copier
            {
              fig_write_rom (ucon64.fname, ucon64.parport);
              fputc ('\n', stdout);
            }
        }
      break;

    case UCON64_XFIGS:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump SRAM contents
        fig_read_sram (ucon64.fname, ucon64.parport);
      else                                      // file exists => restore SRAM
        fig_write_sram (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XFIGC:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump cart SRAM contents
        fig_read_cart_sram (ucon64.fname, ucon64.parport);
      else                                      // file exists => restore SRAM
        fig_write_cart_sram (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XGBX:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump cartridge/flash card
        gbx_read_rom (ucon64.fname, ucon64.parport);
      else                                      // file exists => send it to the programmer
        gbx_write_rom (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XGBXS:
      if (access (ucon64.fname, F_OK) != 0)
        gbx_read_sram (ucon64.fname, ucon64.parport, -1);
      else
        gbx_write_sram (ucon64.fname, ucon64.parport, -1);
      fputc ('\n', stdout);
      break;

    case UCON64_XGBXB:
      if (access (ucon64.fname, F_OK) != 0)
        gbx_read_sram (ucon64.fname, ucon64.parport,
                       strtol (option_arg, NULL, 10));
      else
        gbx_write_sram (ucon64.fname, ucon64.parport,
                        strtol (option_arg, NULL, 10));
      fputc ('\n', stdout);
      break;

    case UCON64_XGD3:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump cartridge
        {
          gd3_read_rom (ucon64.fname, ucon64.parport); // dumping is not yet supported
          fputc ('\n', stdout);
        }
      else
        {
          if (!ucon64.nfo->backup_header_len)
            fputs ("ERROR: This ROM has no header. Convert to a Game Doctor compatible format\n",
                   stderr);
          else                                  // file exists => send it to the copier
            {
              gd3_write_rom (ucon64.fname, ucon64.parport, ucon64.nfo);
              fputc ('\n', stdout);
            }
        }
      break;

    case UCON64_XGD3S:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump SRAM contents
        gd3_read_sram (ucon64.fname, ucon64.parport); // dumping is not yet supported
      else                                      // file exists => restore SRAM
        gd3_write_sram (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XGD3R:
      if (access (ucon64.fname, F_OK) != 0)
        gd3_read_saver (ucon64.fname, ucon64.parport);
      else
        gd3_write_saver (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XGD6:
      if (access (ucon64.fname, F_OK) != 0)
        {
          gd6_read_rom (ucon64.fname, ucon64.parport);
          fputc ('\n', stdout);
        }
      else
        {
          if (!ucon64.nfo->backup_header_len)
            fputs ("ERROR: This ROM has no header. Convert to a Game Doctor compatible format\n",
                   stderr);
          else
            {
              gd6_write_rom (ucon64.fname, ucon64.parport, ucon64.nfo);
              fputc ('\n', stdout);
            }
        }
      break;

    case UCON64_XGD6S:
      if (access (ucon64.fname, F_OK) != 0)
        gd6_read_sram (ucon64.fname, ucon64.parport);
      else
        gd6_write_sram (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XGD6R:
      if (access (ucon64.fname, F_OK) != 0)
        gd6_read_saver (ucon64.fname, ucon64.parport);
      else
        gd6_write_saver (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XGG:
      if (access (ucon64.fname, F_OK) != 0)
        {
          smsgg_read_rom (ucon64.fname, ucon64.parport, 32 * MBIT);
          fputc ('\n', stdout);
        }
      else
        {
          if (ucon64.nfo->backup_header_len)
            fputs ("ERROR: This ROM has a header. Remove it with " OPTION_LONG_S "stp or " OPTION_LONG_S "mgd\n",
                   stderr);
          else if (ucon64.nfo->interleaved)
            fputs ("ERROR: This ROM seems to be interleaved, but uCON64 does not support\n"
                   "       sending interleaved ROMs to the SMS-PRO/GG-PRO. Convert ROM with " OPTION_LONG_S "mgd\n",
                   stderr);
          else
            {
              smsgg_write_rom (ucon64.fname, ucon64.parport);
              fputc ('\n', stdout);
            }
        }
      break;

    case UCON64_XGGS:
      if (access (ucon64.fname, F_OK) != 0)
        smsgg_read_sram (ucon64.fname, ucon64.parport, -1);
      else
        smsgg_write_sram (ucon64.fname, ucon64.parport, -1);
      fputc ('\n', stdout);
      break;

    case UCON64_XGGB:
      if (access (ucon64.fname, F_OK) != 0)
        smsgg_read_sram (ucon64.fname, ucon64.parport,
                         strtol (option_arg, NULL, 10));
      else
        smsgg_write_sram (ucon64.fname, ucon64.parport,
                          strtol (option_arg, NULL, 10));
      fputc ('\n', stdout);
      break;

    case UCON64_XIC2:
      smcic2_write_rom (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XLIT:
      if (!access (ucon64.fname, F_OK) && ucon64.backup)
        printf ("Wrote backup to %s\n", mkbak (ucon64.fname, BAK_MOVE));
      lynxit_read_rom (ucon64.fname, ucon64.parport);
      break;

    case UCON64_XMCCL:
      if (!access (ucon64.fname, F_OK) && ucon64.backup)
        printf ("Wrote backup to %s\n", mkbak (ucon64.fname, BAK_MOVE));
      mccl_read (ucon64.fname, ucon64.parport);
      break;

    case UCON64_XMCD:
      if (!access (ucon64.fname, F_OK) && ucon64.backup)
        printf ("Wrote backup to %s\n", mkbak (ucon64.fname, BAK_MOVE));
      mcd_read_rom (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XMD:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump flash card
        {
          md_read_rom (ucon64.fname, ucon64.parport, 64 * MBIT); // reads 32 Mbit if Sharp card
          fputc ('\n', stdout);
        }
      else                                      // file exists => send it to the MD-PRO
        {
          if (ucon64.nfo->backup_header_len)    // binary with header is possible
            fputs ("ERROR: This ROM has a header. Remove it with " OPTION_LONG_S "stp or " OPTION_LONG_S "bin\n",
                   stderr);
          else if (genesis_get_copier_type () != BIN)
            fputs ("ERROR: This ROM is not in binary/BIN/RAW format. uCON64 only supports sending\n"
                   "       binary files to the MD-PRO. Convert ROM with " OPTION_LONG_S "bin\n",
                   stderr);
          else
            {
              md_write_rom (ucon64.fname, ucon64.parport);
              fputc ('\n', stdout);
            }
        }
      break;

    case UCON64_XMDS:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump SRAM contents
        md_read_sram (ucon64.fname, ucon64.parport, -1);
      else                                      // file exists => restore SRAM
        md_write_sram (ucon64.fname, ucon64.parport, -1);
      fputc ('\n', stdout);
      break;

    case UCON64_XMDB:
      if (access (ucon64.fname, F_OK) != 0)
        md_read_sram (ucon64.fname, ucon64.parport,
                      strtol (option_arg, NULL, 10));
      else
        md_write_sram (ucon64.fname, ucon64.parport,
                       strtol (option_arg, NULL, 10));
      fputc ('\n', stdout);
      break;

    case UCON64_XMSG:
      if (access (ucon64.fname, F_OK) != 0)
        {
          msg_read_rom (ucon64.fname, ucon64.parport);
          fputc ('\n', stdout);
        }
      else
        {
          if (!ucon64.nfo->backup_header_len)
            fputs ("ERROR: This ROM has no header. Convert to an MSG compatible format\n",
                   stderr);
          else if (ucon64.nfo->interleaved)
            fputs ("ERROR: This ROM seems to be bit-swapped, but the MSG does not support\n"
                   "       bit-swapped ROMs. Convert to an MSG compatible format\n",
                   stderr);
          else
            {
              msg_write_rom (ucon64.fname, ucon64.parport);
              fputc ('\n', stdout);
            }
        }
      break;

    case UCON64_XPCE:
      if (access (ucon64.fname, F_OK) != 0)
        pce_read_rom (ucon64.fname, ucon64.parport, 32 * MBIT);
      else
        pce_write_rom (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XPL:
      if (access (ucon64.fname, F_OK) != 0)
        pl_read_rom (ucon64.fname, ucon64.parport);
      else
        pl_write_rom (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XPLI:
      pl_info (ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XSF:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump flash card
        sf_read_rom (ucon64.fname, ucon64.parport, 64 * MBIT);
      else                                      // file exists => send it to the Super Flash
        sf_write_rom (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XSFS:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump SRAM contents
        sf_read_sram (ucon64.fname, ucon64.parport);
      else                                      // file exists => restore SRAM
        sf_write_sram (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XSMC: // we don't use WF_NO_ROM => no need to check for file
      if (!ucon64.nfo->backup_header_len)
        fputs ("ERROR: This ROM has no header. Convert to an SMC compatible format with " OPTION_LONG_S "ffe\n",
               stderr);
      else
        {
          smc_write_rom (ucon64.fname, ucon64.parport);
          fputc ('\n', stdout);
        }
      break;

    case UCON64_XSMCR:
      if (access (ucon64.fname, F_OK) != 0)
        smc_read_rts (ucon64.fname, ucon64.parport);
      else
        smc_write_rts (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XSMD:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump cartridge
        {
          smd_read_rom (ucon64.fname, ucon64.parport);
          fputc ('\n', stdout);
        }
      else                                      // file exists => send it to the copier
        {
          if (!ucon64.nfo->backup_header_len)
            fputs ("ERROR: This ROM has no header. Convert to an SMD compatible format\n",
                   stderr);
          else if (!ucon64.nfo->interleaved)
            fputs ("ERROR: This ROM does not seem to be interleaved, but the SMD only supports\n"
                   "       interleaved ROMs. Convert to an SMD compatible format\n",
                   stderr);
          else
            {
              smd_write_rom (ucon64.fname, ucon64.parport);
              fputc ('\n', stdout);
            }
        }
      break;

    case UCON64_XSMDS:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump SRAM contents
        smd_read_sram (ucon64.fname, ucon64.parport);
      else                                      // file exists => restore SRAM
        smd_write_sram (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XSWC:
      enableRTS = 0;                            // falling through
    case UCON64_XSWC2:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump cartridge
        {
          swc_read_rom (ucon64.fname, ucon64.parport, ucon64.io_mode);
          fputc ('\n', stdout);
        }
      else
        {
          if (!ucon64.nfo->backup_header_len)
            fputs ("ERROR: This ROM has no header. Convert to an SWC compatible format\n",
                   stderr);
          else if (ucon64.nfo->interleaved)
            fputs ("ERROR: This ROM seems to be interleaved, but the SWC does not support\n"
                   "       interleaved ROMs. Convert to an SWC compatible format\n",
                   stderr);
          else
            {
              if (enableRTS != 0)
                enableRTS = 1;
              // file exists => send it to the copier
              swc_write_rom (ucon64.fname, ucon64.parport, enableRTS);
              fputc ('\n', stdout);
            }
        }
      break;

    case UCON64_XSWCS:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump SRAM contents
        swc_read_sram (ucon64.fname, ucon64.parport);
      else                                      // file exists => restore SRAM
        swc_write_sram (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XSWCC:
      if (access (ucon64.fname, F_OK) != 0)     // file does not exist => dump SRAM contents
        swc_read_cart_sram (ucon64.fname, ucon64.parport, ucon64.io_mode);
      else                                      // file exists => restore SRAM
        swc_write_cart_sram (ucon64.fname, ucon64.parport, ucon64.io_mode);
      fputc ('\n', stdout);
      break;

    case UCON64_XSWCR:
      if (access (ucon64.fname, F_OK) != 0)
        swc_read_rts (ucon64.fname, ucon64.parport);
      else
        swc_write_rts (ucon64.fname, ucon64.parport);
      fputc ('\n', stdout);
      break;

    case UCON64_XV64:
      if (access (ucon64.fname, F_OK) != 0)
        {
          doctor64_read (ucon64.fname, ucon64.parport);
          fputc ('\n', stdout);
        }
      else
        {
          if (!ucon64.nfo->interleaved)
            fputs ("ERROR: This ROM does not seem to be interleaved, but the Doctor V64 only\n"
                   "       supports interleaved ROMs. Convert to a Doctor V64 compatible format\n",
                   stderr);
          else
            {
              doctor64_write (ucon64.fname, ucon64.nfo->backup_header_len,
                              (int) ucon64.fsize, ucon64.parport);
              fputc ('\n', stdout);
            }
        }
      break;
#endif // USE_PARALLEL

#if     defined USE_PARALLEL || defined USE_USB
    case UCON64_XF2A:
      if (access (ucon64.fname, F_OK) != 0)
        f2a_read_rom (ucon64.fname, 32);
      else
        f2a_write_rom (ucon64.fname, UCON64_UNKNOWN);
      fputc ('\n', stdout);
      break;

    case UCON64_XF2AMULTI:
      f2a_write_rom (NULL, strtol (option_arg, NULL, 10) * MBIT);
      fputc ('\n', stdout);
      break;

    case UCON64_XF2AC:
      if (!access (ucon64.fname, F_OK) && ucon64.backup)
        printf ("Wrote backup to %s\n", mkbak (ucon64.fname, BAK_MOVE));
      f2a_read_rom (ucon64.fname, strtol (option_arg, NULL, 10));
      fputc ('\n', stdout);
      break;

    case UCON64_XF2AS:
      if (access (ucon64.fname, F_OK) != 0)
        f2a_read_sram (ucon64.fname, UCON64_UNKNOWN);
      else
        f2a_write_sram (ucon64.fname, UCON64_UNKNOWN);
      fputc ('\n', stdout);
      break;

    case UCON64_XF2AB:
      if (access (ucon64.fname, F_OK) != 0)
        f2a_read_sram (ucon64.fname, strtol (option_arg, NULL, 10));
      else
        f2a_write_sram (ucon64.fname, strtol (option_arg, NULL, 10));
      fputc ('\n', stdout);
      break;
#endif // USE_PARALLEL || USE_USB

#ifdef  USE_USB
    case UCON64_XQD16:
      if (ucon64.nfo->interleaved)
        fputs ("ERROR: This ROM seems to be interleaved, but the Quickdev16 does not support\n"
               "       interleaved ROMs. Convert to an SWC compatible format\n",
               stderr);
      else
        {
          quickdev16_write_rom (ucon64.fname);
          fputc ('\n', stdout);
        }
      break;

    case UCON64_XUFOSD:
      if (snes_get_copier_type () != UFOSD)
        fputs ("ERROR: This ROM is not in Super UFO Pro 8 SD format. Convert with " OPTION_LONG_S "ufosd\n",
               stderr);
      else if (ucon64.nfo->interleaved)
        fputs ("ERROR: This ROM seems to be interleaved, but the Super UFO Pro 8 SD does not\n"
               "       support interleaved ROMs. Convert with " OPTION_LONG_S "ufosd\n",
               stderr);
      else
        {
          ufosd_write_rom (ucon64.fname);
          fputc ('\n', stdout);
        }
      break;
#endif // USE_USB

    case UCON64_Z64:
      n64_z64 (ucon64.nfo);
      break;

    default:
      break;
    }

  return 0;
}
