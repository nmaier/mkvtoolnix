/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_PROPEDIT_ATTACHMENT_TARGET_H
#define MTX_PROPEDIT_ATTACHMENT_TARGET_H

#include "common/common_pch.h"

#include "propedit/target.h"

using namespace libebml;

class attachment_target_c: public target_c {
public:
  struct options_t {
    std::pair<std::string, bool> m_name, m_description, m_mime_type;
  };

  enum command_e {
    ac_add,
    ac_delete,
    ac_replace,
  };

  enum selector_type_e {
    st_id,
    st_uid,
    st_name,
    st_mime_type,
  };

protected:
  command_e m_command;
  options_t m_options;
  selector_type_e m_selector_type;
  uint64_t m_selector_num_arg;
  std::string m_selector_string_arg;
  memory_cptr m_file_content;

public:
  attachment_target_c();
  virtual ~attachment_target_c();

  virtual void validate();

  virtual bool operator ==(target_c const &cmp) const;

  virtual void parse_spec(command_e command, const std::string &spec, options_t const &options);
  virtual void dump_info() const;

  virtual bool has_changes() const;

  virtual void execute();
};

inline bool
operator ==(attachment_target_c::options_t const &a,
            attachment_target_c::options_t const &b) {
  return (a.m_name        == b.m_name)
      && (a.m_description == b.m_description)
      && (a.m_mime_type   == b.m_mime_type);
}

#endif // MTX_PROPEDIT_ATTACHMENT_TARGET_H
