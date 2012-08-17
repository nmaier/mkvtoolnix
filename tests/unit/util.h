/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for helper functions for unit tests

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_TESTS_UNIT_INIT_H
#define MTX_TESTS_UNIT_INIT_H

#include "common/common_pch.h"

#include <iostream>

#include <ebml/EbmlBinary.h>
#include <ebml/EbmlDate.h>
#include <ebml/EbmlFloat.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>
#include <ebml/EbmlVoid.h>

namespace mtxut {

using namespace libebml;

void
dump(EbmlElement *element,
     bool with_values = false,
     unsigned int level = 0) {
  std::string indent_str, value_str;
  size_t i;

  for (i = 1; i <= level; ++i)
    indent_str += " ";

  if (with_values) {
    if (dynamic_cast<EbmlUInteger *>(element))
      value_str = to_string(uint64(*static_cast<EbmlUInteger *>(element)));

    else if (dynamic_cast<EbmlSInteger *>(element))
      value_str = to_string(int64(*static_cast<EbmlSInteger *>(element)));

    else if (dynamic_cast<EbmlFloat *>(element))
      value_str = to_string(double(*static_cast<EbmlFloat *>(element)));

    else if (dynamic_cast<EbmlUnicodeString *>(element))
      value_str = UTFstring_to_cstrutf8(UTFstring(*static_cast<EbmlUnicodeString *>(element)));

    else if (dynamic_cast<EbmlString *>(element))
      value_str = std::string(*static_cast<EbmlString *>(element));

    else if (dynamic_cast<EbmlDate *>(element))
      value_str = to_string(static_cast<EbmlDate *>(element)->GetEpochDate());

    else
      value_str = (boost::format("(type: %1%)") %
                   (  dynamic_cast<EbmlBinary *>(element) ? "binary"
                    : dynamic_cast<EbmlMaster *>(element) ? "master"
                    : dynamic_cast<EbmlVoid *>(element)   ? "void"
                    :                                       "unknown")).str();

    value_str = " " + value_str;
  }

  std::cout << indent_str << EBML_NAME(element) << value_str << std::endl;

  EbmlMaster *master = dynamic_cast<EbmlMaster *>(element);
  if (!master)
    return;

  for (auto el : *master)
    dump(el, with_values, level + 1);
}

}

#endif // MTX_TESTS_UNIT_UTIL_H
