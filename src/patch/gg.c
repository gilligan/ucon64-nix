/********************************************************************
 * Copyright (c) 2001 by WyrmCorp <http://wyrmcorp.com>.
 * All rights reserved. Distributed under the BSD Software License.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. Neither the name of the <ORGANIZATION> nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *
 * uggconv - Universal Game Genie (tm) Code Convertor
 *
 * Game Genie (tm) is a trademark of Galoob and is used for
 * purely informational purposes.  No endorsement of this
 * utility by Galoob is implied.
 *
 * This application is 100% ANSI C.  Compile with
 *   gcc uggconv.c -o uggconv
 * or the equivalent for your platform.
 */
/*
Portions copyright (c) 2001 - 2002                    NoisyB
Portions copyright (c) 2002, 2016 - 2017, 2020 - 2021 dbjh
*/
#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif
#include <ctype.h>
#ifdef  _MSC_VER
#pragma warning(pop)
#endif
#include "misc/archive.h"
#include "misc/file.h"
#include "misc/misc.h"
#include "ucon64_misc.h"
#include "console/snes.h"
#include "patch/gg.h"


#define GAME_GENIE_MAX_STRLEN 12 /* longest string is "XXX-XXX-XXX" */

#define encodeNES(v, n, m, s) (data[n] |= ((v) >> (s)) & (m))
#define decodeNES(v, n, m, s) (v |= (data[n] & (m)) << (s))
#define encodeSNES(x, y) (transposed |= (((address & (0xc00000 >> (2*(y)))) << (2*(y))) >> (2*(x))))
#define decodeSNES(x, y) (address |= (((transposed & (0xc00000 >> (2*(x)))) << (2*(x))) >> (2*(y))))


static st_ucon64_obj_t gg_obj[] =
  {
    {0, WF_INIT | WF_PROBE},
    {0, WF_INIT | WF_PROBE | WF_NO_ROM}
  };

const st_getopt2_t gg_usage[] =
  {
    {
      "gge", 1, 0, UCON64_GGE,
      "CODE", "encode and display Game Genie code\n"
      "example: " OPTION_LONG_S "gge" OPTARG_S "CODE " OPTION_LONG_S "sms or "
      OPTION_LONG_S "gge" OPTARG_S "CODE " OPTION_LONG_S "gb\n"
      "CODE" OPTARG_S "'AAAA:VV' or CODE" OPTARG_S "'AAAA:VV:CC'\n"
      OPTION_LONG_S "gge" OPTARG_S "CODE " OPTION_LONG_S "gen\n"
      "CODE" OPTARG_S "'AAAAAA:VVVV'\n"
      OPTION_LONG_S "gge" OPTARG_S "CODE " OPTION_LONG_S "nes\n"
      "CODE" OPTARG_S "'AAAA:VV' or CODE" OPTARG_S "'AAAA:VV:CC'\n"
      OPTION_LONG_S "gge" OPTARG_S "CODE " OPTION_LONG_S "snes\n"
      "CODE" OPTARG_S "'AAAAAA:VV'",
      &gg_obj[1]
    },
    {
      "ggd", 1, 0, UCON64_GGD,
      "GG_CODE", "decode Game Genie code\n"
      "example: " OPTION_LONG_S "ggd" OPTARG_S "GG_CODE " OPTION_LONG_S "sms or "
      OPTION_LONG_S "ggd" OPTARG_S "GG_CODE " OPTION_LONG_S "gb\n"
      "GG_CODE" OPTARG_S "'XXX-XXX' or GG_CODE" OPTARG_S "'XXX-XXX-XXX'\n"
      OPTION_LONG_S "ggd" OPTARG_S "GG_CODE " OPTION_LONG_S "gen\n"
      "GG_CODE" OPTARG_S "'XXXX-XXXX'\n"
      OPTION_LONG_S "ggd" OPTARG_S "GG_CODE " OPTION_LONG_S "nes\n"
      "GG_CODE" OPTARG_S "'XXXXXX' or GG_CODE" OPTARG_S "'XXXXXXXX'\n"
      OPTION_LONG_S "ggd" OPTARG_S "GG_CODE " OPTION_LONG_S "snes\n"
      "GG_CODE" OPTARG_S "'XXXX-XXXX'",
      &gg_obj[1]
    },
    {
      "gg", 1, 0, UCON64_GG,
      "GG_CODE", "apply Game Genie code (permanently)\n"
      "example: like above but a ROM is required\n"
      "supported are:\n"
      "Game Boy/(Super GB)/GB Pocket/Color GB/(GB Advance),\n"
      "Sega Master System(II/III)/Game Gear (Handheld),\n"
      "Genesis/Sega Mega Drive/Sega CD/32X/Nomad,\n"
      "Nintendo Entertainment System/NES/Famicom/Game Axe (Redant),\n"
      "Super Nintendo Entertainment System/SNES/Super Famicom",
      &gg_obj[0]
    },
    {NULL, 0, 0, 0, NULL, NULL, NULL}
  };


