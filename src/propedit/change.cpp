/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <ebml/EbmlBinary.h>
#include <ebml/EbmlFloat.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>
#include <matroska/KaxSegment.h>

#include "common/common_pch.h"
#include "common/ebml.h"
#include "common/output.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "propedit/change.h"
#include "propedit/propedit.h"

change_c::change_c(change_c::change_type_e type,
                   const std::string &name,
                   const std::string &value)
  : m_type(type)
  , m_name(name)
  , m_value(value)
  , m_ui_value(0)
  , m_si_value(0)
  , m_b_value(false)
  , m_x_value(128)
  , m_fp_value(0)
  , m_master(nullptr)
{
}

void
change_c::validate(std::vector<property_element_c> *property_table) {
  if (!property_table)
    return;

  for (auto &property : *property_table)
    if (property.m_name == m_name) {
      m_property = property;

      if (change_c::ct_delete == m_type)
        validate_deletion_of_mandatory();
      else
        parse_value();

      return;
    }

  mxerror(boost::format(Y("The name '%1%' is not a valid property name for the current edit specification in '%2%'.\n")) % m_name % get_spec());
}

std::string
change_c::get_spec() {
  return change_c::ct_delete == m_type ? (boost::format("--delete %1%")                                                 % m_name          ).str()
       :                                 (boost::format("--%1% %2%=%3%") % (change_c::ct_add == m_type ? "add" : "set") % m_name % m_value).str();
}

void
change_c::dump_info()
  const
{
  mxinfo(boost::format("    change:\n"
                       "      type:  %1%\n"
                       "      name:  %2%\n"
                       "      value: %3%\n")
         % static_cast<int>(m_type)
         % m_name
         % m_value);
}

bool
change_c::lookup_property(std::vector<property_element_c> &table) {
  for (auto &property : table)
    if (property.m_name == m_name) {
      m_property = property;
      return true;
    }

  return false;
}

void
change_c::parse_value() {
  switch (m_property.m_type) {
    case property_element_c::EBMLT_STRING:  parse_ascii_string();          break;
    case property_element_c::EBMLT_USTRING: parse_unicode_string();        break;
    case property_element_c::EBMLT_UINT:    parse_unsigned_integer();      break;
    case property_element_c::EBMLT_INT:     parse_signed_integer();        break;
    case property_element_c::EBMLT_BOOL:    parse_boolean();               break;
    case property_element_c::EBMLT_BINARY:  parse_binary();                break;
    case property_element_c::EBMLT_FLOAT:   parse_floating_point_number(); break;
    default:                                assert(false);
  }
}

void
change_c::parse_ascii_string() {
  size_t i;
  for (i = 0; m_value.length() > i; ++i)
    if (127 < static_cast<unsigned char>(m_value[i]))
      mxerror(boost::format(Y("The property value contains non-ASCII characters, but the property is not a Unicode string in '%1%'. %2%\n")) % get_spec() % FILE_NOT_MODIFIED);

  m_s_value = m_value;
}

void
change_c::parse_unicode_string() {
  m_s_value = m_value;
}

void
change_c::parse_unsigned_integer() {
  if (!parse_number(m_value, m_ui_value))
    mxerror(boost::format(Y("The property value is not a valid unsigned integer in '%1%'. %2%\n")) % get_spec() % FILE_NOT_MODIFIED);
}

void
change_c::parse_signed_integer() {
  if (!parse_number(m_value, m_si_value))
    mxerror(boost::format(Y("The property value is not a valid signed integer in '%1%'. %2%\n")) % get_spec() % FILE_NOT_MODIFIED);
}

void
change_c::parse_boolean() {
  try {
    m_b_value = parse_bool(m_value);
  } catch (...) {
    mxerror(boost::format(Y("The property value is not a valid boolean in '%1%'. %2%\n")) % get_spec() % FILE_NOT_MODIFIED);
  }
}

void
change_c::parse_floating_point_number() {
  if (!parse_number(m_value, m_fp_value))
    mxerror(boost::format(Y("The property value is not a valid floating point number in '%1%'. %2%\n")) % get_spec() % FILE_NOT_MODIFIED);
}

void
change_c::parse_binary() {
  try {
    m_x_value = bitvalue_c(m_value, 128);
  } catch (...) {
    mxerror(boost::format(Y("The property value is not a valid binary spec or it is not exactly 128 bits long in '%1%'. %2%\n")) % get_spec() % FILE_NOT_MODIFIED);
  }
}

