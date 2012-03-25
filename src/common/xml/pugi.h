/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   pugixml helpers

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_XML_PUGI_H
#define MTX_COMMON_XML_PUGI_H

#include "common/common_pch.h"

#include "pugixml.hpp"

namespace mtx {
  namespace xml {
    typedef std::shared_ptr<pugi::xml_document> document_cptr;
  }
}

#endif // MTX_COMMON_XML_PUGI_H