static st_ucon64_nfo_t *gg_rominfo;
static int CPUaddress;
static int gg_argc;
static const char *gg_argv[128];
static const char genesisChars[] = "ABCDEFGHJKLMNPRSTVWXYZ0123456789";


static char
hexDigit (int value)
{
  switch (toupper (value))
    {
    case 0:
      return '0';
    case 1:
      return '1';
    case 2:
      return '2';
    case 3:
      return '3';
    case 4:
      return '4';
    case 5:
      return '5';
    case 6:
      return '6';
    case 7:
      return '7';
    case 8:
      return '8';
    case 9:
      return '9';
    case 10:
      return 'A';
    case 11:
      return 'B';
    case 12:
      return 'C';
    case 13:
      return 'D';
    case 14:
      return 'E';
    case 15:
      return 'F';
    default:
      break;
    }
  return '?';
}


static int
hexValue (char digit)
{
  switch (toupper ((int) digit))
    {
    case '0':
      return 0;
    case '1':
      return 1;
    case '2':
      return 2;
    case '3':
      return 3;
    case '4':
      return 4;
    case '5':
      return 5;
    case '6':
      return 6;
    case '7':
      return 7;
    case '8':
      return 8;
    case '9':
      return 9;
    case 'A':
      return 10;
    case 'B':
      return 11;
    case 'C':
      return 12;
    case 'D':
      return 13;
    case 'E':
      return 14;
    case 'F':
      return 15;
    default:
      break;
    }
  return 0;
}


static int
hexByteValue (char x, char y)
{
  return (hexValue (x) << 4) + hexValue (y);
}


static int
gameGenieDecodeGameBoy (const char *in, char *out)
{
  int address = 0;
  int value = 0;
  int haveCheck = 0;
  int i;
  int in_len = (int) strlen (in);

  if (in_len == 11 && in[3] == '-' && in[7] == '-')
    haveCheck = 1;
  else if (in_len == 7 && in[3] == '-')
    haveCheck = 0;
  else
    return -1;
  for (i = 0; i < in_len; ++i)
    if (in[i] != '-' && !isxdigit ((int) in[i]))
      return -1;
  if (hexValue (in[6]) < 8)
    return -1;

  value = hexByteValue (in[0], in[1]);

  address =
    (hexValue (in[2]) << 8) | (hexValue (in[4]) << 4) | hexValue (in[5]) |
    ((~hexValue (in[6]) & 0xf) << 12);

  if (haveCheck)
    {
      int check = hexByteValue (in[8], in[10]);
      check = ~check;
      check = (((check & 0xfc) >> 2) | ((check & 0x03) << 6)) ^ 0x45;
      sprintf (out, "%04X:%02X:%02X", address, value, check);
    }
  else
    sprintf (out, "%04X:%02X", address, value);

  return 0;
}


