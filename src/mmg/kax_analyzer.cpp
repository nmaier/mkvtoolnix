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

    mxinfo(boost::format("%1%: %2% size %3% at %4%\n") % i % name % data[i]->size % data[i]->pos);
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
    merge_void_elements();

  } catch (kax_analyzer_c::update_element_result_e result) {
    return result;
  }

  return uer_success;
}

/** \brief Sets the segment size to the length of the file
 */
void
kax_analyzer_c::adjust_segment_size() {
  KaxSegment *new_segment = new KaxSegment;
  file->setFilePointer(segment->GetElementPosition());
  new_segment->WriteHead(*file, segment->HeadSize() - 4);

  file->setFilePointer(0, seek_end);
  if (!new_segment->ForceSize(file->getFilePointer() - segment->HeadSize() - segment->GetElementPosition())) {
    segment->OverwriteHead(*file);
    delete new_segment;
    throw uer_error_segment_size_for_element;
  }

  new_segment->OverwriteHead(*file);
  delete segment;
  segment = new_segment;
}

/** \brief Create an EbmlVoid element at a specific location

    This function creates an EbmlVoid element at the position \c
    file_pos with the size \c void_size. If \c void_size is not big
    enough to contain an EbmlVoid element then the space is
    overwritten with zero bytes.

    The \c data member structure is also updated to reflect the
    changes made to the file. If \c add_new_data_element is \c true
    and a void element was actually written then a new element will be
    added to \c data at position \c data_idx.

    If \c add_new_data_element is \c false and a void element could
    not be written then the element at \c data_idx is removed from \c
    data. Otherwise the element at position \c data_idx is updated.

    \param file_pos The position in the file for the new void element.
    \param void_size The size of the new void element.
    \param data_idx Index into the \c data structure where a new data
      element will be added or which data element to update.
    \param add_new_data_element If \c true then a new data element will
      be added to \c data at position \c data_idx. Otherwise the element
      at position \c data_idx should be updated.

    \return \c true if a new void element was created and \c false if
      there was not enough space for it.
 */
bool
kax_analyzer_c::create_void_element(int64_t file_pos,
                                    int void_size,
                                    int data_idx,
                                    bool add_new_data_element) {
  // See if enough space was freed to fit an EbmlVoid element in.
  file->setFilePointer(file_pos);

  if (2 > void_size) {
    // No, so just write zero bytes.
    unsigned char zero = 0;
    file->write(&zero, void_size);

    if (!add_new_data_element)
      data.erase(data.begin() + data_idx, data.begin() + data_idx + 1);

    return false;
  }

  // Yes. Write a new EbmlVoid element and update the internal records.
  EbmlVoid evoid;
  evoid.SetSize(void_size);
  evoid.UpdateSize();
  evoid.SetSize(void_size - evoid.HeadSize());
  evoid.Render(*file);

  if (add_new_data_element)
    data.insert(data.begin() + data_idx, new analyzer_data_c(EbmlVoid::ClassInfos.GlobalId, evoid.GetElementPosition(), void_size));

  else {
    data[data_idx]->id   = EbmlVoid::ClassInfos.GlobalId;
    data[data_idx]->pos  = evoid.GetElementPosition();
    data[data_idx]->size = void_size;
  }

  return true;
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

    // Create a void element to cover the freed space.
    if (create_void_element(data[data_idx]->pos + new_size, old_size - new_size, data_idx + 1, true))
      ++data_idx;

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

    // Overwrite with a void element.
    create_void_element(data[data_idx]->pos, data[data_idx]->size, data_idx, false);
  }
}

/** \brief Merges consecutive EbmlVoid elements into a single one

    Iterates over the level 1 elements in the file and merges
    consecutive EbmlVoid elements into a single one which covers
    the same space as the smaller ones combined.

    Void elements at the end of the file are removed as well.
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

  // See how many void elements there are at the end of the file.
  start_idx = data.size() - 1;

  while ((0 <= start_idx) && (EbmlVoid::ClassInfos.GlobalId == data[start_idx]->id))
    --start_idx;
  ++start_idx;

  // If there are none then we're done.
  if (data.size() <= start_idx)
    return;

  // Truncate the file after the last non-void element and update the segment size.
  file->truncate(data[start_idx]->pos);
  adjust_segment_size();
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

    // Update the internal records.
    data[data_idx]->id = e->Generic().GlobalId;

    // Create a new void element after the element we've just written.
    create_void_element(data[data_idx]->pos + element_size, data[data_idx]->size - element_size, data_idx + 1, true);

    // We're done.
    return;
  }

  // We haven't found a suitable place. So store the element at the end of the file
  // and update the internal records.
  file->setFilePointer(0, seek_end);
  e->Render(*file, write_defaults);
  data.push_back(new analyzer_data_c(e->Generic().GlobalId, file->getFilePointer() - e->ElementSize(write_defaults), e->ElementSize(write_defaults)));

  // Adjust the segment's size.
  adjust_segment_size();
}

/** \brief Adds an element to one of the meta seek entries

    This function iterates over all meta seek elements and looks
    for one that has enough space (via following EbmlVoid elements or
    because it is located at the end of the file) for indexing
    the element \c e.

    If no such element is found then a new meta seek element is
    created at an appropriate place, and that element is indexed.

    \param e Pointer to the element to index.
 */
