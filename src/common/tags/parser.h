/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definition of global variables and functions for the XML tag parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_TAGPARSER_H
#define __MTX_COMMON_TAGPARSER_H

#include "common/common_pch.h"

namespace libmatroska {
  class KaxTags;
};

using namespace libmatroska;

void parse_xml_tags(const std::string &name, KaxTags *tags);

#endif // __MTX_COMMON_TAGPARSER_H
