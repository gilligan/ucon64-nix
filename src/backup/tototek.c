/*
tototek.c - General ToToTEK flash card programmer routines for uCON64

Copyright (c) 2004, 2015, 2017, 2020 dbjh

Based on Delphi source code by ToToTEK Multi Media. Information in that source
code has been used with permission. However, ToToTEK Multi Media explicitly
stated that the information in that source code may be freely distributed.


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
#include "misc/bswap.h"
#include "misc/misc.h"
#include "misc/parallel.h"
#include "misc/term.h"
#include "backup/tototek.h"


#ifdef USE_PARALLEL

static void set_data_read (void);
//static void set_data_write (void);
static void set_ai (unsigned char ai);
static void init_port (void);
static void deinit_port (void);
static void end_port (void);
static void set_addr_read (unsigned int addr);
static void set_addr_write (unsigned int addr);
static unsigned char get_id_byte (unsigned char addr);

static unsigned short port_8, port_9, port_a, port_b, port_c;


void
ttt_init_io (unsigned short port)
{
#if     (defined __unix__ || defined __BEOS__) && !defined __MSDOS__
  init_conio ();
#endif
  if (register_func (ttt_deinit_io) == -1)
    {
      fputs ("ERROR: Could not register function with register_func()\n", stderr);
      exit (1);
    }

  port_8 = port;                                // original code used 0x378 for port_8
  port_9 = port + 1;
  port_a = port + 2;
  port_b = port + 3;
  port_c = port + 4;

  parport_print_info ();

  init_port ();
}


void
ttt_deinit_io (void)
{
#if     (defined __unix__ || defined __BEOS__) && !defined __MSDOS__
  deinit_conio ();
#endif

  end_port ();
  deinit_port ();
}


static void
set_data_read (void)                            // original name: set_data_read
{
  outportb (port_a, 0);                         // nastb=1,nib_sel=0,ndstb=1,nwrite=1
}


#if 0
static void
set_data_write (void)                           // original name: set_data_write
{
  outportb (port_a, 1);                         // nastb=1,nib_sel=0,ndstb=1,nwrite=0
}
#endif


static void
set_ai (unsigned char ai)                       // original name: {epp_}set_ai
{
  outportb (port_a, 1);                         // set_data_write ()
  outportb (port_b, ai);
}


void
ttt_set_ai_data (unsigned char ai, unsigned char data) // original name: set_ai_data
{
  set_ai (ai);
  outportb (port_a, 1);                         // set_data_write ()
  outportb (port_c, data);
}


static void
init_port (void)                                // original name: init_port
{
  ttt_set_ai_data (6, 0);                       // rst=0, wei=0(dis.), rdi=0(dis.)
  ttt_set_ai_data (6, 0x84);                    // rst=1, wei=0(dis.), rdi=0(dis.)
}


static void
deinit_port (void)                              // original name: Close_end_port
{
  outportb (port_a, 1);
  outportb (port_b, 0);
  outportb (port_a, 4);                         // port normal now
}


static void
end_port (void)                                 // original name: end_port
{
  ttt_set_ai_data (6, 0);                       // rst=0, wei=0(dis.), rdi=0(dis.)
  outportb (port_a, 4);                         // set_normal ninit=1, nwrite=1
}


void
ttt_rom_enable (void)                           // original name: romCS_on
{
  ttt_set_ai_data (6, 0x84);
}


void
ttt_rom_disable (void)                          // original name: romCS_off
{
  ttt_set_ai_data (6, 0x80);
}


void
ttt_ram_enable (void)                           // original name: ramCS_on
{
  ttt_set_ai_data (6, 0x88);
}


void
ttt_ram_disable (void)                          // original name: ramCS_off
{
  ttt_set_ai_data (6, 0x80);
}


static void
set_addr_write (unsigned int addr)              // original name: set_Long_adrw
{
  ttt_set_ai_data (0, (unsigned char) (addr & 0xff)); // a[7..0]
  addr >>= 8;
  ttt_set_ai_data (1, (unsigned char) (addr & 0xff)); // a[15..8]
  addr >>= 8;
  ttt_set_ai_data (2, (unsigned char) (addr & 0xff)); // a[23..16]
  set_ai (3);
}


static void
set_addr_read (unsigned int addr)               // original name: set_Long_adr
{
  set_addr_write (addr);
  set_data_read ();                             // ninit=0, nwrite=1
}


void
ttt_write_mem (unsigned int addr, unsigned char b) // original name: WriteMEMb
{
  set_addr_write (addr);
  outportb (port_c, b);
}


static unsigned char
get_id_byte (unsigned char addr)                // original name: GETID
{
  unsigned char byte;

  ttt_rom_enable ();
  ttt_set_ai_data (0, addr);                    // a[7..0] = 0
  set_ai (3);
  outportb (port_c, 0x90);                      // write_data ()

  set_ai (3);
  set_data_read ();                             // ninit=0, nwrite=1
  byte = inportb (port_c);                      // read_data ()
  ttt_rom_disable ();

  return byte;
}


unsigned short
ttt_get_id (void)                               // original name: getIDword
{
  unsigned short word;

//  eep_reset ();
  word = get_id_byte (0);                       // msg
  word <<= 8;
  word |= get_id_byte (2);                      // ID

  return word;
}


void
ttt_read_rom_b (unsigned int addr, unsigned char *buf) // original name: read_buffb
{
  unsigned int count;

  ttt_rom_enable ();
  for (count = 0; count < 0x100; count++)
    {
      set_addr_read (addr + count);
      buf[count] = inportb (port_c);            // read_data ()
    }
  ttt_rom_disable ();
}


void
ttt_read_rom_w (unsigned int addr, unsigned char *buf) // original name: read_buff
{
  unsigned int count;

  set_addr_read (addr);
  for (count = 0; count < 0x80; count++)
#ifdef  WORDS_BIGENDIAN
    ((unsigned short *) buf)[count] = bswap_16 (inportw (port_c)); // read_dataw ()
#else
    ((unsigned short *) buf)[count] = inportw (port_c); // read_dataw ()
#endif
}


void
ttt_read_ram_b (unsigned int addr, unsigned char *buf) // original name: read_rambuff
{
  unsigned int count;

  ttt_ram_enable ();
  for (count = 0; count < 0x100; count++)
    {
      set_addr_read (addr + count);
      buf[count] = inportb (port_c);            // read_data ()
    }
  ttt_ram_disable ();
}


void
ttt_read_ram_w (unsigned int addr, unsigned char *buf) // original name: readpagerambuff
{
  unsigned int count;

  set_addr_read (addr);
  for (count = 0; count < 0x80; count++)
    ((unsigned short *) buf)[count] = inportw (port_c);
  // read_dataw (); data is doubled for MD-PRO => no problems with endianess
}


// Sharp function
void
ttt_erase_block (unsigned int addr)             // original name: EraseBLOCK
{
  ttt_rom_enable ();
  ttt_write_mem (addr, 0x50);
  ttt_rom_disable ();

  ttt_rom_enable ();
  ttt_write_mem (addr, 0x20);
  outportb (port_c, 0xd0);
  ttt_rom_disable ();

  ttt_rom_enable ();
  set_ai (3);
  set_data_read ();                             // set read mode
  while (inportb (port_c) < 0x80)
    ;
  ttt_rom_disable ();
}


void
ttt_write_byte_sharp (unsigned int addr, unsigned char b) // original name: writeEEPDataB
{
  ttt_rom_enable ();
  ttt_write_mem (addr, 0x40);
  outportb (port_c, b);
  ttt_rom_disable ();
}


void
ttt_write_byte_intel (unsigned int addr, unsigned char b) // original name: writeIntelEEPDataB
{                                               // MD-PRO function
  ttt_rom_enable ();
  ttt_write_mem (addr, 0x40);
  outportb (port_c, b);

  set_data_read ();
  while (inportb (port_c) < 0x80)
    ;
  ttt_rom_disable ();
}


void
ttt_write_page_rom (unsigned int addr, unsigned char *buf) // original name: writeEEPDataPAGE
{
  int i;

  // send command 0xe8
  ttt_rom_enable ();
  ttt_write_mem (addr, 0xe8);

  // read status
  set_ai (3);
  set_data_read ();
  while (inportb (port_c) < 0x80)
    ;

  set_ai (3);
  outportb (port_c, 0x1f);
  ttt_set_ai_data (6, 0x94);
  set_addr_write (addr);
  for (i = 0; i <= 0x1f; i++)
    outportb (port_c, buf[(addr + i) & 0x3fff]);
  ttt_set_ai_data (6, 0x84);
  set_ai (3);
  outportb (port_c, 0xd0);

  // read status
  set_ai (3);
  set_data_read ();
  while (inportb (port_c) < 0x80)
    ;
  ttt_rom_disable ();
}


void
ttt_write_byte_ram (unsigned int addr, unsigned char b)  // original name: writeRAMDataB
{
  ttt_ram_enable ();
  ttt_write_mem (addr, b);
  ttt_ram_disable ();
}


void
ttt_write_page_ram (unsigned int addr, unsigned char *buf) // original name: writeRAMDataPAGE
{
  unsigned int i;

  ttt_ram_enable ();
  ttt_set_ai_data (6, 0x98);
  set_addr_write (addr);
  for (i = 0; i < 0x100; i++)
    outportb (port_c, buf[(addr + i) & 0x3fff]);
  ttt_ram_disable ();
}


void
ttt_write_page_ram2 (unsigned int addr, unsigned char *buf) // original name: writeRAMDBDataPAGE
{                                               // MD-PRO function
  unsigned int i;

  ttt_ram_enable ();
  ttt_set_ai_data (6, 0x98);
  set_addr_write (addr * 2);
  for (i = 0; i < 0x80; i++)
    {
      outportb (port_c, buf[(addr + i) & 0x3fff]);
      outportb (port_c, buf[(addr + i) & 0x3fff]);
    }
  ttt_ram_disable ();
}


#if 0
void
readstatus (void)
{                                               // MD-PRO function
  // send command 0xe8
  ttt_rom_enable ();
  set_ai (3);
  set_data_read ();
  while (inportb (port_c) < 0x80)
    ;
  ttt_rom_disable ();
}
#endif

#endif // USE_PARALLEL