static int
gameGenieEncodeGameBoy (const char *in, char *out)
{
  int haveCheck = 0;
  int i;
  int in_len = (int) strlen (in);

  if (in_len == 10 && in[4] == ':' && in[7] == ':')
    haveCheck = 1;
  else if (in_len == 7 && in[4] == ':')
    haveCheck = 0;
  else
    return -1;
  for (i = 0; i < in_len; ++i)
    if (in[i] != ':' && !isxdigit ((int) in[i]))
      return -1;
  if (hexValue (in[0]) > 7)
    return -1;

  out[0] = (char) toupper ((int) in[5]);
  out[1] = (char) toupper ((int) in[6]);
  out[2] = (char) toupper ((int) in[1]);
  out[3] = '-';
  out[4] = (char) toupper ((int) in[2]);
  out[5] = (char) toupper ((int) in[3]);
  out[6] = hexDigit (~hexValue (in[0]) & 0xf);
  out[7] = '\0';

  if (haveCheck)
    {
      int check = hexByteValue (in[8], in[9]);
      check ^= 0x45;
      check = ~(((check & 0xc0) >> 6) | ((check & 0x3f) << 2));
      i = (check & 0xf0) >> 4;
      out[7] = '-';
      out[8] = hexDigit (i & 0xf);
      out[9] = hexDigit ((i ^ 8) & 0xf);
      out[10] = hexDigit (check & 0xf);
      out[11] = '\0';
    }

  return 0;
}


static int
isGenesisChar (char c)
{
  return strchr (genesisChars, toupper ((int) c)) != 0;
}


static int
genesisValue (char c)
{
  return (int) (strchr (genesisChars, toupper ((int) c)) - genesisChars);
}


static int
gameGenieDecodeMegadrive (const char *in, char *out)
{
  int address = 0;
  int value = 0;
  int data[8];
  int i;

  if (strlen (in) != 9 || in[4] != '-')
    return -1;
  for (i = 0; i < 9; ++i)
    if (in[i] != '-' && !isGenesisChar (in[i]))
      return -1;

  for (i = 0; i < 4; ++i)
    data[i] = genesisValue (in[i]);
  for (i = 5; i < 9; ++i)
    data[i - 1] = genesisValue (in[i]);

  address |= (data[3] & 0x0f) << 20;
  address |= (data[4] & 0x1e) << 15;
  address |= (data[1] & 0x03) << 14;
  address |= (data[2] & 0x1f) << 9;
  address |= (data[3] & 0x10) << 4;
  address |= (data[6] & 0x07) << 5;
  address |= (data[7] & 0x1f);

  value |= (data[5] & 0x01) << 15;
  value |= (data[6] & 0x18) << 10;
  value |= (data[4] & 0x01) << 12;
  value |= (data[5] & 0x1e) << 7;
  value |= (data[0] & 0x1f) << 3;
  value |= (data[1] & 0x1c) >> 2;

  sprintf (out, "%06X:%04X", address, value);

  return 0;
}


static int
gameGenieEncodeMegadrive (const char *in, char *out)
{
  unsigned int address = 0;
  unsigned int value = 0;
  int data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  int i;

  if (strlen (in) != 11 || in[6] != ':')
    return -1;
  for (i = 0; i < 11; ++i)
    if (in[i] != ':' && !isxdigit ((int) in[i]))
      return -1;

  sscanf (in, "%x:%x", &address, &value);

  data[3] |= (address >> 20) & 0x0f;
  data[4] |= (address >> 15) & 0x1e;
  data[1] |= (address >> 14) & 0x03;
  data[2] |= (address >> 9) & 0x1f;
  data[3] |= (address >> 4) & 0x10;
  data[6] |= (address >> 5) & 0x07;
  data[7] |= (address & 0x1f);

  data[5] |= (value >> 15) & 0x01;
  data[6] |= (value >> 10) & 0x18;
  data[4] |= (value >> 12) & 0x01;
  data[5] |= (value >> 7) & 0x1e;
  data[0] |= (value >> 3) & 0x1f;
  data[1] |= (value << 2) & 0x1c;

  for (i = 0; i < 4; ++i)
    out[i] = genesisChars[data[i]];
  out[4] = '-';
  for (i = 5; i < 9; ++i)
    out[i] = genesisChars[data[i - 1]];
  out[9] = '\0';

  return 0;
}


