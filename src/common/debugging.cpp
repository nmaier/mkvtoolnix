/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   debugging functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

#include <ebml/EbmlDate.h>
#include <ebml/EbmlDummy.h>
#include <ebml/EbmlFloat.h>
#include <ebml/EbmlMaster.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>
#include <ebml/EbmlVoid.h>

#include "common/debugging.h"
#include "common/logger.h"
#include "common/strings/editing.h"
#include "common/strings/formatting.h"

using namespace libebml;

std::unordered_map<std::string, std::string> debugging_c::ms_debugging_options;
bool debugging_c::ms_send_to_logger = false;

bool
debugging_c::requested(const char *option,
                       std::string *arg) {
  auto options = split(option, "|");

  for (auto &current_option : options) {
    auto option_ptr = ms_debugging_options.find(current_option);

    if (ms_debugging_options.end() != option_ptr) {
      if (arg)
        *arg = option_ptr->second;
      return true;
    }
  }

  return false;
}

void
debugging_c::request(const std::string &options,
                     bool enable) {
  auto all_options = split(options);

  for (auto &one_option : all_options) {
    auto parts = split(one_option, "=", 2);
    if (!parts[0].size())
      continue;
    if (parts[0] == "!")
      ms_debugging_options.clear();
    else if (parts[0] == "to_logger")
      debugging_c::send_to_logger(true);
    else if (!enable)
      ms_debugging_options.erase(parts[0]);
    else
      ms_debugging_options[parts[0]] = 1 == parts.size() ? std::string("") : parts[1];
  }

  debugging_option_c::invalidate_cache();
}

void
debugging_c::init() {
  auto env_vars = std::vector<std::string>{ "MKVTOOLNIX_DEBUG", "MTX_DEBUG", balg::to_upper_copy(get_program_name()) + "_DEBUG" };

  for (auto &name : env_vars) {
    auto value = getenv(name.c_str());
    if (value)
      request(value);
  }
}

void
debugging_c::send_to_logger(bool enable) {
  ms_send_to_logger = enable;
}

void
debugging_c::output(std::string const &msg) {
  if (ms_send_to_logger)
    log_it(msg);
  else
    mxmsg(MXMSG_INFO, msg);
}

void
debugging_c::hexdump(const void *buffer_to_dump,
                     size_t length) {
  static auto s_fmt_line = boost::format{"Debug> %|1$08x|  "};
  static auto s_fmt_byte = boost::format{"%|1$02x| "};

  std::stringstream dump, ascii;
  auto buffer     = static_cast<const unsigned char *>(buffer_to_dump);
  auto buffer_idx = 0u;

  while (buffer_idx < length) {
    if ((buffer_idx % 16) == 0) {
      if (0 < buffer_idx) {
        dump << ' ' << ascii.str() << '\n';
        ascii.str("");
      }
      dump << (s_fmt_line % buffer_idx);

    } else if ((buffer_idx % 8) == 0) {
      dump  << ' ';
      ascii << ' ';
    }

    ascii << (((32 <= buffer[buffer_idx]) && (128 > buffer[buffer_idx])) ? static_cast<char>(buffer[buffer_idx]) : '.');
    dump  << (s_fmt_byte % static_cast<unsigned int>(buffer[buffer_idx]));

    ++buffer_idx;
  }

  if ((buffer_idx % 16) != 0)
    dump << std::string(3u * (16 - (buffer_idx % 16)) + ((buffer_idx % 8) ? 1 : 0), ' ');
  dump << ' ' << ascii.str() << '\n';

  debugging_c::output(dump.str());
}

// ------------------------------------------------------------

std::vector<debugging_option_c::option_c> debugging_option_c::ms_registered_options;

size_t
debugging_option_c::register_option(std::string const &option) {
  auto itr = brng::find_if(ms_registered_options, [&option](option_c const &opt) { return opt.m_option == option; });
  if (itr != ms_registered_options.end())
    return std::distance(ms_registered_options.begin(), itr);

  ms_registered_options.emplace_back(option);

  return ms_registered_options.size() - 1;
}

