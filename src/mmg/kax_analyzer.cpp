/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  kax_analyzer.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief Matroska file analyzer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <vector>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/KaxSeekHead.h>

#include "kax_analyzer.h"

using namespace std;
using namespace libebml;

kax_analyzer_c::kax_analyzer_c(wxWindow *nparent, string nname):
  file_name(nname), segment(NULL), parent(nparent) {
  try {
    file = new mm_io_c(file_name.c_str(), MODE_READ);
  } catch (...) {
    throw error_c(string("Could not open the file " + file_name + "."));
  }
}

kax_analyzer_c::~kax_analyzer_c() {
  uint32_t i;

  delete file;
  for (i = 0; i < data.size(); i++)
    delete data[i];
  if (segment != NULL)
    delete segment;
}

bool kax_analyzer_c::probe(string file_name) {
  try {
    unsigned char data[4];
    mm_io_c in(file_name.c_str(), MODE_READ);

    if (in.read(data, 4) != 4)
      return false;
    if ((data[0] != 0x1A) || (data[1] != 0x45) ||
        (data[2] != 0xDF) || (data[3] != 0xA3))
      return false;
    return true;
  } catch (...) {
    return false;
  }
}

#define PFX "Not a valid Matroska file "
#define in_parent(p) (file->getFilePointer() < \
                      (p->GetElementPosition() + p->ElementSize()))

void kax_analyzer_c::process() {
  int upper_lvl_el;
  EbmlElement *l0, *l1;
  EbmlStream es(*file);

  // Find the EbmlHead element. Must be the first one.
  l0 = es.FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
  if (l0 == NULL)
    throw error_c(PFX "(no EBML head found)");
      
  // Don't verify its data for now.
  l0->SkipData(es, l0->Generic().Context);
  delete l0;

  while (1) {
    // Next element must be a segment
    l0 = es.FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (l0 == NULL)
      throw error_c(PFX "(no segment/level 0 element found)");
    if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)
      break;
    l0->SkipData(es, l0->Generic().Context);
    delete l0;
  }

  segment = static_cast<KaxSegment *>(l0);
  upper_lvl_el = 0;
  // We've got our segment, so let's find all level 1 elements.
  l1 = es.FindNextElement(segment->Generic().Context, upper_lvl_el,
                          0xFFFFFFFFL, true, 1);
  while ((l1 != NULL) && (upper_lvl_el <= 0)) {

    data.push_back(new analyzer_data_c(l1->Generic().GlobalId,
                                       l1->GetElementPosition(),
                                       l1->ElementSize()));
    l1->SkipData(es, l1->Generic().Context);
    delete l1;

    if (!in_parent(segment))
      break;

    l1 = es.FindNextElement(segment->Generic().Context, upper_lvl_el,
                            0xFFFFFFFFL, true);
  } // while (l1 != NULL)
}

EbmlElement *kax_analyzer_c::read_element(uint32_t num,
                                          const EbmlCallbacks &callbacks) {
  EbmlElement *e;
  int upper_lvl_el;

  upper_lvl_el = 0;
  file->setFilePointer(data[num]->pos);
  EbmlStream es(*file);
  e = es.FindNextElement(segment->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                         true, 1);
  if ((e == NULL) || !(EbmlId(*e) == callbacks.GlobalId)) {
    if (e != NULL)
      delete e;
    return NULL;
  }
  upper_lvl_el = 0;
  e->Read(es, callbacks.Context, upper_lvl_el, e, true);

  return e;
}

bool kax_analyzer_c::update_element(EbmlElement *e) {
  uint32_t i, k, found_where, found_what;
  int64_t space_here;
  bool previous_type_found;
  vector<KaxSeekHead *>all_heads;
  EbmlElement *new_e;

  if (e == NULL)
    return false;

  try {
    previous_type_found = false;
    found_what = 0;
    for (i = 0; i < data.size(); i++)
      if (data[i]->id == EbmlId(*e)) {
        previous_type_found = true;
        space_here = data[i]->size;
        for (k = 0; (((i + k) < data.size()) &&
                     (data[i + k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
          space_here += data[i + k]->size;
        if (space_here >= e->ElementSize()) {
          found_what = 1;
          found_where = i;
          break;
        }
      }

    if (!found_what) {
      for (i = 0; i < data.size(); i++) {
        space_here = 0;
        for (k = 0; (((i + k) < data.size()) &&
                     (data[i + k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
          space_here += data[i + k]->size;
        if (space_here >= e->ElementSize()) {
          found_what = 2;
          found_where = i;
          break;
        }
        if (k > 1)
          i += k - 1;
      }
    }

    if (!found_what) {
      // 1. Append chapters to the end of the file.
      // 2. Ajust the segment size.
      // 3. Overwrite the original element(s) if any were found.
      found_where = segment->GetElementPosition() + segment->ElementSize();

    } else {
      // 1. Overwrite the original element.
      // 2. Adjust any following EbmlVoid element if necessary.
    }

    // 1. Find all seek heads.
    for (i = 0; i < data.size(); i++) {
      if (data[i]->id == KaxSeekHead::ClassInfos.GlobalId) {
        new_e = read_element(i, KaxSeekHead::ClassInfos);
        if (new_e != NULL)
          all_heads.push_back(static_cast<KaxSeekHead *>(new_e));
      }
    }

    // 2. Look for a seek head that already pointed to the element that was
    //    overwritten.
    if (found_what == 1) {
      for (i = 0; i < all_heads.size(); i++) {
        for (k = 0; k < all_heads[i]->ListSize(); k++) {
          KaxSeek *seek;
          KaxSeekID *sid;
          KaxSeekPosition *spos;
          seek = static_cast<KaxSeek *>((*all_heads[i])[k]);
          sid = FindChild<KaxSeekID>(*seek);
          spos = FindChild<KaxSeekPosition>(*seek);
          if ((sid != NULL) && (spos != NULL) &&
              (segment->GetGlobalPosition(uint64(*spos)) ==
               data[found_where]->pos)) {
            EbmlId this_id(sid->GetBuffer(), sid->GetSize());
            if (this_id == EbmlId(*e)) {
              wxMessageBox(_("Cool, I can overwrite a thingy in the seek "
                             "head."),
                           _("yeah!"), wxOK | wxCENTER | wxICON_INFORMATION);
              throw 0;
            }
          }
        }
      }
    }

    // 3. If none found: Look for a seek head that has enough space available
    //    (maybe with EbmlVoids) to contain another entry.

    // 4. If none found: Look for the first seek head. Link to a new seek head
    //    entry at the end of the file which will contain all the entries the
    //    original seek head contained + the link to the updated element.
    //    Ajust segment size.

    // 5. If no seek head found at all: Issue a warning that the element might
    //    not be found by players.
    wxMessageBox(_("Wrote to the Matroska file, but the meta seek entry "
                   "could not be updated. This might mean that players will "
                   "have a hard time finding this element. Please use your "
                   "favorite player to check this file."),
                 _("File structure warning"), wxOK | wxCENTER |
                 wxICON_EXCLAMATION);
    throw 0;
  } catch (...) {
    for (i = 0; i < all_heads.size(); i++)
      delete all_heads[i];
    all_heads.clear();
  }

  wxString s;
  s.Printf("found_what: %u, found_where: %u", found_what, found_where);
  wxMessageBox(s, "jojo", wxOK | wxCENTER | wxICON_INFORMATION);

  return false;
}
