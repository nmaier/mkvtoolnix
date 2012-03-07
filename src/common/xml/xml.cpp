/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/xml/xml.h"

std::string
escape_xml(const std::string &source) {
  std::string dst;
  std::string::const_iterator src = source.begin();
  while (src != source.end()) {
    if (*src == '&')
      dst += "&amp;";
    else if (*src == '>')
      dst += "&gt;";
    else if (*src == '<')
      dst += "&lt;";
    else if (*src == '"')
      dst += "&quot;";
    else
      dst += *src;
    src++;
  }

  return dst;
}

std::string
create_xml_node_name(const char *name,
                     const char **atts) {
  int i;
  std::string node_name = std::string("<") + name;
  for (i = 0; (nullptr != atts[i]) && (nullptr != atts[i + 1]); i += 2)
    node_name += std::string(" ") + atts[i] + "=\"" +
      escape_xml(atts[i + 1]) + "\"";
  node_name += ">";

  return node_name;
}

