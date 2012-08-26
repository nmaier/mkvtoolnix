/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxAttachments.h>

#include "common/strings/parsing.h"
#include "propedit/attachment_target.h"

using namespace libmatroska;

attachment_target_c::attachment_target_c()
  : target_c()
  , m_command{ac_add}
  , m_selector_type{st_id}
  , m_selector_num_arg{}
{
}

attachment_target_c::~attachment_target_c() {
}

bool
attachment_target_c::operator ==(target_c const &cmp)
  const {
  auto a_cmp = dynamic_cast<attachment_target_c const *>(&cmp);
  if (!a_cmp)
    return false;
  return (m_command             == a_cmp->m_command)
      && (m_options             == a_cmp->m_options)
      && (m_selector_type       == a_cmp->m_selector_type)
      && (m_selector_num_arg    == a_cmp->m_selector_num_arg)
      && (m_selector_string_arg == a_cmp->m_selector_string_arg);
}

void
attachment_target_c::validate() {
  if ((ac_add != m_command) && (ac_replace != m_command))
    return;

  try {
    m_file_content = mm_file_io_c::slurp(m_file_name);
  } catch (mtx::mm_io::exception &ex) {
    mxerror(boost::format(Y("The file '%1%' could not be opened for reading (%2%).\n")) % m_file_name % ex.what());
  }
}

void
attachment_target_c::dump_info()
  const {
  // TODO: dump_info()
  assert(false);
  mxinfo(boost::format("  attachment target:\n"
                       "    file_name: %1%\n")
         % m_file_name);
}

bool
attachment_target_c::has_changes()
  const {
  return true;
}

void
attachment_target_c::parse_spec(command_e command,
                                const std::string &spec,
                                options_t const &options) {
  m_command = command;
  m_options = options;

  if (ac_add == m_command) {
    if (spec.empty())
      throw std::invalid_argument{"empty file name"};
    m_file_name = spec;
    return;
  }

  boost::regex s_spec_re;
  boost::smatch matches;
  unsigned int offset;

  if (ac_replace == m_command) {
    // captures:                    1    2        3            4                    5         6
    s_spec_re = boost::regex{"^ (?: (=)? (\\d+) : (.+) ) | (?: (name | mime-type) : ([^:]+) : (.+) ) $", boost::regex::perl | boost::regex::icase | boost::regex::mod_x};
    offset    = 1;

  } else if (ac_delete == m_command) {
    // captures:                    1    2                     3                    4
    s_spec_re = boost::regex{"^ (?: (=)? (\\d+)        ) | (?: (name | mime-type) : (.+) ) $",           boost::regex::perl | boost::regex::icase | boost::regex::mod_x};
    offset    = 0;

  } else
    assert(false);

  if (!boost::regex_match(spec, matches, s_spec_re))
    throw std::invalid_argument{"generic format error"};

  if (matches[3 + offset].str().empty()) {
    // First case: by ID/UID
    if (ac_replace == m_command)
      m_file_name   = matches[3].str();
    m_selector_type = matches[1].str() == "=" ? st_uid : st_id;
    if (!parse_number(matches[2].str(), m_selector_num_arg))
      throw std::invalid_argument{"ID/UID not a number"};

  } else {
    // Second case: by name/MIME type
    if (ac_replace == m_command)
      m_file_name         = matches[6].str();
    m_selector_type       = balg::to_lower_copy(matches[3 + offset].str()) == "name" ? st_name : st_mime_type;
    m_selector_string_arg = matches[4 + offset].str();
  }

  if ((ac_replace == m_command) && m_file_name.empty())
    throw std::invalid_argument{"empty file name"};
}

void
attachment_target_c::execute() {
  // TODO: execute()
  assert(false);

  // add_or_replace_all_master_elements(m_new_attachments.get());

  // if (!m_level1_element->ListSize())
  //   return;

  // fix_mandatory_attachment_elements(m_level1_element);
  // if (!m_level1_element->CheckMandatory())
  //   mxerror(boost::format(Y("Error parsing the attachments in '%1%': some mandatory elements are missing.\n")) % m_file_name);
}
