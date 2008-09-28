/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   helper functions that need libebml/libmatroska

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __COMMONEBML_H
#define __COMMONEBML_H

#include "os.h"

#include <string>

#include <ebml/EbmlMaster.h>
#include <ebml/EbmlUnicodeString.h>

#include <matroska/KaxTracks.h>

using namespace std;
using namespace libebml;
using namespace libmatroska;

#define can_be_cast(c, e) (dynamic_cast<c *>(e) != NULL)

bool MTX_DLL_API is_valid_utf8_string(const string &c);
UTFstring MTX_DLL_API cstr_to_UTFstring(const string &c);
UTFstring MTX_DLL_API cstrutf8_to_UTFstring(const string &c);
string MTX_DLL_API UTFstring_to_cstr(const UTFstring &u);
string MTX_DLL_API UTFstring_to_cstrutf8(const UTFstring &u);

int64_t MTX_DLL_API kt_get_default_duration(KaxTrackEntry &track);
int64_t MTX_DLL_API kt_get_number(KaxTrackEntry &track);
int64_t MTX_DLL_API kt_get_uid(KaxTrackEntry &track);
string MTX_DLL_API kt_get_codec_id(KaxTrackEntry &track);
int MTX_DLL_API kt_get_max_blockadd_id(KaxTrackEntry &track);
string MTX_DLL_API kt_get_language(KaxTrackEntry &track);

int MTX_DLL_API kt_get_a_channels(KaxTrackEntry &track);
float MTX_DLL_API kt_get_a_sfreq(KaxTrackEntry &track);
float MTX_DLL_API kt_get_a_osfreq(KaxTrackEntry &track);
int MTX_DLL_API kt_get_a_bps(KaxTrackEntry &track);

int MTX_DLL_API kt_get_v_pixel_width(KaxTrackEntry &track);
int MTX_DLL_API kt_get_v_pixel_height(KaxTrackEntry &track);

#define is_id(e, ref) (e->Generic().GlobalId == ref::ClassInfos.GlobalId)

#define FINDFIRST(p, c) (static_cast<c *> \
  (((EbmlMaster *)p)->FindFirstElt(c::ClassInfos, false)))
#define FINDNEXT(p, c, e) (static_cast<c *> \
  (((EbmlMaster *)p)->FindNextElt(*e, false)))

template <typename type>type &GetEmptyChild(EbmlMaster &master) {
  EbmlElement *e;
  EbmlMaster *m;

  e = master.FindFirstElt(type::ClassInfos, true);
  if ((m = dynamic_cast<EbmlMaster *>(e)) != NULL) {
    while (m->ListSize() > 0) {
      delete (*m)[0];
      m->Remove(0);
    }
  }

  return *(static_cast<type *>(e));
}

template <typename type>type &GetNextEmptyChild(EbmlMaster &master,
                                                const type &past_elt) {
  EbmlElement *e;
  EbmlMaster *m;

  e = master.FindNextElt(past_elt, true);
  if ((m = dynamic_cast<EbmlMaster *>(e)) != NULL) {
    while (m->ListSize() > 0) {
      delete (*m)[0];
      m->Remove(0);
    }
  }

  return *(static_cast<type *>(e));
}

template <typename type>type &AddEmptyChild(EbmlMaster &master) {
  EbmlElement *e;
  EbmlMaster *m;

  e = new type;
  if ((m = dynamic_cast<EbmlMaster *>(e)) != NULL) {
    while (m->ListSize() > 0) {
      delete (*m)[0];
      m->Remove(0);
    }
  }
  master.PushElement(*e);

  return *(static_cast<type *>(e));
}

EbmlElement *MTX_DLL_API empty_ebml_master(EbmlElement *e);
EbmlElement *MTX_DLL_API create_ebml_element(const EbmlCallbacks &callbacks,
                                             const EbmlId &id);
EbmlMaster *MTX_DLL_API sort_ebml_master(EbmlMaster *e);

const EbmlCallbacks *MTX_DLL_API
find_ebml_callbacks(const EbmlCallbacks &base, const EbmlId &id);
const EbmlCallbacks *MTX_DLL_API
find_ebml_callbacks(const EbmlCallbacks &base, const char *debug_name);
const EbmlCallbacks *MTX_DLL_API
find_ebml_parent_callbacks(const EbmlCallbacks &base, const EbmlId &id);
const EbmlSemantic *MTX_DLL_API
find_ebml_semantic(const EbmlCallbacks &base, const EbmlId &id);

#endif // __COMMONEBML_H
