/** \brief command line parsing

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
   \author Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/common_pch.h"

#include <errno.h>
#include <ctype.h>
#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
#include <signal.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(SYS_WINDOWS)
#include <windows.h>
#endif

#include <algorithm>
#include <iostream>
#include <list>
#include <sstream>
#include <tuple>
#include <typeinfo>

#include <matroska/KaxChapters.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>

#include "common/chapters/chapters.h"
#include "common/command_line.h"
#include "common/ebml.h"
#include "common/extern_data.h"
#include "common/file_types.h"
#include "common/fs_sys_helpers.h"
#include "common/iso639.h"
#include "common/mm_io.h"
#include "common/segmentinfo.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "common/unique_numbers.h"
#include "common/version.h"
#include "common/webm.h"
#include "common/xml/ebml_segmentinfo_converter.h"
#include "common/xml/ebml_tags_converter.h"
#include "merge/cluster_helper.h"
#include "merge/mkvmerge.h"
#include "merge/output_control.h"

using namespace libmatroska;

/** \brief Outputs usage information
*/
static void
set_usage() {
  usage_text  =   "";
  usage_text += Y("mkvmerge -o out [global options] [options1] <file1> [@optionsfile ...]\n");
  usage_text +=   "\n";
  usage_text += Y(" Global options:\n");
  usage_text += Y("  -v, --verbose            verbose status\n");
  usage_text += Y("  -q, --quiet              suppress status output\n");
  usage_text += Y("  -o, --output out         Write to the file 'out'.\n");
  usage_text += Y("  -w, --webm               Create WebM compliant file.\n");
  usage_text += Y("  --title <title>          Title for this output file.\n");
  usage_text += Y("  --global-tags <file>     Read global tags from a XML file.\n");
  usage_text +=   "\n";
  usage_text += Y(" Chapter handling:\n");
  usage_text += Y("  --chapters <file>        Read chapter information from the file.\n");
  usage_text += Y("  --chapter-language <lng> Set the 'language' element in chapter entries.\n");
  usage_text += Y("  --chapter-charset <cset> Charset for a simple chapter file.\n");
  usage_text += Y("  --cue-chapter-name-format <format>\n"
                  "                           Pattern for the conversion from CUE sheet\n"
                  "                           entries to chapter names.\n");
  usage_text += Y("  --default-language <lng> Use this language for all tracks unless\n"
                  "                           overridden with the --language option.\n");
  usage_text +=   "\n";
  usage_text += Y(" Segment info handling:\n");
  usage_text += Y("  --segmentinfo <file>     Read segment information from the file.\n");
  usage_text += Y("  --segment-uid <SID1,[SID2...]>\n"
                  "                           Set the segment UIDs to SID1, SID2 etc.\n");
  usage_text +=   "\n";
  usage_text += Y(" General output control (advanced global options):\n");
  usage_text += Y("  --track-order <FileID1:TID1,FileID2:TID2,FileID3:TID3,...>\n"
                  "                           A comma separated list of both file IDs\n"
                  "                           and track IDs that controls the order of the\n"
                  "                           tracks in the output file.\n");
  usage_text += Y("  --cluster-length <n[ms]> Put at most n data blocks into each cluster.\n"
                  "                           If the number is postfixed with 'ms' then\n"
                  "                           put at most n milliseconds of data into each\n"
                  "                           cluster.\n");
  usage_text += Y("  --no-cues                Do not write the cue data (the index).\n");
  usage_text += Y("  --clusters-in-meta-seek  Write meta seek data for clusters.\n");
  usage_text += Y("  --disable-lacing         Do not Use lacing.\n");
  usage_text += Y("  --enable-durations       Enable block durations for all blocks.\n");
  usage_text += Y("  --timecode-scale <n>     Force the timecode scale factor to n.\n");
  usage_text +=   "\n";
  usage_text += Y(" File splitting, linking, appending and concatenating (more global options):\n");
  usage_text += Y("  --split <d[K,M,G]|HH:MM:SS|s>\n"
                  "                           Create a new file after d bytes (KB, MB, GB)\n"
                  "                           or after a specific time.\n");
  usage_text += Y("  --split timecodes:A[,B...]\n"
                  "                           Create a new file after each timecode A, B\n"
                  "                           etc.\n");
  usage_text += Y("  --split parts:start1-end2[,[+]start2-end2,...]\n"
                  "                           Keep ranges of timecodes start-end, either in separate\n"
                  "                           files or append to previous range's file.\n");
  usage_text += Y("  --split-max-files <n>    Create at most n files.\n");
  usage_text += Y("  --link                   Link splitted files.\n");
  usage_text += Y("  --link-to-previous <SID> Link the first file to the given SID.\n");
  usage_text += Y("  --link-to-next <SID>     Link the last file to the given SID.\n");
  usage_text += Y("  --append-to <SFID1:STID1:DFID1:DTID1,SFID2:STID2:DFID2:DTID2,...>\n"
                  "                           A comma separated list of file and track IDs\n"
                  "                           that controls which track of a file is\n"
                  "                           appended to another track of the preceding\n"
                  "                           file.\n");
  usage_text += Y("  --append-mode <file|track>\n"
                  "                           Selects how mkvmerge calculates timecodes when\n"
                  "                           appending files.\n");
  usage_text += Y("  <file1> + <file2>        Append file2 to file1.\n");
  usage_text += Y("  <file1> +<file2>         Same as \"<file1> + <file2>\".\n");
  usage_text += Y("  = <file>                 Don't look for and concatenate files with the same\n"
                  "                           base name but with a different trailing number.\n");
  usage_text += Y("  =<file>                  Same as \"= <file>\".\n");
  usage_text += Y("  ( <file1> <file2> )      Treat file1 and file2 as if they were concatenated\n"
                  "                           into a single big file.\n");
  usage_text +=   "\n";
  usage_text += Y(" Attachment support (more global options):\n");
  usage_text += Y("  --attachment-description <desc>\n"
                  "                           Description for the following attachment.\n");
  usage_text += Y("  --attachment-mime-type <mime type>\n"
                  "                           Mime type for the following attachment.\n");
  usage_text += Y("  --attachment-name <name> The name should be stored for the \n"
                  "                           following attachment.\n");
  usage_text += Y("  --attach-file <file>     Creates a file attachment inside the\n"
                  "                           Matroska file.\n");
  usage_text += Y("  --attach-file-once <file>\n"
                  "                           Creates a file attachment inside the\n"
                  "                           first Matroska file written.\n");
  usage_text +=   "\n";
  usage_text += Y(" Options for each input file:\n");
  usage_text += Y("  -a, --audio-tracks <n,m,...>\n"
                  "                           Copy audio tracks n,m etc. Default: copy all\n"
                  "                           audio tracks.\n");
  usage_text += Y("  -A, --no-audio           Don't copy any audio track from this file.\n");
  usage_text += Y("  -d, --video-tracks <n,m,...>\n"
                  "                           Copy video tracks n,m etc. Default: copy all\n"
                  "                           video tracks.\n");
  usage_text += Y("  -D, --no-video           Don't copy any video track from this file.\n");
  usage_text += Y("  -s, --subtitle-tracks <n,m,...>\n"
                  "                           Copy subtitle tracks n,m etc. Default: copy\n"
                  "                           all subtitle tracks.\n");
  usage_text += Y("  -S, --no-subtitles       Don't copy any subtitle track from this file.\n");
  usage_text += Y("  -b, --button-tracks <n,m,...>\n"
                  "                           Copy buttons tracks n,m etc. Default: copy\n"
                  "                           all buttons tracks.\n");
  usage_text += Y("  -B, --no-buttons         Don't copy any buttons track from this file.\n");
  usage_text += Y("  -m, --attachments <n[:all|first],m[:all|first],...>\n"
                  "                           Copy the attachments with the IDs n, m etc to\n"
                  "                           all or only the first output file. Default: copy\n"
                  "                           all attachments to all output files.\n");
  usage_text += Y("  -M, --no-attachments     Don't copy attachments from a source file.\n");
  usage_text += Y("  -t, --tags <TID:file>    Read tags for the track from a XML file.\n");
  usage_text += Y("  --track-tags <n,m,...>   Copy the tags for tracks n,m etc. Default: copy\n"
                  "                           tags for all tracks.\n");
  usage_text += Y("  -T, --no-track-tags      Don't copy tags for tracks from the source file.\n");
  usage_text += Y("  --no-global-tags         Don't keep global tags from the source file.\n");
  usage_text += Y("  --no-chapters            Don't keep chapters from the source file.\n");
  usage_text += Y("  -y, --sync <TID:d[,o[/p]]>\n"
                  "                           Synchronize, adjust the track's timecodes with\n"
                  "                           the id TID by 'd' ms.\n"
                  "                           'o/p': Adjust the timecodes by multiplying with\n"
                  "                           'o/p' to fix linear drifts. 'p' defaults to\n"
                  "                           1 if omitted. Both 'o' and 'p' can be\n"
                  "                           floating point numbers.\n");
  usage_text += Y("  --default-track <TID[:bool]>\n"
                  "                           Sets the 'default' flag for this track or\n"
                  "                           forces it not to be present if bool is 0.\n");
  usage_text += Y("  --forced-track <TID[:bool]>\n"
                  "                           Sets the 'forced' flag for this track or\n"
                  "                           forces it not to be present if bool is 0.\n");
  usage_text += Y("  --blockadd <TID:x>       Sets the max number of block additional\n"
                  "                           levels for this track.\n");
  usage_text += Y("  --track-name <TID:name>  Sets the name for a track.\n");
  usage_text += Y("  --cues <TID:none|iframes|all>\n"
                  "                           Create cue (index) entries for this track:\n"
                  "                           None at all, only for I frames, for all.\n");
  usage_text += Y("  --language <TID:lang>    Sets the language for the track (ISO639-2\n"
                  "                           code, see --list-languages).\n");
  usage_text += Y("  --aac-is-sbr <TID[:0|1]> The track with the ID is HE-AAC/AAC+/SBR-AAC\n"
                  "                           or not. The value ':1' can be omitted.\n");
  usage_text += Y("  --timecodes <TID:file>   Read the timecodes to be used from a file.\n");
  usage_text += Y("  --default-duration <TID:Xs|ms|us|ns|fps>\n"
                  "                           Force the default duration of a track to X.\n"
                  "                           X can be a floating point number or a fraction.\n");
  usage_text += Y("  --nalu-size-length <TID:n>\n"
                  "                           Force the NALU size length to n bytes with\n"
                  "                           2 <= n <= 4 with 4 being the default.\n");
  usage_text +=   "\n";
  usage_text += Y(" Options that only apply to video tracks:\n");
  usage_text += Y("  -f, --fourcc <FOURCC>    Forces the FourCC to the specified value.\n"
                  "                           Works only for video tracks.\n");
  usage_text += Y("  --aspect-ratio <TID:f|a/b>\n"
                  "                           Sets the display dimensions by calculating\n"
                  "                           width and height for this aspect ratio.\n");
  usage_text += Y("  --aspect-ratio-factor <TID:f|a/b>\n"
                  "                           First calculates the aspect ratio by multi-\n"
                  "                           plying the video's original aspect ratio\n"
                  "                           with this factor and calculates the display\n"
                  "                           dimensions from this factor.\n");
  usage_text += Y("  --display-dimensions <TID:width>x<height>\n"
                  "                           Explicitly set the display dimensions.\n");
  usage_text += Y("  --cropping <TID:left,top,right,bottom>\n"
                  "                           Sets the cropping parameters.\n");
  usage_text += Y("  --stereo-mode <TID:n|keyword>\n"
                  "                           Sets the stereo mode parameter. It can\n"
                  "                           either be a number 0 - 14 or a keyword\n"
                  "                           (see documentation for the full list).\n");
  usage_text +=   "\n";
  usage_text += Y(" Options that only apply to text subtitle tracks:\n");
  usage_text += Y("  --sub-charset <TID:charset>\n"
                  "                           Determines the charset the text subtitles are\n"
                  "                           read as for the conversion to UTF-8.\n");
  usage_text +=   "\n";
  usage_text += Y(" Options that only apply to VobSub subtitle tracks:\n");
  usage_text += Y("  --compression <TID:method>\n"
                  "                           Sets the compression method used for the\n"
                  "                           specified track ('none' or 'zlib').\n");
  usage_text +=   "\n\n";
  usage_text += Y(" Other options:\n");
  usage_text += Y("  -i, --identify <file>    Print information about the source file.\n");
  usage_text += Y("  -l, --list-types         Lists supported input file types.\n");
  usage_text += Y("  --list-languages         Lists all ISO639 languages and their\n"
                  "                           ISO639-2 codes.\n");
  usage_text += Y("  --capabilities           Lists optional features mkvmerge was compiled with.\n");
  usage_text += Y("  --priority <priority>    Set the priority mkvmerge runs with.\n");
  usage_text += Y("  --ui-language <code>     Force the translations for 'code' to be used.\n");
  usage_text += Y("  --command-line-charset <charset>\n"
                  "                           Charset for strings on the command line\n");
  usage_text += Y("  --output-charset <cset>  Output messages in this charset\n");
  usage_text += Y("  -r, --redirect-output <file>\n"
                  "                           Redirects all messages into this file.\n");
  usage_text += Y("  --debug <topic>          Turns on debugging output for 'topic'.\n");
  usage_text += Y("  --engage <feature>       Turns on experimental feature 'feature'.\n");
  usage_text += Y("  @optionsfile             Reads additional command line options from\n"
                  "                           the specified file (see man page).\n");
  usage_text += Y("  -h, --help               Show this help.\n");
  usage_text += Y("  -V, --version            Show version information.\n");
#if defined(HAVE_CURL_EASY_H)
  usage_text += std::string("  --check-for-updates      ") + Y("Check online for the latest release.") + "\n";
#endif
  usage_text +=   "\n\n";
  usage_text += Y("Please read the man page/the HTML documentation to mkvmerge. It\n"
                  "explains several details in great length which are not obvious from\n"
                  "this listing.\n");

  version_info = get_version_info("mkvmerge", vif_full);
}

