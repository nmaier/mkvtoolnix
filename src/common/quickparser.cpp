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
 * quick Matroska file parsing
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ebml/EbmlHead.h>
#include <ebml/EbmlStream.h>

#include <matroska/KaxCluster.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxSeekHead.h>

#include "commonebml.h"
#include "error.h"
#include "quickparser.h"

using namespace std;
using namespace libebml;
using namespace libmatroska;

kax_quickparser_c::kax_quickparser_c(mm_io_c &_in,
                                     bool parse_fully):
  in(_in) {
  int upper_lvl_el, k;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL;
  segment_child_t new_child, *child;
  vector<segment_child_t>::const_iterator it;

  in.setFilePointer(0, seek_beginning);
  EbmlStream es(in);

  // Find the EbmlHead element. Must be the first one.
  l0 = es.FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
  if (l0 == NULL)
    throw error_c(_("Error: No EBML head found."));

  // Don't verify its data for now.
  l0->SkipData(es, l0->Generic().Context);
  delete l0;

  while (1) {
    // Next element must be a segment
    l0 = es.FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (l0 == NULL)
      throw error_c(_("No segment/level 0 element found."));
    if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)
      break;

    l0->SkipData(es, l0->Generic().Context);
    delete l0;
  }

  upper_lvl_el = 0;
  l1 = es.FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                          true, 1);
  while ((l1 != NULL) && (upper_lvl_el <= 0)) {
    if (is_id(l1, KaxCluster) && !parse_fully)
      break;

    new_child.pos = l1->GetElementPosition();
    new_child.size = l1->ElementSize();
    new_child.id = l1->Generic().GlobalId;
    children.push_back(new_child);

    if ((l0->GetElementPosition() + l0->ElementSize()) <
        l1->GetElementPosition()) {
      delete l1;
      break;
    }

    if (upper_lvl_el < 0) {
      upper_lvl_el++;
      if (upper_lvl_el < 0)
        break;

    }

    l1->SkipData(es, l1->Generic().Context);
    delete l1;
    l1 = es.FindNextElement(l0->Generic().Context, upper_lvl_el,
                            0xFFFFFFFFL, true);

  } // while (l1 != NULL)

  for (k = 0; k < children.size(); k++) {
    child = &children[k];
    if (child->id == KaxSeekHead::ClassInfos.GlobalId) {
      EbmlMaster *m;
      KaxSeek *seek;
      KaxSeekID *seek_id;
      int64_t pos;
      int i;
      bool found;

      in.setFilePointer(child->pos, seek_beginning);
      upper_lvl_el = 0;
      l1 = es.FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                              true, 1);
      if (l1 == NULL)
        continue;
      if (!is_id(l1, KaxSeekHead)) {
        delete l1;
        continue;
      }

      m = static_cast<EbmlMaster *>(l1);
      m->Read(es, l1->Generic().Context, upper_lvl_el, l2, true);
      for (i = 0; i < m->ListSize(); i++) {
        if (!is_id((*m)[i], KaxSeek))
          continue;
        seek = static_cast<KaxSeek *>((*m)[i]);
        seek_id = FINDFIRST(seek, KaxSeekID);
        pos = seek->Location() + l0->GetElementPosition() + l0->HeadSize();
        if ((pos == 0) || (seek_id == NULL))
          continue;

        found = false;
        foreach(it, children)
          if ((*it).pos == pos) {
            found = true;
            break;
          }
        if (!found) {
          new_child.pos = pos;
          new_child.size = -1;
          new_child.id = EbmlId(seek_id->GetBuffer(), seek_id->GetSize());
          children.push_back(new_child);
        }
      }
      delete l1;
    }
  }

  delete l0;

  current_child = children.begin();
}

int
kax_quickparser_c::num_elements(const EbmlId &id)
  const {
  vector<segment_child_t>::const_iterator it;
  int num;

  num = 0;
  foreach(it, children)
    if ((*it).id == id)
      num++;

  return num;
}

segment_child_t *
kax_quickparser_c::get_next(const EbmlId &id) {
  while (current_child != children.end())
    if ((*current_child).id == id) {
      segment_child_t *ptr;
      ptr = &(*current_child);
      current_child++;
      return ptr;
    } else
      current_child++;
  return NULL;
}

void
kax_quickparser_c::reset() {
  current_child = children.begin();
}

EbmlMaster *
kax_quickparser_c::read_all(const EbmlCallbacks &callbacks) {
  EbmlElement *e, *l2;
  EbmlMaster *m, *src;
  segment_child_t *child;
  int upper_lvl_el;

  m = NULL;
  EbmlStream es(in);
  reset();
  for (child = get_next(callbacks.GlobalId); child != NULL;
       child = get_next(callbacks.GlobalId)) {
    in.setFilePointer(child->pos);
    upper_lvl_el = 0;
    e = es.FindNextElement(KaxSegment::ClassInfos.Context, upper_lvl_el,
                           0xFFFFFFFFL, true);
    if (e == NULL)
      continue;
    if (!(e->Generic().GlobalId == callbacks.GlobalId)) {
      delete e;
      continue;
    }

    l2 = NULL;
    e->Read(es, callbacks.Context, upper_lvl_el, l2, true);
    if (m == NULL)
      m = static_cast<EbmlMaster *>(e);
    else {
      src = static_cast<EbmlMaster *>(e);
      while (src->ListSize() > 0) {
        m->PushElement(*(*src)[0]);
        src->Remove(0);
      }
      delete e;
    }
  }
  reset();

  if ((m != NULL) && (m->ListSize() == 0)) {
    delete m;
    return NULL;
  }

  return m;
}
