/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_PROPEDIT_TRACK_TARGET_H
#define MTX_PROPEDIT_TRACK_TARGET_H

#include "common/common_pch.h"

#include "propedit/change.h"
#include "propedit/target.h"

using namespace libebml;

class track_target_c: public target_c {
public:
  enum selection_mode_e {
    sm_undefined,
    sm_by_number,
    sm_by_uid,
    sm_by_position,
    sm_by_type_and_position,
  };

  selection_mode_e m_selection_mode;
  uint64_t m_selection_param;
  track_type m_selection_track_type;

  std::vector<change_cptr> m_changes;

public:
  track_target_c(std::string const &spec);
  virtual ~track_target_c();

  virtual void validate();

  virtual void add_change(change_c::change_type_e type, const std::string &spec);
  virtual void parse_spec(std::string const &spec);

  virtual void set_level1_element(ebml_element_cptr level1_element_cp, ebml_element_cptr track_headers_cp);
  virtual void dump_info() const;

  virtual bool operator ==(target_c const &cmp) const;

  virtual bool has_changes() const;
  virtual bool has_add_or_set_change() const;

  virtual void execute();

  virtual void merge_changes(track_target_c &other);

protected:
  virtual bool non_track_target() const;
  virtual bool sub_master_is_track() const;
  virtual bool requires_sub_master() const;
};

#endif // MTX_PROPEDIT_TRACK_TARGET_H