/** \brief Prints information about what has been compiled into mkvmerge
*/
static void
print_capabilities() {
  mxinfo(boost::format("VERSION=%1%\n") % version_info);
#if defined(HAVE_BZLIB_H)
  mxinfo("BZ2\n");
#endif
#if defined(HAVE_LZO)
  mxinfo("LZO\n");
#endif
#if defined(HAVE_FLAC_FORMAT_H)
  mxinfo("FLAC\n");
#endif
}

int64_t
create_track_number(generic_reader_c *reader,
                    int64_t tid) {
  // Specs say that track numbers should start at 1.
  static int s_track_number = 1;

  bool found   = false;
  int file_num = -1;
  size_t i;
  for (i = 0; i < g_files.size(); i++)
    if (g_files[i].reader == reader) {
      found = true;
      file_num = i;
      break;
    }

  if (!found)
    mxerror(boost::format(Y("create_track_number: file_num not found. %1%\n")) % BUGMSG);

  int64_t tnum = -1;
  found        = false;
  for (i = 0; i < g_track_order.size(); i++)
    if ((g_track_order[i].file_id == file_num) &&
        (g_track_order[i].track_id == tid)) {
      found = true;
      tnum = i + 1;
      break;
    }
  if (found) {
    found = false;
    for (i = 0; i < g_packetizers.size(); i++)
      if ((g_packetizers[i].packetizer != nullptr) &&
          (g_packetizers[i].packetizer->get_track_num() == tnum)) {
        tnum = s_track_number;
        break;
      }
  } else
    tnum = s_track_number;

  if (tnum >= s_track_number)
    s_track_number = tnum + 1;

  return tnum;
}

static std::string
guess_mime_type_and_report(std::string file_name) {
  std::string mime_type = guess_mime_type(file_name, true);
  if (mime_type != "") {
    mxinfo(boost::format(Y("Automatic MIME type recognition for '%1%': %2%\n")) % file_name % mime_type);
    return mime_type;
  }

  mxerror(boost::format(Y("No MIME type has been set for the attachment '%1%', and it could not be guessed.\n")) % file_name);
  return "";
}

static void
handle_segmentinfo() {
  // segment families
  KaxSegmentFamily *family = FINDFIRST(g_kax_info_chap.get(), KaxSegmentFamily);
  while (nullptr != family) {
    g_segfamily_uids.add_family_uid(*family);
    family = FINDNEXT(g_kax_info_chap.get(), KaxSegmentFamily, family);
  }

  EbmlBinary *uid = FINDFIRST(g_kax_info_chap.get(), KaxSegmentUID);
  if (nullptr != uid)
    g_forced_seguids.push_back(bitvalue_cptr(new bitvalue_c(*uid)));

  uid = FINDFIRST(g_kax_info_chap.get(), KaxNextUID);
  if (nullptr != uid)
    g_seguid_link_next = bitvalue_cptr(new bitvalue_c(*uid));

  uid = FINDFIRST(g_kax_info_chap.get(), KaxPrevUID);
  if (nullptr != uid)
    g_seguid_link_previous = bitvalue_cptr(new bitvalue_c(*uid));

  auto segment_filename = FindChild<KaxSegmentFilename>(g_kax_info_chap.get());
  if (segment_filename)
    g_segment_filename = UTFstring_to_cstrutf8(*segment_filename);

  auto next_segment_filename = FindChild<KaxNextFilename>(g_kax_info_chap.get());
  if (next_segment_filename)
    g_next_segment_filename = UTFstring_to_cstrutf8(*next_segment_filename);

  auto previous_segment_filename = FindChild<KaxPrevFilename>(g_kax_info_chap.get());
  if (previous_segment_filename)
    g_previous_segment_filename = UTFstring_to_cstrutf8(*previous_segment_filename);
}

static void
list_file_types() {
  std::vector<file_type_t> &file_types = file_type_t::get_supported();

  mxinfo(Y("Supported file types:\n"));

  for (auto &file_type : file_types)
    mxinfo(boost::format("  %1% [%2%]\n") % file_type.title % file_type.extensions);
}

/** \brief Identify a file type and its contents

   This function called for \c --identify. It sets up dummy track info
   data for the reader, probes the input file, creates the file reader
   and calls its identify function.
*/
static void
identify(std::string &filename) {
  track_info_c ti;
  filelist_t file;

  if ('=' == filename[0]) {
    ti.m_disable_multi_file = true;
    filename                = filename.substr(1);
  }

  verbose             = 0;
  g_suppress_warnings = true;
  g_identifying       = true;
  file.name           = filename;
  file.all_names.push_back(filename);

  get_file_type(file);

  ti.m_fname = file.name;

  if (FILE_TYPE_IS_UNKNOWN == file.type)
    mxerror(boost::format(Y("File %1% has unknown type. Please have a look at the supported file types ('mkvmerge --list-types') and "
                            "contact the author Moritz Bunkus <moritz@bunkus.org> if your file type is supported but not recognized properly.\n"))
            % file.name);

  file.ti = new track_info_c(ti);

  g_files.push_back(file);

  create_readers();

  g_files[0].reader->identify();
  g_files[0].reader->display_identification_results();
}

/** \brief Parse a number postfixed with a time-based unit

   This function parsers a number that is postfixed with one of the
   four units 's', 'ms', 'us' or 'ns'. If the postfix is 'fps' then
   this means 'frames per second'. If the postfix is 'p' or 'i' then
   it is also interpreted as the number of 'progressive frames per
   second' or 'interlaced frames per second'.

   It returns a number of nanoseconds.
*/
int64_t
parse_number_with_unit(const std::string &s,
                       const std::string &subject,
                       const std::string &argument,
                       std::string display_s = "") {
  boost::regex re1("(-?\\d+\\.?\\d*)(s|ms|us|ns|fps|p|i)?",  boost::regex::perl | boost::regex::icase);
  boost::regex re2("(-?\\d+)/(-?\\d+)(s|ms|us|ns|fps|p|i)?", boost::regex::perl | boost::regex::icase);

  std::string unit, s_n, s_d;
  int64_t n, d;
  double d_value = 0.0;
  bool is_fraction = false;

  if (display_s.empty())
    display_s = s;

  boost::smatch matches;
  if (boost::regex_match(s, matches, re1)) {
    parse_double(matches[1], d_value);
    if (matches.size() > 2)
      unit = matches[2];
    d = 1;

  } else if (boost::regex_match(s, matches, re2)) {
    parse_int(matches[1], n);
    parse_int(matches[2], d);
    if (matches.size() > 3)
      unit = matches[3];
    is_fraction = true;

  } else
    mxerror(boost::format(Y("'%1%' is not a valid %2% in '%3% %4%'.\n")) % s % subject % argument % display_s);

  int64_t multiplier = 1000000000;
  balg::to_lower(unit);

  if (unit == "ms")
    multiplier = 1000000;
  else if (unit == "us")
    multiplier = 1000;
  else if (unit == "ns")
    multiplier = 1;
  else if ((unit == "fps") || (unit == "p") || (unit == "i")) {
    if (is_fraction)
      return 1000000000ll * d / n / (unit == "i" ? 2 : 1);

    if (unit == "i")
      d_value /= 2;

    if (29.97 == d_value)
      return (int64_t)(100100000.0 / 3.0);
    else if (23.976 == d_value)
      return (int64_t)(1001000000.0 / 24.0);
    else
      return (int64_t)(1000000000.0 / d_value);

  } else if (unit != "s")
    mxerror(boost::format(Y("'%1%' does not contain a valid unit ('s', 'ms', 'us' or 'ns') in '%2% %3%'.\n")) % s % argument % display_s);

  if (is_fraction)
    return multiplier * n / d;
  else
    return (int64_t)(multiplier * d_value);
}