static char
mapNesChar (char hex)
{
  switch (toupper ((int) hex))
    {
    case '0':
      return 'A';
    case '1':
      return 'P';
    case '2':
      return 'Z';
    case '3':
      return 'L';
    case '4':
      return 'G';
    case '5':
      return 'I';
    case '6':
      return 'T';
    case '7':
      return 'Y';
    case '8':
      return 'E';
    case '9':
      return 'O';
    case 'A':
      return 'X';
    case 'B':
      return 'U';
    case 'C':
      return 'K';
    case 'D':
      return 'S';
    case 'E':
      return 'V';
    case 'F':
      return 'N';
    default:
      break;
    }
  return '?';
}


static char
unmapNesChar (char genie)
{
  switch (toupper ((int) genie))
    {
    case 'A':
      return '0';
    case 'P':
      return '1';
    case 'Z':
      return '2';
    case 'L':
      return '3';
    case 'G':
      return '4';
    case 'I':
      return '5';
    case 'T':
      return '6';
    case 'Y':
      return '7';
    case 'E':
      return '8';
    case 'O':
      return '9';
    case 'X':
      return 'A';
    case 'U':
      return 'B';
    case 'K':
      return 'C';
    case 'S':
      return 'D';
    case 'V':
      return 'E';
    case 'N':
      return 'F';
    default:
      break;
    }
  return '0';
}


static int
isNesChar (char c)
{
  return strchr ("APZLGITYEOXUKSVN-", toupper ((int) c)) != 0;
}


static int
gameGenieDecodeNES (const char *in, char *out)
{
  int address = 0;
  int value = 0;
  int check = 0;
  int haveCheck = 0;
  int data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  int i;
  int in_len = (int) strlen (in);

  if (in_len == 8)
    haveCheck = 1;
  else if (in_len == 6)
    haveCheck = 0;
  else
    return -1;
  for (i = 0; i < in_len; ++i)
    if (!isNesChar (in[i]))
      return -1;

  for (i = 0; i < 6; ++i)
    data[i] = hexValue (unmapNesChar (in[i]));
  if (haveCheck)
    {
      data[6] = hexValue (unmapNesChar (in[6]));
      data[7] = hexValue (unmapNesChar (in[7]));
    }

  decodeNES (address, 1, 8, 4);
  decodeNES (address, 2, 7, 4);
  decodeNES (address, 3, 7, 12);
  decodeNES (address, 3, 8, 0);
  decodeNES (address, 4, 7, 0);
  decodeNES (address, 4, 8, 8);
  decodeNES (address, 5, 7, 8);
  if (haveCheck)
    {
      decodeNES (value, 0, 7, 0);
      decodeNES (value, 0, 8, 4);
      decodeNES (value, 1, 7, 4);
      decodeNES (value, 7, 8, 0);
      decodeNES (check, 6, 7, 0);
      decodeNES (check, 5, 8, 0);
      decodeNES (check, 6, 8, 4);
      decodeNES (check, 7, 7, 4);
    }
  else
    {
      decodeNES (value, 0, 7, 0);
      decodeNES (value, 0, 8, 4);
      decodeNES (value, 1, 7, 4);
      decodeNES (value, 5, 8, 0);
    }

  if (haveCheck)
    sprintf (out, "%04X:%02X:%02X", address, value, check);
  else
    sprintf (out, "%04X:%02X", address, value);

  return 0;
}


