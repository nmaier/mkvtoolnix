/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Matroska file analyzer and updater

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTags.h>

#include "common/ebml.h"
#include "common/error.h"
#include "common/kax_analyzer.h"
#include "common/strings/editing.h"

using namespace libebml;
using namespace libmatroska;

#define in_parent(p) (!p->IsFiniteSize() || (m_file->getFilePointer() < (p->GetElementPosition() + p->ElementSize())))

#define CONSOLE_PERCENTAGE_WIDTH 25

bool
operator <(const kax_analyzer_data_cptr &d1,
           const kax_analyzer_data_cptr &d2) {
  return d1->m_pos < d2->m_pos;
}

std::string
kax_analyzer_data_c::to_string() const {
  const EbmlCallbacks *callbacks = find_ebml_callbacks(EBML_INFO(KaxSegment), m_id);

  if ((NULL == callbacks) && (EBML_ID(EbmlVoid) == m_id))
    callbacks = &EBML_CLASS_CALLBACK(EbmlVoid);

  std::string name;
  if (NULL != callbacks)
    name = EBML_INFO_NAME(*callbacks);

  else {
    std::string format = (boost::format("0x%%|0%1%x|") % (EBML_ID_LENGTH(m_id) * 2)).str();
    name               = (boost::format(format)        %  EBML_ID_VALUE(m_id)).str();
  }

  return (boost::format("%1% size %2% at %3%") % name % m_size % m_pos).str();
}

kax_analyzer_c::kax_analyzer_c(std::string file_name)
  : m_file_name(file_name)
  , m_file(NULL)
  , m_close_file(true)
  , m_stream(NULL)
  , m_debugging_requested(debugging_requested("kax_analyzer"))
{
}

kax_analyzer_c::kax_analyzer_c(mm_file_io_c *file)
  : m_file_name(file->get_file_name())
  , m_file(file)
  , m_close_file(false)
  , m_stream(NULL)
  , m_debugging_requested(debugging_requested("kax_analyzer"))
{
}

kax_analyzer_c::~kax_analyzer_c() {
  close_file();
}

void
kax_analyzer_c::close_file() {
  if (m_close_file) {
    delete m_file;
    m_file = NULL;

    delete m_stream;
    m_stream = NULL;
  }
}

void
kax_analyzer_c::reopen_file(const open_mode mode) {
  if (NULL != m_file)
    return;

  m_file   = new mm_file_io_c(m_file_name, mode);
  m_stream = new EbmlStream(*m_file);
}

void
kax_analyzer_c::_log_debug_message(const std::string &message) {
  mxinfo(message);
}

bool
kax_analyzer_c::analyzer_debugging_requested(const std::string &section) {
  return m_debugging_requested || debugging_requested(std::string("kax_analyzer_") + section);
}

void
kax_analyzer_c::debug_dump_elements() {
  size_t i;
  for (i = 0; i < m_data.size(); i++)
    log_debug_message(boost::format("%1%: %2%\n") % i % m_data[i]->to_string());
}

void
kax_analyzer_c::debug_dump_elements_maybe(const std::string &hook_name) {
  if (!analyzer_debugging_requested(hook_name))
    return;

  log_debug_message(boost::format("kax_analyzer_%1% dumping elements:\n") % hook_name);
  debug_dump_elements();
}

void
kax_analyzer_c::validate_data_structures(const std::string &hook_name) {
  bool gap_debugging = analyzer_debugging_requested("gaps");
  bool ok            = true;
  size_t i;

  for (i = 0; m_data.size() -1 > i; i++) {
    if ((m_data[i]->m_pos + m_data[i]->m_size) > m_data[i + 1]->m_pos) {
      log_debug_message(boost::format("kax_analyzer_%1%: Interal data structure corruption at pos %2% (size + position > next position); dumping elements\n") % hook_name % i);
      ok = false;
    } else if (gap_debugging && ((m_data[i]->m_pos + m_data[i]->m_size) < m_data[i + 1]->m_pos)) {
      log_debug_message(boost::format("kax_analyzer_%1%: Gap found at pos %2% (size + position < next position); dumping elements\n") % hook_name % i);
      ok = false;
    }
  }

  if (!ok) {
    debug_dump_elements();
    debug_abort_process();
  }
}

