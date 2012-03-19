/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   even easier interface to CURL

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "config.h"

#if defined(HAVE_CURL_EASY_H)

# include "common/common_pch.h"

# include "common/curl.h"
# include "common/strings/parsing.h"

# include <curl/easy.h>

static size_t
curl_write_data_cb(void *buffer,
                   size_t size,
                   size_t nmemb,
                   void *user_data) {
  static_cast<url_retriever_c *>(user_data)->add_data(std::string(static_cast<char *>(buffer), size * nmemb));
  return size * nmemb;
}

static size_t
curl_header_cb(void *buffer,
               size_t size,
               size_t nmemb,
               void *user_data) {
  static_cast<url_retriever_c *>(user_data)->parse_header(std::string(static_cast<char *>(buffer), size * nmemb));
  return size * nmemb;
}

url_retriever_c::url_retriever_c()
  : m_connect_timeout(10)
  , m_download_timeout(0)
  , m_total_size(-1)
  , m_debug(debugging_requested("curl|url_retriever"))
{
}

url_retriever_c &
url_retriever_c::set_timeout(int connect_timeout,
                             int download_timeout) {
  m_connect_timeout  = connect_timeout;
  m_download_timeout = download_timeout;
  return *this;
}

url_retriever_c &
url_retriever_c::set_progress_cb(progress_cb_t const &progress_cb) {
  m_progress_cb = progress_cb;
  return *this;
}

CURLcode
url_retriever_c::retrieve(std::string const &url,
                          std::string &data) {
  m_data       = &data;
  m_total_size = -1;

  curl_global_init(CURL_GLOBAL_ALL);
  CURL *handle = curl_easy_init();

  curl_easy_setopt(handle, CURLOPT_VERBOSE,        0);
  curl_easy_setopt(handle, CURLOPT_NOPROGRESS,     1);
  curl_easy_setopt(handle, CURLOPT_TIMEOUT,        m_download_timeout);
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, m_connect_timeout);
  curl_easy_setopt(handle, CURLOPT_URL,            url.c_str());
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION,  curl_write_data_cb);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA,      this);
  curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, curl_header_cb);
  curl_easy_setopt(handle, CURLOPT_HEADERDATA,     this);

  CURLcode result = curl_easy_perform(handle);

  curl_easy_cleanup(handle);

  return result;
}

void
url_retriever_c::add_data(std::string const &data) {
  *m_data += data;

  mxdebug_if(m_debug, boost::format("Data received: %1% bytes, now %2% total\n") % data.size() % m_data->size());

  if (m_progress_cb)
    m_progress_cb(m_data->size(), m_total_size);
}

void
url_retriever_c::parse_header(std::string const &header) {
  boost::smatch matches;
  if (boost::regex_search(header, matches, boost::regex("^content-length\\s*:\\s*(\\d+)", boost::regex::perl | boost::regex::icase)))
    parse_int(matches[1].str(), m_total_size);
  mxdebug_if(m_debug, boost::format("Header received: %1% total size: %2%\n") % header % m_total_size);
}

#endif  // defined(HAVE_CURL_EASY_H)