static int
gameGenieEncodeNES (const char *in, char *out)
{
  unsigned int address = 0;
  unsigned int value = 0;
  unsigned int check = 0;
  int haveCheck = 0;
  int data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  int i;
  int in_len = (int) strlen (in);

  if (in_len == 10 && in[4] == ':' && in[7] == ':')
    haveCheck = 1;
  else if (in_len == 7 && in[4] == ':')
    haveCheck = 0;
  else
    return -1;
  for (i = 0; i < in_len; ++i)
    if (in[i] != ':' && !isxdigit ((int) in[i]))
      return -1;

  if (haveCheck)
    sscanf (in, "%x:%x:%x", &address, &value, &check);
  else
    sscanf (in, "%x:%x", &address, &value);

  /* Encode address with transposition cipher.
   * Do not encode the high address bit (optional, Galoob isn't
   * consistent but usually doesn't.
   */
  encodeNES (address, 1, 8, 4);
  encodeNES (address, 2, 7, 4);
  encodeNES (address, 3, 7, 12);
  encodeNES (address, 3, 8, 0);
  encodeNES (address, 4, 7, 0);
  encodeNES (address, 4, 8, 8);
  encodeNES (address, 5, 7, 8);
  if (haveCheck)
    {
      encodeNES (value, 0, 7, 0);
      encodeNES (value, 0, 8, 4);
      encodeNES (value, 1, 7, 4);
      encodeNES (value, 7, 8, 0);
      encodeNES (check, 6, 7, 0);
      encodeNES (check, 5, 8, 0);
      encodeNES (check, 6, 8, 4);
      encodeNES (check, 7, 7, 4);
    }
  else
    {
      encodeNES (value, 0, 7, 0);
      encodeNES (value, 0, 8, 4);
      encodeNES (value, 1, 7, 4);
      encodeNES (value, 5, 8, 0);
    }

  if (haveCheck)
    {
      sprintf (out, "%c%c%c%c%c%c%c%c",
               mapNesChar (hexDigit (data[0])),
               mapNesChar (hexDigit (data[1])),
               mapNesChar (hexDigit (data[2])),
               mapNesChar (hexDigit (data[3])),
               mapNesChar (hexDigit (data[4])),
               mapNesChar (hexDigit (data[5])),
               mapNesChar (hexDigit (data[6])),
               mapNesChar (hexDigit (data[7])));
    }
  else
    {
      sprintf (out, "%c%c%c%c%c%c",
               mapNesChar (hexDigit (data[0])),
               mapNesChar (hexDigit (data[1])),
               mapNesChar (hexDigit (data[2])),
               mapNesChar (hexDigit (data[3])),
               mapNesChar (hexDigit (data[4])),
               mapNesChar (hexDigit (data[5])));
    }

  return 0;
}


static char
mapSnesChar (char hex)
{
  switch (toupper ((int) hex))
    {
    case '0':
      return 'D';
    case '1':
      return 'F';
    case '2':
      return '4';
    case '3':
      return '7';
    case '4':
      return '0';
    case '5':
      return '9';
    case '6':
      return '1';
    case '7':
      return '5';
    case '8':
      return '6';
    case '9':
      return 'B';
    case 'A':
      return 'C';
    case 'B':
      return '8';
    case 'C':
      return 'A';
    case 'D':
      return '2';
    case 'E':
      return '3';
    case 'F':
      return 'E';
    default:
      break;
    }
  return ' ';
}


static char
unmapSnesChar (char genie)
{
  switch (toupper ((int) genie))
    {
    case 'D':
      return '0';
    case 'F':
      return '1';
    case '4':
      return '2';
    case '7':
      return '3';
    case '0':
      return '4';
    case '9':
      return '5';
    case '1':
      return '6';
    case '5':
      return '7';
    case '6':
      return '8';
    case 'B':
      return '9';
    case 'C':
      return 'A';
    case '8':
      return 'B';
    case 'A':
      return 'C';
    case '2':
      return 'D';
    case '3':
      return 'E';
    case 'E':
      return 'F';
    default:
      break;
    }
  return ' ';
}


