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
  if (dynamic_cast<KaxTag *>(e) != NULL) {
    KaxTag &t = *static_cast<KaxTag *>(e);
    GetChild<KaxTagTargets>(t);
    GetChild<KaxTagSimple>(t);

  } else if (dynamic_cast<KaxTagSimple *>(e) != NULL) {
    KaxTagSimple &s                       = *static_cast<KaxTagSimple *>(e);
    KaxTagName &n                         = GetChild<KaxTagName>(s);
    *static_cast<EbmlUnicodeString *>(&n) = UTFstring(n);
    KaxTagLangue &l                       = GetChild<KaxTagLangue>(s);
    *static_cast<EbmlString *>(&l)        = std::string(l);
    KaxTagDefault &d                      = GetChild<KaxTagDefault>(s);
    *static_cast<EbmlUInteger *>(&d)      = uint64(d);

  } else if (dynamic_cast<KaxTagTargets *>(e) != NULL) {
    KaxTagTargets &t = *static_cast<KaxTagTargets *>(e);
    GetChild<KaxTagTargetTypeValue>(t);
    KaxTagTargetTypeValue &v         = GetChild<KaxTagTargetTypeValue>(t);
    *static_cast<EbmlUInteger *>(&v) = uint64(v);

  }

  if (dynamic_cast<EbmlMaster *>(e) != NULL) {
    size_t i;

    EbmlMaster *m = static_cast<EbmlMaster *>(e);
    for (i = 0; m->ListSize() > i; i++)
      fix_mandatory_tag_elements((*m)[i]);
  }
}

KaxTags *
select_tags_for_chapters(KaxTags &tags,
                         KaxChapters &chapters) {
  KaxTags *new_tags = NULL;
  size_t tags_idx;
  for (tags_idx = 0; tags_idx < tags.ListSize(); tags_idx++) {
    if (dynamic_cast<KaxTag *>(tags[tags_idx]) == NULL)
      continue;

    bool copy = true;
    KaxTagTargets *targets = FINDFIRST(static_cast<EbmlMaster *>(tags[tags_idx]), KaxTagTargets);

    if (NULL != targets) {
      size_t targets_idx;
      for (targets_idx = 0; targets_idx < targets->ListSize(); targets_idx++) {
        KaxTagEditionUID *t_euid = dynamic_cast<KaxTagEditionUID *>((*targets)[targets_idx]);
        if ((NULL != t_euid) && (find_edition_with_uid(chapters, uint64(*t_euid)) == NULL)) {
          copy = false;
          break;
        }

        KaxTagChapterUID *t_cuid = dynamic_cast<KaxTagChapterUID *>((*targets)[targets_idx]);
        if ((NULL != t_cuid) && (find_chapter_with_uid(chapters, uint64(*t_cuid)) == NULL)) {
          copy = false;
          break;
        }
      }
    }

    if (copy) {
      if (NULL == new_tags)
        new_tags = new KaxTags;
      new_tags->PushElement(*(tags[tags_idx]->Clone()));
    }
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
  std::string rvalue;

  if (EbmlId(m) == EBML_ID(KaxTagSimple)) {
    KaxTagName *tname = FINDFIRST(&m, KaxTagName);
    if ((NULL != tname) && (name == UTFstring(*tname)))
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
    KaxTagSimple &simple = find_simple_tag(name, m);
    KaxTagString *tvalue = FINDFIRST(&simple, KaxTagString);
    if (NULL != tvalue)
      return UTFstring_to_cstrutf8(UTFstring(*tvalue));
  } catch (...) {
  }

  return "";
}

std::string
get_simple_tag_name(const KaxTagSimple &tag) {
  KaxTagName *tname = FINDFIRST(&tag, KaxTagName);
  if (NULL == tname)
    return "";

  return UTFstring_to_cstrutf8(UTFstring(*tname));
}

std::string
get_simple_tag_value(const KaxTagSimple &tag) {
  KaxTagString *tstring = FINDFIRST(&tag, KaxTagString);
  if (NULL == tstring)
    return "";

  return UTFstring_to_cstrutf8(UTFstring(*tstring));
}

void
set_simple_tag_name(KaxTagSimple &tag,
                    const std::string &name) {
  GetChildAs<KaxTagName, EbmlUnicodeString>(tag) = cstrutf8_to_UTFstring(name);
}

void
set_simple_tag_value(KaxTagSimple &tag,
                     const std::string &value) {
  GetChildAs<KaxTagString, EbmlUnicodeString>(tag) = cstrutf8_to_UTFstring(value);
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
  KaxTagTargets *targets = FINDFIRST(&tag, KaxTagTargets);
  if (NULL == targets)
    return -1;

  KaxTagTrackUID *tuid = FINDFIRST(targets, KaxTagTrackUID);
  if (NULL == tuid)
    return -1;

  return uint64(*tuid);
}

int64_t
get_tag_cuid(const KaxTag &tag) {
  KaxTagTargets *targets = FINDFIRST(&tag, KaxTagTargets);
  if (NULL == targets)
    return -1;

  KaxTagChapterUID *cuid = FINDFIRST(targets, KaxTagChapterUID);
  if (NULL == cuid)
    return -1;

  return uint64(*cuid);
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

    if (has_level_type) {
      KaxTagTargets *targets = FINDFIRST(&tag, KaxTagTargets);
      if (NULL != targets)
        GetChildAs<KaxTagTargetTypeValue, EbmlUInteger>(*targets) = target_type_value;
    }
  }
}

static int
count_simple_tags_recursively(EbmlMaster &master,
                              int count) {
  size_t master_idx;

  for (master_idx = 0; master.ListSize() > master_idx; ++master_idx)
    if (is_id(master[master_idx], KaxTagSimple))
      ++count;

    else if (dynamic_cast<EbmlMaster *>(master[master_idx]) != NULL)
      count = count_simple_tags_recursively(*static_cast<EbmlMaster *>(master[master_idx]), count);

  return count;
}

int
count_simple_tags(EbmlMaster &master) {
  return count_simple_tags_recursively(master, 0);
}

void
remove_track_uid_tag_targets(EbmlMaster *tag) {
  size_t idx_tag;
  for (idx_tag = 0; tag->ListSize() > idx_tag; idx_tag++) {
    EbmlElement *el = (*tag)[idx_tag];

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