/** \brief Parse tags and add them to the list of all tags

   Also tests the tags for missing mandatory elements.
*/
void
parse_and_add_tags(const std::string &file_name) {
  auto tags = mtx::xml::ebml_tags_converter_c::parse_file(file_name);

  for (auto element : *tags) {
    auto tag = dynamic_cast<KaxTag *>(element);
    if (nullptr != tag) {
      if (!tag->CheckMandatory())
        mxerror(boost::format(Y("Error parsing the tags in '%1%': some mandatory elements are missing.\n")) % file_name);
      add_tags(tag);
    }
  }

  tags->RemoveAll();
}

/** \brief Parse the \c --xtracks arguments

   The argument is a comma separated list of track IDs.
*/
static void
parse_arg_tracks(std::string s,
                 item_selector_c<bool> &tracks,
                 const std::string &opt) {
  tracks.clear();

  if (balg::starts_with(s, "!")) {
    s.erase(0, 1);
    tracks.set_reversed();
  }

  std::vector<std::string> elements = split(s, ",");
  strip(elements);

  size_t i;
  for (i = 0; i < elements.size(); i++) {
    int64_t tid;
    if (!parse_int(elements[i], tid))
      mxerror(boost::format(Y("Invalid track ID in '%1% %2%'.\n")) % opt % s);
    tracks.add(tid);
  }
}

/** \brief Parse the \c --sync argument

   The argument must have the form <tt>TID:d</tt> or
   <tt>TID:d,l1/l2</tt>, e.g. <tt>0:200</tt>.  The part before the
   comma is the displacement in ms.
   The optional part after comma is the linear factor which defaults
   to 1 if not given.
*/
static void
parse_arg_sync(std::string s,
               const std::string &opt,
               track_info_c &ti) {
  timecode_sync_t tcsync;

  // Extract the track number.
  std::string orig               = s;
  std::vector<std::string> parts = split(s, ":", 2);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid sync option. No track ID specified in '%1% %2%'.\n")) % opt % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '%1% %2%'.\n")) % opt % s);

  s = parts[1];
  if (s.size() == 0)
    mxerror(boost::format(Y("Invalid sync option specified in '%1% %2%'.\n")) % opt % orig);

  if (parts[1] == "reset") {
    ti.m_reset_timecodes_specs[id] = true;
    return;
  }

  // Now parse the actual sync values.
  int idx = s.find(',');
  if (idx >= 0) {
    std::string linear = s.substr(idx + 1);
    s.erase(idx);
    idx = linear.find('/');
    if (idx < 0) {
      tcsync.numerator   = strtod(linear.c_str(), nullptr);
      tcsync.denominator = 1.0;

    } else {
      std::string div = linear.substr(idx + 1);
      linear.erase(idx);
      double d1  = strtod(linear.c_str(), nullptr);
      double d2  = strtod(div.c_str(), nullptr);
      if (0.0 == d2)
        mxerror(boost::format(Y("Invalid sync option specified in '%1% %2%'. The divisor is zero.\n")) % opt % orig);

      tcsync.numerator   = d1;
      tcsync.denominator = d2;
    }
    if ((tcsync.numerator * tcsync.denominator) <= 0.0)
      mxerror(boost::format(Y("Invalid sync option specified in '%1% %2%'. The linear sync value may not be equal to or smaller than zero.\n")) % opt % orig);

  }

  tcsync.displacement     = (int64_t)atoi(s.c_str()) * 1000000ll;
  ti.m_timecode_syncs[id] = tcsync;
}

/** \brief Parse the \c --aspect-ratio argument

   The argument must have the form \c TID:w/h or \c TID:float, e.g. \c 0:16/9
*/
static void
parse_arg_aspect_ratio(const std::string &s,
                       const std::string &opt,
                       bool is_factor,
                       track_info_c &ti) {
  display_properties_t dprop;

  std::string msg      = is_factor ? Y("Aspect ratio factor") : Y("Aspect ratio");

  dprop.ar_factor      = is_factor;
  std::vector<std::string> parts = split(s, ":", 2);
  if (parts.size() != 2)
    mxerror(boost::format(Y("%1%: missing track ID in '%2% %3%'.\n")) % msg % opt % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("%1%: invalid track ID in '%2% %3%'.\n")) % msg % opt % s);

  dprop.width  = -1;
  dprop.height = -1;

  int idx      = parts[1].find('/');
  if (0 > idx)
    idx = parts[1].find(':');
  if (0 > idx) {
    dprop.aspect_ratio          = strtod(parts[1].c_str(), nullptr);
    ti.m_display_properties[id] = dprop;
    return;
  }

  std::string div = parts[1].substr(idx + 1);
  parts[1].erase(idx);
  if (parts[1].empty())
    mxerror(boost::format(Y("%1%: missing dividend in '%2% %3%'.\n")) % msg % opt %s);

  if (div.empty())
    mxerror(boost::format(Y("%1%: missing divisor in '%2% %3%'.\n")) % msg % opt % s);

  double w = strtod(parts[1].c_str(), nullptr);
  double h = strtod(div.c_str(), nullptr);
  if (0.0 == h)
    mxerror(boost::format(Y("%1%: divisor is 0 in '%2% %3%'.\n")) % msg % opt % s);

  dprop.aspect_ratio          = w / h;
  ti.m_display_properties[id] = dprop;
}

/** \brief Parse the \c --display-dimensions argument

   The argument must have the form \c TID:wxh, e.g. \c 0:640x480.
*/
static void
parse_arg_display_dimensions(const std::string s,
                             track_info_c &ti) {
  display_properties_t dprop;

  std::vector<std::string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Display dimensions: not given in the form <TID>:<width>x<height>, e.g. 1:640x480 (argument was '%1%').\n")) % s);

  std::vector<std::string> dims = split(parts[1], "x", 2);
  int64_t id = 0;
  int w, h;
  if ((dims.size() != 2) || !parse_int(parts[0], id) || !parse_int(dims[0], w) || !parse_int(dims[1], h) || (0 >= w) || (0 >= h))
    mxerror(boost::format(Y("Display dimensions: not given in the form <TID>:<width>x<height>, e.g. 1:640x480 (argument was '%1%').\n")) % s);

  dprop.aspect_ratio          = -1.0;
  dprop.width                 = w;
  dprop.height                = h;

  ti.m_display_properties[id] = dprop;
}

/** \brief Parse the \c --cropping argument

   The argument must have the form \c TID:left,top,right,bottom e.g.
   \c 0:10,5,10,5
*/
static void
parse_arg_cropping(const std::string &s,
                   track_info_c &ti) {
  pixel_crop_t crop;

  std::string err_msg        = Y("Cropping parameters: not given in the form <TID>:<left>,<top>,<right>,<bottom> e.g. 0:10,5,10,5 (argument was '%1%').\n");

  std::vector<std::string> v = split(s, ":");
  if (v.size() != 2)
    mxerror(boost::format(err_msg) % s);

  int64_t id = 0;
  if (!parse_int(v[0], id))
    mxerror(boost::format(err_msg) % s);

  v = split(v[1], ",");
  if (v.size() != 4)
    mxerror(boost::format(err_msg) % s);

  if (!parse_int(v[0], crop.left) || !parse_int(v[1], crop.top) || !parse_int(v[2], crop.right) || !parse_int(v[3], crop.bottom))
    mxerror(boost::format(err_msg) % s);

  ti.m_pixel_crop_list[id] = crop;
}

/** \brief Parse the \c --stereo-mode argument

   The argument must either be a number starting at 0 or
   one of these keywords:

   0: mono
   1: side by side (left eye is first)
   2: top-bottom (right eye is first)
   3: top-bottom (left eye is first)
   4: checkerboard (right is first)
   5: checkerboard (left is first)
   6: row interleaved (right is first)
   7: row interleaved (left is first)
   8: column interleaved (right is first)
   9: column interleaved (left is first)
   10: anaglyph (cyan/red)
   11: side by side (right eye is first)
*/
static void
parse_arg_stereo_mode(const std::string &s,
                      track_info_c &ti) {
  std::string errmsg = Y("Stereo mode parameter: not given in the form <TID>:<n|keyword> where n is a number between 0 and %1% "
                         "or one of these keywords: %2% (argument was '%3%').\n");

  std::vector<std::string> v = split(s, ":");
  if (v.size() != 2)
    mxerror(boost::format(errmsg) % stereo_mode_c::max_index() % stereo_mode_c::displayable_modes_list() % s);

  int64_t id = 0;
  if (!parse_int(v[0], id))
    mxerror(boost::format(errmsg) % stereo_mode_c::max_index() % stereo_mode_c::displayable_modes_list() % s);

  stereo_mode_c::mode mode = stereo_mode_c::parse_mode(v[1]);
  if (stereo_mode_c::invalid != mode) {
    ti.m_stereo_mode_list[id] = mode;
    return;
  }

  int index;
  if (!parse_int(v[1], index) || !stereo_mode_c::valid_index(index))
    mxerror(boost::format(errmsg) % stereo_mode_c::max_index() % stereo_mode_c::displayable_modes_list() % s);

  ti.m_stereo_mode_list[id] = static_cast<stereo_mode_c::mode>(index);
}

/** \brief Parse the duration formats to \c --split

  This function is called by ::parse_split if the format specifies
  a duration after which a new file should be started.
*/
static void
parse_arg_split_duration(const std::string &arg) {
  std::string s = arg;

  if (balg::istarts_with(s, "duration:"))
    s.erase(0, strlen("duration:"));

  int64_t split_after;
  if (!parse_timecode(s, split_after))
    mxerror(boost::format(Y("Invalid time for '--split' in '--split %1%'. Additional error message: %2%\n")) % arg % timecode_parser_error);

  g_cluster_helper->add_split_point(split_point_t(split_after, split_point_t::SPT_DURATION, false));
}