static int
gameGenieDecodeSNES (const char *in, char *out)
{
  int value, hirom, address, transposed;

  if (!isxdigit ((int) in[0]) || !isxdigit ((int) in[1]) ||
      !isxdigit ((int) in[2]) || !isxdigit ((int) in[3]) ||
      in[4] != '-' ||
      !isxdigit ((int) in[5]) || !isxdigit ((int) in[6]) ||
      !isxdigit ((int) in[7]) || !isxdigit ((int) in[8]) || in[9] != 0)
    return -1;

  value = hexByteValue (unmapSnesChar (in[0]), unmapSnesChar (in[1]));

  transposed = hexValue (unmapSnesChar (in[8])) +
               (hexValue (unmapSnesChar (in[7])) << 4) +
               (hexValue (unmapSnesChar (in[6])) << 8) +
               (hexValue (unmapSnesChar (in[5])) << 12) +
               (hexValue (unmapSnesChar (in[3])) << 16) +
               (hexValue (unmapSnesChar (in[2])) << 20);

  address = 0;
  decodeSNES (0, 4);
  decodeSNES (1, 5);
  decodeSNES (2, 8);
  decodeSNES (3, 9);
  decodeSNES (4, 7);
  decodeSNES (5, 0);
  decodeSNES (6, 1);
  decodeSNES (7, 10);
  decodeSNES (8, 11);
  decodeSNES (9, 2);
  decodeSNES (10, 3);
  decodeSNES (11, 6);

  /* if a ROM was specified snes.c will handle ucon64.snes_hirom */
  if (UCON64_ISSET (ucon64.snes_hirom))         /* -hi or -nhi switch was specified */
    hirom = ucon64.snes_hirom;
  /* if only a ROM was specified (not -hi or -nhi) the next if will fail for a
      handful of ROMs, namely Sufami Turbo ROMs and Extended ROMs */
  else if (gg_rominfo != 0)
    hirom = gg_rominfo->header_start > SNES_HEADER_START ? 1 : 0;
  else
    hirom = 1;                                  /* I am less sure about the LoROM */
                                                /*  CPU -> ROM address conversion */
  CPUaddress = address;
  if (hirom)
    {
      if (address >= 0xc00000 /* && address <= 0xffffff */)
        address -= 0xc00000;
      else if (address >= 0x800000 /* && address <= 0xbfffff */)
        address -= 0x800000;
      else if (address >= 0x400000 /* && address <= 0x7fffff */)
        address -= 0x400000;
    }
  else
    {
      if (address >= 0x808000)
        address -= 0x808000;
      else if (address >= 0x008000)
        address -= 0x008000;
      address = (address & 0x7fff) | ((address & 0xff0000) >> 1);
    }

  sprintf (out, "%06X:%02X", address, value);

  return 0;
}


static int
gameGenieEncodeSNES (const char *in, char *out)
{
  int value, transposed;
  unsigned int address;

  if (!isxdigit ((int) in[0]) || !isxdigit ((int) in[1]) ||
      !isxdigit ((int) in[2]) || !isxdigit ((int) in[3]) ||
      !isxdigit ((int) in[4]) || !isxdigit ((int) in[5]) ||
      in[6] != ':' || !isxdigit ((int) in[7]) || !isxdigit ((int) in[8]) || in[9] != 0)
    return -1;

  value = hexByteValue (mapSnesChar (in[7]), mapSnesChar (in[8]));
  sscanf (in, "%x", &address);

  transposed = 0;
  encodeSNES (0, 4);
  encodeSNES (1, 5);
  encodeSNES (2, 8);
  encodeSNES (3, 9);
  encodeSNES (4, 7);
  encodeSNES (5, 0);
  encodeSNES (6, 1);
  encodeSNES (7, 10);
  encodeSNES (8, 11);
  encodeSNES (9, 2);
  encodeSNES (10, 3);
  encodeSNES (11, 6);

  sprintf (out, "%02X%c%c-%c%c%c%c", value,
           mapSnesChar (hexDigit ((transposed & 0xf00000) >> 20)),
           mapSnesChar (hexDigit ((transposed & 0x0f0000) >> 16)),
           mapSnesChar (hexDigit ((transposed & 0x00f000) >> 12)),
           mapSnesChar (hexDigit ((transposed & 0x000f00) >> 8)),
           mapSnesChar (hexDigit ((transposed & 0x0000f0) >> 4)),
           mapSnesChar (hexDigit ((transposed & 0x00000f))));

  return 0;
}


#if 0
static void
usage (void)
{
  puts ("uggconv v1.0 - Universal Game Genie (tm) Convertor");
  puts ("Copyright (c) 2001 by WyrmCorp <http://wyrmcorp.com>");
  puts ("\nUsage:");
  puts ("Game Boy/Gear: uggconv -g [XXX-XXX] [XXX-XXX-XXX] [AAAA:VV] [AAAA:VV:CC] ...");
  puts ("Megadrive:     uggconv -m [XXXX-XXXX] [AAAAAA:VVVV] ...");
  puts ("NES:           uggconv -n [XXXXXX] [XXXXXXXX] [AAAA:VV] [AAAA:VV:CC] ...");
  puts ("SNES:          uggconv -s [XXXX-XXXX] [AAAAAA:VV] ...");
  exit (1);
}
#endif


