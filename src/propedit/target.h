/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_PROPEDIT_TARGET_H
#define MTX_PROPEDIT_TARGET_H

#include "common/common_pch.h"

#include <ebml/EbmlMaster.h>

#include "propedit/change.h"

#define INVALID_TRACK_TYPE static_cast<track_type>(0)

using namespace libebml;

class target_c {
protected:
  std::string m_spec;

  ebml_element_cptr m_level1_element_cp, m_track_headers_cp;
  EbmlMaster *m_level1_element, *m_master, *m_sub_master;

  uint64_t m_track_uid;
  track_type m_track_type;

  std::string m_file_name;

public:
  target_c();
  virtual ~target_c();

  virtual void validate() = 0;

  virtual void dump_info() const = 0;

  virtual void add_change(change_c::change_type_e type, const std::string &spec);

  virtual bool operator ==(target_c const &cmp) const = 0;
  virtual bool operator !=(target_c const &cmp) const;

  virtual void set_level1_element(ebml_element_cptr level1_element, ebml_element_cptr track_headers = ebml_element_cptr{});

  virtual bool has_changes() const = 0;

  virtual void execute() = 0;

  virtual std::string const &get_spec() const;
  virtual uint64_t get_track_uid() const;
  virtual EbmlMaster *get_level1_element() const;

protected:
  virtual void add_or_replace_all_master_elements(EbmlMaster *source);
};
typedef std::shared_ptr<target_c> target_cptr;

#endif // MTX_PROPEDIT_TARGET_H
