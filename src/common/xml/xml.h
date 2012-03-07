/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   XML helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_XML_H
#define __MTX_COMMON_XML_H

#include "common/common_pch.h"

std::string escape_xml(const std::string &src);
std::string create_xml_node_name(const char *name, const char **atts);

#endif  // __MTX_COMMON_XML_H
