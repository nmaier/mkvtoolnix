/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   even easier interface to CURL

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if defined(HAVE_CURL_EASY_H)

# include "common/curl.h"

# include <curl/easy.h>

static size_t
curl_write_data_cb(void *buffer,
                   size_t size,
                   size_t nmemb,
                   void *user_data) {
  std::string *s = static_cast<std::string *>(user_data);
  *s += std::string(static_cast<char *>(buffer), size * nmemb);
  return size * nmemb;
}

CURLcode
retrieve_via_curl(const std::string &url,
                  std::string &data,
                  int connect_timeout) {
  curl_global_init(CURL_GLOBAL_ALL);
  CURL *handle = curl_easy_init();

  curl_easy_setopt(handle, CURLOPT_VERBOSE,        0);
  curl_easy_setopt(handle, CURLOPT_NOPROGRESS,     1);
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, connect_timeout);
  curl_easy_setopt(handle, CURLOPT_URL,            url.c_str());
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION,  curl_write_data_cb);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA,      &data);

  CURLcode result = curl_easy_perform(handle);

  curl_easy_cleanup(handle);

  return result;
}

#endif  // defined(HAVE_CURL_EASY_H)
