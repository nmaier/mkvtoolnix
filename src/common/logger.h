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
};

template<typename T>
logger_c &
operator <<(logger_c &logger,
            T const &message) {
  logger.log(message);
  return logger;
}

#endif // MTX__COMMON_CLI_PARSER_H
