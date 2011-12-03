/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PROPEDIT_TARGET_H
#define __PROPEDIT_TARGET_H

#include "common/os.h"

#include <string>
#include <vector>

#include <ebml/EbmlMaster.h>
#include <matroska/KaxTags.h>

#include "propedit/change.h"

using namespace libebml;

class target_c {
public:
  enum target_type_e {
    tt_undefined,
    tt_segment_info,
    tt_track,
    tt_tags,
    tt_chapters,
  };

  enum selection_mode_e {
    sm_undefined,
    sm_by_number,
    sm_by_uid,
    sm_by_position,
    sm_by_type_and_position,
  };

  enum tag_operation_mode_e {
    tom_undefined,
    tom_all,
    tom_global,
    tom_track,
  };

  target_type_e m_type;
  std::string m_spec;
  selection_mode_e m_selection_mode;
  tag_operation_mode_e m_tag_operation_mode;
  uint64_t m_selection_param;
  track_type m_selection_track_type;

  ebml_element_cptr m_level1_element_cp, m_track_headers_cp;
  EbmlMaster *m_level1_element, *m_master, *m_sub_master;
  uint64_t m_track_uid;
  track_type m_track_type;

  std::vector<change_cptr> m_changes;

  std::string m_file_name;

public:
  target_c(target_type_e type);

  void validate();

  void add_change(change_c::change_type_e type, const std::string &spec);
  void parse_target_spec(std::string spec);
  void parse_tags_spec(const std::string &spec);
  void parse_chapter_spec(const std::string &spec);
  void dump_info() const;

  bool operator ==(const target_c &cmp) const;
  bool operator !=(const target_c &cmp) const;

  bool has_changes() const;
  bool has_add_or_set_change() const;

  void set_level1_element(ebml_element_cptr level1_element, ebml_element_cptr track_headers = ebml_element_cptr(NULL));

  void execute();

protected:
  void add_or_replace_all_master_elements(EbmlMaster *source);

  void parse_track_spec(const std::string &spec);
  void add_or_replace_tags();
  void add_or_replace_global_tags(KaxTags *tags);
  void add_or_replace_track_tags(KaxTags *tags);

  void parse_chapters_spec(const std::string &spec);
  void add_or_replace_chapters();
};
typedef counted_ptr<target_c> target_cptr;

#endif // __PROPEDIT_TARGET_H