void
kax_analyzer_c::add_to_meta_seek(EbmlElement *e) {
  int data_idx, first_seek_head_idx = -1;

  for (data_idx = 0; data.size() > data_idx; ++data_idx) {
    // We only have to do work on SeekHead elements. Skip the others.
    if (data[data_idx]->id != KaxSeekHead::ClassInfos.GlobalId)
      continue;

    // Calculate how much free space there is behind the seek head.
    // merge_void_elemens() guarantees that there is no EbmlVoid element
    // at the end of the file and that all consecutive EbmlVoid elements
    // have been merged into a single element.
    int available_space = data[data_idx]->size;
    if (((data_idx + 1) < data.size()) && (data[data_idx + 1]->id == EbmlVoid::ClassInfos.GlobalId))
      available_space += data[data_idx + 1]->size;

    // Read the seek head, index the element and see how much space it needs.
    EbmlElement *element   = read_element(data_idx, KaxSeekHead::ClassInfos);
    KaxSeekHead *seek_head = dynamic_cast<KaxSeekHead *>(element);
    if (NULL == seek_head)
      throw uer_error_unknown;

    if (-1 == first_seek_head_idx)
      first_seek_head_idx = data_idx;

    seek_head->IndexThis(*e, *segment);
    seek_head->UpdateSize(true);

    // We can use this seek head if it is at the end of the file, or if there
    // is enough space behind it in form of void elements.
    if ((data.size() != (data_idx + 1)) && (seek_head->ElementSize(true) > available_space)) {
      delete seek_head;
      continue;
    }

    // Write the seek head.
    file->setFilePointer(data[data_idx]->pos);
    seek_head->Render(*file, true);

    // If this seek head is located at the end of the file then we have
    // to adjust the segment size.
    if (data.size() == (data_idx + 1))
      adjust_segment_size();

    else
      // Otherwise adjust the following EbmlVoid element: decrease its size.
      create_void_element(data[data_idx]->pos + seek_head->ElementSize(true),
                          data[data_idx]->size + data[data_idx + 1]->size - seek_head->ElementSize(true),
                          data_idx + 1, false);

    // Update the internal record.
    data[data_idx]->size = seek_head->ElementSize(true);

    delete seek_head;

    // We're done.
    return;
  }

  // No suitable meta seek head found -- we have to write a new one.

  // If we have found a prior seek head then we copy that one to the end
  // of the file including the newly indexed element and write a one-element
  // seek head at the first meta seek's position pointing to the one at the
  // end.
  if (-1 != first_seek_head_idx) {
    // Read the first seek head...
    EbmlElement *element   = read_element(first_seek_head_idx, KaxSeekHead::ClassInfos);
    KaxSeekHead *seek_head = dynamic_cast<KaxSeekHead *>(element);
    if (NULL == seek_head)
      throw uer_error_unknown;

    // ...index our element...
    seek_head->IndexThis(*e, *segment);
    seek_head->UpdateSize(true);

    // ...write the seek head at the end of the file...
    file->setFilePointer(0, seek_end);
    seek_head->Render(*file, true);

    // ...and update the internal records.
    data.push_back(new analyzer_data_c(KaxSeekHead::ClassInfos.GlobalId, seek_head->GetElementPosition(), seek_head->ElementSize(true)));

    // Update the segment size.
    adjust_segment_size();

    // Create a new seek head and write it to the file.
    KaxSeekHead *forward_seek_head = new KaxSeekHead;
    forward_seek_head->IndexThis(*seek_head, *segment);
    forward_seek_head->UpdateSize(true);

    file->setFilePointer(data[first_seek_head_idx]->pos);
    forward_seek_head->Render(*file, true);

    // Update the internal record to reflect that there's a new seek head.
    int void_size = data[first_seek_head_idx]->size - forward_seek_head->ElementSize(true);
    data[first_seek_head_idx]->size = forward_seek_head->ElementSize(true);

    // Create a void element behind the small new first seek head.
    create_void_element(data[first_seek_head_idx]->pos + data[first_seek_head_idx]->size, void_size, first_seek_head_idx + 1, true);

    // We're done.
    delete forward_seek_head;
    delete seek_head;

    return;
  }

  // We don't have a seek head to copy. Create one before the first chapter if possible.
  KaxSeekHead *new_seek_head = new KaxSeekHead;
  new_seek_head->IndexThis(*e, *segment);
  new_seek_head->UpdateSize(true);

  for (data_idx = 0; data.size() > data_idx; ++data_idx) {
    // We can only overwrite void elements. Skip the others.
    if (data[data_idx]->id != EbmlVoid::ClassInfos.GlobalId)
      continue;

    // Skip the element if it doesn't offer enough space for the seek head.
    if (data[data_idx]->size < new_seek_head->ElementSize(true))
      continue;

    // We've found a suitable spot. Write the seek head.
    file->setFilePointer(data[data_idx]->pos);
    new_seek_head->Render(*file, true);

    // Write a void element after the newly written seek head in order to
    // cover the space previously occupied by the old void element.
    create_void_element(data[data_idx]->pos + new_seek_head->ElementSize(true), data[data_idx]->size - new_seek_head->ElementSize(true), data_idx + 1, true);

    // Adjust the internal records for the new seek head.
    data[data_idx]->size = new_seek_head->ElementSize(true);
    data[data_idx]->id   = KaxSeekHead::ClassInfos.GlobalId;

    // We're done.
    delete new_seek_head;

    return;
  }

  delete new_seek_head;

  // We cannot write a seek head before the first cluster. This is not supported at the moment.
  throw uer_error_not_indexable;
}
