/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   definition of functions helping dealing with tags

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __TAG_COMMON_H
#define __TAG_COMMON_H

#include "os.h"

#include <string>

namespace libmatroska {
  class KaxTags;
  class KaxTag;
  class KaxTagSimple;
  class KaxChapters;
};

namespace libebml {
  class EbmlElement;
  class EbmlMaster;
};

using namespace std;
using namespace libebml;
using namespace libmatroska;

void MTX_DLL_API fix_mandatory_tag_elements(EbmlElement *e);

KaxTags *MTX_DLL_API select_tags_for_chapters(KaxTags &tags,
                                              KaxChapters &chapters);

KaxTagSimple &MTX_DLL_API find_simple_tag(const string &name, EbmlMaster &m);
KaxTagSimple &MTX_DLL_API find_simple_tag(const UTFstring &name,
                                          EbmlMaster &m);
string MTX_DLL_API get_simple_tag_value(const string &name, EbmlMaster &m);
int64_t MTX_DLL_API get_tag_tuid(const KaxTag &tag);
int64_t MTX_DLL_API get_tag_cuid(const KaxTag &tag);

string MTX_DLL_API get_simple_tag_name(const KaxTagSimple &tag);
string MTX_DLL_API get_simple_tag_value(const KaxTagSimple &tag);

void MTX_DLL_API set_simple_tag_name(KaxTagSimple &tag, const string &name);
void MTX_DLL_API set_simple_tag_value(KaxTagSimple &tag, const string &value);
void MTX_DLL_API set_simple_tag(KaxTagSimple &tag, const string &name,
                                const string &valuevalue);

void MTX_DLL_API convert_old_tags(KaxTags &tags);

#endif // __TAG_COMMON_H