static int
gg_main (int argc, const char **argv)
{
  char buffer[GAME_GENIE_MAX_STRLEN];
  int i;
  int result = 0;

#if 0
  if (argc < 3 || strlen (argv[1]) != 2 || argv[1][0] != '-')
    usage ();
  if (strchr ("gmns", argv[1][1]) == 0)
    usage ();
#endif

  for (i = 2; i < argc; ++i)
    {
      if (strchr (argv[i], ':') == 0)
        {
          switch (argv[1][1])
            {
            case 'g':
              result = gameGenieDecodeGameBoy (argv[i], buffer);
              break;
            case 'm':
              result = gameGenieDecodeMegadrive (argv[i], buffer);
              break;
            case 'n':
              result = gameGenieDecodeNES (argv[i], buffer);
              break;
            case 's':
              result = gameGenieDecodeSNES (argv[i], buffer);
              break;
            }
        }
      else
        {
          switch (argv[1][1])
            {
            case 'g':
              result = gameGenieEncodeGameBoy (argv[i], buffer);
              break;
            case 'm':
              result = gameGenieEncodeMegadrive (argv[i], buffer);
              break;
            case 'n':
              result = gameGenieEncodeNES (argv[i], buffer);
              break;
            case 's':
              result = gameGenieEncodeSNES (argv[i], buffer);
              break;
            }
        }
      if (result != 0)
        printf ("%s is badly formed\n", argv[i]);
      else
        {
          if (CPUaddress != -1)                 /* SNES decode specific */
            printf ("%s = %s (CPU address: %06X)\n", argv[i], buffer, CPUaddress);
          else
            printf ("%s = %s\n", argv[i], buffer);
        }
    }
  return 0;
}


static void
apply_code (FILE *destfile, int offset, char *bufold, char *bufnew, int size)
{
  fputc ('\n', stdout);
  dumper (stdout, bufold, size, offset, DUMPER_HEX);

  fseek (destfile, offset, SEEK_SET);
  fwrite (bufnew, 1, size, destfile);

  dumper (stdout, bufnew, size, offset, DUMPER_HEX);
}


int
gg_display (st_ucon64_nfo_t *rominfo, const char *code)
{
  gg_argv[0] = "uggconv";

  switch (ucon64.console)
    {
    case UCON64_SNES:
      gg_argv[1] = "-s";
      break;

    case UCON64_GEN:
      gg_argv[1] = "-m";
      break;

    case UCON64_NES:
      gg_argv[1] = "-n";
      break;

    case UCON64_GB:
    case UCON64_SMS:
      gg_argv[1] = "-g";
      break;

    default:
      fprintf (stderr,
               "ERROR: You must specify a ROM or force the console type\n"
               "       For example, the force recognition switch for SNES is " OPTION_LONG_S "snes\n");
      return -1;
    }
  gg_argv[2] = code;
  gg_argc = 3;

  if (rominfo && ucon64.fsize > 0)              /* check if rominfo contains valid ROM info */
    gg_rominfo = rominfo;
  else
    gg_rominfo = 0;
  CPUaddress = -1;
  gg_main (gg_argc, gg_argv);

  return 0;
}