void
kax_analyzer_c::verify_data_structures_against_file(const std::string &hook_name) {
  kax_analyzer_c actual_content(m_file);
  actual_content.process();

  unsigned int num_items = std::max(m_data.size(), actual_content.m_data.size());
  bool ok                = m_data.size() == actual_content.m_data.size();
  size_t max_info_len    = 0;
  std::vector<std::string> info_this, info_actual, info_markings;
  size_t i;

  for (i = 0; num_items > i; ++i) {
    info_this.push_back(                 m_data.size() > i ?                m_data[i]->to_string() : empty_string);
    info_actual.push_back(actual_content.m_data.size() > i ? actual_content.m_data[i]->to_string() : empty_string);

    max_info_len           = std::max(max_info_len, info_this.back().length());

    bool row_is_identical  = info_this.back() == info_actual.back();
    ok                    &= row_is_identical;

    info_markings.push_back(row_is_identical ? " " : "*");
  }

  if (ok)
    return;

  log_debug_message(boost::format("verify_data_structures_against_file(%1%) failed. Dumping this on the left, actual on the right.\n") % hook_name);
  std::string format = (boost::format("%%1%% %%|2$-%1%s| %%3%%\n") % max_info_len).str();

  for (i = 0; num_items > i; ++i)
    log_debug_message(boost::format(format) % info_markings[i] % info_this[i] % info_actual[i]);

  debug_abort_process();
}

bool
kax_analyzer_c::probe(std::string file_name) {
  try {
    unsigned char data[4];
    mm_file_io_c in(file_name.c_str());

    if (in.read(data, 4) != 4)
      return false;

    return ((0x1a == data[0]) && (0x45 == data[1]) && (0xdf == data[2]) && (0xa3 == data[3]));
  } catch (...) {
    return false;
  }
}

bool
kax_analyzer_c::process(kax_analyzer_c::parse_mode_e parse_mode,
                        const open_mode mode) {
  bool parse_fully = parse_mode_full == parse_mode;

  try {
    reopen_file(mode);
  } catch (...) {
    return false;
  }

  int64_t file_size = m_file->get_size();
  show_progress_start(file_size);

  m_segment.clear();
  m_data.clear();

  m_file->setFilePointer(0);
  m_stream = new EbmlStream(*m_file);

  // Find the EbmlHead element. Must be the first one.
  EbmlElement *l0 = m_stream->FindNextID(EBML_INFO(EbmlHead), 0xFFFFFFFFL);
  if (NULL == l0)
    throw error_c(Y("Not a valid Matroska file (no EBML head found)"));

  // Don't verify its data for now.
  l0->SkipData(*m_stream, EBML_CONTEXT(l0));
  delete l0;

  while (1) {
    // Next element must be a segment
    l0 = m_stream->FindNextID(EBML_INFO(KaxSegment), 0xFFFFFFFFFFFFFFFFLL);
    if (NULL == l0)
      throw error_c(Y("Not a valid Matroska file (no segment/level 0 element found)"));

    if (EbmlId(*l0) == EBML_ID(KaxSegment))
      break;

    l0->SkipData(*m_stream, EBML_CONTEXT(l0));
    delete l0;
  }

  m_segment            = counted_ptr<KaxSegment>(static_cast<KaxSegment *>(l0));
  int upper_lvl_el     = 0;
  bool aborted         = false;
  bool cluster_found   = false;
  bool meta_seek_found = false;

  // We've got our segment, so let's find all level 1 elements.
  EbmlElement *l1 = m_stream->FindNextElement(EBML_CONTEXT(m_segment), upper_lvl_el, 0xFFFFFFFFFFFFFFFFLL, true, 1);
  while ((NULL != l1) && (0 >= upper_lvl_el)) {
    m_data.push_back(kax_analyzer_data_c::create(EbmlId(*l1), l1->GetElementPosition(), l1->ElementSize(true)));

    cluster_found   |= is_id(l1, KaxCluster);
    meta_seek_found |= is_id(l1, KaxSeekHead);

    l1->SkipData(*m_stream, EBML_CONTEXT(l1));
    delete l1;
    l1 = NULL;

    aborted = !show_progress_running((int)(m_file->getFilePointer() * 100 / file_size));

    if (!in_parent(m_segment) || aborted || (cluster_found && meta_seek_found && !parse_fully))
      break;

    l1 = m_stream->FindNextElement(EBML_CONTEXT(m_segment), upper_lvl_el, 0xFFFFFFFFL, true);
  } // while (l1 != NULL)

  if (NULL != l1)
    delete l1;

  if (!aborted && !parse_fully)
    read_all_meta_seeks();

  show_progress_done();

  if (!aborted) {
    if (parse_mode_full != parse_mode)
      fix_element_sizes(file_size);

    return true;
  }

  m_segment.clear();
  m_data.clear();

  return false;
}