void
change_c::execute(EbmlMaster *master,
                  EbmlMaster *sub_master) {
  m_master = !m_property.m_sub_master_callbacks ? master : sub_master;

  if (!m_master)
    return;

  if (change_c::ct_delete == m_type)
    execute_delete();
  else
    execute_add_or_set();
}

void
change_c::execute_delete() {
  size_t idx               = 0;
  unsigned int num_deleted = 0;
  while (m_master->ListSize() > idx) {
    if (m_property.m_callbacks->GlobalId == (*m_master)[idx]->Generic().GlobalId) {
      m_master->Remove(idx);
      ++num_deleted;
    } else
      ++idx;
  }

  if (1 < verbose)
    mxinfo(boost::format(Y("Change for '%1%' executed. Number of entries deleted: %2%\n")) % get_spec() % num_deleted);
}

void
change_c::execute_add_or_set() {
  size_t idx;
  unsigned int num_found = 0;
  for (idx = 0; m_master->ListSize() > idx; ++idx) {
    if (m_property.m_callbacks->GlobalId != (*m_master)[idx]->Generic().GlobalId)
      continue;

    if (change_c::ct_set == m_type)
      set_element_at(idx);

    ++num_found;
  }

  if (0 == num_found) {
    do_add_element();
    if (1 < verbose)
      mxinfo(boost::format(Y("Change for '%1%' executed. No property of this type found. One entry added.\n")) % get_spec());
    return;
  }

  if (change_c::ct_set == m_type) {
    if (1 < verbose)
      mxinfo(boost::format(Y("Change for '%1%' executed. Number of entries set: %2%.\n")) % get_spec() % num_found);
    return;
  }

  const EbmlSemantic *semantic = get_semantic();
  if (semantic && semantic->Unique)
    mxerror(boost::format(Y("This property is unique. More instances cannot be added in '%1%'. %2%\n")) % get_spec() % FILE_NOT_MODIFIED);

  do_add_element();

 if (1 < verbose)
    mxinfo(boost::format(Y("Change for '%1%' executed. One entry added.\n")) % get_spec());
}

void
change_c::do_add_element() {
  m_master->PushElement(m_property.m_callbacks->Create());
  set_element_at(m_master->ListSize() - 1);
}

void
change_c::set_element_at(int idx) {
  EbmlElement *e = (*m_master)[idx];

  switch (m_property.m_type) {
    case property_element_c::EBMLT_STRING:  static_cast<EbmlString        *>(e)->SetValue(m_s_value);                                break;
    case property_element_c::EBMLT_USTRING: static_cast<EbmlUnicodeString *>(e)->SetValue(cstrutf8_to_UTFstring(m_s_value));         break;
    case property_element_c::EBMLT_UINT:    static_cast<EbmlUInteger      *>(e)->SetValue(m_ui_value);                               break;
    case property_element_c::EBMLT_INT:     static_cast<EbmlSInteger      *>(e)->SetValue(m_si_value);                               break;
    case property_element_c::EBMLT_BOOL:    static_cast<EbmlUInteger      *>(e)->SetValue(m_b_value ? 1 : 0);                        break;
    case property_element_c::EBMLT_FLOAT:   static_cast<EbmlFloat         *>(e)->SetValue(m_fp_value);                               break;
    case property_element_c::EBMLT_BINARY:  static_cast<EbmlBinary        *>(e)->CopyBuffer(m_x_value.data(), m_x_value.size() / 8); break;
    default:                                assert(false);
  }
}

void
change_c::validate_deletion_of_mandatory() {
  const EbmlSemantic *semantic = get_semantic();
  if (semantic && semantic->Mandatory)
    mxerror(boost::format(Y("This property is mandatory and cannot be deleted in '%1%'. %2%\n")) % get_spec() % FILE_NOT_MODIFIED);
}

const EbmlSemantic *
change_c::get_semantic() {
  return find_ebml_semantic(KaxSegment::ClassInfos, m_property.m_callbacks->GlobalId);
}

change_cptr
change_c::parse_spec(change_c::change_type_e type,
                     const std::string &spec) {
  std::string name, value;
  if (ct_delete == type)
    name = spec;

  else {
    auto parts = split(spec, "=", 2);
    if (2 != parts.size())
      throw std::runtime_error(Y("missing value"));

    name  = parts[0];
    value = parts[1];
  }

  if (name.empty())
    throw std::runtime_error(Y("missing property name"));

  return std::make_shared<change_c>(type, name, value);
}