int
gg_apply (st_ucon64_nfo_t *rominfo, const char *code)
{
  unsigned int size = (unsigned int) ucon64.fsize -
                         (rominfo ? rominfo->backup_header_len : 0),
               offset, address, value, writefile;
  int check = -1, result = -1;
  char buf[GAME_GENIE_MAX_STRLEN], buf2[GAME_GENIE_MAX_STRLEN],
       dest_name[FILENAME_MAX];
  FILE *destfile;

  if (rominfo && ucon64.fsize > 0)              /* check if rominfo contains valid ROM info */
    gg_rominfo = rominfo;
  else
    {
      fprintf (stderr, "ERROR: You must specify a ROM to apply the code to\n");
      return -1;
    }

  CPUaddress = -1;
  switch (ucon64.console)
    {
      case UCON64_GB:
      case UCON64_SMS:
        result = gameGenieDecodeGameBoy (code, buf);
        break;
      case UCON64_GEN:
        result = gameGenieDecodeMegadrive (code, buf);
        break;
      case UCON64_NES:
        result = gameGenieDecodeNES (code, buf);
        break;
      case UCON64_SNES:
        result = gameGenieDecodeSNES (code, buf);
        break;
      default:
        break;
    }

  if (result != 0)
    {
      fprintf (stderr, "ERROR: Game Genie code %s is badly formed\n", code);
      return -1;
    }

  if (CPUaddress != -1)                         /* SNES decode specific */
    printf ("%s = %s (CPU address: %06X)\n", code, buf, CPUaddress);
  else
    printf ("%s = %s\n", code, buf);
  sscanf (buf, "%x:%x:%x", &address, &value, (unsigned int *) &check);

  if (address >= size)
    {
      fprintf (stderr, "ERROR: Address is too high for this ROM (%u)\n", address);
      return -1;
    }

  strcpy (dest_name, ucon64.fname);
  writefile = !(ucon64.console == UCON64_NES && check == -1);

  if (writefile)
    {
      ucon64_file_handler (dest_name, NULL, 0);
      fcopy (ucon64.fname, 0, ucon64.fsize, dest_name, "wb"); /* no copy if one file */

      if ((destfile = fopen (dest_name, "r+b")) == NULL)
        {
          fprintf (stderr, ucon64_msg[OPEN_WRITE_ERROR], dest_name);
          return -1;
        }
    }
  else
    {
      puts ("NOTE: A 6 digit code is too generic. File will not be modified.\n"
            "      Listing possible 8 digit codes");
      if ((destfile = fopen (dest_name, "rb")) == NULL)
        {
          fprintf (stderr, ucon64_msg[OPEN_READ_ERROR], dest_name);
          return -1;
        }
    }

  offset = address + rominfo->backup_header_len;
  if (ucon64.console == UCON64_NES)
    {
      while (offset < ucon64.fsize)
        {
          fseek (destfile, offset, SEEK_SET);
          buf[0] = (char) fgetc (destfile);

          if (check != -1)
            { /* 8 digit code */
              if (buf[0] == (char) check)
                {
                  buf2[0] = (char) value;
                  apply_code (destfile, offset, buf, buf2, 1);
                }
            }
          else
            { /* 6 digit code => display the corresponding 8 digit code(s) */
              char longcode[9];

              fputc ('\n', stdout);
              dumper (stdout, buf, 1, offset, DUMPER_HEX);

              address = offset - rominfo->backup_header_len;
              sprintf (buf, "%04X:%02X:%02X", address & 0xffff, value,
                       (unsigned char) buf[0]);
              gameGenieEncodeNES (buf, longcode);
              printf ("%s = %s\n", buf, longcode);
            }

          offset += 8 * 1024;
        }
    }
  else if (ucon64.console == UCON64_GEN)
    {
      fseek (destfile, offset, SEEK_SET);
      buf[0] = (char) fgetc (destfile);
      buf[1] = (char) fgetc (destfile);

      buf2[0] = (char) (value >> 8);
      buf2[1] = (char) value;
      apply_code (destfile, offset, buf, buf2, 2);
    }
  else /* UCON64_SNES || UCON64_SMS || UCON64_GB */
    {
      fseek (destfile, offset, SEEK_SET);
      buf[0] = (char) fgetc (destfile);

      /* for SNES check is always -1 */
      if (check == -1 || buf[0] == (char) check)
        {
          buf2[0] = (char) value;
          apply_code (destfile, offset, buf, buf2, 1);
        }
    }

  fclose (destfile);
  if (writefile)
    {
      fputc ('\n', stdout);
      printf (ucon64_msg[WROTE], dest_name);
    }
  return 0;
}