EbmlElement *
kax_analyzer_c::read_element(kax_analyzer_data_c *element_data) {
  reopen_file();

  EbmlStream es(*m_file);
  m_file->setFilePointer(element_data->m_pos);

  int upper_lvl_el               = 0;
  EbmlElement *e                 = es.FindNextElement(EBML_CONTEXT(m_segment), upper_lvl_el, 0xFFFFFFFFL, true, 1);
  const EbmlCallbacks *callbacks = find_ebml_callbacks(EBML_INFO(KaxSegment), element_data->m_id);

  if ((NULL == e) || (NULL == callbacks) || (EbmlId(*e) != EBML_INFO_ID(*callbacks))) {
    delete e;
    return NULL;
  }

  upper_lvl_el = 0;
  e->Read(*m_stream, EBML_INFO_CONTEXT(*callbacks), upper_lvl_el, e, true);

  return e;
}

#define call_and_validate(function_call, hook_name)            \
  function_call;                                               \
  debug_dump_elements_maybe(hook_name);                        \
  validate_data_structures(hook_name);                         \
  if (analyzer_debugging_requested("verify"))                  \
    verify_data_structures_against_file(hook_name);            \
  if (debugging_requested("kax_analyzer_" hook_name "_break")) \
    return uer_success;

kax_analyzer_c::update_element_result_e
kax_analyzer_c::update_element(EbmlElement *e,
                               bool write_defaults) {
  reopen_file();

  fix_mandatory_elements(e);

  placement_strategy_e strategy = get_placement_strategy_for(e);

  try {
    call_and_validate({},                                         "update_element_0");
    call_and_validate(overwrite_all_instances(EbmlId(*e)),        "update_element_1");
    call_and_validate(merge_void_elements(),                      "update_element_2");
    call_and_validate(write_element(e, write_defaults, strategy), "update_element_3");
    call_and_validate(remove_from_meta_seeks(EbmlId(*e)),         "update_element_4");
    call_and_validate(merge_void_elements(),                      "update_element_5");
    call_and_validate(add_to_meta_seek(e),                        "update_element_6");
    call_and_validate(merge_void_elements(),                      "update_element_7");

  } catch (kax_analyzer_c::update_element_result_e result) {
    debug_dump_elements_maybe("update_element_exception");
    return result;
  }

  return uer_success;
}

kax_analyzer_c::update_element_result_e
kax_analyzer_c::remove_elements(EbmlId id) {
  reopen_file();

  try {
    call_and_validate({},                          "remove_elements_0");
    call_and_validate(overwrite_all_instances(id), "remove_elements_1");
    call_and_validate(merge_void_elements(),       "remove_elements_2");
    call_and_validate(remove_from_meta_seeks(id),  "remove_elements_3");
    call_and_validate(merge_void_elements(),       "remove_elements_4");

  } catch (kax_analyzer_c::update_element_result_e result) {
    debug_dump_elements_maybe("update_element_exception");
    return result;
  }

  return uer_success;
}

/** \brief Sets the m_segment size to the length of the m_file
 */
void
kax_analyzer_c::adjust_segment_size() {
  // If the old segment's size is unknown then don't try to force a
  // finite size as this will fail most of the time: an
  // infinite/unknown size is coded by the value 0 which is often
  // stored as a single byte (e.g. Haali's muxer does this).
  if (!m_segment->IsFiniteSize())
    return;

  counted_ptr<KaxSegment> new_segment = counted_ptr<KaxSegment>(new KaxSegment);
  m_file->setFilePointer(m_segment->GetElementPosition());
  new_segment->WriteHead(*m_file, m_segment->HeadSize() - 4);

  m_file->setFilePointer(0, seek_end);
  if (!new_segment->ForceSize(m_file->getFilePointer() - m_segment->HeadSize() - m_segment->GetElementPosition())) {
    m_segment->OverwriteHead(*m_file);
    throw uer_error_segment_size_for_element;
  }

  new_segment->OverwriteHead(*m_file);
  m_segment = new_segment;
}

/** \brief Create an EbmlVoid element at a specific location

    This function fills a gap in the file with an EbmlVoid. If an
    EbmlVoid element is located directly behind the gap then this
    element is overwritten as well.

    The function calculates the size of the new void element by taking
    the next non-EbmlVoid's position and subtracting from it the end
    position of the current element indicated by the \c data_idx
    parameter.

    If the space is not big enough to contain an EbmlVoid element then
    the EBML head of the following element is moved one byte to the
    front and its size field is extended by one byte. That way the
    file stays compatible with all parsers, and only a small number of
    bytes have to be moved around.

    The \c m_data member structure is also updated to reflect the
    changes made to the file.

    The function relies on \c m_data[data_idx] to be up to date
    regarding its size. If the size of \c m_data[data_idx] is zero
    then it is assumed that the element shall be overwritten with an
    EbmlVoid element, and \c m_data[data_idx] will be removed from the
    \c m_data structure.

    \param data_idx Index into the \c m_data structure pointing to the
     current element after which the gap is located.

    \return \c true if a new void element was created and \c false if
      there was no need to create one or if there was not enough
      space.
 */
