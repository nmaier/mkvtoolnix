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

#ifndef __THEORA_COMMON_H
#define __THEORA_COMMON_H

#include "os.h"

#include "error.h"

#define THEORA_HEADERTYPE_IDENTIFICATION 0x80
#define THEORA_HEADERTYPE_COMMENT        0x81
#define THEORA_HEADERTYPE_SETUP          0x82

struct MTX_DLL_API theora_identification_header_t {
  uint8_t headertype;
  char theora_string[6];

  uint8_t vmaj;
  uint8_t vmin;
  uint8_t vrev;

  uint16_t fmbw;
  uint16_t fmbh;

  uint32_t picw;
  uint32_t pich;
  uint8_t picx;
  uint8_t picy;

  uint32_t frn;
  uint32_t frd;

  uint32_t parn;
  uint32_t pard;

  uint8_t cs;
  uint8_t pf;

  uint32_t nombr;
  uint8_t qual;

  uint8_t kfgshift;

  theora_identification_header_t();
};

void MTX_DLL_API
theora_parse_identification_header(unsigned char *buffer, int size,
                                   theora_identification_header_t &header)
  throw(error_c);

#endif // __THEORA_COMMON_H
