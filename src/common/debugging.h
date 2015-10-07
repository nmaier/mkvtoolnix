/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_DEBUGGING_H
#define MTX_COMMON_DEBUGGING_H

#include "common/common_pch.h"

#include <sstream>
#include <unordered_map>

class debugging_c {
protected:
  static bool ms_send_to_logger;
  static std::unordered_map<std::string, std::string> ms_debugging_options;

public:
  static void send_to_logger(bool enable);
  static void output(std::string const &msg);
  static void output(boost::format const &msg) {
    output(msg.str());
  }

  static void hexdump(const void *buffer_to_dump, size_t lenth);

  static bool requested(const char *option, std::string *arg = nullptr);
  static bool requested(const std::string &option, std::string *arg = nullptr) {
    return requested(option.c_str(), arg);
  }
  static void request(const std::string &options, bool enable = true);
  static void init();
};

class debugging_option_c {
  struct option_c {
    boost::tribool m_requested;
    std::string m_option;

    option_c(std::string const &option)
      : m_requested{boost::logic::indeterminate}
      , m_option{option}
    {
    }

    bool get() {
      if (boost::logic::indeterminate(m_requested))
        m_requested = debugging_c::requested(m_option);

      return m_requested;
    }
  };

protected:
  mutable size_t m_registered_idx;
  std::string m_option;

private:
  static std::vector<option_c> ms_registered_options;

public:
  debugging_option_c(std::string const &option)
    : m_registered_idx{std::numeric_limits<size_t>::max()}
    , m_option{option}
  {
  }

  operator bool() const {
    if (m_registered_idx == std::numeric_limits<size_t>::max())
      m_registered_idx = register_option(m_option);

    return ms_registered_options.at(m_registered_idx).get();
  }

public:
  static size_t register_option(std::string const &option);
  static void invalidate_cache();
};

#define mxdebug(msg) debugging_c::output((boost::format("Debug> %1%:%2%: %3%") % __FILE__ % __LINE__ % (msg)).str())

#define mxdebug_if(condition, msg) \
  if (condition) {                 \
    mxdebug(msg);                  \
  }

class mm_io_c;
namespace libebml {
class EbmlElement;
};

class ebml_dumper_c {
public:
  enum target_type_e {
    STDOUT,
    MM_IO,
    LOGGER,
  };

private:
  bool m_values, m_addresses, m_indexes;
  size_t m_max_level;
  target_type_e m_target_type;
  mm_io_c *m_io_target;
  std::stringstream m_buffer;

public:
  ebml_dumper_c();

  ebml_dumper_c &values(bool p_values);
  ebml_dumper_c &addresses(bool p_addresses);
  ebml_dumper_c &indexes(bool p_indexes);
  ebml_dumper_c &max_level(int p_max_level);
  ebml_dumper_c &target(target_type_e p_target_type, mm_io_c *p_io_target = nullptr);

  ebml_dumper_c &dump(libebml::EbmlElement const *element);
  ebml_dumper_c &dump_if(bool do_it, libebml::EbmlElement const *element) {
    return do_it ? dump(element) : *this;
  }

private:
  void dump_impl(libebml::EbmlElement const *element, size_t level, size_t index);
  std::string to_string(libebml::EbmlElement const *element) const;
};

void dump_ebml_elements(libebml::EbmlElement *element, bool with_values = false);

#endif  // MTX_COMMON_DEBUGGING_H