bool
kax_analyzer_c::handle_void_elements(size_t data_idx) {
  // Is the element at the end of the file? If so truncate the file
  // and remove the element from the m_data structure if that was
  // requested. Then we're done.
  if (m_data.size() == (data_idx + 1)) {
    m_file->truncate(m_data[data_idx]->m_pos + m_data[data_idx]->m_size);
    adjust_segment_size();
    if (0 == m_data[data_idx]->m_size)
      m_data.erase(m_data.begin() + data_idx);
    return false;
  }

  // Are the following elements EbmlVoid elements?
  size_t end_idx = data_idx + 1;
  while ((m_data.size() > end_idx) && (m_data[end_idx]->m_id == EBML_ID(EbmlVoid)))
    ++end_idx;

  if (end_idx > data_idx + 1)
    // Yes, there is at least one. Remove these elements from the list
    // in order to create a new EbmlVoid element covering their space
    // as well.
    m_data.erase(m_data.begin() + data_idx + 1, m_data.begin() + end_idx);

  // Calculate how much space we have to cover with a void
  // element. This is the difference between the next element's
  // position and the current element's end.
  int64_t void_pos = m_data[data_idx]->m_pos + m_data[data_idx]->m_size;
  int void_size    = m_data[data_idx + 1]->m_pos - void_pos;

  // If the difference is 0 then we have nothing to do.
  if (0 == void_size)
    return false;

  // See if we have enough space to fit an EbmlVoid element in. An
  // EbmlVoid element needs at least two bytes (one for the ID, one
  // for the size).
  if (1 == void_size) {
    // No. The most compatible way to deal with this situation is to
    // move the element ID of the following element one byte to the
    // front and extend the following element's size field by one
    // byte.

    EbmlElement *e = read_element(m_data[data_idx + 1]);

    if (NULL == e)
      return false;

    // However, this might not work if the element's size was already
    // eight bytes long.
    if (8 == e->GetSizeLength()) {
      // In this case try doing the same with the previous
      // element. The whole element has be moved one byte to the back.
      delete e;

      e = read_element(m_data[data_idx]);
      if (NULL == e)
        return false;

      counted_ptr<EbmlElement> af_e(e);

      // Again the test for maximum size length.
      if (8 == e->GetSizeLength())
        return false;

      // Copy the content one byte to the back.
      unsigned int id_length = EBML_ID_LENGTH(static_cast<const EbmlId &>(*e));
      uint64_t content_pos   = m_data[data_idx]->m_pos + id_length + e->GetSizeLength();
      uint64_t content_size  = m_data[data_idx + 1]->m_pos - content_pos - 1;
      memory_cptr buffer     = memory_c::alloc(content_size);

      m_file->setFilePointer(content_pos);
      if (m_file->read(buffer, content_size) != content_size)
        return false;

      m_file->setFilePointer(content_pos + 1);
      if (m_file->write(buffer) != content_size)
        return false;

      // Prepare the new codec size and write it.
      binary head[8];           // Class D + 64 bits coded size
      int coded_size = CodedSizeLength(content_size, e->GetSizeLength() + 1, true);
      CodedValueLength(content_size, coded_size, head);
      m_file->setFilePointer(m_data[data_idx]->m_pos + id_length);
      if (m_file->write(head, coded_size) != static_cast<unsigned int>(coded_size))
        return false;

      // Update internal structures.
      m_data[data_idx]->m_size += 1;

      return true;
    }

    binary head[4 + 8];         // Class D + 64 bits coded size
    unsigned int head_size = EBML_ID_LENGTH(static_cast<const EbmlId &>(*e));
    EbmlId(*e).Fill(head);

    int coded_size = CodedSizeLength(e->GetSize(), e->GetSizeLength() + 1, true);
    CodedValueLength(e->GetSize(), coded_size, &head[head_size]);
    head_size += coded_size;

    delete e;

    m_file->setFilePointer(m_data[data_idx + 1]->m_pos - 1);
    m_file->write(head, head_size);

    --m_data[data_idx + 1]->m_pos;
    ++m_data[data_idx + 1]->m_size;

    // Update meta seek indices for m_data[data_idx]'s new position.
    e = read_element(m_data[data_idx + 1]);

    remove_from_meta_seeks(EbmlId(*e));
    merge_void_elements();
    add_to_meta_seek(e);
    merge_void_elements();

    delete e;

    return false;
  }

  m_file->setFilePointer(void_pos);

  // Yes. Write a new EbmlVoid element and update the internal records.
  EbmlVoid evoid;
  evoid.SetSize(void_size);
  evoid.UpdateSize();
  evoid.SetSize(void_size - evoid.HeadSize());
  evoid.Render(*m_file);

  m_data.insert(m_data.begin() + data_idx + 1, kax_analyzer_data_c::create(EBML_ID(EbmlVoid), void_pos, void_size));

  // Now check if we should overwrite the current element with the
  // EbmlVoid element. That is the case if the current element's size
  // is 0. In that case simply remove the element from the m_data
  // vector.
  if (0 == m_data[data_idx]->m_size)
    m_data.erase(m_data.begin() + data_idx);

  return true;
}

