/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definition of functions helping dealing with tags

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_TAG_COMMON_H
#define __MTX_COMMON_TAG_COMMON_H

#include "common/common_pch.h"

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

using namespace libebml;
using namespace libmatroska;

typedef std::shared_ptr<KaxTags> kax_tags_cptr;

void fix_mandatory_tag_elements(EbmlElement *e);
void remove_track_uid_tag_targets(EbmlMaster *tag);

KaxTags *select_tags_for_chapters(KaxTags &tags, KaxChapters &chapters);

KaxTagSimple &find_simple_tag(const std::string &name, EbmlMaster &m);
KaxTagSimple &find_simple_tag(const UTFstring &name, EbmlMaster &m);
std::string get_simple_tag_value(const std::string &name, EbmlMaster &m);
int64_t get_tag_tuid(const KaxTag &tag);
int64_t get_tag_cuid(const KaxTag &tag);

std::string get_simple_tag_name(const KaxTagSimple &tag);
std::string get_simple_tag_value(const KaxTagSimple &tag);

void set_simple_tag_name(KaxTagSimple &tag, const std::string &name);
void set_simple_tag_value(KaxTagSimple &tag, const std::string &value);
void set_simple_tag(KaxTagSimple &tag, const std::string &name, const std::string &valuevalue);

int count_simple_tags(EbmlMaster &master);

void convert_old_tags(KaxTags &tags);

#endif // __MTX_COMMON_TAG_COMMON_H
