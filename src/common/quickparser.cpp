/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   quick Matroska file parsing

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ebml/EbmlHead.h>
#include <ebml/EbmlStream.h>

#include <matroska/KaxCluster.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxSeekHead.h>

#include "common/common.h"
#include "common/ebml.h"
#include "common/error.h"
#include "common/quickparser.h"

using namespace libebml;
using namespace libmatroska;

kax_quickparser_c::kax_quickparser_c(mm_io_c &in,
                                     bool parse_fully)
  : m_in(in)
{
  in.setFilePointer(0, seek_beginning);
  EbmlStream es(in);

  // Find the EbmlHead element. Must be the first one.
  EbmlElement *l0 = es.FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
  if (NULL == l0)
    throw error_c(Y("Error: No EBML head found."));

  // Don't verify its data for now.
  l0->SkipData(es, l0->Generic().Context);
  delete l0;

  while (1) {
    // Next element must be a segment
    l0 = es.FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (NULL == l0)
      throw error_c(Y("No segment/level 0 element found."));

    if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)
      break;

    l0->SkipData(es, l0->Generic().Context);
    delete l0;
  }

  int upper_lvl_el = 0;
  EbmlElement *l1 = es.FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true, 1);
  while ((NULL != l1) && (0 >= upper_lvl_el)) {
    if (is_id(l1, KaxCluster) && !parse_fully)
      break;

    m_children.push_back(segment_child_t(l1->GetElementPosition(), l1->ElementSize(), l1->Generic().GlobalId));

    if ((l0->GetElementPosition() + l0->ElementSize()) < l1->GetElementPosition()) {
      delete l1;
      break;
    }

    if (0 > upper_lvl_el) {
      upper_lvl_el++;
      if (0 > upper_lvl_el)
        break;

    }

    l1->SkipData(es, l1->Generic().Context);
    delete l1;
    l1 = es.FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

  } // while (l1 != NULL)

  int child_idx;
  for (child_idx = 0; m_children.size() > child_idx; child_idx++) {
    segment_child_t *child = &m_children[child_idx];
    if (child->m_id != KaxSeekHead::ClassInfos.GlobalId)
      continue;

    in.setFilePointer(child->m_pos, seek_beginning);
    upper_lvl_el   = 0;
    EbmlElement *l1 = es.FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true, 1);
    if (NULL == l1)
      continue;

    if (!is_id(l1, KaxSeekHead)) {
      delete l1;
      continue;
    }

    EbmlMaster *m   = static_cast<EbmlMaster *>(l1);
    EbmlElement *l2 = NULL;
    m->Read(es, l1->Generic().Context, upper_lvl_el, l2, true);

    int master_idx;
    for (master_idx = 0; m->ListSize() > master_idx; master_idx++) {
      KaxSeek *seek = dynamic_cast<KaxSeek *>((*m)[master_idx]);
      if (NULL == seek)
        continue;

      KaxSeekID *seek_id = FINDFIRST(seek, KaxSeekID);
      int64_t pos        = seek->Location() + l0->GetElementPosition() + l0->HeadSize();

      if ((0 == pos) || (NULL == seek_id))
        continue;

      bool found = false;
      std::vector<segment_child_t>::const_iterator it;
      mxforeach(it, m_children)
        if ((*it).m_pos == pos) {
          found = true;
          break;
        }

      if (!found)
        m_children.push_back(segment_child_t(pos, -1, EbmlId(seek_id->GetBuffer(), seek_id->GetSize())));
    }
    delete l1;
  }

  delete l0;

  m_current_child = m_children.begin();
}

int
kax_quickparser_c::num_elements(const EbmlId &id)
  const {
  std::vector<segment_child_t>::const_iterator it;
  int num = 0;

  mxforeach(it, m_children)
    if ((*it).m_id == id)
      num++;

  return num;
}

segment_child_t *
kax_quickparser_c::get_next(const EbmlId &id) {
  while (m_current_child != m_children.end())
    if ((*m_current_child).m_id == id) {
      segment_child_t *ptr = &(*m_current_child);
      m_current_child++;
      return ptr;
    } else
      m_current_child++;

  return NULL;
}

void
kax_quickparser_c::reset() {
  m_current_child = m_children.begin();
}

EbmlMaster *
kax_quickparser_c::read_all(const EbmlCallbacks &callbacks) {
  segment_child_t *child;
  EbmlMaster *m = NULL;
  EbmlStream es(m_in);

  reset();

  for (child = get_next(callbacks.GlobalId); child != NULL; child = get_next(callbacks.GlobalId)) {
    m_in.setFilePointer(child->m_pos);
    int upper_lvl_el = 0;
    EbmlElement *e   = es.FindNextElement(KaxSegment::ClassInfos.Context, upper_lvl_el, 0xFFFFFFFFL, true);
    if (NULL == e)
      continue;

    if (!(e->Generic().GlobalId == callbacks.GlobalId)) {
      delete e;
      continue;
    }

    EbmlElement *l2 = NULL;
    e->Read(es, callbacks.Context, upper_lvl_el, l2, true);
    if (NULL == m)
      m = static_cast<EbmlMaster *>(e);
    else {
      EbmlMaster *src = static_cast<EbmlMaster *>(e);
      while (src->ListSize() > 0) {
        m->PushElement(*(*src)[0]);
        src->Remove(0);
      }
      delete e;
    }
  }

  reset();

  if ((NULL != m) && (m->ListSize() == 0)) {
    delete m;
    return NULL;
  }

  return m;
}