/** \brief Parse the timecode format to \c --split

  This function is called by ::parse_split if the format specifies
  a timecodes after which a new file should be started.
*/
static void
parse_arg_split_timecodes(const std::string &arg) {
  std::string s = arg;

  if (balg::istarts_with(s, "timecodes:"))
    s.erase(0, 10);

  std::vector<std::string> timecodes = split(s, ",");
  for (auto &timecode : timecodes) {
    int64_t split_after;
    if (!parse_timecode(timecode, split_after))
      mxerror(boost::format(Y("Invalid time for '--split' in '--split %1%'. Additional error message: %2%.\n")) % arg % timecode_parser_error);
    g_cluster_helper->add_split_point(split_point_t(split_after, split_point_t::SPT_TIMECODE, true));
  }
}

static void
parse_arg_split_parts(const std::string &arg) {
  std::string s = arg;

  if (balg::istarts_with(s, "parts:"))
    s.erase(0, 6);

  if (s.empty())
      mxerror(boost::format(Y("Invalid missing start/end specifications for '--split' in '--split %1%'.\n")) % arg);

  std::vector<std::tuple<int64_t, int64_t, bool> > split_points;
  for (auto const &part_spec : split(s, ",")) {
    auto pair = split(part_spec, "-");
    if (pair.size() != 2)
      mxerror(boost::format(Y("Invalid start/end specification for '--split' in '--split %1%' (curent part: %2%).\n")) % arg % part_spec);

    bool create_new_file = true;
    if (pair[0].substr(0, 1) == "+") {
      if (!split_points.empty())
        create_new_file = false;
      pair[0].erase(0, 1);
    }

    int64_t start;
    if (pair[0].empty())
      start = split_points.empty() ? 0 : std::get<1>(split_points.back());
    else if (!parse_timecode(pair[0], start))
      mxerror(boost::format(Y("Invalid start time for '--split' in '--split %1%' (current part: %2%). Additional error message: %3%.\n")) % arg % part_spec % timecode_parser_error);

    int64_t end;
    if (pair[1].empty())
      end = std::numeric_limits<int64_t>::max();
    else if (!parse_timecode(pair[1], end))
      mxerror(boost::format(Y("Invalid end time for '--split' in '--split %1%' (current part: %2%). Additional error message: %3%.\n")) % arg % part_spec % timecode_parser_error);

    if (end <= start)
      mxerror(boost::format(Y("Invalid end time for '--split' in '--split %1%' (current part: %2%). The end time must be bigger than the start time.\n")) % arg % part_spec);

    if (!split_points.empty() && (start < std::get<1>(split_points.back())))
      mxerror(boost::format(Y("Invalid start time for '--split' in '--split %1%' (current part: %2%). The start time must be bigger than or equal to the previous' part's end time.\n")) % arg % part_spec);

    split_points.push_back(std::make_tuple(start, end, create_new_file));
  }

  int64_t previous_end = 0;

  for (auto &split_point : split_points) {
    if (previous_end < std::get<0>(split_point))
      g_cluster_helper->add_split_point(split_point_t{ previous_end, split_point_t::SPT_PARTS, true, true, std::get<2>(split_point) });
    g_cluster_helper->add_split_point(split_point_t{ std::get<0>(split_point), split_point_t::SPT_PARTS, true, false, std::get<2>(split_point) });
    previous_end = std::get<1>(split_point);
  }

  if (std::get<1>(split_points.back()) < std::numeric_limits<int64_t>::max())
    g_cluster_helper->add_split_point(split_point_t{ std::get<1>(split_points.back()), split_point_t::SPT_PARTS, true, true });
}

/** \brief Parse the size format to \c --split

  This function is called by ::parse_split if the format specifies
  a size after which a new file should be started.
*/
static void
parse_arg_split_size(const std::string &arg) {
  std::string s       = arg;
  std::string err_msg = Y("Invalid split size in '--split %1%'.\n");

  if (balg::istarts_with(s, "size:"))
    s.erase(0, strlen("size:"));

  if (s.empty())
    mxerror(boost::format(err_msg) % arg);

  // Size in bytes/KB/MB/GB
  char mod         = tolower(s[s.length() - 1]);
  int64_t modifier = 1;
  if ('k' == mod)
    modifier = 1024;
  else if ('m' == mod)
    modifier = 1024 * 1024;
  else if ('g' == mod)
    modifier = 1024 * 1024 * 1024;
  else if (!isdigit(mod))
    mxerror(boost::format(err_msg) % arg);

  if (1 != modifier)
    s.erase(s.size() - 1);

  int64_t split_after;
  if (!parse_int(s, split_after))
    mxerror(boost::format(err_msg) % arg);

  g_cluster_helper->add_split_point(split_point_t(split_after * modifier, split_point_t::SPT_SIZE, false));
}

/** \brief Parse the \c --split argument

   The \c --split option takes several formats.

   \arg size based: If only a number is given or the number is
   postfixed with '<tt>K</tt>', '<tt>M</tt>' or '<tt>G</tt>' this is
   interpreted as the size after which to split.
   This format is parsed by ::parse_split_size

   \arg time based: If a number postfixed with '<tt>s</tt>' or in a
   format matching '<tt>HH:MM:SS</tt>' or '<tt>HH:MM:SS.mmm</tt>' is
   given then this is interpreted as the time after which to split.
   This format is parsed by ::parse_split_duration
*/
static void
parse_arg_split(const std::string &arg) {
  std::string err_msg = Y("Invalid format for '--split' in '--split %1%'.\n");

  if (arg.size() < 2)
    mxerror(boost::format(err_msg) % arg);

  std::string s = arg;

  // HH:MM:SS
  if (balg::istarts_with(s, "duration:"))
    parse_arg_split_duration(arg);

  else if (balg::istarts_with(s, "size:"))
    parse_arg_split_size(arg);

  else if (balg::istarts_with(s, "timecodes:"))
    parse_arg_split_timecodes(arg);

  else if (balg::istarts_with(s, "parts:"))
    parse_arg_split_parts(arg);

  else if ((   (s.size() == 8)
            || (s.size() == 12))
           && (':' == s[2]) && (':' == s[5])
           && isdigit(s[0]) && isdigit(s[1]) && isdigit(s[3])
           && isdigit(s[4]) && isdigit(s[6]) && isdigit(s[7]))
    // HH:MM:SS
    parse_arg_split_duration(arg);

  else {
    char mod = tolower(s[s.size() - 1]);

    if ('s' == mod)
      parse_arg_split_duration(arg);

    else if (('k' == mod) || ('m' == mod) || ('g' == mod) || isdigit(mod))
      parse_arg_split_size(arg);

    else
      mxerror(boost::format(err_msg) % arg);
  }

  g_cluster_helper->dump_split_points();
}

/** \brief Parse the \c --default-track argument

   The argument must have the form \c TID or \c TID:boolean. The former
   is equivalent to \c TID:1.
*/
static void
parse_arg_default_track(const std::string &s,
                        track_info_c &ti) {
  bool is_default      = true;
  std::vector<std::string> parts = split(s, ":", 2);
  int64_t id           = 0;

  strip(parts);
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '--default-track %1%'.\n")) % s);

  try {
    if (2 == parts.size())
      is_default = parse_bool(parts[1]);
  } catch (...) {
    mxerror(boost::format(Y("Invalid boolean option specified in '--default-track %1%'.\n")) % s);
  }

  ti.m_default_track_flags[id] = is_default;
}

/** \brief Parse the \c --forced-track argument

   The argument must have the form \c TID or \c TID:boolean. The former
   is equivalent to \c TID:1.
*/
static void
parse_arg_forced_track(const std::string &s,
                        track_info_c &ti) {
  bool is_forced                 = true;
  std::vector<std::string> parts = split(s, ":", 2);
  int64_t id                     = 0;

  strip(parts);
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '--forced-track %1%'.\n")) % s);

  try {
    if (2 == parts.size())
      is_forced = parse_bool(parts[1]);
  } catch (...) {
    mxerror(boost::format(Y("Invalid boolean option specified in '--forced-track %1%'.\n")) % s);
  }

  ti.m_forced_track_flags[id] = is_forced;
}

/** \brief Parse the \c --cues argument

   The argument must have the form \c TID:cuestyle, e.g. \c 0:none.
*/
static void
parse_arg_cues(const std::string &s,
               track_info_c &ti) {

  // Extract the track number.
  std::vector<std::string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid cues option. No track ID specified in '--cues %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '--cues %1%'.\n")) % s);

  if (parts[1].empty())
    mxerror(boost::format(Y("Invalid cues option specified in '--cues %1%'.\n")) % s);

  if (parts[1] == "all")
    ti.m_cue_creations[id] = CUE_STRATEGY_ALL;
  else if (parts[1] == "iframes")
    ti.m_cue_creations[id] = CUE_STRATEGY_IFRAMES;
  else if (parts[1] == "none")
    ti.m_cue_creations[id] = CUE_STRATEGY_NONE;
  else
    mxerror(boost::format(Y("'%1%' is an unsupported argument for --cues.\n")) % s);
}

/** \brief Parse the \c --compression argument

   The argument must have the form \c TID:compression, e.g. \c 0:bz2.
*/
static void
parse_arg_compression(const std::string &s,
                  track_info_c &ti) {
  // Extract the track number.
  std::vector<std::string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid compression option. No track ID specified in '--compression %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '--compression %1%'.\n")) % s);

  if (parts[1].size() == 0)
    mxerror(boost::format(Y("Invalid compression option specified in '--compression %1%'.\n")) % s);

  std::vector<std::string> available_compression_methods;
  available_compression_methods.push_back("none");
  available_compression_methods.push_back("zlib");
  available_compression_methods.push_back("mpeg4_p2");
  available_compression_methods.push_back("analyze_header_removal");

  ti.m_compression_list[id] = COMPRESSION_UNSPECIFIED;
  balg::to_lower(parts[1]);

  if (parts[1] == "zlib")
    ti.m_compression_list[id] = COMPRESSION_ZLIB;

  if (parts[1] == "none")
    ti.m_compression_list[id] = COMPRESSION_NONE;

  if ((parts[1] == "mpeg4_p2") || (parts[1] == "mpeg4p2"))
    ti.m_compression_list[id] = COMPRESSION_MPEG4_P2;

  if (parts[1] == "analyze_header_removal")
      ti.m_compression_list[id] = COMPRESSION_ANALYZE_HEADER_REMOVAL;

#ifdef HAVE_LZO
  if ((parts[1] == "lzo") || (parts[1] == "lzo1x"))
    ti.m_compression_list[id] = COMPRESSION_LZO;
  else
    available_compression_methods.push_back("lzo");
#endif

#ifdef HAVE_BZLIB_H
  if ((parts[1] == "bz2") || (parts[1] == "bzlib"))
    ti.m_compression_list[id] = COMPRESSION_BZ2;
  else
    available_compression_methods.push_back("bz2");
#endif

  if (ti.m_compression_list[id] == COMPRESSION_UNSPECIFIED)
    mxerror(boost::format(Y("'%1%' is an unsupported argument for --compression. Available compression methods are: %2%\n")) % s % join(", ", available_compression_methods));
}

