/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_PROPEDIT_CHANGE_H
#define MTX_PROPEDIT_CHANGE_H

#include "common/common_pch.h"

#include "common/bitvalue.h"
#include "common/property_element.h"

class change_c;
typedef std::shared_ptr<change_c> change_cptr;

class change_c {
public:
  enum change_type_e {
    ct_add,
    ct_set,
    ct_delete,
  };

  change_type_e m_type;
  std::string m_name, m_value;

  property_element_c m_property;

  std::string m_s_value;
  uint64_t m_ui_value;
  int64_t m_si_value;
  bool m_b_value;
  bitvalue_c m_x_value;
  double m_fp_value;

  EbmlMaster *m_master;

public:
  change_c(change_type_e type, const std::string &name, const std::string &value);

  void validate(std::vector<property_element_c> *property_table = nullptr);
  void dump_info() const;

  bool lookup_property(std::vector<property_element_c> &table);

  std::string get_spec();

  void execute(EbmlMaster *master, EbmlMaster *sub_master);

public:
  static change_cptr parse_spec(change_type_e type, std::string const &spec);

protected:
  void parse_ascii_string();
  void parse_binary();
  void parse_boolean();
  void parse_floating_point_number();
  void parse_signed_integer();
  void parse_unicode_string();
  void parse_unsigned_integer();
  void parse_value();

  void execute_add_or_set();
  void execute_delete();
  void do_add_element();
  void set_element_at(int idx);

  void validate_deletion_of_mandatory();

  const EbmlSemantic *get_semantic();
};

#endif // MTX_PROPEDIT_CHANGE_H
