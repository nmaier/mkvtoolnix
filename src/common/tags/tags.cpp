/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   tag helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common_pch.h"
#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/matroska.h"
#include "common/tags/tags.h"

#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>

using namespace libebml;
using namespace libmatroska;

void
fix_mandatory_tag_elements(EbmlElement *e) {
  if (dynamic_cast<KaxTag *>(e)) {
    KaxTag &t = *static_cast<KaxTag *>(e);
    GetChild<KaxTagTargets>(t);
    GetChild<KaxTagSimple>(t);

  } else if (dynamic_cast<KaxTagSimple *>(e))
    FixMandatoryElement<KaxTagName, KaxTagLangue, KaxTagDefault>(static_cast<KaxTagSimple *>(e));

  else if (dynamic_cast<KaxTagTargets *>(e)) {
    KaxTagTargets &t = *static_cast<KaxTagTargets *>(e);
    GetChild<KaxTagTargetTypeValue>(t);
    FixMandatoryElement<KaxTagTargetTypeValue>(t);

  }

  if (dynamic_cast<EbmlMaster *>(e))
    for (auto child : *static_cast<EbmlMaster *>(e))
      fix_mandatory_tag_elements(child);
}

KaxTags *
select_tags_for_chapters(KaxTags &tags,
                         KaxChapters &chapters) {
  KaxTags *new_tags = nullptr;

  for (auto tag_child : tags) {
    auto tag = dynamic_cast<KaxTag *>(tag_child);
    if (!tag)
      continue;

    bool copy              = true;
    KaxTagTargets *targets = FindChild<KaxTagTargets>(tag);

    if (targets) {
      for (auto child : *targets) {
        auto t_euid = dynamic_cast<KaxTagEditionUID *>(child);
        if (t_euid && !find_edition_with_uid(chapters, t_euid->GetValue())) {
          copy = false;
          break;
        }

        auto t_cuid = dynamic_cast<KaxTagChapterUID *>(child);
        if (t_cuid && !find_chapter_with_uid(chapters, t_cuid->GetValue())) {
          copy = false;
          break;
        }
      }
    }

    if (!copy)
      continue;

    if (!new_tags)
      new_tags = new KaxTags;
    new_tags->PushElement(*(tag->Clone()));
  }

  return new_tags;
}

KaxTagSimple &
find_simple_tag(const std::string &name,
                EbmlMaster &m) {
  return find_simple_tag(cstrutf8_to_UTFstring(name), m);
}

KaxTagSimple &
find_simple_tag(const UTFstring &name,
                EbmlMaster &m) {
  if (EbmlId(m) == EBML_ID(KaxTagSimple)) {
    KaxTagName *tname = FindChild<KaxTagName>(&m);
    if (tname && (name == UTFstring(*tname)))
      return *static_cast<KaxTagSimple *>(&m);
  }

  size_t i;
  for (i = 0; i < m.ListSize(); i++)
    if ((EbmlId(*m[i]) == EBML_ID(KaxTag)) || (EbmlId(*m[i]) == EBML_ID(KaxTagSimple))) {
      try {
        return find_simple_tag(name, *static_cast<EbmlMaster *>(m[i]));
      } catch (...) {
      }
    }

  throw false;
}

std::string
get_simple_tag_value(const std::string &name,
                     EbmlMaster &m) {
  try {
    auto tvalue = FindChild<KaxTagString>(&find_simple_tag(name, m));
    if (tvalue)
      return tvalue->GetValueUTF8();
  } catch (...) {
  }

  return "";
}

std::string
get_simple_tag_name(const KaxTagSimple &tag) {
  KaxTagName *tname = FindChild<KaxTagName>(&tag);
  return tname ? tname->GetValueUTF8() : std::string{""};
}

std::string
get_simple_tag_value(const KaxTagSimple &tag) {
  KaxTagString *tstring = FindChild<KaxTagString>(&tag);
  return tstring ? tstring->GetValueUTF8() : std::string{""};
}

void
set_simple_tag_name(KaxTagSimple &tag,
                    const std::string &name) {
  GetChild<KaxTagName>(tag).SetValueUTF8(name);
}

void
set_simple_tag_value(KaxTagSimple &tag,
                     const std::string &value) {
  GetChild<KaxTagString>(tag).SetValueUTF8(value);
}

void
set_simple_tag(KaxTagSimple &tag,
               const std::string &name,
               const std::string &value) {
  set_simple_tag_name(tag, name);
  set_simple_tag_value(tag, value);
}

int64_t
get_tag_tuid(const KaxTag &tag) {
  auto targets = FindChild<KaxTagTargets>(&tag);
  if (!targets)
    return -1;

  auto tuid = FindChild<KaxTagTrackUID>(targets);
  if (!tuid)
    return -1;

  return tuid->GetValue();
}

int64_t
get_tag_cuid(const KaxTag &tag) {
  auto targets = FindChild<KaxTagTargets>(&tag);
  if (!targets)
    return -1;

  auto cuid = FindChild<KaxTagChapterUID>(targets);
  if (!cuid)
    return -1;

  return cuid->GetValue();
}

/** \brief Convert older tags to current specs
*/
void
convert_old_tags(KaxTags &tags) {
  bool has_level_type = false;
  try {
    find_simple_tag("LEVEL_TYPE", tags);
    has_level_type = true;
  } catch (...) {
  }

  size_t tags_idx;
  for (tags_idx = 0; tags_idx < tags.ListSize(); tags_idx++) {
    if (!is_id(tags[tags_idx], KaxTag))
      continue;

    KaxTag &tag = *static_cast<KaxTag *>(tags[tags_idx]);

    int target_type_value = TAG_TARGETTYPE_TRACK;
    size_t tag_idx        = 0;
    while (tag_idx < tag.ListSize()) {
      tag_idx++;
      if (!is_id(tag[tag_idx - 1], KaxTagSimple))
        continue;

      KaxTagSimple &simple = *static_cast<KaxTagSimple *>(tag[tag_idx - 1]);

      std::string name  = get_simple_tag_name(simple);
      std::string value = get_simple_tag_value(simple);

      if (name == "CATALOG")
        set_simple_tag_name(simple, "CATALOG_NUMBER");

      else if (name == "DATE")
        set_simple_tag_name(simple, "DATE_RELEASED");

      else if (name == "LEVEL_TYPE") {
        if (value == "MEDIA")
          target_type_value = TAG_TARGETTYPE_ALBUM;
        tag_idx--;
        delete tag[tag_idx];
        tag.Remove(tag_idx);
      }
    }

    if (!has_level_type)
      continue;

    auto targets = FindChild<KaxTagTargets>(&tag);
    if (targets)
      GetChild<KaxTagTargetTypeValue>(*targets).SetValue(target_type_value);
  }
}

int
count_simple_tags(EbmlMaster &master) {
  int count = 0;

  for (auto child : master)
    if (is_id(child, KaxTagSimple))
      ++count;

    else if (dynamic_cast<EbmlMaster *>(child))
      count += count_simple_tags(*static_cast<EbmlMaster *>(child));

  return count;
}

void
remove_track_uid_tag_targets(EbmlMaster *tag) {
  for (auto el : *tag) {
    if (!is_id(el, KaxTagTargets))
      continue;

    KaxTagTargets *targets = static_cast<KaxTagTargets *>(el);
    size_t idx_target      = 0;

    while (targets->ListSize() > idx_target) {
      EbmlElement *uid_el = (*targets)[idx_target];
      if (is_id(uid_el, KaxTagTrackUID)) {
        targets->Remove(idx_target);
        delete uid_el;

      } else
        ++idx_target;
    }
  }
}
