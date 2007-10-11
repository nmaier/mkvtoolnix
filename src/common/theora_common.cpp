/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   Ogg Theora helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include "bit_cursor.h"
#include "common.h"
#include "theora_common.h"

theora_identification_header_t::theora_identification_header_t() {
  memset(this, 0, sizeof(theora_identification_header_t));
}

void
theora_parse_identification_header(unsigned char *buffer,
                                   int size,
                                   theora_identification_header_t &header)
  throw(error_c) {

  bit_cursor_c bc(buffer, size);
  int i;

  header.headertype = bc.get_bits(8);
  if (THEORA_HEADERTYPE_IDENTIFICATION != header.headertype)
    throw error_c(mxsprintf("Wrong header type: 0x%02x != 0x%02x",
                            header.headertype,
                            THEORA_HEADERTYPE_IDENTIFICATION));

  for (i = 0; 6 > i; ++i)
    header.theora_string[i] = bc.get_bits(8);
  if (strncmp(header.theora_string, "theora", 6))
    throw error_c(mxsprintf("Wrong identifaction string: '%6s' != 'theora'",
                            header.theora_string));

  header.vmaj = bc.get_bits(8);
  header.vmin = bc.get_bits(8);
  header.vrev = bc.get_bits(8);

  if ((3 != header.vmaj) || (2 != header.vmin))
    throw error_c(mxsprintf("Wrong Theora version: %d.%d.%d != 3.2.x",
                            header.vmaj, header.vmin, header.vrev));

  header.fmbw = bc.get_bits(16) * 16;
  header.fmbh = bc.get_bits(16) * 16;
  header.picw = bc.get_bits(24);
  header.pich = bc.get_bits(24);
  header.picx = bc.get_bits(8);
  header.picy = bc.get_bits(8);

  header.frn = bc.get_bits(32);
  header.frd = bc.get_bits(32);

  header.parn = bc.get_bits(24);
  header.pard = bc.get_bits(24);

  header.cs = bc.get_bits(8);
  header.nombr = bc.get_bits(24);
  header.qual = bc.get_bits(6);

  header.kfgshift = bc.get_bits(5);

  header.pf = bc.get_bits(2);

  if (0 != bc.get_bits(3))
    throw error_c("Reserved bits are not 0");
}
