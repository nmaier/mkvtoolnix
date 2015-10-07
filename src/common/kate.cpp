/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Kate helper functions

   Written by ogg.k.ogg.k <ogg.k.ogg.k@googlemail.com>.
   Adapted from code by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/bit_cursor.h"
#include "common/kate.h"

kate_identification_header_t::kate_identification_header_t() {
  memset(this, 0, sizeof(kate_identification_header_t));
}

static int32_t
get_bits32_le(bit_reader_c &bc) {
  int32_t v = 0;

  for (int n = 0; n < 4; ++n) {
    v |= bc.get_bits(8) << (n * 8);
  }

  return v;
}

void
kate_parse_identification_header(const unsigned char *buffer,
                                 int size,
                                 kate_identification_header_t &header) {
  bit_reader_c bc(buffer, size);
  int i;

  header.headertype = bc.get_bits(8);
  if (KATE_HEADERTYPE_IDENTIFICATION != header.headertype)
    throw mtx::kate::header_parsing_x(boost::format(Y("Wrong header type: 0x%|1$02x| != 0x%|2$02x|")) % header.headertype % KATE_HEADERTYPE_IDENTIFICATION);

  for (i = 0; 7 > i; ++i)
    header.kate_string[i] = bc.get_bits(8);
  if (memcmp(header.kate_string, "kate\0\0\0", 7))
    throw mtx::kate::header_parsing_x(boost::format(Y("Wrong identification string: '%|1$7s|' != 'kate\\0\\0\\0'")) % header.kate_string); /* won't print NULs well, but hey */

  bc.get_bits(8); // we don't need those - they are reserved

  header.vmaj = bc.get_bits(8);
  header.vmin = bc.get_bits(8);

  // do not test vmin, as the header is stable for minor version changes
  static const int supported_version = 0;
  if (header.vmaj > supported_version)
    throw mtx::kate::header_parsing_x(boost::format(Y("Wrong Kate version: %1%.%2% > %3%.x")) % header.vmaj % header.vmin % supported_version);

  header.nheaders = bc.get_bits(8);
  header.tenc     = bc.get_bits(8);
  header.tdir     = bc.get_bits(8);
  bc.get_bits(8);           // we don't need those - they are reserved
  header.kfgshift = bc.get_bits(8);

  bc.skip_bits(32); // from bitstream 0.3, these 32 bits are allocated
  bc.skip_bits(32);         // we don't need those - they are reserved

  header.gnum     = get_bits32_le(bc);
  header.gden     = get_bits32_le(bc);

  for (i = 0; 16 > i; ++i)
    header.language[i] = bc.get_bits(8);
  if (header.language[15])
    throw mtx::kate::header_parsing_x(Y("Language is not NUL terminated"));
  for (i = 0; 16 > i; ++i)
    header.category[i] = bc.get_bits(8);
  if (header.category[15])
    throw mtx::kate::header_parsing_x(Y("Category is not NUL terminated"));
}
