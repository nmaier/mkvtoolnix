/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   tag helper functions
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "tag_common.h"

#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>

using namespace std;
using namespace libebml;
using namespace libmatroska;

void
fix_mandatory_tag_elements(EbmlElement *e) {
  if (dynamic_cast<KaxTagSimple *>(e) != NULL) {
    KaxTagSimple &s = *static_cast<KaxTagSimple *>(e);
    GetChild<KaxTagLangue>(s);
    GetChild<KaxTagDefault>(s);

  } else if (dynamic_cast<KaxTagTargets *>(e) != NULL) {
    KaxTagTargets &t = *static_cast<KaxTagTargets *>(e);
    GetChild<KaxTagTargetTypeValue>(t);

  }

  if (dynamic_cast<EbmlMaster *>(e) != NULL) {
    EbmlMaster *m;
    int i;

    m = static_cast<EbmlMaster *>(e);
    for (i = 0; i < m->ListSize(); i++)
      fix_mandatory_tag_elements((*m)[i]);
  }
}

KaxTags *
select_tags_for_chapters(KaxTags &tags,
                         KaxChapters &chapters) {
  KaxTags *new_tags;
  KaxTagTargets *targets;
  KaxTagEditionUID *t_euid;
  KaxTagChapterUID *t_cuid;
  int tags_idx, targets_idx;
  bool copy;

  new_tags = NULL;
  for (tags_idx = 0; tags_idx < tags.ListSize(); tags_idx++) {
    if (dynamic_cast<KaxTag *>(tags[tags_idx]) == NULL)
      continue;
    copy = true;
    targets = FINDFIRST(static_cast<EbmlMaster *>(tags[tags_idx]),
                        KaxTagTargets);
    if (targets != NULL) {
      for (targets_idx = 0; targets_idx < targets->ListSize(); targets_idx++) {
        t_euid = dynamic_cast<KaxTagEditionUID *>((*targets)[targets_idx]);
        if ((t_euid != NULL) &&
            (find_edition_with_uid(chapters, uint64(*t_euid)) == NULL)) {
          copy = false;
          break;
        }
        t_cuid = dynamic_cast<KaxTagChapterUID *>((*targets)[targets_idx]);
        if ((t_cuid != NULL) &&
            (find_chapter_with_uid(chapters, uint64(*t_cuid)) == NULL)) {
          copy = false;
          break;
        }
      }
    }

    if (copy) {
      if (new_tags == NULL)
        new_tags = new KaxTags;
      new_tags->PushElement(*(tags[tags_idx]->Clone()));
    }
  }

  return new_tags;
}

KaxTagSimple &
find_simple_tag(const string &name,
                EbmlMaster &m) {
  return find_simple_tag(cstrutf8_to_UTFstring(name), m);
}

KaxTagSimple &
find_simple_tag(const UTFstring &name,
                EbmlMaster &m) {
  int i;
  string rvalue;

  if (EbmlId(m) == KaxTagSimple::ClassInfos.GlobalId) {
    KaxTagName *tname;

    tname = FINDFIRST(&m, KaxTagName);
    if ((tname != NULL) &&
        (name == UTFstring(*static_cast<EbmlUnicodeString *>(tname))))
      return *static_cast<KaxTagSimple *>(&m);
  }

  for (i = 0; i < m.ListSize(); i++)
    if ((EbmlId(*m[i]) == KaxTag::ClassInfos.GlobalId) ||
        (EbmlId(*m[i]) == KaxTagSimple::ClassInfos.GlobalId)) {
      try {
        return find_simple_tag(name, *static_cast<EbmlMaster *>(m[i]));
      } catch (...) {
      }
    }

  throw "";
}

string
get_simple_tag_value(const string &name,
                     EbmlMaster &m) {
  try {
    KaxTagSimple &simple = find_simple_tag(name, m);
    KaxTagString *tvalue;

    tvalue = FINDFIRST(&simple, KaxTagString);
    if (tvalue != NULL)
      return UTFstring_to_cstrutf8(UTFstring(*static_cast<EbmlUnicodeString *>
                                             (tvalue)));
  } catch (...) {
  }

  return "";
}

