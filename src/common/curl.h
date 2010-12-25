/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   even easier interface to CURL

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_CURL_H
# define __MTX_COMMON_CURL_H

# include "common/common_pch.h"

# if defined(HAVE_CURL_EASY_H)

#  if defined(SYS_WINDOWS)
#   include <ws2tcpip.h>
#  endif  // defined(SYS_WINDOWS)
#  include <curl/curl.h>
#  include <string>

CURLcode retrieve_via_curl(const std::string &url, std::string &data, int connect_timeout = 10);

# endif  // defined(HAVE_CURL_EASY_H)

#endif  // __MTX_COMMON_CURL_H