/** \brief Parse the argument for a couple of options

   Some options have similar parameter styles. The arguments must have
   the form \c TID:value, e.g. \c 0:XVID.
*/
static void
parse_arg_language(const std::string &s,
                   std::map<int64_t, std::string> &storage,
                   const std::string &opt,
                   const char *topic,
                   bool check,
                   bool empty_ok = false) {
  // Extract the track number.
  std::vector<std::string>parts = split(s, ":", 2);
  strip(parts);
  if (parts.empty())
    mxerror(boost::format(Y("No track ID specified in '--%1% %2%'.\n")) % opt % s);
  if (1 == parts.size()) {
    if (!empty_ok)
      mxerror(boost::format(Y("No %1% specified in '--%2% %3%'.\n")) % topic % opt % s);
    parts.push_back("");
  }

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '--%1% %2%'.\n")) % opt % s);

  if (check) {
    if (parts[1].empty())
      mxerror(boost::format(Y("Invalid %1% specified in '--%2% %3%'.\n")) % topic % opt % s);

    int index = map_to_iso639_2_code(parts[1].c_str());
    if (-1 == index)
      mxerror(boost::format(Y("'%1%' is neither a valid ISO639-2 nor a valid ISO639-1 code. "
                              "See 'mkvmerge --list-languages' for a list of all languages and their respective ISO639-2 codes.\n")) % parts[1]);

    parts[1] = iso639_languages[index].iso639_2_code;
  }

  storage[id] = parts[1];
}

/** \brief Parse the \c --subtitle-charset argument

   The argument must have the form \c TID:charset, e.g. \c 0:ISO8859-15.
*/
static void
parse_arg_sub_charset(const std::string &s,
                      track_info_c &ti) {
  // Extract the track number.
  std::vector<std::string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid sub charset option. No track ID specified in '--sub-charset %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '--sub-charset %1%'.\n")) % s);

  if (parts[1].empty())
    mxerror(boost::format(Y("Invalid sub charset specified in '--sub-charset %1%'.\n")) % s);

  ti.m_sub_charsets[id] = parts[1];
}

/** \brief Parse the \c --tags argument

   The argument must have the form \c TID:filename, e.g. \c 0:tags.xml.
*/
static void
parse_arg_tags(const std::string &s,
               const std::string &opt,
               track_info_c &ti) {
  // Extract the track number.
  std::vector<std::string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid tags option. No track ID specified in '%1% %2%'.\n")) % opt % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '%1% %2%'.\n")) % opt % s);

  if (parts[1].empty())
    mxerror(boost::format(Y("Invalid tags file name specified in '%1% %2%'.\n")) % opt % s);

  ti.m_all_tags[id] = parts[1];
}

/** \brief Parse the \c --fourcc argument

   The argument must have the form \c TID:fourcc, e.g. \c 0:XVID.
*/
static void
parse_arg_fourcc(const std::string &s,
                 const std::string &opt,
                 track_info_c &ti) {
  std::string orig               = s;
  std::vector<std::string> parts = split(s, ":", 2);

  if (parts.size() != 2)
    mxerror(boost::format(Y("FourCC: Missing track ID in '%1% %2%'.\n")) % opt % orig);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("FourCC: Invalid track ID in '%1% %2%'.\n")) % opt % orig);

  if (parts[1].size() != 4)
    mxerror(boost::format(Y("The FourCC must be exactly four characters long in '%1% %2%'.\n")) % opt % orig);

  ti.m_all_fourccs[id] = parts[1];
}

/** \brief Parse the argument for \c --track-order

   The argument must be a comma separated list of track IDs.
*/
static void
parse_arg_track_order(const std::string &s) {
  track_order_t to;

  std::vector<std::string> parts = split(s, ",");
  strip(parts);

  size_t i;
  for (i = 0; i < parts.size(); i++) {
    std::vector<std::string> pair = split(parts[i].c_str(), ":");

    if (pair.size() != 2)
      mxerror(boost::format(Y("'%1%' is not a valid pair of file ID and track ID in '--track-order %2%'.\n")) % parts[i] % s);

    if (!parse_int(pair[0], to.file_id))
      mxerror(boost::format(Y("'%1%' is not a valid file ID in '--track-order %2%'.\n")) % pair[0] % s);

    if (!parse_int(pair[1], to.track_id))
      mxerror(boost::format(Y("'%1%' is not a valid file ID in '--track-order %2%'.\n")) % pair[1] % s);

    g_track_order.push_back(to);
  }
}

/** \brief Parse the argument for \c --append-to

   The argument must be a comma separated list. Each of the list's items
   consists of four numbers separated by colons. These numbers are:
   -# the source file ID,
   -# the source track ID,
   -# the destination file ID and
   -# the destination track ID.

   File IDs are simply the file's position in the command line regarding
   all input files starting at zero. The first input file has the file ID
   0, the second one the ID 1 etc. The track IDs are just the usual track IDs
   used everywhere.

   The "destination" file and track ID identifies the track that is to be
   appended to the one specified by the "source" file and track ID.
*/
static void
parse_arg_append_to(const std::string &s) {
  g_append_mapping.clear();
  std::vector<std::string> entries = split(s, ",");
  strip(entries);

  for (auto &entry : entries) {
    append_spec_t mapping;

    std::vector<std::string> parts = split(entry, ":");

    if (   (parts.size() != 4)
        || !parse_int(parts[0], mapping.src_file_id)
        || !parse_int(parts[1], mapping.src_track_id)
        || !parse_int(parts[2], mapping.dst_file_id)
        || !parse_int(parts[3], mapping.dst_track_id)
        || (0 > mapping.src_file_id)
        || (0 > mapping.src_track_id)
        || (0 > mapping.dst_file_id)
        || (0 > mapping.dst_track_id))
      mxerror(boost::format(Y("'%1%' is not a valid mapping of file and track IDs in '--append-to %2%'.\n")) % entry % s);

    g_append_mapping.push_back(mapping);
  }
}

static void
parse_arg_append_mode(const std::string &s) {
  if ((s == "track") || (s == "track-based"))
    g_append_mode = APPEND_MODE_TRACK_BASED;

  else if ((s == "file") || (s == "file-based"))
    g_append_mode = APPEND_MODE_FILE_BASED;

  else
    mxerror(boost::format(Y("'%1%' is not a valid append mode in '--append-mode %1%'.\n")) % s);
}

