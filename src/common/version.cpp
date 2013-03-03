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

# include "common/compression.h"
# include "common/curl.h"
#endif  // defined(HAVE_CURL_EASY_H)

#include "common/debugging.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "common/version.h"

#define VERSIONNAME "Old Devil"

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

  boost::smatch matches;
  if (!boost::regex_search(s, matches, s_version_number_re))
    return;

  size_t idx;
  for (idx = 1; 5 >= idx; ++idx)
    if (!matches[idx].str().empty())
      parse_number(matches[idx].str(), parts[idx - 1]);

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
static mtx::xml::document_cptr
retrieve_and_parse_xml(std::string const &url) {
  bool debug = debugging_requested("version_check|releases_info|curl");

  std::string data;
  auto result = url_retriever_c().set_timeout(10, 20).retrieve(url, data);

  if (0 != result) {
    mxdebug_if(debug, boost::format("CURL error for %2%: %1%\n") % static_cast<unsigned int>(result) % url);
    return mtx::xml::document_cptr();
  }

  try {
    data = compressor_c::create_from_file_name(url)->decompress(data);

    mtx::xml::document_cptr doc(new pugi::xml_document);
    std::stringstream sdata(data);
    auto xml_result = doc->load(sdata);

    if (xml_result) {
      mxdebug_if(debug, boost::format("Doc loaded fine from %1%\n") % url);
      return doc;

    } else
      mxdebug_if(debug, boost::format("Doc load error for %1%: %1% at %2%\n") % url % xml_result.description() % xml_result.offset);

  } catch (mtx::compression_x &ex) {
    mxdebug_if(debug, boost::format("Decompression exception for %2%: %1%\n") % ex.what() % url);
  }

  return mtx::xml::document_cptr();
}

mtx_release_version_t
get_latest_release_version() {
  bool debug      = debugging_requested("version_check|curl");
  std::string url = MTX_VERSION_CHECK_URL;
  debugging_requested("version_check_url", &url);

  mxdebug_if(debug, boost::format("Update check started with URL %1%\n") % url);

  mtx_release_version_t release;

  auto doc = retrieve_and_parse_xml(url + ".gz");
  if (!doc)
    doc = retrieve_and_parse_xml(url);
  if (!doc)
    return release;

  release.latest_source                   = version_number_t(doc->select_single_node("/mkvtoolnix-releases/latest-source/version").node().child_value());
  release.latest_windows_build            = version_number_t((boost::format("%1% build %2%")
                                                             % doc->select_single_node("/mkvtoolnix-releases/latest-windows-pre/version").node().child_value()
                                                             % doc->select_single_node("/mkvtoolnix-releases/latest-windows-pre/build").node().child_value()).str());
  release.valid                           = release.latest_source.valid;
  release.urls["general"]                 = doc->select_single_node("/mkvtoolnix-releases/latest-source/url").node().child_value();
  release.urls["source_code"]             = doc->select_single_node("/mkvtoolnix-releases/latest-source/source-code-url").node().child_value();
  release.urls["windows_pre_build"]       = doc->select_single_node("/mkvtoolnix-releases/latest-windows-pre/url").node().child_value();
  for (auto arch : std::vector<std::string>{ "x86", "amd64" })
    for (auto package : std::vector<std::string>{ "installer", "portable" })
      release.urls[std::string{"windows_"} + arch + "_" + package] = doc->select_single_node((std::string{"/mkvtoolnix-releases/latest-windows-binary/"} + package + "-url/" + arch).c_str()).node().child_value();

  if (debug) {
    std::stringstream urls;
    brng::for_each(release.urls, [&](std::pair<std::string, std::string> const &kv) { urls << " " << kv.first << ":" << kv.second; });
    mxdebug(boost::format("update check: current %1% latest source %2% latest winpre %3% URLs%4%\n")
            % release.current_version.to_string() % release.latest_source.to_string() % release.latest_windows_build.to_string() % urls);
  }

  return release;
}

mtx::xml::document_cptr
get_releases_info() {
  std::string url = MTX_RELEASES_INFO_URL;
  debugging_requested("releases_info_url", &url);

  return retrieve_and_parse_xml(url + ".gz");
}
#endif  // defined(HAVE_CURL_EASY_H)
