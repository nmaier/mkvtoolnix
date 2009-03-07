/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Matroska file analyzer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

// The Debian g++ 3.3.1 has problems in its standard C++ headers with min
// being defined differently. So just include these files now when min
// has not been defined yet.
#if __GNUC__ != 2
#include <limits>
#endif
#include <iostream>

#include "os.h"

#include <wx/progdlg.h>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxSeekHead.h>

#include "commonebml.h"
#include "kax_analyzer.h"
#include "mmg.h"

using namespace std;
using namespace libebml;

#define in_parent(p) (file->getFilePointer() < (p->GetElementPosition() + p->ElementSize()))

kax_analyzer_c::kax_analyzer_c(wxWindow *parent,
                               string name)
  : m_parent(parent)
  , file_name(name)
  , segment(NULL)
{
  try {
    file = new mm_file_io_c(file_name.c_str(), MODE_WRITE);
  } catch (...) {
    throw error_c(boost::format(Y("Could not open the file '%1%'.")) % file_name);
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

void
kax_analyzer_c::debug_dump_elements() {
  int i;
  for (i = 0; i < data.size(); i++) {
    const EbmlCallbacks *callbacks = find_ebml_callbacks(KaxSegment::ClassInfos, data[i]->id);
    std::string name;

    if ((NULL == callbacks) && (EbmlVoid::ClassInfos.GlobalId == data[i]->id))
      callbacks = &EbmlVoid::ClassInfos;

    if (NULL != callbacks)
      name = callbacks->DebugName;
    else {
      std::string format = (boost::format("0x%%|0%1%x|") % (data[i]->id.Length * 2)).str();
      name               = (boost::format(format)        %  data[i]->id.Value).str();
    }

    mxinfo(boost::format("%1%: %2% size %3% at %4%\n") % i % name % data[i]->pos % data[i]->size);
  }
}

bool
kax_analyzer_c::probe(string file_name) {
  try {
    unsigned char data[4];
    mm_file_io_c in(file_name.c_str());

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

bool
kax_analyzer_c::process() {
  segment = NULL;

  file->setFilePointer(0);
  EbmlStream es(*file);

  unsigned int i;
  for (i = 0; i < data.size(); i++)
    delete data[i];
  data.clear();

  // Find the EbmlHead element. Must be the first one.
  EbmlElement *l0 = es.FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
  if (l0 == NULL)
    throw error_c(Y("Not a valid Matroska file (no EBML head found)"));

  // Don't verify its data for now.
  l0->SkipData(es, l0->Generic().Context);
  delete l0;

  while (1) {
    // Next element must be a segment
    l0 = es.FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (l0 == NULL)
      throw error_c(Y("Not a valid Matroska file (no segment/level 0 element found)"));
    if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)
      break;
    l0->SkipData(es, l0->Generic().Context);
    delete l0;
  }

  wxProgressDialog progdlg(Z("Analysis is running"), Z("The Matroska file is analyzed."), 100, m_parent, wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_REMAINING_TIME);

  segment           = static_cast<KaxSegment *>(l0);
  int64_t file_size = file->get_size();
  int upper_lvl_el  = 0;
  bool cont         = true;

  // We've got our segment, so let's find all level 1 elements.
  EbmlElement *l1 = es.FindNextElement(segment->Generic().Context, upper_lvl_el, 0xFFFFFFFFFFFFFFFFLL, true, 1);
  while ((l1 != NULL) && (upper_lvl_el <= 0)) {
    data.push_back(new analyzer_data_c(l1->Generic().GlobalId, l1->GetElementPosition(), l1->ElementSize(true)));
    while (app->Pending())
      app->Dispatch();

    l1->SkipData(es, l1->Generic().Context);
    delete l1;
    l1 = NULL;

    if (!in_parent(segment) || !(cont = progdlg.Update((int)(file->getFilePointer() * 100 / file_size))))
      break;

    l1 = es.FindNextElement(segment->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);
  } // while (l1 != NULL)

  if (l1 != NULL)
    delete l1;

  if (cont)
    return true;

  delete segment;
  segment = NULL;
  for (i = 0; i < data.size(); i++)
    delete data[i];
  data.clear();
  return false;
}

EbmlElement *
kax_analyzer_c::read_element(analyzer_data_c *element_data,
                             const EbmlCallbacks &callbacks) {
  EbmlElement *e;
  int upper_lvl_el;

  upper_lvl_el = 0;
  file->setFilePointer(element_data->pos);
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

void
kax_analyzer_c::overwrite_elements(EbmlElement *e,
                                   int found_where,
                                   bool write_defaults) {
  vector<analyzer_data_c *>::iterator dit;
  string info;
  int64_t pos, size;
  uint32_t i, k;

  // 1. Overwrite the original element.
  info += (boost::format(Y("Found a suitable place at %1%.\n")) % data[found_where]->pos).str();
  file->setFilePointer(data[found_where]->pos);
  e->Render(*file, write_defaults);

  if (e->ElementSize(write_defaults) > data[found_where]->size) {
    int64_t new_size;
    EbmlVoid evoid;

    // 2a. Adjust any following EbmlVoid element if necessary.
    pos = file->getFilePointer();
    for (i = found_where + 1; pos >= (data[i]->pos + data[i]->size); i++) {
      data[i]->delete_this = true;
      info += (boost::format(Y("Deleting internal element %1% at %2% with size %3%.\n")) % i % data[i]->pos % data[i]->size).str();
    }
    new_size = (data[i]->pos + data[i]->size) - file->getFilePointer();
    info += (boost::format(Y("Adjusting EbmlVoid element %1% at %2% with size %3% to new pos %4% with new size %5%.\n"))
             % i % data[i]->pos % data[i]->size % file->getFilePointer() % new_size).str();
    data[i]->pos = file->getFilePointer();
    data[i]->size = new_size;
    evoid.SetSize(new_size);
    evoid.UpdateSize();
    evoid.SetSize(new_size - evoid.HeadSize());
    evoid.Render(*file);

  } else if (e->ElementSize(write_defaults) < data[found_where]->size) {
    // 2b. Insert a new EbmlVoid element.
    if ((data[found_where]->size - e->ElementSize(write_defaults)) < 5) {
      char zero[5] = {0, 0, 0, 0, 0};
      file->write(zero, data[found_where]->size - e->ElementSize(write_defaults));
      info += Y("Inserting new EbmlVoid not possible, remaining size too small.\n");
    } else {
      EbmlVoid evoid;
      info += (boost::format(Y("Inserting new EbmlVoid element at %1% with size %2%.\n")) % file->getFilePointer() % (data[found_where]->size - e->ElementSize(write_defaults))).str();
      evoid.SetSize(data[found_where]->size - e->ElementSize(write_defaults));
      evoid.UpdateSize();
      evoid.SetSize(data[found_where]->size - e->ElementSize(write_defaults) - evoid.HeadSize());
      evoid.Render(*file);
      dit = data.begin();
      dit += found_where + 1;
      data.insert(dit, new analyzer_data_c(evoid.Generic().GlobalId, evoid.GetElementPosition(), evoid.ElementSize()));
    }

  } else {
    info += Y("Great! Exact size. Just overwriting :)\n");
    file->setFilePointer(data[found_where]->pos);
    e->Render(*file, write_defaults);
  }
  data[found_where]->size = e->ElementSize(write_defaults);
  data[found_where]->id = e->Generic().GlobalId;

  // Glue EbmlVoid elements.
  i = 0;
  while (i < data.size()) {
    if (data[i]->id == EbmlVoid::ClassInfos.GlobalId) {
      EbmlVoid evoid;
      size = data[i]->size;
      for (k = i + 1; ((k < data.size()) &&
                       (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
        size += data[k]->size;
      if (size > data[i]->size) {
        evoid.SetSize(size);
        evoid.SetSize(size - evoid.HeadSize());
        file->setFilePointer(data[i]->pos);
        evoid.Render(*file);
        data[i]->size = size;
        while (((i + 1) < data.size()) &&
               (data[i + 1]->id == EbmlVoid::ClassInfos.GlobalId))
          data.erase(data.begin() + i + 1);
      }
    }
    i++;
  }

//   mxinfo("overwrite_elements: %s", info.c_str());
}

kax_analyzer_c::update_element_result_e
kax_analyzer_c::update_element(EbmlElement *e,
                               bool write_defaults) {
  try {
    overwrite_all_instances(e->Generic().GlobalId);
    merge_void_elements();
    write_element(e, write_defaults);
    remove_from_meta_seeks(e->Generic().GlobalId);
    merge_void_elements();
    add_to_meta_seek(e);

  } catch (kax_analyzer_c::update_element_result_e result) {
    return result;
  }

  return uer_success;
}

kax_analyzer_c::update_element_result_e
kax_analyzer_c::old_update_element(EbmlElement *e,
                                   bool write_defaults) {
  uint32_t i, k, found_where, found_what;
  int64_t space_here, pos;
  vector<KaxSeekHead *> all_heads;
  vector<int64_t> free_space;
  KaxSegment *new_segment;
  KaxSeekHead *new_head;
  EbmlElement *new_e;
  string info;

  if (e == NULL)
    return uer_error_unknown;

  found_where = 0;
  for (i = 0; i < data.size(); i++)
    data[i]->delete_this = false;

  try {
//     mxinfo(boost::format("INFO 0:\n"));
//     dump_analyzer_data(data);

    found_what = 0;
    for (i = 0; i < data.size(); i++)
      if (data[i]->id == EbmlId(*e)) {
        space_here = data[i]->size;
        for (k = i + 1; ((k < data.size()) &&
                         (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
          space_here += data[k]->size;
        if (space_here >= e->ElementSize(write_defaults)) {
          found_what = 1;
          found_where = i;
          break;
        }
      }

    if (!found_what) {
      for (i = 0; i < data.size(); i++) {
        space_here = 0;
        for (k = i; ((k < data.size()) &&
                     (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
          space_here += data[k]->size;
        if (space_here >= e->ElementSize(write_defaults)) {
          found_what = 2;
          found_where = i;
          break;
        }
      }
    }

    if (!found_what) {
      info += Y("No suitable place found. Appending at the end.\n");
      // 1. Append e to the end of the file.
      file->setFilePointer(0, seek_end);
      e->Render(*file, write_defaults);

      // 2. Ajust the segment size.
      new_segment = new KaxSegment;
      file->setFilePointer(segment->GetElementPosition());
      new_segment->WriteHead(*file, segment->HeadSize() - 4);
      file->setFilePointer(0, seek_end);
      if (!new_segment->ForceSize(file->getFilePointer() -
                                  segment->HeadSize() -
                                  segment->GetElementPosition())) {
        segment->OverwriteHead(*file);
        delete new_segment;
        return uer_error_segment_size_for_element;
      }
      new_segment->OverwriteHead(*file);
      delete segment;
      segment = new_segment;

      // 3. Overwrite the original element(s) if any were found.
      for (i = 0; i < data.size(); i++)
        if (data[i]->id == EbmlId(*e)) {
          EbmlVoid evoid;
          file->setFilePointer(data[i]->pos);
          info += (boost::format(Y("Overwriting/voiding element at %1%.\n")) % data[i]->pos).str();
          evoid.SetSize(data[i]->size);
          evoid.UpdateSize();
          evoid.SetSize(data[i]->size - evoid.HeadSize());
          evoid.Render(*file);
          data[i]->id = EbmlVoid::ClassInfos.GlobalId;
        }
      data.push_back(new analyzer_data_c(e->Generic().GlobalId, e->GetElementPosition(), e->ElementSize(write_defaults)));
      found_where = data.size() - 1;

    } else
      overwrite_elements(e, found_where, write_defaults);

    // Remove the internal elements. Avoids re-analyzing the file.
    i = 0;
    while (data.size() > i) {
      if (data[i]->delete_this) {
        delete data[i];
        data.erase(data.begin() + i);
      } else
        ++i;
    }

//     mxinfo(boost::format("INFO 1:\n%1%\n") % info);
//     dump_analyzer_data(data);

    // 1. Find all seek heads.
    free_space.clear();
    for (i = 0; i < data.size(); i++) {
      if (data[i]->id == KaxSeekHead::ClassInfos.GlobalId) {
        new_e = read_element(i, KaxSeekHead::ClassInfos);
        if (new_e != NULL) {
          all_heads.push_back(static_cast<KaxSeekHead *>(new_e));
          space_here = data[i]->size;
          for (k = i + 1; ((k < data.size()) && (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
            space_here += data[k]->size;
          free_space.push_back(space_here);
        }
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
          if ((sid != NULL) && (spos != NULL) && (segment->GetGlobalPosition(uint64(*spos)) == data[found_where]->pos)) {
            EbmlId this_id(sid->GetBuffer(), sid->GetSize());
            if (this_id == EbmlId(*e))
              throw uer_success;
          }
        }
      }
    }

    // 3. If none found: Look for a seek head that has enough space available
    //    (maybe with EbmlVoids) to contain another entry.
    found_what = 0;
    for (i = 0; i < all_heads.size(); i++) {
      KaxSeekHead *head;
      head = all_heads[i];
      head->IndexThis(*e, *segment);
      head->UpdateSize();
      if (head->ElementSize() <= free_space[i]) {
        for (k = 0; k < data.size(); k++)
          if (data[k]->pos == all_heads[i]->GetElementPosition()) {
            found_what = i + 1;
            found_where = k;
            break;
          }
        if (found_what == 0)
          mxerror(Y("found_what == 0. Should not have happened. Please file a bug report.\n"));
        break;
      } else {
        delete (*head)[head->ListSize() - 1];
        head->Remove(head->ListSize() - 1);
      }
    }

    if (found_what) {
      overwrite_elements(all_heads[found_what - 1], found_where, write_defaults);
      throw uer_success;
    }

    // 4. If none found: Look for the first seek head. Link to a new seek head
    //    entry at the end of the file which will contain all the entries the
    //    original seek head contained + the link to the updated element.
    //    Ajust segment size.
    if (all_heads.size() > 0) {
      info += Y("Appending new seek head.\n");
      // Append the new seek head to the end of the file.
      pos = all_heads[0]->GetElementPosition();
      new_head = new KaxSeekHead;
      file->setFilePointer(0, seek_end);
      all_heads[0]->IndexThis(*e, *segment);
      all_heads[0]->UpdateSize();
      all_heads[0]->Render(*file);
      data.push_back(new analyzer_data_c(all_heads[0]->Generic().GlobalId, all_heads[0]->GetElementPosition(), all_heads[0]->ElementSize()));

      // Adjust the segment size.
      info += Y("Adjusting segment size.\n");
      new_segment = new KaxSegment;
      file->setFilePointer(segment->GetElementPosition());
      new_segment->WriteHead(*file, segment->HeadSize() - 4);
      file->setFilePointer(0, seek_end);
      if (!new_segment->ForceSize(file->getFilePointer() -
                                  segment->HeadSize() -
                                  segment->GetElementPosition())) {
        segment->OverwriteHead(*file);
        delete new_segment;
        throw uer_error_segment_size_for_meta_seek;
      }
      new_segment->OverwriteHead(*file);
      delete segment;
      segment = new_segment;

      // Create a new seek head to replace the old one.
      info += Y("Creating new seek head.\n");
      new_head->IndexThis(*all_heads[0], *segment);
      found_what = 0;
      for (i = 0; i < data.size(); i++)
        if (data[i]->pos == pos) {
          found_what = 1;
          found_where = i;
        }
      if (!found_what)
        mxerror(Y("found_what == 0, 2nd time. Should not have happened. Please file a bug report.\n"));
      overwrite_elements(new_head, found_where, write_defaults);

      all_heads.push_back(all_heads[0]);
      all_heads.erase(all_heads.begin());
      all_heads.insert(all_heads.begin(), new_head);

//       mxinfo(boost::format("INFO 2:\n%1%\n") % info);
//       dump_analyzer_data(data);

      throw uer_success;
    }

    // 5. No seek head found at all: Search for enough space to create a
    //    seek head before the first cluster.
    new_head = new KaxSeekHead;
    new_head->IndexThis(*e, *segment);
    new_head->UpdateSize();
    found_what = 0;
    for (i = 0; ((i < data.size()) && !(data[i]->id == KaxCluster::ClassInfos.GlobalId)); i++) {
      space_here = 0;
      for (k = i; ((k < data.size()) && (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
        space_here += data[k]->size;
      if (space_here >= new_head->ElementSize()) {
        found_what = 1;
        found_where = i;
        break;
      }
    }
    if (found_what) {
      all_heads.insert(all_heads.begin(), new_head);
      overwrite_elements(new_head, found_where, write_defaults);
//       dump_analyzer_data(data);

      throw uer_success;
    }

    delete new_head;

    // 6. If no seek head found at all: Issue a warning that the element might
    //    not be found by players.
    throw uer_error_meta_seek;

  } catch (update_element_result_e result) {
    for (i = 0; i < all_heads.size(); i++)
      delete all_heads[i];
    all_heads.clear();

//     mxinfo("DUMP after work:\n");
//     dump_analyzer_data(data);
//     process();
//     mxinfo("DUMP after re-process()ing:\n");
//     dump_analyzer_data(data);

    return result;
  }

  for (i = 0; i < all_heads.size(); i++)
    delete all_heads[i];
  all_heads.clear();

  return uer_error_unknown;
}

/** \brief Removes all seek entries for a specific element

    Iterates over the level 1 elements in the file and reads each seek
    head it finds. All entries for the given \c id are removed from
    the seek head. If the seek head has been changed then it is
    rewritten to its original position. The space freed up is filled
    with a new EbmlVoid element.

    \param id The ID of the elements that should be removed.
 */
void
kax_analyzer_c::remove_from_meta_seeks(EbmlId id) {
  int data_idx;

  for (data_idx = 0; data.size() > data_idx; ++data_idx) {
    // We only have to do work on SeekHead elements. Skip the others.
    if (data[data_idx]->id != KaxSeekHead::ClassInfos.GlobalId)
      continue;

    // Read the element from the file. Remember its size so that a new
    // EbmlVoid element can be constructed afterwards.
    EbmlElement *element   = read_element(data_idx, KaxSeekHead::ClassInfos);
    KaxSeekHead *seek_head = dynamic_cast<KaxSeekHead *>(element);
    if (NULL == seek_head)
      throw uer_error_unknown;

    int64_t old_size = seek_head->ElementSize(true);

    // Iterate over its children and delete the ones we're looking for.
    bool modified = false;
    int sh_idx    = 0;
    while (seek_head->ListSize() > sh_idx) {
      if ((*seek_head)[sh_idx]->Generic().GlobalId != KaxSeek::ClassInfos.GlobalId) {
        ++sh_idx;
        continue;
      }

      KaxSeek *seek_entry = dynamic_cast<KaxSeek *>((*seek_head)[sh_idx]);

      if (!seek_entry->IsEbmlId(id)) {
        ++sh_idx;
        continue;
      }

      delete (*seek_head)[sh_idx];
      seek_head->Remove(sh_idx);

      modified = true;
    }

    // Only rewrite the element to the file if it has been modified.
    if (!modified) {
      delete element;
      continue;
    }

    // First make sure the new element is smaller than the old one.
    // The following code cannot deal with the other case.
    seek_head->UpdateSize(true);
    int64_t new_size = seek_head->ElementSize(true);
    if (new_size > old_size)
      throw uer_error_unknown;

    // Overwrite the element itself and update its internal record.
    file->setFilePointer(data[data_idx]->pos);
    seek_head->Render(*file, true);

    data[data_idx]->size = new_size;

    // See if enough space was freed to fit an EbmlVoid element in.
    file->setFilePointer(data[data_idx]->pos + new_size);
    int64_t size_difference = old_size - new_size;

    if (5 > size_difference) {
      // No, so just write zero bytes.
      static char zero[5] = {0, 0, 0, 0, 0};
      file->write(zero, size_difference);

    } else {
      // Yes. Write a new EbmlVoid element and update the internal records.
      EbmlVoid evoid;
      evoid.SetSize(size_difference);
      evoid.UpdateSize();
      evoid.SetSize(size_difference - evoid.HeadSize());
      evoid.Render(*file);

      data.insert(data.begin() + data_idx + 1, new analyzer_data_c(evoid.Generic().GlobalId, evoid.GetElementPosition(), size_difference));

      ++data_idx;
    }

    delete element;
  }
}

/** \brief Overwrites all instances of a specific level 1 element

    Iterates over the level 1 elements in the file and overwrites
    each instance of a specific level 1 element given by \c id.
    It is replaced with a new EbmlVoid element.

    \param id The ID of the elements that should be overwritten.
 */
void
kax_analyzer_c::overwrite_all_instances(EbmlId id) {
  int data_idx;

  for (data_idx = 0; data.size() > data_idx; ++data_idx) {
    // We only have to do work on specific elements. Skip the others.
    if (data[data_idx]->id != id)
      continue;

    // Check that there's enough space for an EbmlVoid element.
    file->setFilePointer(data[data_idx]->pos);

    if (5 > data[data_idx]->size) {
      // No, so just write zero bytes.
      static char zero[5] = {0, 0, 0, 0, 0};
      file->write(zero, data[data_idx]->size);

    } else {
      // Yes. Write a new EbmlVoid element and update the internal records.
      EbmlVoid evoid;
      evoid.SetSize(data[data_idx]->size);
      evoid.UpdateSize();
      evoid.SetSize(data[data_idx]->size - evoid.HeadSize());
      evoid.Render(*file);

      data[data_idx]->id = EbmlVoid::ClassInfos.GlobalId;
    }
  }
}

/** \brief Merges consecutive EbmlVoid elements into a single one

    Iterates over the level 1 elements in the file and merges
    consecutive EbmlVoid elements into a single one which covers
    the same space as the smaller ones combined.
 */
void
kax_analyzer_c::merge_void_elements() {
  int start_idx = 0;

  while (data.size() > start_idx) {
    // We only have to do work on EbmlVoid elements. Skip the others.
    if (data[start_idx]->id != EbmlVoid::ClassInfos.GlobalId) {
      ++start_idx;
      continue;
    }

    // Found an EbmlVoid element. See how many consecutive EbmlVoid elements
    // there are at this position and calculate the combined size.
    int end_idx  = start_idx + 1;
    int new_size = data[start_idx]->size;
    while ((data.size() > end_idx) && (data[end_idx]->id == EbmlVoid::ClassInfos.GlobalId)) {
      new_size += data[end_idx]->size;
      ++end_idx;
    }

    // Is this only a single EbmlVoid element? If yes continue.
    if (end_idx == (start_idx + 1)) {
      start_idx += 2;
      continue;
    }

    // Write the new EbmlVoid element to the file.
    file->setFilePointer(data[start_idx]->pos);

    EbmlVoid evoid;
    evoid.SetSize(new_size);
    evoid.UpdateSize();
    evoid.SetSize(new_size - evoid.HeadSize());
    evoid.Render(*file);

    // Update the internal records to reflect the changes.
    data[start_idx]->size = new_size;
    data.erase(data.begin() + start_idx + 1, data.begin() + end_idx);

    start_idx += 2;
  }

  mxinfo("--[ merge_void_elements ]---------------\n");
  debug_dump_elements();
}

/** \brief Finds a suitable spot for an element and writes it to the file

    First, a suitable spot for the element is determined by looking at
    EbmlVoid elements. If none is found in the middle of the file then
    the element will be appended at the end.

    Second, the element is written at the location determined in the
    first step. If EbmlVoid elements are overwritten then a new,
    smaller one is created which covers the remainder of the
    overwritten one.

    Third, the internal records are updated to reflect the changes.

    \param e Pointer to the element to write.
    \param write_defaults Boolean that decides whether or not elements
      which contain their default value are written to the file.
 */
void
kax_analyzer_c::write_element(EbmlElement *e,
                              bool write_defaults) {
  e->UpdateSize(write_defaults);
  int element_size = e->ElementSize(write_defaults);

  int data_idx;
  for (data_idx = 0; data.size() > data_idx; ++data_idx) {
    // We're only interested in EbmlVoid elements. Skip the others.
    if (data[data_idx]->id != EbmlVoid::ClassInfos.GlobalId)
      continue;

    // Skip the element if it doesn't provide enough space.
    if (data[data_idx]->size < element_size)
      continue;

    // We've found our element. Overwrite it.
    file->setFilePointer(data[data_idx]->pos);
    e->Render(*file, write_defaults);

    // Check if there's enough space left to add a new EbmlVoid
    // element after the element we've just written.
    file->setFilePointer(data[data_idx]->pos + element_size);

    int remaining_size = data[data_idx]->size - element_size;
    if (5 > remaining_size) {
      // No, so just write zero bytes.
      static char zero[5] = {0, 0, 0, 0, 0};
      file->write(zero, remaining_size);

    } else {
      // Yes. Write a new EbmlVoid element and update the internal records.
      EbmlVoid evoid;
      evoid.SetSize(remaining_size);
      evoid.UpdateSize();
      evoid.SetSize(remaining_size - evoid.HeadSize());
      evoid.Render(*file);

      data.insert(data.begin() + data_idx + 1, new analyzer_data_c(evoid.Generic().GlobalId, evoid.GetElementPosition(), remaining_size));
    }

    // Update the internal records.
    data[data_idx]->id = e->Generic().GlobalId;

    // We're done.
    return;
  }

  // We've not found a suitable place. So store the element at the end of the file.
  mxerror("booooooom!\n");
}

void
kax_analyzer_c::add_to_meta_seek(EbmlElement *e) {
}
