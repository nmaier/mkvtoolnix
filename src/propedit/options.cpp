/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxChapters.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>

#include "propedit/chapter_target.h"
#include "propedit/options.h"
#include "propedit/propedit.h"
#include "propedit/segment_info_target.h"
#include "propedit/tag_target.h"
#include "propedit/track_target.h"

options_c::options_c()
  : m_show_progress(false)
  , m_parse_mode(kax_analyzer_c::parse_mode_fast)
{
}

void
options_c::validate() {
  if (m_file_name.empty())
    mxerror(Y("No file name given.\n"));

  if (!has_changes())
    mxerror(Y("Nothing to do.\n"));

  for (auto &target : m_targets)
    target->validate();
}

void
options_c::execute() {
  for (auto &target : m_targets)
    target->execute();
}

target_cptr
options_c::add_track_or_segmentinfo_target(std::string const &spec) {
  static std::string const track_prefix("track:");

  auto spec_lower = balg::to_lower_copy(spec);
  target_cptr target;

  if ((spec_lower == "segment_info") || (spec_lower == "segmentinfo") || (spec_lower == "info"))
    target = target_cptr{new segment_info_target_c()};

  else if (balg::istarts_with(spec_lower, track_prefix)) {
    auto spec_short = spec_lower.substr(track_prefix.length());
    target          = target_cptr{new track_target_c(spec_short)};
    static_cast<track_target_c *>(target.get())->parse_spec(spec_short);

  } else
    assert(false);

  for (auto &existing_target : m_targets)
    if (*existing_target == *target)
      return existing_target;

  m_targets.push_back(target);

  return target;
}

void
options_c::add_tags(std::string const &spec) {
  target_cptr target{new tag_target_c{}};
  static_cast<tag_target_c *>(target.get())->parse_tags_spec(spec);
  m_targets.push_back(target);
}

void
options_c::add_chapters(const std::string &spec) {
  target_cptr target{new chapter_target_c{}};
  static_cast<chapter_target_c *>(target.get())->parse_chapter_spec(spec);
  m_targets.push_back(target);
}

void
options_c::set_file_name(const std::string &file_name) {
  if (!m_file_name.empty())
    mxerror(boost::format(Y("More than one file name has been given ('%1%' and '%2%').\n")) % m_file_name % file_name);

  m_file_name = file_name;
}

void
options_c::set_parse_mode(const std::string &parse_mode) {
  if (parse_mode == "full")
    m_parse_mode = kax_analyzer_c::parse_mode_full;

  else if (parse_mode == "fast")
    m_parse_mode = kax_analyzer_c::parse_mode_fast;

  else
    throw false;
}

void
options_c::dump_info()
  const
{
  mxinfo(boost::format("options:\n"
                       "  file_name:     %1%\n"
                       "  show_progress: %2%\n"
                       "  parse_mode:    %3%\n")
         % m_file_name
         % m_show_progress
         % static_cast<int>(m_parse_mode));

  for (auto &target : m_targets)
    target->dump_info();
}

bool
options_c::has_changes()
  const
{
  return !m_targets.empty();
}

void
options_c::remove_empty_targets() {
  boost::remove_erase_if(m_targets, [](target_cptr &target) { return !target->has_changes(); });
}

template<typename T> static ebml_element_cptr
read_element(kax_analyzer_c *analyzer,
             const std::string &category,
             bool require_existance = true) {
  int index = analyzer->find(T::ClassInfos.GlobalId);
  ebml_element_cptr e;

  if (-1 != index)
    e = analyzer->read_element(index);

  if (require_existance && (!e || !dynamic_cast<T *>(e.get())))
    mxerror(boost::format(Y("Modification of properties in the section '%1%' was requested, but no corresponding level 1 element was found in the file. %2%\n")) % category % FILE_NOT_MODIFIED);

  return e;
}

void
options_c::find_elements(kax_analyzer_c *analyzer) {
  ebml_element_cptr tracks(read_element<KaxTracks>(analyzer, Y("Track headers")));
  ebml_element_cptr info, tags, chapters;

  for (auto &target_ptr : m_targets) {
    target_c &target = *target_ptr;
    if (dynamic_cast<segment_info_target_c *>(&target)) {
      if (!info)
        info = read_element<KaxInfo>(analyzer, Y("Segment information"));
      target.set_level1_element(info);

    } else if (dynamic_cast<tag_target_c *>(&target)) {
      if (!tags) {
        tags = read_element<KaxTags>(analyzer, Y("Tags"), false);
        if (!tags)
          tags = ebml_element_cptr(new KaxTags);
      }

      target.set_level1_element(tags, tracks);

    } else if (dynamic_cast<chapter_target_c *>(&target)) {
      if (!chapters) {
        chapters = read_element<KaxChapters>(analyzer, Y("Chapters"), false);
        if (!chapters)
          chapters = ebml_element_cptr(new KaxChapters);
      }

      target.set_level1_element(chapters, tracks);

    } else if (dynamic_cast<track_target_c *>(&target))
      target.set_level1_element(tracks);

    else
      assert(false);
  }

  merge_targets();
}

void
options_c::merge_targets() {
  std::map<uint64_t, track_target_c *> targets_by_track_uid;
  std::vector<target_cptr> targets_to_keep;

  for (auto &target : m_targets) {
    auto track_target = dynamic_cast<track_target_c *>(target.get());
    if (!track_target || dynamic_cast<segment_info_target_c *>(target.get())) {
      targets_to_keep.push_back(target);
      continue;
    }

    auto existing_target_it = targets_by_track_uid.find(track_target->get_track_uid());
    auto track_uid          = target->get_track_uid();
    if (targets_by_track_uid.end() == existing_target_it) {
      targets_to_keep.push_back(target);
      targets_by_track_uid[track_uid] = track_target;
      continue;
    }

    existing_target_it->second->merge_changes(*track_target);

    mxwarn(boost::format(Y("The edit specifications '%1%' and '%2%' resolve to the same track with the UID %3%.\n"))
           % existing_target_it->second->get_spec() % track_target->get_spec() % track_uid);
  }

  m_targets.swap(targets_to_keep);
}

void
options_c::options_parsed() {
  remove_empty_targets();
  m_show_progress = 1 < verbose;
}