/** \brief Parse the argument for \c --default-duration

   The argument must be a tuple consisting of a track ID and the default
   duration separated by a colon. The duration must be postfixed by 'ms',
   'us', 'ns', 'fps', 'p' or 'i' (see \c parse_number_with_unit).
*/
static void
parse_arg_default_duration(const std::string &s,
                           track_info_c &ti) {
  std::vector<std::string> parts = split(s, ":");
  if (parts.size() != 2)
    mxerror(boost::format(Y("'%1%' is not a valid tuple of track ID and default duration in '--default-duration %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("'%1%' is not a valid track ID in '--default-duration %2%'.\n")) % parts[0] % s);

  ti.m_default_durations[id] = parse_number_with_unit(parts[1], "default duration", "--default-duration");
}

/** \brief Parse the argument for \c --nalu-size-length

   The argument must be a tuple consisting of a track ID and the NALU size
   length, an integer between 2 and 4 inclusively.
*/
static void
parse_arg_nalu_size_length(const std::string &s,
                           track_info_c &ti) {
  static bool s_nalu_size_length_3_warning_printed = false;

  std::vector<std::string> parts = split(s, ":");
  if (parts.size() != 2)
    mxerror(boost::format(Y("'%1%' is not a valid tuple of track ID and NALU size length in '--nalu-size-length %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("'%1%' is not a valid track ID in '--nalu-size-length %2%'.\n")) % parts[0] % s);

  int64_t nalu_size_length;
  if (!parse_int(parts[1], nalu_size_length) || (2 > nalu_size_length) || (4 < nalu_size_length))
    mxerror(boost::format(Y("The NALU size length must be a number between 2 and 4 inclusively in '--nalu-size-length %1%'.\n")) % s);

  if ((3 == nalu_size_length) && !s_nalu_size_length_3_warning_printed) {
    s_nalu_size_length_3_warning_printed = true;
    mxwarn(Y("Using a NALU size length of 3 bytes might result in tracks that won't be decodable with certain AVC/h.264 codecs.\n"));
  }

  ti.m_nalu_size_lengths[id] = nalu_size_length;
}

/** \brief Parse the argument for \c --blockadd

   The argument must be a tupel consisting of a track ID and the max number
   of BlockAdditional IDs.
*/
static void
parse_arg_max_blockadd_id(const std::string &s,
                          track_info_c &ti) {
  std::vector<std::string> parts = split(s, ":");
  if (parts.size() != 2)
    mxerror(boost::format(Y("'%1%' is not a valid pair of track ID and block additional in '--blockadd %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("'%1%' is not a valid track ID in '--blockadd %2%'.\n")) % parts[0] % s);

  int64_t max_blockadd_id = 0;
  if (!parse_int(parts[1], max_blockadd_id) || (max_blockadd_id < 0))
    mxerror(boost::format(Y("'%1%' is not a valid block additional max in '--blockadd %2%'.\n")) % parts[1] % s);

  ti.m_max_blockadd_ids[id] = max_blockadd_id;
}

/** \brief Parse the argument for \c --aac-is-sbr

   The argument can either be just a number (the track ID) or a tupel
   "trackID:number" where the second number is either "0" or "1". If only
   a track ID is given then "number" is assumed to be "1".
*/
static void
parse_arg_aac_is_sbr(const std::string &s,
                     track_info_c &ti) {
  std::vector<std::string> parts = split(s, ":", 2);

  int64_t id = 0;
  if (!parse_int(parts[0], id) || (id < 0))
    mxerror(boost::format(Y("Invalid track ID specified in '--aac-is-sbr %1%'.\n")) % s);

  if ((parts.size() == 2) && (parts[1] != "0") && (parts[1] != "1"))
    mxerror(boost::format(Y("Invalid boolean specified in '--aac-is-sbr %1%'.\n")) % s);

  ti.m_all_aac_is_sbr[id] = (1 == parts.size()) || (parts[1] == "1");
}

static void
parse_arg_priority(const std::string &arg) {
  static const char *s_process_priorities[5] = {"lowest", "lower", "normal", "higher", "highest"};

  int i;
  for (i = 0; 5 > i; ++i)
    if ((arg == s_process_priorities[i])) {
      set_process_priority(i - 2);
      return;
    }

  mxerror(boost::format(Y("'%1%' is not a valid priority class.\n")) % arg);
}

static void
parse_arg_previous_segment_uid(const std::string &param,
                               const std::string &arg) {
  if (g_seguid_link_previous)
    mxerror(boost::format(Y("The previous UID was already given in '%1% %2%'.\n")) % param % arg);

  try {
    g_seguid_link_previous = bitvalue_cptr(new bitvalue_c(arg, 128));
  } catch (...) {
    mxerror(boost::format(Y("Unknown format for the previous UID in '%1% %2%'.\n")) % param % arg);
  }
}

static void
parse_arg_next_segment_uid(const std::string &param,
                           const std::string &arg) {
  if (g_seguid_link_next)
    mxerror(boost::format(Y("The next UID was already given in '%1% %2%'.\n")) % param % arg);

  try {
    g_seguid_link_next = bitvalue_cptr(new bitvalue_c(arg, 128));
  } catch (...) {
    mxerror(boost::format(Y("Unknown format for the next UID in '%1% %2%'.\n")) % param % arg);
  }
}

static void
parse_arg_segment_uid(const std::string &param,
                      const std::string &arg) {
  std::vector<std::string> parts = split(arg, ",");
  for (auto &part : parts) {
    try {
      g_forced_seguids.push_back(bitvalue_cptr(new bitvalue_c(part, 128)));
    } catch (...) {
      mxerror(boost::format(Y("Unknown format for the segment UID '%3%' in '%1% %2%'.\n")) % param % arg % part);
    }
  }
}

static void
parse_arg_cluster_length(std::string arg) {
  int idx = arg.find("ms");
  if (0 <= idx) {
    arg.erase(idx);
    int64_t max_ms_per_cluster;
    if (!parse_int(arg, max_ms_per_cluster) || (100 > max_ms_per_cluster) || (32000 < max_ms_per_cluster))
      mxerror(boost::format(Y("Cluster length '%1%' out of range (100..32000).\n")) % arg);

    g_max_ns_per_cluster     = max_ms_per_cluster * 1000000;
    g_max_blocks_per_cluster = 65535;

  } else {
    if (!parse_int(arg, g_max_blocks_per_cluster) || (0 > g_max_blocks_per_cluster) || (65535 < g_max_blocks_per_cluster))
      mxerror(boost::format(Y("Cluster length '%1%' out of range (0..65535).\n")) % arg);

    g_max_ns_per_cluster = 32000000000ull;
  }
}

static void
parse_arg_attach_file(attachment_t &attachment,
                      const std::string &arg,
                      bool attach_once) {
  try {
    mm_file_io_c test(arg);
  } catch (...) {
    mxerror(boost::format(Y("The file '%1%' cannot be attached because it does not exist or cannot be read.\n")) % arg);
  }

  attachment.name         = arg;
  attachment.to_all_files = !attach_once;

  if (attachment.mime_type.empty())
    attachment.mime_type  = guess_mime_type_and_report(arg);

  try {
    mm_io_cptr io = mm_file_io_c::open(attachment.name);

    if (0 == io->get_size())
      mxerror(boost::format(Y("The size of attachment '%1%' is 0.\n")) % attachment.name);

    attachment.data = memory_c::alloc(io->get_size());
    io->read(attachment.data->get_buffer(), attachment.data->get_size());

  } catch (...) {
    mxerror(boost::format(Y("The attachment '%1%' could not be read.\n")) % attachment.name);
  }

  add_attachment(attachment);
  attachment.clear();
}

static void
parse_arg_chapter_language(const std::string &arg,
                           track_info_c &ti) {
  if (g_chapter_language != "")
    mxerror(boost::format(Y("'--chapter-language' may only be given once in '--chapter-language %1%'.\n")) % arg);

  if (g_chapter_file_name != "")
    mxerror(boost::format(Y("'--chapter-language' must be given before '--chapters' in '--chapter-language %1%'.\n")) % arg);

  int i = map_to_iso639_2_code(arg.c_str());
  if (-1 == i)
    mxerror(boost::format(Y("'%1%' is neither a valid ISO639-2 nor a valid ISO639-1 code in '--chapter-language %1%'. "
                            "See 'mkvmerge --list-languages' for a list of all languages and their respective ISO639-2 codes.\n")) % arg);

  g_chapter_language    = iso639_languages[i].iso639_2_code;
  ti.m_chapter_language = iso639_languages[i].iso639_2_code;
}

static void
parse_arg_chapter_charset(const std::string &arg,
                          track_info_c &ti) {
  if (g_chapter_charset != "")
    mxerror(boost::format(Y("'--chapter-charset' may only be given once in '--chapter-charset %1%'.\n")) % arg);

  if (g_chapter_file_name != "")
    mxerror(boost::format(Y("'--chapter-charset' must be given before '--chapters' in '--chapter-charset %1%'.\n")) % arg);

  g_chapter_charset    = arg;
  ti.m_chapter_charset = arg;
}

static void
parse_arg_chapters(const std::string &param,
                   const std::string &arg) {
  if (g_chapter_file_name != "")
    mxerror(boost::format(Y("Only one chapter file allowed in '%1% %2%'.\n")) % param % arg);

  g_chapter_file_name = arg;
  g_kax_chapters      = parse_chapters(g_chapter_file_name, 0, -1, 0, g_chapter_language.c_str(), g_chapter_charset.c_str(), false, nullptr, &g_tags_from_cue_chapters);
}

static void
parse_arg_segmentinfo(const std::string &param,
                      const std::string &arg) {
  if (g_segmentinfo_file_name != "")
    mxerror(boost::format(Y("Only one segment info file allowed in '%1% %2%'.\n")) % param % arg);

  g_segmentinfo_file_name = arg;
  g_kax_info_chap         = mtx::xml::ebml_segmentinfo_converter_c::parse_file(arg);

  handle_segmentinfo();
}

static void
parse_arg_timecode_scale(const std::string &arg) {
  if (TIMECODE_SCALE_MODE_NORMAL != g_timecode_scale_mode)
    mxerror(Y("'--timecode-scale' was used more than once.\n"));

  int64_t temp = 0;
  if (!parse_int(arg, temp))
    mxerror(Y("The argument to '--timecode-scale' must be a number.\n"));

  if (-1 == temp)
    g_timecode_scale_mode = TIMECODE_SCALE_MODE_AUTO;
  else {
    if ((10000000 < temp) || (1 > temp))
      mxerror(Y("The given timecode scale factor is outside the valid range (1...10000000 or -1 for 'sample precision even if a video track is present').\n"));

    g_timecode_scale      = temp;
    g_timecode_scale_mode = TIMECODE_SCALE_MODE_FIXED;
  }
}

static void
parse_arg_default_language(const std::string &arg) {
  int i = map_to_iso639_2_code(arg.c_str());
  if (-1 == i)
    mxerror(boost::format(Y("'%1%' is neither a valid ISO639-2 nor a valid ISO639-1 code in '--default-language %1%'. "
                            "See 'mkvmerge --list-languages' for a list of all languages and their respective ISO639-2 codes.\n")) % arg);

  g_default_language = iso639_languages[i].iso639_2_code;
}

static void
parse_arg_attachments(const std::string &param,
                      std::string arg,
                      track_info_c &ti) {
  if (balg::starts_with(arg, "!")) {
    arg.erase(0, 1);
    ti.m_attach_mode_list.set_reversed();
  }

  std::vector<std::string> elements = split(arg, ",");

  size_t i;
  for (i = 0; elements.size() > i; ++i) {
    std::vector<std::string> pair = split(elements[i], ":");

    if (1 == pair.size())
      pair.push_back("all");

    else if (2 != pair.size())
      mxerror(boost::format(Y("The argument '%1%' to '%2%' is invalid: too many colons in element '%3%'.\n")) % arg % param % elements[i]);

    int64_t id;
    if (!parse_int(pair[0], id))
      mxerror(boost::format(Y("The argument '%1%' to '%2%' is invalid: '%3%' is not a valid track ID.\n")) % arg % param % pair[0]);

    if (pair[1] == "all")
      ti.m_attach_mode_list.add(id, ATTACH_MODE_TO_ALL_FILES);

    else if (pair[1] == "first")
      ti.m_attach_mode_list.add(id, ATTACH_MODE_TO_FIRST_FILE);

    else
      mxerror(boost::format(Y("The argument '%1%' to '%2%' is invalid: '%3%' must be either 'all' or 'first'.\n")) % arg % param % pair[1]);
  }
}

void
handle_file_name_arg(const std::string &this_arg,
                     std::vector<std::string>::const_iterator &sit,
                     const std::vector<std::string>::const_iterator &end,
                     bool &append_next_file,
                     track_info_c *&ti) {
  std::vector<std::string> file_names;

  if (this_arg == "(") {
    bool end_found = false;
    while ((sit + 1) < end) {
      sit++;
      if (*sit == ")") {
        end_found = true;
        break;
      }
      file_names.push_back(*sit);
    }

    if (!end_found)
      mxerror(Y("The closing parenthesis ')' are missing.\n"));

  } else
    file_names.push_back(this_arg);

  for (auto &file_name : file_names) {
    if (file_name.empty())
      mxerror(Y("An empty file name is not valid.\n"));

    else if (g_outfile == file_name)
      mxerror(boost::format(Y("The name of the output file '%1%' and of one of the input files is the same. This would cause mkvmerge to overwrite "
                              "one of your input files. This is most likely not what you want.\n")) % g_outfile);
  }

  if (!ti->m_atracks.empty() && ti->m_atracks.none())
    mxerror(Y("'-A' and '-a' used on the same source file.\n"));

  if (!ti->m_vtracks.empty() && ti->m_vtracks.none())
    mxerror(Y("'-D' and '-d' used on the same source file.\n"));

  if (!ti->m_stracks.empty() && ti->m_stracks.none())
    mxerror(Y("'-S' and '-s' used on the same source file.\n"));

  if (!ti->m_btracks.empty() && ti->m_btracks.none())
    mxerror(Y("'-B' and '-b' used on the same source file.\n"));

  filelist_t file;
  file.all_names = file_names;
  file.name      = file_names[0];

  if (file_names.size() == 1) {
    if ('+' == this_arg[0]) {
      append_next_file = true;
      file.name.erase(0, 1);

    } else if ('=' == this_arg[0]) {
      ti->m_disable_multi_file = true;
      file.name.erase(0, 1);
    }

    if (append_next_file) {
      if (g_files.empty())
        mxerror(Y("The first file cannot be appended because there are no files to append to.\n"));

      file.appending   = true;
      append_next_file = false;
    }
  }

  ti->m_fname = file.name;

  get_file_type(file);

  if (FILE_TYPE_IS_UNKNOWN == file.type)
    mxerror(boost::format(Y("The file '%1%' has unknown type. Please have a look at the supported file types ('mkvmerge --list-types') and "
                            "contact the author Moritz Bunkus <moritz@bunkus.org> if your file type is supported but not recognized properly.\n")) % file.name);

  if (FILE_TYPE_CHAPTERS != file.type) {
    file.ti = ti;

    g_files.push_back(file);
  } else
    delete ti;

  ti = new track_info_c;
  g_chapter_charset.clear();
  g_chapter_language.clear();
}

/** \brief Parses and handles command line arguments

   Also probes input files for their type and creates the appropriate
   file reader.

   Options are parsed in several passes because some options must be
   handled/known before others. The first pass finds
   '<tt>--identify</tt>'. The second pass handles options that only
   print some information and exit right afterwards
   (e.g. '<tt>--version</tt>' or '<tt>--list-types</tt>'). The third
   pass looks for '<tt>--output-file</tt>'. The fourth pass handles
   everything else.
*/
static void
parse_args(std::vector<std::string> args) {
  set_usage();
  while (handle_common_cli_args(args, ""))
    set_usage();

  // Check if only information about the file is wanted. In this mode only
  // two parameters are allowed: the --identify switch and the file.
  if ((   (2 == args.size())
       || (3 == args.size()))
      && (   (args[0] == "-i")
          || (args[0] == "--identify")
          || (args[0] == "--identify-verbose")
          || (args[0] == "-I")
          || (args[0] == "--identify-for-mmg"))) {
    if ((args[0] == "--identify-verbose") || (args[0] == "-I"))
      g_identify_verbose = true;

    if (args[0] == "--identify-for-mmg") {
      g_identify_verbose = true;
      g_identify_for_mmg = true;
    }

    if (3 == args.size())
      verbose = 3;

    identify(args[1]);
    mxexit();
  }

  // First parse options that either just print some infos and then exit.
  std::vector<std::string>::const_iterator sit;
  mxforeach(sit, args) {
    const std::string &this_arg = *sit;
    std::string next_arg        = ((sit + 1) == args.end()) ? "" : *(sit + 1);

    if ((this_arg == "-l") || (this_arg == "--list-types")) {
      list_file_types();
      mxexit(0);

    } else if (this_arg == "--list-languages") {
      list_iso639_languages();
      mxexit(0);

    } else if ((this_arg == "-i") || (this_arg == "--identify") || (this_arg == "-I") || (this_arg == "--identify-verbose") || (this_arg == "--identify-for-mmg"))
      mxerror(boost::format(Y("'%1%' can only be used with a file name. No further options are allowed if this option is used.\n")) % this_arg);

    else if (this_arg == "--capabilities") {
      print_capabilities();
      mxexit(0);

    }

  }

  mxinfo(boost::format("%1%\n") % get_version_info("mkvmerge", vif_full));

  // Now parse options that are needed right at the beginning.
  mxforeach(sit, args) {
    const std::string &this_arg = *sit;
    bool no_next_arg            = (sit + 1) == args.end();
    std::string next_arg        = no_next_arg ? "" : *(sit + 1);

    if ((this_arg == "-o") || (this_arg == "--output")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks a file name.\n")) % this_arg);

      if (g_outfile != "")
        mxerror(Y("Only one output file allowed.\n"));

      g_outfile = next_arg;
      sit++;

    } else if ((this_arg == "-w") || (this_arg == "--webm"))
      set_output_compatibility(OC_WEBM);
  }

  if (g_outfile.empty()) {
    mxinfo(Y("Error: no output file name was given.\n\n"));
    usage(2);
  }

  if (!outputting_webm() && is_webm_file_name(g_outfile)) {
    set_output_compatibility(OC_WEBM);
    mxinfo(boost::format(Y("Automatically enabling WebM compliance mode due to output file name extension.\n")));
  }

  track_info_c *ti      = new track_info_c;
  bool inputs_found     = false;
  bool append_next_file = false;
  attachment_t attachment;

  mxforeach(sit, args) {
    const std::string &this_arg = *sit;
    bool no_next_arg            = (sit + 1) == args.end();
    std::string next_arg        = no_next_arg ? "" : *(sit + 1);

    // Ignore the options we took care of in the first step.
    if (   (this_arg == "-o")
        || (this_arg == "--output")
        || (this_arg == "--command-line-charset")
        || (this_arg == "--engage")) {
      sit++;
      continue;
    }

    if (   (this_arg == "-w")
        || (this_arg == "--webm"))
      continue;

    // Global options
    if ((this_arg == "--priority")) {
      if (no_next_arg)
        mxerror(Y("'--priority' lacks its argument.\n"));

      parse_arg_priority(next_arg);
      sit++;

    } else if ((this_arg == "-q") || (this_arg == "--quiet"))
      verbose = 0;

    else if ((this_arg == "-v") || (this_arg == "--verbose"))
      verbose++;

    else if (this_arg == "--title") {
      if (no_next_arg)
        mxerror(Y("'--title' lacks the title.\n"));

      g_segment_title     = next_arg;
      g_segment_title_set = true;
      sit++;

    } else if (this_arg == "--split") {
      if ((no_next_arg) || (next_arg[0] == 0))
        mxerror(Y("'--split' lacks the size.\n"));

      parse_arg_split(next_arg);
      sit++;

    } else if (this_arg == "--split-max-files") {
      if ((no_next_arg) || (next_arg[0] == 0))
        mxerror(Y("'--split-max-files' lacks the number of files.\n"));

      if (!parse_int(next_arg, g_split_max_num_files) || (2 > g_split_max_num_files))
        mxerror(Y("Wrong argument to '--split-max-files'.\n"));

      sit++;

    } else if (this_arg == "--link") {
      g_no_linking = false;

    } else if (this_arg == "--link-to-previous") {
      if (no_next_arg || next_arg.empty())
        mxerror(Y("'--link-to-previous' lacks the previous UID.\n"));

      parse_arg_previous_segment_uid(this_arg, next_arg);
      sit++;

    } else if (this_arg == "--link-to-next") {
      if (no_next_arg || next_arg.empty())
        mxerror(Y("'--link-to-next' lacks the next UID.\n"));

      parse_arg_next_segment_uid(this_arg, next_arg);
      sit++;

    } else if (this_arg == "--segment-uid") {
      if (no_next_arg || next_arg.empty())
        mxerror(Y("'--segment-uid' lacks the segment UID.\n"));

      parse_arg_segment_uid(this_arg, next_arg);
      sit++;

    } else if (this_arg == "--cluster-length") {
      if (no_next_arg)
        mxerror(Y("'--cluster-length' lacks the length.\n"));

      parse_arg_cluster_length(next_arg);
      sit++;

    } else if (this_arg == "--no-cues")
      g_write_cues = false;

    else if (this_arg == "--clusters-in-meta-seek")
      g_write_meta_seek_for_clusters = true;

    else if (this_arg == "--disable-lacing")
      g_no_lacing = true;

    else if (this_arg == "--enable-durations")
      g_use_durations = true;

    else if (this_arg == "--attachment-description") {
      if (no_next_arg)
        mxerror(Y("'--attachment-description' lacks the description.\n"));

      if (attachment.description != "")
        mxwarn(Y("More than one description was given for a single attachment.\n"));
      attachment.description = next_arg;
      sit++;

    } else if (this_arg == "--attachment-mime-type") {
      if (no_next_arg)
        mxerror(Y("'--attachment-mime-type' lacks the MIME type.\n"));

      if (attachment.mime_type != "")
        mxwarn(boost::format(Y("More than one MIME type was given for a single attachment. '%1%' will be discarded and '%2%' used instead.\n"))
               % attachment.mime_type % next_arg);
      attachment.mime_type = next_arg;
      sit++;

    } else if (this_arg == "--attachment-name") {
      if (no_next_arg)
        mxerror(Y("'--attachment-name' lacks the name.\n"));

      if (attachment.stored_name != "")
        mxwarn(boost::format(Y("More than one name was given for a single attachment. '%1%' will be discarded and '%2%' used instead.\n"))
               % attachment.stored_name % next_arg);
      attachment.stored_name = next_arg;
      sit++;

    } else if ((this_arg == "--attach-file") || (this_arg == "--attach-file-once")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the file name.\n")) % this_arg);

      parse_arg_attach_file(attachment, next_arg, this_arg == "--attach-file-once");
      sit++;

      inputs_found = true;

    } else if (this_arg == "--global-tags") {
      if (no_next_arg)
        mxerror(Y("'--global-tags' lacks the file name.\n"));

      parse_and_add_tags(next_arg);
      sit++;

      inputs_found = true;

    } else if (this_arg == "--chapter-language") {
      if (no_next_arg)
        mxerror(Y("'--chapter-language' lacks the language.\n"));

      parse_arg_chapter_language(next_arg, *ti);
      sit++;

    } else if (this_arg == "--chapter-charset") {
      if (no_next_arg)
        mxerror(Y("'--chapter-charset' lacks the charset.\n"));

      parse_arg_chapter_charset(next_arg, *ti);
      sit++;

    } else if (this_arg == "--cue-chapter-name-format") {
      if (no_next_arg)
        mxerror(Y("'--cue-chapter-name-format' lacks the format.\n"));

      if (g_chapter_file_name != "")
        mxerror(Y("'--cue-chapter-name-format' must be given before '--chapters'.\n"));

      g_cue_to_chapter_name_format = next_arg;
      sit++;

    } else if (this_arg == "--chapters") {
      if (no_next_arg)
        mxerror(Y("'--chapters' lacks the file name.\n"));

      parse_arg_chapters(this_arg, next_arg);
      sit++;

      inputs_found = true;

    } else if (this_arg == "--segmentinfo") {
      if (no_next_arg)
        mxerror(Y("'--segmentinfo' lacks the file name.\n"));

      parse_arg_segmentinfo(this_arg, next_arg);
      sit++;

      inputs_found = true;

    } else if (this_arg == "--no-chapters")
      ti->m_no_chapters = true;

    else if ((this_arg == "-M") || (this_arg == "--no-attachments"))
      ti->m_attach_mode_list.set_none();

    else if ((this_arg == "-m") || (this_arg == "--attachments")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks its argument.\n")) % this_arg);

      parse_arg_attachments(this_arg, next_arg, *ti);
      sit++;

    } else if (this_arg == "--no-global-tags")
      ti->m_no_global_tags = true;

    else if (this_arg == "--meta-seek-size") {
      mxwarn(Y("The option '--meta-seek-size' is no longer supported. Please read mkvmerge's documentation, especially the section about the MATROSKA FILE LAYOUT.\n"));
      sit++;

    } else if (this_arg == "--timecode-scale") {
      if (no_next_arg)
        mxerror(Y("'--timecode-scale' lacks its argument.\n"));

      parse_arg_timecode_scale(next_arg);
      sit++;
    }

    // Options that apply to the next input file only.
    else if ((this_arg == "-A") || (this_arg == "--noaudio") || (this_arg == "--no-audio"))
      ti->m_atracks.set_none();

    else if ((this_arg == "-D") || (this_arg == "--novideo") || (this_arg == "--no-video"))
      ti->m_vtracks.set_none();

    else if ((this_arg == "-S") || (this_arg == "--nosubs") || (this_arg == "--no-subs") || (this_arg == "--no-subtitles"))
      ti->m_stracks.set_none();

    else if ((this_arg == "-B") || (this_arg == "--nobuttons") || (this_arg == "--no-buttons"))
      ti->m_btracks.set_none();

    else if ((this_arg == "-T") || (this_arg == "--no-track-tags"))
      ti->m_track_tags.set_none();

    else if ((this_arg == "-a") || (this_arg == "--atracks") || (this_arg == "--audio-tracks")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track number(s).\n")) % this_arg);

      parse_arg_tracks(next_arg, ti->m_atracks, this_arg);
      sit++;

    } else if ((this_arg == "-d") || (this_arg == "--vtracks") || (this_arg == "--video-tracks")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track number(s).\n")) % this_arg);

      parse_arg_tracks(next_arg, ti->m_vtracks, this_arg);
      sit++;

    } else if ((this_arg == "-s") || (this_arg == "--stracks") || (this_arg == "--sub-tracks") || (this_arg == "--subtitle-tracks")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track number(s).\n")) % this_arg);

      parse_arg_tracks(next_arg, ti->m_stracks, this_arg);
      sit++;

    } else if ((this_arg == "-b") || (this_arg == "--btracks") || (this_arg == "--button-tracks")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track number(s).\n")) % this_arg);

      parse_arg_tracks(next_arg, ti->m_btracks, this_arg);
      sit++;

    } else if ((this_arg == "--track-tags")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track number(s).\n")) % this_arg);

      parse_arg_tracks(next_arg, ti->m_track_tags, this_arg);
      sit++;

    } else if ((this_arg == "-f") || (this_arg == "--fourcc")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the FourCC.\n")) % this_arg);

      parse_arg_fourcc(next_arg, this_arg, *ti);
      sit++;

    } else if (this_arg == "--aspect-ratio") {
      if (no_next_arg)
        mxerror(Y("'--aspect-ratio' lacks the aspect ratio.\n"));

      parse_arg_aspect_ratio(next_arg, this_arg, false, *ti);
      sit++;

    } else if (this_arg == "--aspect-ratio-factor") {
      if (no_next_arg)
        mxerror(Y("'--aspect-ratio-factor' lacks the aspect ratio factor.\n"));

      parse_arg_aspect_ratio(next_arg, this_arg, true, *ti);
      sit++;

    } else if (this_arg == "--display-dimensions") {
      if (no_next_arg)
        mxerror(Y("'--display-dimensions' lacks the dimensions.\n"));

      parse_arg_display_dimensions(next_arg, *ti);
      sit++;

    } else if (this_arg == "--cropping") {
      if (no_next_arg)
        mxerror(Y("'--cropping' lacks the crop parameters.\n"));

      parse_arg_cropping(next_arg, *ti);
      sit++;

    } else if (this_arg == "--stereo-mode") {
      if (no_next_arg)
        mxerror(Y("'--stereo-mode' lacks its argument.\n"));

      parse_arg_stereo_mode(next_arg, *ti);
      sit++;

    } else if ((this_arg == "-y") || (this_arg == "--sync")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the delay.\n")) % this_arg);

      parse_arg_sync(next_arg, this_arg, *ti);
      sit++;

    } else if (this_arg == "--cues") {
      if (no_next_arg)
        mxerror(Y("'--cues' lacks its argument.\n"));

      parse_arg_cues(next_arg, *ti);
      sit++;

    } else if (this_arg == "--default-track") {
      if (no_next_arg)
        mxerror(Y("'--default-track' lacks its argument.\n"));

      parse_arg_default_track(next_arg, *ti);
      sit++;

    } else if (this_arg == "--forced-track") {
      if (no_next_arg)
        mxerror(Y("'--forced-track' lacks its argument.\n"));

      parse_arg_forced_track(next_arg, *ti);
      sit++;

    } else if (this_arg == "--language") {
      if (no_next_arg)
        mxerror(Y("'--language' lacks its argument.\n"));

      parse_arg_language(next_arg, ti->m_languages, "language", "language", true);
      sit++;

    } else if (this_arg == "--default-language") {
      if (no_next_arg)
        mxerror(Y("'--default-language' lacks its argument.\n"));

      parse_arg_default_language(next_arg);
      sit++;

    } else if ((this_arg == "--sub-charset") || (this_arg == "--subtitle-charset")) {
      if (no_next_arg)
        mxerror(Y("'--sub-charset' lacks its argument.\n"));

      parse_arg_sub_charset(next_arg, *ti);
      sit++;

    } else if ((this_arg == "-t") || (this_arg == "--tags")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks its argument.\n")) % this_arg);

      parse_arg_tags(next_arg, this_arg, *ti);
      sit++;

    } else if (this_arg == "--aac-is-sbr") {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track ID.\n")) % this_arg);

      parse_arg_aac_is_sbr(next_arg, *ti);
      sit++;

    } else if (this_arg == "--compression") {
      if (no_next_arg)
        mxerror(Y("'--compression' lacks its argument.\n"));

      parse_arg_compression(next_arg, *ti);
      sit++;

    } else if (this_arg == "--blockadd") {
      if (no_next_arg)
        mxerror(Y("'--blockadd' lacks its argument.\n"));

      parse_arg_max_blockadd_id(next_arg, *ti);
      sit++;

    } else if (this_arg == "--track-name") {
      if (no_next_arg)
        mxerror(Y("'--track-name' lacks its argument.\n"));

      parse_arg_language(next_arg, ti->m_track_names, "track-name", Y("track name"), false, true);
      sit++;

    } else if (this_arg == "--timecodes") {
      if (no_next_arg)
        mxerror(Y("'--timecodes' lacks its argument.\n"));

      parse_arg_language(next_arg, ti->m_all_ext_timecodes, "timecodes", Y("timecodes"), false);
      sit++;

    } else if (this_arg == "--track-order") {
      if (no_next_arg)
        mxerror(Y("'--track-order' lacks its argument.\n"));

      if (!g_track_order.empty())
        mxerror(Y("'--track-order' may only be given once.\n"));

      parse_arg_track_order(next_arg);
      sit++;

    } else if (this_arg == "--append-to") {
      if (no_next_arg)
        mxerror(Y("'--append-to' lacks its argument.\n"));

      parse_arg_append_to(next_arg);
      sit++;

    } else if (this_arg == "--append-mode") {
      if (no_next_arg)
        mxerror(Y("'--append-mode' lacks its argument.\n"));

      parse_arg_append_mode(next_arg);
      sit++;

    } else if (this_arg == "--default-duration") {
      if (no_next_arg)
        mxerror(Y("'--default-duration' lacks its argument.\n"));

      parse_arg_default_duration(next_arg, *ti);
      sit++;

    } else if (this_arg == "--nalu-size-length") {
      if (no_next_arg)
        mxerror(Y("'--nalu-size-length' lacks its argument.\n"));

      parse_arg_nalu_size_length(next_arg, *ti);
      sit++;

    } else if (this_arg == "+")
      append_next_file = true;

    else if (this_arg == "=")
      ti->m_disable_multi_file = true;

    // The argument is an input file.
    else
      handle_file_name_arg(this_arg, sit, args.end(), append_next_file, ti);
  }

  if (!g_cluster_helper->splitting() && !g_no_linking)
    mxwarn(Y("'--link' is only useful in combination with '--split'.\n"));

  delete ti;

  if (!inputs_found && g_files.empty())
    mxerror(Y("No input files were given. No output will be created.\n"));
}

/** \brief Initialize global variables
*/
static void
init_globals() {
  clear_list_of_unique_uint32(UNIQUE_ALL_IDS);
}

/** \brief Global program initialization

   Both platform dependant and independant initialization is done here.
   For Unix like systems a signal handler is installed. The locale's
   \c LC_MESSAGES is set.
*/
static void
setup() {
  mtx_common_init("mkvmerge");
  g_kax_tracks = new KaxTracks();

#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
  signal(SIGUSR1, sighandler);
  signal(SIGINT, sighandler);
#endif

  g_cluster_helper = new cluster_helper_c();
}

/** \brief Setup and high level program control

   Calls the functions for setup, handling the command line arguments,
   creating the readers, the main loop, finishing the current output
   file and cleaning up.
*/
int
main(int argc,
     char **argv) {
  init_globals();
  setup();

  parse_args(command_line_utf8(argc, argv));

  g_cluster_helper->init_debugging();

  int64_t start = get_current_time_millis();

  create_readers();

  if (g_packetizers.empty() && !g_files.empty())
    mxerror(Y("No streams to output were found. Aborting.\n"));

  create_next_output_file();
  main_loop();
  finish_file(true);

  mxinfo(boost::format(Y("Muxing took %1%.\n")) % create_minutes_seconds_time_string((get_current_time_millis() - start + 500) / 1000, true));

  cleanup();

  mxexit();
}
