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

#include "propedit/change.h"

using namespace libebml;

class target_c {
public:
  enum target_type_e {
    tt_undefined,
    tt_segment_info,
    tt_track,
  };

  enum selection_mode_e {
    sm_undefined,
    sm_by_number,
    sm_by_uid,
    sm_by_position,
    sm_by_type_and_position,
  };

  target_type_e m_type;
  std::string m_spec;
  selection_mode_e m_selection_mode;
  uint64_t m_selection_param;
  track_type m_selection_track_type;

  EbmlMaster *m_level1_element, *m_master, *m_sub_master;
  uint64_t m_track_uid;

  std::vector<change_cptr> m_changes;

public:
  target_c(target_type_e type);

  void validate();

  void add_change(change_c::change_type_e type, const std::string &spec);
  void parse_target_spec(std::string spec);
  void dump_info() const;

  bool operator ==(const target_c &cmp) const;
  bool operator !=(const target_c &cmp) const;

  bool has_changes() const;

  void set_level1_element(EbmlMaster *level1_element);

protected:
  void parse_track_spec(const std::string &spec);
};
typedef counted_ptr<target_c> target_cptr;

#endif // __PROPEDIT_TARGET_H
