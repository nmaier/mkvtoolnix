/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * definition of global variables and functions for the XML tag parser
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __TAGPARSER_H
#define __TAGPARSER_H

namespace libmatroska {
  class KaxTags;
  class KaxChapters;
};

namespace libebml {
  class EbmlElement;
};

using namespace libebml;
using namespace libmatroska;

void MTX_DLL_API parse_xml_tags(const char *name, KaxTags *tags);

void MTX_DLL_API fix_mandatory_tag_elements(EbmlElement *e);

KaxTags *MTX_DLL_API select_tags_for_chapters(KaxTags &tags,
                                              KaxChapters &chapters);

#endif // __TAGPARSER_H
