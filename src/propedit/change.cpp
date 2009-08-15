/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/output.h"
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
{
}

void
change_c::validate(std::vector<property_element_c> *property_table) {
  if (NULL == property_table)
    return;

  std::vector<property_element_c>::iterator property_it;
  mxforeach(property_it, *property_table)
    if (property_it->m_name == m_name) {
      m_property = *property_it;
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
  std::vector<property_element_c>::iterator property_it;
  mxforeach(property_it, table)
    if (property_it->m_name == m_name) {
      m_property = *property_it;
      return true;
    }

  return false;
}

void
change_c::parse_value() {
  switch (m_property.m_type) {
    case EBMLT_STRING:  parse_ascii_string();          break;
    case EBMLT_USTRING: parse_unicode_string();        break;
    case EBMLT_UINT:    parse_unsigned_integer();      break;
    case EBMLT_INT:     parse_signed_integer();        break;
    case EBMLT_BOOL:    parse_boolean();               break;
    case EBMLT_BINARY:  parse_binary();                break;
    case EBMLT_FLOAT:   parse_floating_point_number(); break;
    default:            assert(false);
  }
}

void
change_c::parse_ascii_string() {
  int i;
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
  if (!parse_uint(m_value, m_ui_value))
    mxerror(boost::format(Y("The property value is not a valid unsigned integer in '%1%'. %2%\n")) % get_spec() % FILE_NOT_MODIFIED);
}

void
change_c::parse_signed_integer() {
  if (!parse_int(m_value, m_si_value))
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
  if (!parse_double(m_value, m_fp_value))
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

