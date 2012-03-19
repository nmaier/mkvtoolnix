/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   version information

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if defined(HAVE_CURL_EASY_H)
# include <sstream>

# include "common/curl.h"
# include "pugixml.hpp"
#endif  // defined(HAVE_CURL_EASY_H)

#include "common/debugging.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "common/version.h"

#define VERSIONNAME "Piper"

version_number_t::version_number_t()
  : valid(false)
{
  memset(parts, 0, 5 * sizeof(unsigned int));
}

version_number_t::version_number_t(const std::string &s)
  : valid(false)
{
  memset(parts, 0, 5 * sizeof(unsigned int));

  if (debugging_requested("version_check"))
    mxinfo(boost::format("version check: Parsing %1%\n") % s);

  // Match the following:
  // 4.4.0
  // 4.4.0.5 build 123
  // 4.4.0-build20101201-123
  // mkvmerge v4.4.0
  // * Optional prefix "mkvmerge v"
  // * At least three digits separated by dots
  // * Optional fourth digit separated by a dot
  // * Optional build number that can have two forms:
  //   - " build nnn"
  //   - "-buildYYYYMMDD-nnn" (date is ignored)
  static boost::regex s_version_number_re("^ (?: mkv[a-z]+ \\s+ v)?"     // Optional prefix mkv... v
                                          "(\\d+) \\. (\\d+) \\. (\\d+)" // Three digitss separated by dots; $1 - $3
                                          "(?: \\. (\\d+) )?"            // Optional fourth digit separated by a dot; $4
                                          "(?:"                          // Optional build number including its prefix
                                          " (?: \\s* build \\s*"         //   Build number prefix: either " build " or...
                                          "  |  - build \\d{8} - )"      //   ... "-buildYYYYMMDD-"
                                          " (\\d+)"                      //   The build number itself; $5
                                          ")?",
                                          boost::regex::perl | boost::regex::mod_x);

  boost::match_results<std::string::const_iterator> matches;
  if (!boost::regex_search(s, matches, s_version_number_re))
    return;

  size_t idx;
  for (idx = 1; 5 >= idx; ++idx)
    if (!matches[idx].str().empty())
      parse_uint(matches[idx].str(), parts[idx - 1]);

  valid = true;

  if (debugging_requested("version_check"))
    mxinfo(boost::format("version check: parse OK; result: %1%\n") % to_string());
}

version_number_t::version_number_t(const version_number_t &v) {
  memcpy(parts, v.parts, 5 * sizeof(unsigned int));
  valid = v.valid;
}

int
version_number_t::compare(const version_number_t &cmp)
  const
{
  size_t idx;
  for (idx = 0; 5 > idx; ++idx)
    if (parts[idx] < cmp.parts[idx])
      return -1;
    else if (parts[idx] > cmp.parts[idx])
      return 1;
  return 0;
}

bool
version_number_t::operator <(const version_number_t &cmp)
  const
{
  return compare(cmp) == -1;
}

std::string
version_number_t::to_string()
  const
{
  if (!valid)
    return "<invalid>";

  std::string v = ::to_string(parts[0]) + "." + ::to_string(parts[1]) + "." + ::to_string(parts[2]);
  if (0 != parts[3])
    v += "." + ::to_string(parts[3]);
  if (0 != parts[4])
    v += " build " + ::to_string(parts[4]);

  return v;
}

mtx_release_version_t::mtx_release_version_t()
  : current_version(get_current_version())
  , valid(false)
{
}

std::string
get_version_info(const std::string &program,
                 version_info_flags_e flags) {
  std::string short_version_info;
  if (!program.empty())
    short_version_info += program + " ";
  short_version_info += (boost::format("v%1% ('%2%')") % VERSION % VERSIONNAME).str();
#if !defined(HAVE_BUILD_TIMESTAMP)
  return short_version_info;
#else  // !defined(HAVE_BUILD_TIMESTAMP)
  if (!(flags & vif_full))
    return short_version_info;

  if (flags & vif_untranslated)
    return (boost::format("%1% built on %2% %3%") % short_version_info % __DATE__ % __TIME__).str();

  return (boost::format(Y("%1% built on %2% %3%")) % short_version_info % __DATE__ % __TIME__).str();
#endif  // !defined(HAVE_BUILD_TIMESTAMP)
}

int
compare_current_version_to(const std::string &other_version_str) {
  return version_number_t(VERSION).compare(version_number_t(other_version_str));
}

version_number_t
get_current_version() {
  return version_number_t(VERSION);
}

#if defined(HAVE_CURL_EASY_H)
mtx_release_version_t
get_latest_release_version() {
  bool debug = debugging_requested("version_check");

  std::string url = MTX_VERSION_CHECK_URL;
  debugging_requested("version_check_url", &url);

  mxdebug_if(debug, boost::format("Update check started with URL %1%\n") % url);

  mtx_release_version_t release;
  std::string data;
  url_retriever_c retriever;
  CURLcode result = retriever.retrieve(url, data);

  if (0 != result) {
    mxdebug_if(debug, boost::format("Update check CURL error: %1%\n") % static_cast<unsigned int>(result));
    return release;
  }

  mxdebug_if(debug, boost::format("Update check OK; data length %1%\n") % data.length());

  try {
    pugi::xml_document doc;
    std::stringstream sdata(data);
    auto result = doc.load(sdata);

    if (!result) {
      mxdebug_if(debug, boost::format("XML parsing failed: %1% at %2%\n") % result.description() % result.offset);
      return release;
    }

    release.latest_source              = version_number_t(doc.select_single_node("/mkvtoolnix-releases/latest-source/version").node().child_value());
    release.latest_windows_build       = version_number_t((boost::format("%1% build %2%")
                                                           % doc.select_single_node("/mkvtoolnix-releases/latest-windows-pre/version").node().child_value()
                                                           % doc.select_single_node("/mkvtoolnix-releases/latest-windows-pre/build").node().child_value()).str());
    release.source_download_url        = doc.select_single_node("/mkvtoolnix-releases/latest-source/url").node().child_value();
    release.windows_build_download_url = doc.select_single_node("/mkvtoolnix-releases/latest-windows-pre/url").node().child_value();
    release.valid                      = release.latest_source.valid;

  } catch (pugi::xpath_exception &ex) {
    mxdebug_if(debug, boost::format("XPath exception: %1% / %2%\n") % ex.what() % ex.result().description());
    release.valid = false;
  }

  mxdebug_if(debug, boost::format("update check: current %1% latest source %2% latest winpre %3%\n") % release.current_version.to_string() % release.latest_source.to_string() % release.latest_windows_build.to_string());

  return release;
}
#endif  // defined(HAVE_CURL_EASY_H)