/** \brief Removes all seek entries for a specific element

    Iterates over the level 1 elements in the m_file and reads each seek
    head it finds. All entries for the given \c id are removed from
    the seek head. If the seek head has been changed then it is
    rewritten to its original position. The space freed up is filled
    with a new EbmlVoid element.

    \param id The ID of the elements that should be removed.
 */
void
kax_analyzer_c::remove_from_meta_seeks(EbmlId id) {
  size_t data_idx;

  for (data_idx = 0; m_data.size() > data_idx; ++data_idx) {
    // We only have to do work on SeekHead elements. Skip the others.
    if (m_data[data_idx]->m_id != EBML_ID(KaxSeekHead))
      continue;

    // Read the element from the m_file. Remember its size so that a new
    // EbmlVoid element can be constructed afterwards.
    EbmlElement *element   = read_element(data_idx);
    KaxSeekHead *seek_head = dynamic_cast<KaxSeekHead *>(element);
    if (NULL == seek_head)
      throw uer_error_unknown;

    int64_t old_size = seek_head->ElementSize(true);

    // Iterate over its children and delete the ones we're looking for.
    bool modified = false;
    size_t sh_idx = 0;
    while (seek_head->ListSize() > sh_idx) {
      if (EbmlId(*(*seek_head)[sh_idx]) != EBML_ID(KaxSeek)) {
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

    // Only rewrite the element to the m_file if it has been modified.
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
    m_file->setFilePointer(m_data[data_idx]->m_pos);
    seek_head->Render(*m_file, true);

    delete element;

    m_data[data_idx]->m_size = new_size;

    // Create a void element to cover the freed space.
    handle_void_elements(data_idx);
  }
}

/** \brief Overwrites all instances of a specific level 1 element

    Iterates over the level 1 elements in the m_file and overwrites
    each instance of a specific level 1 element given by \c id.
    It is replaced with a new EbmlVoid element.

    \param id The ID of the elements that should be overwritten.
 */
void
kax_analyzer_c::overwrite_all_instances(EbmlId id) {
  size_t data_idx;

  for (data_idx = 0; m_data.size() > data_idx; ++data_idx) {
    // We only have to do work on specific elements. Skip the others.
    if (m_data[data_idx]->m_id != id)
      continue;

    // Overwrite with a void element.
    m_data[data_idx]->m_size = 0;
    handle_void_elements(data_idx);
  }
}

/** \brief Merges consecutive EbmlVoid elements into a single one

    Iterates over the level 1 elements in the m_file and merges
    consecutive EbmlVoid elements into a single one which covers
    the same space as the smaller ones combined.

    Void elements at the end of the m_file are removed as well.
 */
void
kax_analyzer_c::merge_void_elements() {
  size_t start_idx = 0;

  while (m_data.size() > start_idx) {
    // We only have to do work on EbmlVoid elements. Skip the others.
    if (m_data[start_idx]->m_id != EBML_ID(EbmlVoid)) {
      ++start_idx;
      continue;
    }

    // Found an EbmlVoid element. See how many consecutive EbmlVoid elements
    // there are at this position and calculate the combined size.
    size_t end_idx  = start_idx + 1;
    size_t new_size = m_data[start_idx]->m_size;
    while ((m_data.size() > end_idx) && (m_data[end_idx]->m_id == EBML_ID(EbmlVoid))) {
      new_size += m_data[end_idx]->m_size;
      ++end_idx;
    }

    // Is this only a single EbmlVoid element? If yes continue.
    if (end_idx == (start_idx + 1)) {
      start_idx += 2;
      continue;
    }

    // Write the new EbmlVoid element to the m_file.
    m_file->setFilePointer(m_data[start_idx]->m_pos);

    EbmlVoid evoid;
    evoid.SetSize(new_size);
    evoid.UpdateSize();
    evoid.SetSize(new_size - evoid.HeadSize());
    evoid.Render(*m_file);

    // Update the internal records to reflect the changes.
    m_data[start_idx]->m_size = new_size;
    m_data.erase(m_data.begin() + start_idx + 1, m_data.begin() + end_idx);

    start_idx += 2;
  }

  // See how many void elements there are at the end of the m_file.
  start_idx = m_data.size() - 1;

  while ((0 <= start_idx) && (EBML_ID(EbmlVoid) == m_data[start_idx]->m_id))
    --start_idx;
  ++start_idx;

  // If there are none then we're done.
  if (m_data.size() <= start_idx)
    return;

  // Truncate the m_file after the last non-void element and update the m_segment size.
  m_file->truncate(m_data[start_idx]->m_pos);
  adjust_segment_size();
}

/** \brief Finds a suitable spot for an element and writes it to the m_file

    First, a suitable spot for the element is determined by looking at
    EbmlVoid elements. If none is found in the middle of the m_file then
    the element will be appended at the end.

    Second, the element is written at the location determined in the
    first step. If EbmlVoid elements are overwritten then a new,
    smaller one is created which covers the remainder of the
    overwritten one.

    Third, the internal records are updated to reflect the changes.

    \param e Pointer to the element to write.
    \param write_defaults Boolean that decides whether or not elements
      which contain their default value are written to the m_file.
 */
void
kax_analyzer_c::write_element(EbmlElement *e,
                              bool write_defaults,
                              placement_strategy_e strategy) {
  e->UpdateSize(write_defaults);
  int64_t element_size = e->ElementSize(write_defaults);

  size_t data_idx;
  for (data_idx = (ps_anywhere == strategy ? 0 : m_data.size() - 1); m_data.size() > data_idx; ++data_idx) {
    // We're only interested in EbmlVoid elements. Skip the others.
    if (m_data[data_idx]->m_id != EBML_ID(EbmlVoid))
      continue;

    // Skip the element if it doesn't provide enough space.
    if (m_data[data_idx]->m_size < element_size)
      continue;

    // We've found our element. Overwrite it.
    m_file->setFilePointer(m_data[data_idx]->m_pos);
    e->Render(*m_file, write_defaults);

    // Update the internal records.
    m_data[data_idx]->m_id   = EbmlId(*e);
    m_data[data_idx]->m_size = e->ElementSize(write_defaults);

    // Create a new void element after the element we've just written.
    handle_void_elements(data_idx);

    // We're done.
    return;
  }

  // We haven't found a suitable place. So store the element at the end of the m_file
  // and update the internal records.
  m_file->setFilePointer(0, seek_end);
  e->Render(*m_file, write_defaults);
  m_data.push_back(kax_analyzer_data_c::create(EbmlId(*e), m_file->getFilePointer() - e->ElementSize(write_defaults), e->ElementSize(write_defaults)));

  // Adjust the m_segment's size.
  adjust_segment_size();
}

/** \brief Adds an element to one of the meta seek entries

    This function iterates over all meta seek elements and looks
    for one that has enough space (via following EbmlVoid elements or
    because it is located at the end of the m_file) for indexing
    the element \c e.

    If no such element is found then a new meta seek element is
    created at an appropriate place, and that element is indexed.

    \param e Pointer to the element to index.
 */
void
kax_analyzer_c::add_to_meta_seek(EbmlElement *e) {
  size_t data_idx;
  int first_seek_head_idx = -1;

  for (data_idx = 0; m_data.size() > data_idx; ++data_idx) {
    // We only have to do work on SeekHead elements. Skip the others.
    if (m_data[data_idx]->m_id != EBML_ID(KaxSeekHead))
      continue;

    // Calculate how much free space there is behind the seek head.
    // merge_void_elemens() guarantees that there is no EbmlVoid element
    // at the end of the m_file and that all consecutive EbmlVoid elements
    // have been merged into a single element.
    size_t available_space = m_data[data_idx]->m_size;
    bool void_present      = false;
    if (((data_idx + 1) < m_data.size()) && (m_data[data_idx + 1]->m_id == EBML_ID(EbmlVoid))) {
      available_space += m_data[data_idx + 1]->m_size;
      void_present     = true;
    }

    // Read the seek head, index the element and see how much space it needs.
    EbmlElement *element   = read_element(data_idx);
    KaxSeekHead *seek_head = dynamic_cast<KaxSeekHead *>(element);
    if (NULL == seek_head)
      throw uer_error_unknown;

    if (-1 == first_seek_head_idx)
      first_seek_head_idx = data_idx;

    seek_head->IndexThis(*e, *m_segment.get_object());
    seek_head->UpdateSize(true);

    // We can use this seek head if it is at the end of the m_file, or if there
    // is enough space behind it in form of void elements.
    if ((m_data.size() != (data_idx + 1)) && (seek_head->ElementSize(true) > available_space)) {
      delete seek_head;
      continue;
    }

    // Write the seek head.
    m_file->setFilePointer(m_data[data_idx]->m_pos);
    seek_head->Render(*m_file, true);

    // Update the internal record.
    m_data[data_idx]->m_size = seek_head->ElementSize(true);
    delete seek_head;

    // If this seek head is located at the end of the m_file then we have
    // to adjust the m_segment size.
    if (m_data.size() == (data_idx + 1))
      adjust_segment_size();

    else
      // Otherwise create an EbmlVoid to fill the gap (if any).
      handle_void_elements(data_idx);

    // We're done.
    return;
  }

  // No suitable meta seek head found -- we have to write a new one.

  // If we have found a prior seek head then we copy that one to the end
  // of the m_file including the newly indexed element and write a one-element
  // seek head at the first meta seek's position pointing to the one at the
  // end.
  if (-1 != first_seek_head_idx) {
    // Read the first seek head...
    EbmlElement *element   = read_element(first_seek_head_idx);
    KaxSeekHead *seek_head = dynamic_cast<KaxSeekHead *>(element);
    if (NULL == seek_head)
      throw uer_error_unknown;

    // ...index our element...
    seek_head->IndexThis(*e, *m_segment.get_object());
    seek_head->UpdateSize(true);

    // ...write the seek head at the end of the m_file...
    m_file->setFilePointer(0, seek_end);
    seek_head->Render(*m_file, true);

    // ...and update the internal records.
    m_data.push_back(kax_analyzer_data_c::create(EBML_ID(KaxSeekHead), seek_head->GetElementPosition(), seek_head->ElementSize(true)));

    // Update the m_segment size.
    adjust_segment_size();

    // Create a new seek head and write it to the m_file.
    KaxSeekHead *forward_seek_head = new KaxSeekHead;
    forward_seek_head->IndexThis(*seek_head, *m_segment.get_object());
    forward_seek_head->UpdateSize(true);

    m_file->setFilePointer(m_data[first_seek_head_idx]->m_pos);
    forward_seek_head->Render(*m_file, true);

    // Update the internal record to reflect that there's a new seek head.
    m_data[first_seek_head_idx]->m_size = forward_seek_head->ElementSize(true);

    // Create a void element behind the small new first seek head.
    handle_void_elements(first_seek_head_idx);

    // We're done.
    delete forward_seek_head;
    delete seek_head;

    return;
  }

  // We don't have a seek head to copy. Create one before the first chapter if possible.
  KaxSeekHead *new_seek_head = new KaxSeekHead;
  new_seek_head->IndexThis(*e, *m_segment.get_object());
  new_seek_head->UpdateSize(true);

  for (data_idx = 0; m_data.size() > data_idx; ++data_idx) {
    // We can only overwrite void elements. Skip the others.
    if (m_data[data_idx]->m_id != EBML_ID(EbmlVoid))
      continue;

    // Skip the element if it doesn't offer enough space for the seek head.
    if (m_data[data_idx]->m_size < static_cast<int64_t>(new_seek_head->ElementSize(true)))
      continue;

    // We've found a suitable spot. Write the seek head.
    m_file->setFilePointer(m_data[data_idx]->m_pos);
    new_seek_head->Render(*m_file, true);

    // Adjust the internal records for the new seek head.
    m_data[data_idx]->m_size = new_seek_head->ElementSize(true);
    m_data[data_idx]->m_id   = EBML_ID(KaxSeekHead);

    // Write a void element after the newly written seek head in order to
    // cover the space previously occupied by the old void element.
    handle_void_elements(data_idx);

    // We're done.
    delete new_seek_head;

    return;
  }

  delete new_seek_head;

  // We cannot write a seek head before the first cluster. This is not supported at the moment.
  throw uer_error_not_indexable;
}

EbmlMaster *
kax_analyzer_c::read_all(const EbmlCallbacks &callbacks) {
  reopen_file();

  EbmlMaster *master = NULL;
  EbmlStream es(*m_file);
  size_t i;

  for (i = 0; m_data.size() > i; ++i) {
    kax_analyzer_data_c &data = *m_data[i].get_object();
    if (EBML_INFO_ID(callbacks) != data.m_id)
      continue;

    m_file->setFilePointer(data.m_pos);
    int upper_lvl_el     = 0;
    EbmlElement *element = es.FindNextElement(EBML_CLASS_CONTEXT(KaxSegment), upper_lvl_el, 0xFFFFFFFFL, true);
    if (NULL == element)
      continue;

    if (EbmlId(*element) != EBML_INFO_ID(callbacks)) {
      delete element;
      continue;
    }

    EbmlElement *l2 = NULL;
    element->Read(*m_stream, EBML_INFO_CONTEXT(callbacks), upper_lvl_el, l2, true);

    if (NULL == master)
      master = static_cast<EbmlMaster *>(element);
    else {
      EbmlMaster *src = static_cast<EbmlMaster *>(element);
      while (src->ListSize() > 0) {
        master->PushElement(*(*src)[0]);
        src->Remove(0);
      }
      delete element;
    }
  }

  if ((NULL != master) && (master->ListSize() == 0)) {
    delete master;
    return NULL;
  }

  return master;
}

void
kax_analyzer_c::read_all_meta_seeks() {
  m_meta_seeks_by_position.clear();

  unsigned int i, num_entries = m_data.size();
  std::map<int64_t, bool> positions_found;

  for (i = 0; i < num_entries; i++)
    positions_found[m_data[i]->m_pos] = true;

  for (i = 0; i < num_entries; i++)
    if (EBML_ID(KaxSeekHead) == m_data[i]->m_id)
      read_meta_seek(m_data[i]->m_pos, positions_found);

  std::sort(m_data.begin(), m_data.end());
}

void
kax_analyzer_c::read_meta_seek(uint64_t pos,
                               std::map<int64_t, bool> &positions_found) {
  if (m_meta_seeks_by_position[pos])
    return;

  m_meta_seeks_by_position[pos] = true;

  m_file->setFilePointer(pos, seek_beginning);

  int upper_lvl_el = 0;
  EbmlElement *l1  = m_stream->FindNextElement(EBML_CONTEXT(m_segment), upper_lvl_el, 0xFFFFFFFFL, true, 1);

  if (NULL == l1)
    return;

  if (!is_id(l1, KaxSeekHead)) {
    delete l1;
    return;
  }

  EbmlElement *l2    = NULL;
  EbmlMaster *master = static_cast<EbmlMaster *>(l1);
  master->Read(*m_stream, EBML_CONTEXT(l1), upper_lvl_el, l2, true);

  unsigned int i;
  for (i = 0; master->ListSize() > i; i++) {
    if (!is_id((*master)[i], KaxSeek))
      continue;

    KaxSeek *seek      = static_cast<KaxSeek *>((*master)[i]);
    KaxSeekID *seek_id = FINDFIRST(seek, KaxSeekID);
    int64_t seek_pos   = seek->Location() + m_segment->GetElementPosition() + m_segment->HeadSize();

    if ((0 == pos) || (NULL == seek_id))
      continue;

    if (positions_found[seek_pos])
      continue;

    EbmlId the_id(seek_id->GetBuffer(), seek_id->GetSize());
    m_data.push_back(kax_analyzer_data_c::create(the_id, seek_pos, -1));
    positions_found[seek_pos] = true;

    if (EBML_ID(KaxSeekHead) == the_id)
      read_meta_seek(seek_pos, positions_found);
  }

  delete l1;
}

void
kax_analyzer_c::fix_element_sizes(uint64_t file_size) {
  unsigned int i;
  for (i = 0; m_data.size() > i; ++i)
    if (-1 == m_data[i]->m_size)
      m_data[i]->m_size = ((i + 1) < m_data.size() ? m_data[i + 1]->m_pos : file_size) - m_data[i]->m_pos;
}

kax_analyzer_c::placement_strategy_e
kax_analyzer_c::get_placement_strategy_for(EbmlElement *e) {
  return EbmlId(*e) == EBML_ID(KaxTags) ? ps_end : ps_anywhere;
}

// ------------------------------------------------------------

bool m_show_progress;
int m_previous_percentage;

console_kax_analyzer_c::console_kax_analyzer_c(std::string file_name)
  : kax_analyzer_c(file_name)
  , m_show_progress(false)
  , m_previous_percentage(-1)
{
}

console_kax_analyzer_c::~console_kax_analyzer_c() {
}

void
console_kax_analyzer_c::set_show_progress(bool show_progress) {
  if (-1 == m_previous_percentage)
    m_show_progress = show_progress;
}

void
console_kax_analyzer_c::show_progress_start(int64_t size) {
  if (!m_show_progress)
    return;

  m_previous_percentage = -1;
  show_progress_running(0);
}

bool
console_kax_analyzer_c::show_progress_running(int percentage) {
  if (!m_show_progress || (percentage == m_previous_percentage))
    return true;

  std::string full_bar(        percentage  * CONSOLE_PERCENTAGE_WIDTH / 100, '=');
  std::string empty_bar((100 - percentage) * CONSOLE_PERCENTAGE_WIDTH / 100, ' ');

  mxinfo(boost::format(Y("Progress: [%1%%2%] %3%%%")) % full_bar % empty_bar % percentage);
  mxinfo("\r");

  m_previous_percentage = percentage;

  return true;
}

void
console_kax_analyzer_c::show_progress_done() {
  if (!m_show_progress)
    return;

  show_progress_running(100);
  mxinfo("\n");
}

void
console_kax_analyzer_c::debug_abort_process() {
}