void
debugging_option_c::invalidate_cache() {
  for (auto &opt : ms_registered_options)
    opt.m_requested = boost::logic::indeterminate;
}

// ------------------------------------------------------------

ebml_dumper_c::ebml_dumper_c()
  : m_values{true}
  , m_addresses{true}
  , m_indexes{true}
  , m_max_level{std::numeric_limits<size_t>::max()}
  , m_target_type{STDOUT}
  , m_io_target{}
{
}

std::string
ebml_dumper_c::to_string(EbmlElement const *element)
  const {
  return dynamic_cast<EbmlUInteger      const *>(element) ? ::to_string(static_cast<EbmlUInteger      const *>(element)->GetValue())
       : dynamic_cast<EbmlSInteger      const *>(element) ? ::to_string(static_cast<EbmlSInteger      const *>(element)->GetValue())
       : dynamic_cast<EbmlFloat         const *>(element) ? ::to_string(static_cast<EbmlFloat         const *>(element)->GetValue(), 9)
       : dynamic_cast<EbmlUnicodeString const *>(element) ?             static_cast<EbmlUnicodeString const *>(element)->GetValueUTF8()
       : dynamic_cast<EbmlString        const *>(element) ?             static_cast<EbmlString        const *>(element)->GetValue()
       : dynamic_cast<EbmlDate          const *>(element) ? ::to_string(static_cast<EbmlDate          const *>(element)->GetEpochDate())
       : (boost::format("(type: %1% size: %2%)") %
          (  dynamic_cast<EbmlBinary const *>(element)    ? "binary"
           : dynamic_cast<EbmlMaster const *>(element)    ? "master"
           : dynamic_cast<EbmlVoid   const *>(element)    ? "void"
           :                                                "unknown")
          % element->GetSize()).str();
}


ebml_dumper_c &
ebml_dumper_c::values(bool p_values) {
  m_values = p_values;
  return *this;
}

ebml_dumper_c &
ebml_dumper_c::addresses(bool p_addresses) {
  m_addresses = p_addresses;
  return *this;
}

ebml_dumper_c &
ebml_dumper_c::indexes(bool p_indexes) {
  m_indexes = p_indexes;
  return *this;
}

ebml_dumper_c &
ebml_dumper_c::max_level(int p_max_level) {
  m_max_level = p_max_level;
  return *this;
}

ebml_dumper_c &
ebml_dumper_c::target(target_type_e p_target_type,
                      mm_io_c *p_io_target) {
  m_target_type = p_target_type;
  m_io_target   = p_io_target;
  return *this;
}

ebml_dumper_c &
ebml_dumper_c::dump(EbmlElement const *element) {
  dump_impl(element, 0, 0);

  switch (m_target_type) {
    case STDOUT: mxinfo(m_buffer.str()); break;
    case LOGGER: log_it(m_buffer.str()); break;
    case MM_IO:  assert(!!m_io_target); m_io_target->puts(m_buffer.str()); break;
    default:     assert(false);
  }

  m_buffer.str("");

  return *this;
}

void
ebml_dumper_c::dump_impl(EbmlElement const *element,
                         size_t level,
                         size_t index) {
  if (level > m_max_level)
    return;

  m_buffer << std::string(level, ' ');

  if (m_indexes)
    m_buffer << index << " ";

  if (!element) {
    m_buffer << "nullptr" << std::endl;
    return;
  }

  m_buffer << EBML_NAME(element);

  if (m_addresses)
    m_buffer << (boost::format(" @%1%") % element);

  if (m_values)
    m_buffer << " " << to_string(element);

  m_buffer << std::endl;

  auto master = dynamic_cast<EbmlMaster const *>(element);
  if (!master)
    return;

  for (auto idx = 0u; master->ListSize() > idx; ++idx)
    dump_impl((*master)[idx], level + 1, idx);
}

void
dump_ebml_elements(EbmlElement *element,
                   bool with_values) {
  ebml_dumper_c{}.values(with_values).dump(element);
}
