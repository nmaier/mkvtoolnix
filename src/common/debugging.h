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

#endif  // MTX_COMMON_DEBUGGING_H
