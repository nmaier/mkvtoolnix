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

#include <boost/regex.hpp>

#if defined(HAVE_CURL_EASY_H)
# include <boost/property_tree/ptree.hpp>
# include <boost/property_tree/xml_parser.hpp>
# include <sstream>

# include "common/curl.h"
#endif  // defined(HAVE_CURL_EASY_H)

#include "common/debugging.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "common/version.h"

#define VERSIONNAME "Ich will"

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
                 bool full) {
  std::string short_version_info;
  if (!program.empty())
    short_version_info += program + " ";
  short_version_info += (boost::format("v%1% ('%2%')") % VERSION % VERSIONNAME).str();
#if !defined(HAVE_BUILD_TIMESTAMP)
  return short_version_info;
#else  // !defined(HAVE_BUILD_TIMESTAMP)
  if (!full)
    return short_version_info;

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
  if (debugging_requested("version_check"))
    mxinfo(boost::format("Update check started with URL %1%\n") % MTX_VERSION_CHECK_URL);

  mtx_release_version_t release;
  std::string data;
  CURLcode result = retrieve_via_curl(MTX_VERSION_CHECK_URL, data);

  if (0 != result) {
    if (debugging_requested("version_check"))
      mxinfo(boost::format("Update check CURL error: %1%\n") % static_cast<unsigned int>(result));
    return release;
  }

  if (debugging_requested("version_check"))
    mxinfo(boost::format("Update check OK; data length %1%\n") % data.length());

  try {
    std::stringstream data_in(data);
    boost::property_tree::ptree pt;
    boost::property_tree::read_xml(data_in, pt);

    release.latest_source              = version_number_t(pt.get("mkvtoolnix-releases.latest-source.version",      std::string("")));
    release.latest_windows_build       = version_number_t(pt.get("mkvtoolnix-releases.latest-windows-pre.version", std::string(""))
                                                          + " build " +
                                                          pt.get("mkvtoolnix-releases.latest-windows-pre.build",   std::string("")));
    release.source_download_url        =                  pt.get("mkvtoolnix-releases.latest-source.url",          std::string(""));
    release.windows_build_download_url =                  pt.get("mkvtoolnix-releases.latest-windows-pre.url",     std::string(""));
    release.valid                      = release.latest_source.valid;

  } catch (boost::property_tree::ptree_error &error) {
    release.valid = false;
  }

  if (debugging_requested("version_check"))
    mxinfo(boost::format("update check: current %1% latest source %2% latest winpre %3%\n")
           % release.current_version.to_string() % release.latest_source.to_string() % release.latest_windows_build.to_string());

  return release;
}
#endif  // defined(HAVE_CURL_EASY_H)
