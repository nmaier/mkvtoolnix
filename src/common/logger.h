/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX__COMMON_CLI_PARSER_H
#define MTX__COMMON_CLI_PARSER_H

#include "common/common_pch.h"

#include "common/strings/utf8.h"

class logger_c;
typedef std::shared_ptr<logger_c> logger_cptr;

class logger_c {
private:
  bfs::path m_file_name;
  int64_t m_log_start;

public:
  logger_c(bfs::path const &file_name);

  void log(std::string const &message);
  void log(boost::format const &message) {
    log(message.str());
  }

private:
  static logger_cptr s_default_logger;
public:
  static logger_c &get_default_logger();
  static void set_default_logger(logger_cptr logger);
};

template<typename T>
logger_c &
operator <<(logger_c &logger,
            T const &message) {
  logger.log(to_utf8(message));
  return logger;
}

#define log_current_location() logger_c::get_default_logger() << (boost::format("Current file & line: %1%:%2%") % __FILE__ % __LINE__)
#define log_it(arg)            logger_c::get_default_logger() << arg

#endif // MTX__COMMON_CLI_PARSER_H
