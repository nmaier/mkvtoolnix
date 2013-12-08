/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   even easier interface to CURL

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_CURL_H
# define MTX_COMMON_CURL_H

# include "common/common_pch.h"

# if defined(HAVE_CURL_EASY_H)

#  if defined(SYS_WINDOWS)
#   include <ws2tcpip.h>
#  endif  // defined(SYS_WINDOWS)
#  include <curl/curl.h>
#  include <string>

class url_retriever_c {
public:
  typedef std::function<void(int64_t, int64_t)> progress_cb_t;

protected:
  progress_cb_t m_progress_cb;
  std::string *m_data;
  int m_connect_timeout, m_download_timeout;
  int64_t m_total_size;
  debugging_option_c m_debug;

public:
  url_retriever_c();

  url_retriever_c &set_timeout(int connect_timeout = 10, int download_timeout = 0);
  url_retriever_c &set_progress_cb(progress_cb_t const &progress_cb);

  CURLcode retrieve(std::string const &url, std::string &data);

  void add_data(std::string const &data);
  void parse_header(std::string const &header);
};

# endif  // defined(HAVE_CURL_EASY_H)

#endif  // MTX_COMMON_CURL_H