string
get_simple_tag_name(const KaxTagSimple &tag) {
  KaxTagName *tname;

  tname = FINDFIRST(&tag, KaxTagName);
  if (tname == NULL)
    return "";
  return UTFstring_to_cstrutf8(UTFstring(*static_cast<EbmlUnicodeString *>
                                         (tname)));
}

string
get_simple_tag_value(const KaxTagSimple &tag) {
  KaxTagString *tstring;

  tstring = FINDFIRST(&tag, KaxTagString);
  if (tstring == NULL)
    return "";
  return UTFstring_to_cstrutf8(UTFstring(*static_cast<EbmlUnicodeString *>
                                         (tstring)));
}

void
set_simple_tag_name(KaxTagSimple &tag,
                    const string &name) {
  *static_cast<EbmlUnicodeString *>(&GetChild<KaxTagName>(tag)) =
    cstrutf8_to_UTFstring(name);
}

void
set_simple_tag_value(KaxTagSimple &tag,
                     const string &value) {
  *static_cast<EbmlUnicodeString *>(&GetChild<KaxTagString>(tag)) =
    cstrutf8_to_UTFstring(value);
}

void
set_simple_tag(KaxTagSimple &tag,
               const string &name,
               const string &value) {
  set_simple_tag_name(tag, name);
  set_simple_tag_value(tag, value);
}

int64_t
get_tag_tuid(const KaxTag &tag) {
  KaxTagTargets *targets;
  KaxTagTrackUID *tuid;

  targets = FINDFIRST(&tag, KaxTagTargets);
  if (targets == NULL)
    return -1;
  tuid = FINDFIRST(targets, KaxTagTrackUID);
  if (tuid == NULL)
    return -1;
  return uint64(*static_cast<EbmlUInteger *>(tuid));
}

int64_t
get_tag_cuid(const KaxTag &tag) {
  KaxTagTargets *targets;
  KaxTagChapterUID *cuid;

  targets = FINDFIRST(&tag, KaxTagTargets);
  if (targets == NULL)
    return -1;
  cuid = FINDFIRST(targets, KaxTagChapterUID);
  if (cuid == NULL)
    return -1;
  return uint64(*static_cast<EbmlUInteger *>(cuid));
}

/** \brief Convert older tags to current specs
 */
void
convert_old_tags(KaxTags &tags) {
  string name, value;
  int i, tags_idx, target_type_value;
  bool has_level_type;

  try {
    find_simple_tag("LEVEL_TYPE", tags);
    has_level_type = true;
  } catch (...) {
    has_level_type = false;
  }

  for (tags_idx = 0; tags_idx < tags.ListSize(); tags_idx++) {
    if (!is_id(tags[tags_idx], KaxTag))
      continue;
    KaxTag &tag = *static_cast<KaxTag *>(tags[tags_idx]);

    target_type_value = TAG_TARGETTYPE_TRACK;
    i = 0;
    while (i < tag.ListSize()) {
      i++;
      if (!is_id(tag[i - 1], KaxTagSimple))
        continue;
      KaxTagSimple &simple = *static_cast<KaxTagSimple *>(tag[i - 1]);

      name = get_simple_tag_name(simple);
      value = get_simple_tag_value(simple);

      if (name == "CATALOG")
        set_simple_tag_name(simple, "CATALOG_NUMBER");
      else if (name == "DATE")
        set_simple_tag_name(simple, "DATE_RELEASED");
      else if (name == "LEVEL_TYPE") {
        if (value == "MEDIA")
          target_type_value = TAG_TARGETTYPE_ALBUM;
        i--;
        delete tag[i];
        tag.Remove(i);
      }
    }

    if (has_level_type) {
      KaxTagTargets *targets;

      targets = FINDFIRST(&tag, KaxTagTargets);
      if (targets != NULL)
        *static_cast<EbmlUInteger *>
          (&GetChild<KaxTagTargetTypeValue>(*targets)) = target_type_value;
    }
  }
}
