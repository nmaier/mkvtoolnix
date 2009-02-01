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

#include "os.h"

#include <boost/regex.hpp>
#include <errno.h>
#include <ctype.h>
#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
#include <signal.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#include <string>
#include <typeinfo>
#include <vector>

#include <matroska/KaxChapters.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>

#include "chapters.h"
#include "cluster_helper.h"
#include "common.h"
#include "commonebml.h"
#include "extern_data.h"
#include "hacks.h"
#include "iso639.h"
#include "mkvmerge.h"
#include "mm_io.h"
#include "output_control.h"
#include "segmentinfo.h"
#include "tagparser.h"
#include "tag_common.h"

#ifdef SYS_WINDOWS
# include "os_windows.h"
#endif

using namespace libmatroska;
using namespace std;

struct ext_file_type_t {
  string ext, desc;

  ext_file_type_t(const string &p_ext,
                  const string &p_desc)
    : ext(p_ext)
    , desc(p_desc)
  {
  }
};

static list<ext_file_type_t> s_input_file_types;

static void
init_input_file_type_list() {
  s_input_file_types.push_back(ext_file_type_t("aac",  Y("AAC (Advanced Audio Coding)")));
  s_input_file_types.push_back(ext_file_type_t("ac3",  Y("A/52 (aka AC3)")));
  s_input_file_types.push_back(ext_file_type_t("avi",  Y("AVI (Audio/Video Interleaved)")));
  s_input_file_types.push_back(ext_file_type_t("btn",  Y("VobBtn buttons")));
  s_input_file_types.push_back(ext_file_type_t("drc",  Y("Dirac elementary stream")));
  s_input_file_types.push_back(ext_file_type_t("dts",  Y("DTS (Digital Theater System)")));
#if defined(HAVE_FLAC_FORMAT_H)
  s_input_file_types.push_back(ext_file_type_t("flac", Y("FLAC lossless audio")));
#endif
  s_input_file_types.push_back(ext_file_type_t("h264", Y("AVC/h.264 elementary streams")));
  s_input_file_types.push_back(ext_file_type_t("idx",  Y("VobSub subtitles")));
  s_input_file_types.push_back(ext_file_type_t("m1v",  Y("MPEG-1 video elementary stream")));
  s_input_file_types.push_back(ext_file_type_t("m2v",  Y("MPEG-2 video elementary stream")));
  s_input_file_types.push_back(ext_file_type_t("mkv",  Y("general Matroska files")));
  s_input_file_types.push_back(ext_file_type_t("mov",  Y("Quicktime/MP4 audio and video")));
  s_input_file_types.push_back(ext_file_type_t("mp2",  Y("MPEG-1 layer II audio (CBR and VBR/ABR)")));
  s_input_file_types.push_back(ext_file_type_t("mp3",  Y("MPEG-1 layer III audio (CBR and VBR/ABR)")));
  s_input_file_types.push_back(ext_file_type_t("mpg",  Y("MPEG program stream")));
  s_input_file_types.push_back(ext_file_type_t("ogg",  Y("audio/video/text subtitles embedded in OGG")));
  s_input_file_types.push_back(ext_file_type_t("rm",   Y("RealMedia audio and video")));
  s_input_file_types.push_back(ext_file_type_t("srt",  Y("SRT text subtitles")));
  s_input_file_types.push_back(ext_file_type_t("ssa",  Y("SSA/ASS text subtitles")));
  s_input_file_types.push_back(ext_file_type_t("tta",  Y("TTA lossless audio")));
  s_input_file_types.push_back(ext_file_type_t("vc1",  Y("VC1 video elementary stream")));
  s_input_file_types.push_back(ext_file_type_t("wav",  Y("WAVE (uncompressed PCM)")));
  s_input_file_types.push_back(ext_file_type_t("wv",   Y("WAVPACK lossless audio")));
}

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
  usage_text += Y("  --no-clusters-in-meta-seek\n"
                  "                           Do not write meta seek data for clusters.\n");
  usage_text += Y("  --disable-lacing         Do not Use lacing.\n");
  usage_text += Y("  --enable-durations       Enable block durations for all blocks.\n");
  usage_text += Y("  --append-to <SFID1:STID1:DFID1:DTID1,SFID2:STID2:DFID2:DTID2,...>\n"
                  "                           A comma separated list of file and track IDs\n"
                  "                           that controls which track of a file is\n"
                  "                           appended to another track of the preceding\n"
                  "                           file.\n");
  usage_text += Y("  --append-mode <file|track>\n"
                  "                           Selects how mkvmerge calculates timecodes when\n"
                  "                           appending files.\n");
  usage_text += Y("  --timecode-scale <n>     Force the timecode scale factor to n.\n");
  usage_text +=   "\n";
  usage_text += Y(" File splitting and linking (more global options):\n");
  usage_text += Y("  --split <d[K,M,G]|HH:MM:SS|s>\n"
                  "                           Create a new file after d bytes (KB, MB, GB)\n"
                  "                           or after a specific time.\n");
  usage_text += Y("  --split timecodes:A[,B...]\n"
                  "                           Create a new file after each timecode A, B\n"
                  "                           etc.\n");
  usage_text += Y("  --split-max-files <n>    Create at most n files.\n");
  usage_text += Y("  --link                   Link splitted files.\n");
  usage_text += Y("  --link-to-previous <SID> Link the first file to the given SID.\n");
  usage_text += Y("  --link-to-next <SID>     Link the last file to the given SID.\n");
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
                  "                           firsts Matroska file written.\n");
  usage_text +=   "\n";
  usage_text += Y(" Options for each input file:\n");
  usage_text += Y("  -a, --atracks <n,m,...>  Copy audio tracks n,m etc. Default: copy all\n"
                  "                           audio tracks.\n");
  usage_text += Y("  -A, --noaudio            Don't copy any audio track from this file.\n");
  usage_text += Y("  -d, --vtracks <n,m,...>  Copy video tracks n,m etc. Default: copy all\n"
                  "                           video tracks.\n");
  usage_text += Y("  -D, --novideo            Don't copy any video track from this file.\n");
  usage_text += Y("  -s, --stracks <n,m,...>  Copy subtitle tracks n,m etc. Default: copy\n"
                  "                           all subtitle tracks.\n");
  usage_text += Y("  -S, --nosubs             Don't copy any text track from this file.\n");
  usage_text += Y("  -b, --btracks <n,m,...>  Copy buttons tracks n,m etc. Default: copy\n"
                  "                           all buttons tracks.\n");
  usage_text += Y("  -B, --nobuttons          Don't copy any buttons track from this file.\n");
  usage_text += Y("  --no-attachments         Don't keep attachments from a Matroska file.\n");
  usage_text += Y("  --no-chapters            Don't keep chapters from a Matroska file.\n");
  usage_text += Y("  --no-tags                Don't keep tags from a Matroska file.\n");
  usage_text += Y("  -y, --sync, --delay <TID:d[,o[/p]]>\n"
                  "                           Synchronize, adjust the track's timecodes with\n"
                  "                           the id TID by 'd' ms.\n"
                  "                           'o/p': Adjust the timecodes by multiplying with\n"
                  "                           'o/p' to fix linear drifts. 'p' defaults to\n"
                  "                           1000 if omitted. Both 'o' and 'p' can be\n"
                  "                           floating point numbers.\n");
  usage_text += Y("  --default-track <TID[:bool]>\n"
                  "                           Sets the 'default' flag for this track or\n"
                  "                           forces it not to be present if bool is 0.\n");
  usage_text += Y("  --blockadd <TID:x>       Sets the max number of block additional\n"
                  "                           levels for this track.\n");
  usage_text += Y("  --track-name <TID:name>  Sets the name for a track.\n");
  usage_text += Y("  --cues <TID:none|iframes|all>\n"
                  "                           Create cue (index) entries for this track:\n"
                  "                           None at all, only for I frames, for all.\n");
  usage_text += Y("  --language <TID:lang>    Sets the language for the track (ISO639-2\n"
                  "                           code, see --list-languages).\n");
  usage_text += Y("  -t, --tags <TID:file>    Read tags for the track from a XML file.\n");
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
                  "                           Explicitely set the display dimensions.\n");
  usage_text += Y("  --cropping <TID:left,top,right,bottom>\n"
                  "                           Sets the cropping parameters.\n");
  usage_text += Y("  --stereo-mode <TID:n|none|left|right|both>\n"
                  "                           Sets the stereo mode parameter. It can\n"
                  "                           either be a numer 0 - 3 or one of the\n"
                  "                           keywords 'none', 'right', 'left' or 'both'.\n");
  usage_text +=   "\n";
  usage_text += Y(" Options that only apply to text subtitle tracks:\n");
  usage_text += Y("  --sub-charset <TID:charset>\n"
                  "                           Sets the charset the text subtitles are\n"
                  "                           written in for the conversion to UTF-8.\n");
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
  usage_text += Y("  --priority <priority>    Set the priority mkvmerge runs with.\n");
  usage_text += Y("  --command-line-charset <charset>\n"
                  "                           Charset for strings on the command line\n");
  usage_text += Y("  --output-charset <cset>  Output messages in this charset\n");
  usage_text += Y("  -r, --redirect-output <file>\n"
                  "                           Redirects all messages into this file.\n");
  usage_text += Y("  @optionsfile             Reads additional command line options from\n"
                  "                           the specified file (see man page).\n");
  usage_text += Y("  -h, --help               Show this help.\n");
  usage_text += Y("  -V, --version            Show version information.\n");
  usage_text +=   "\n\n";
  usage_text += Y("Please read the man page/the HTML documentation to mkvmerge. It\n"
                  "explains several details in great length which are not obvious from\n"
                  "this listing.\n");

  version_info = "mkvmerge v" VERSION " ('" VERSIONNAME "')";
}

static bool s_print_malloc_report = false;

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
  int i;
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
      if ((g_packetizers[i].packetizer != NULL) &&
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

/** \brief Sets the priority mkvmerge runs with

   Depending on the OS different functions are used. On Unix like systems
   the process is being nice'd if priority is negative ( = less important).
   Only the super user can increase the priority, but you shouldn't do
   such work as root anyway.
   On Windows SetPriorityClass is used.
*/
static void
set_process_priority(int priority) {
#if defined(SYS_WINDOWS)
  switch (priority) {
    case -2:
      SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
      break;
    case -1:
      SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
      break;
    case 0:
      SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
      break;
    case 1:
      SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
      break;
    case 2:
      SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
      SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
      break;
  }
#else
  switch (priority) {
    case -2:
      nice(19);
      break;
    case -1:
      nice(2);
      break;
    case 0:
      nice(0);
      break;
    case 1:
      nice(-2);
      break;
    case 2:
      nice(-5);
      break;
  }
#endif
}

static string
guess_mime_type_and_report(string file_name) {
  string mime_type = guess_mime_type(file_name, true);
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
  KaxSegmentFamily *family = FINDFIRST(g_kax_info_chap, KaxSegmentFamily);
  while (NULL != family) {
    g_segfamily_uids.add_family_uid(*family);
    family = FINDNEXT(g_kax_info_chap, KaxSegmentFamily, family);
  }

  EbmlBinary *uid = FINDFIRST(g_kax_info_chap, KaxSegmentUID);
  if (NULL != uid)
    g_forced_seguids.push_back(bitvalue_cptr(new bitvalue_c(*uid)));

  uid = FINDFIRST(g_kax_info_chap, KaxNextUID);
  if (NULL != uid)
    g_seguid_link_next = bitvalue_cptr(new bitvalue_c(*uid));

  uid = FINDFIRST(g_kax_info_chap, KaxPrevUID);
  if (NULL != uid)
    g_seguid_link_previous = bitvalue_cptr(new bitvalue_c(*uid));
}

static void
list_file_types() {
  mxinfo(Y("Known file types:\n"
           "  ext   description\n"
           "  ----  --------------------------\n"));

  list<ext_file_type_t>::const_iterator i;
  mxforeach(i, s_input_file_types)
    mxinfo(boost::format("  %|1$-4s|  %2%\n") % i->ext % i->desc);
}

/** \brief Identify a file type and its contents

   This function called for \c --identify. It sets up dummy track info
   data for the reader, probes the input file, creates the file reader
   and calls its identify function.
*/
static void
identify(const string &filename) {
  track_info_c ti;
  filelist_t file;

  verbose             = 0;
  g_suppress_warnings = true;
  g_identifying       = true;
  file.name           = filename;

  get_file_type(file);

  ti.fname = file.name;

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
   this means 'frames per second'. It returns a number of nanoseconds.
*/
int64_t
parse_number_with_unit(const string &s,
                       const string &subject,
                       const string &argument,
                       string display_s = "") {
  boost::regex re1("(-?\\d+\\.?\\d*)(s|ms|us|ns|fps)?", boost::regex::perl | boost::regbase::icase);
  boost::regex re2("(-?\\d+)/(-?\\d+)(s|ms|us|ns|fps)?", boost::regex::perl | boost::regbase::icase);

  string unit, s_n, s_d;
  int64_t n, d;
  double d_value = 0.0;
  bool is_fraction = false;

  if (display_s.empty())
    display_s = s;

  boost::match_results<std::string::const_iterator> matches;
  if (boost::regex_match(s, matches, re1, boost::match_default)) {
    parse_double(matches[1], d_value);
    if (matches.size() > 2)
      unit = matches[2];
    d = 1;

  } else if (boost::regex_match(s, matches, re2, boost::match_default)) {
    parse_int(matches[1], n);
    parse_int(matches[2], d);
    if (matches.size() > 3)
      unit = matches[3];
    is_fraction = true;

  } else
    mxerror(boost::format(Y("'%1%' is not a valid %2% in '%3% %4%'.\n")) % s % subject % argument % display_s);

  int64_t multiplier = 1000000000;
  unit               = downcase(unit);

  if (unit == "ms")
    multiplier = 1000000;
  else if (unit == "us")
    multiplier = 1000;
  else if (unit == "ns")
    multiplier = 1;
  else if (unit == "fps") {
    if (is_fraction)
      return 1000000000ll * d / n;
    else if (29.97 == d_value)
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

/** \brief Parse a string for a boolean value

   Interpretes the string \c orig as a boolean value. Accepted
   is "true", "yes", "1" as boolean true and "false", "no", "0"
   as boolean false.
*/
bool
parse_bool(const string &orig) {
  string s(downcase(orig));

  if ((s == "yes") || (s == "true") || (s == "1"))
    return true;
  if ((s == "no") || (s == "false") || (s == "0"))
    return false;
  throw false;
}

/** \brief Parse tags and add them to the list of all tags

   Also tests the tags for missing mandatory elements.
*/
void
parse_and_add_tags(const string &file_name) {
  KaxTags *tags = new KaxTags;

  parse_xml_tags(file_name, tags);

  while (tags->ListSize() > 0) {
    KaxTag *tag = dynamic_cast<KaxTag *>((*tags)[0]);
    if (NULL != tag) {
      fix_mandatory_tag_elements(tag);
      if (!tag->CheckMandatory())
        mxerror(boost::format(Y("Error parsing the tags in '%1%': some mandatory elements are missing.\n")) % file_name);
      add_tags(tag);
    }

    tags->Remove(0);
  }

  delete tags;
}

/** \brief Parse the \c --xtracks arguments

   The argument is a comma separated list of track IDs.
*/
static void
parse_arg_tracks(string s,
                 vector<int64_t> &tracks,
                 const string &opt) {
  tracks.clear();

  vector<string> elements = split(s, ",");
  strip(elements);

  int i;
  for (i = 0; i < elements.size(); i++) {
    int64_t tid;
    if (!parse_int(elements[i], tid))
      mxerror(boost::format(Y("Invalid track ID in '%1% %2%'.\n")) % opt % s);
    tracks.push_back(tid);
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
parse_arg_sync(string s,
               const string &opt,
               track_info_c &ti) {
  timecode_sync_t tcsync;

  // Extract the track number.
  string orig          = s;
  vector<string> parts = split(s, ":", 2);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid sync option. No track ID specified in '%1% %2%'.\n")) % opt % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '%1% %2%'.\n")) % opt % s);

  s = parts[1];
  if (s.size() == 0)
    mxerror(boost::format(Y("Invalid sync option specified in '%1% %2%'.\n")) % opt % orig);

  // Now parse the actual sync values.
  int idx = s.find(',');
  if (idx >= 0) {
    string linear = s.substr(idx + 1);
    s.erase(idx);
    idx = linear.find('/');
    if (idx < 0) {
      tcsync.numerator   = strtod(linear.c_str(), NULL);
      tcsync.denominator = 1.0;

    } else {
      string div = linear.substr(idx + 1);
      linear.erase(idx);
      double d1  = strtod(linear.c_str(), NULL);
      double d2  = strtod(div.c_str(), NULL);
      if (0.0 == d2)
        mxerror(boost::format(Y("Invalid sync option specified in '%1% %2%'. The divisor is zero.\n")) % opt % orig);

      tcsync.numerator   = d1;
      tcsync.denominator = d2;
    }
    if ((tcsync.numerator * tcsync.denominator) <= 0.0)
      mxerror(boost::format(Y("Invalid sync option specified in '%1% %2%'. The linear sync value may not be equal to or smaller than zero.\n")) % opt % orig);

  }

  tcsync.displacement   = (int64_t)atoi(s.c_str()) * 1000000ll;

  ti.timecode_syncs[id] = tcsync;
}

/** \brief Parse the \c --aspect-ratio argument

   The argument must have the form \c TID:w/h or \c TID:float, e.g. \c 0:16/9
*/
static void
parse_arg_aspect_ratio(const string &s,
                       const string &opt,
                       bool is_factor,
                       track_info_c &ti) {
  display_properties_t dprop;

  string msg           = is_factor ? Y("Aspect ratio factor") : Y("Aspect ratio");

  dprop.ar_factor      = is_factor;
  vector<string> parts = split(s, ":", 2);
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
    dprop.aspect_ratio        = strtod(parts[1].c_str(), NULL);
    ti.display_properties[id] = dprop;
    return;
  }

  string div = parts[1].substr(idx + 1);
  parts[1].erase(idx);
  if (parts[1].empty())
    mxerror(boost::format(Y("%1%: missing dividend in '%2% %3%'.\n")) % msg % opt %s);

  if (div.empty())
    mxerror(boost::format(Y("%1%: missing divisor in '%2% %3%'.\n")) % msg % opt % s);

  double w = strtod(parts[1].c_str(), NULL);
  double h = strtod(div.c_str(), NULL);
  if (0.0 == h)
    mxerror(boost::format(Y("%1%: divisor is 0 in '%2% %3%'.\n")) % msg % opt % s);

  dprop.aspect_ratio        = w / h;
  ti.display_properties[id] = dprop;
}

/** \brief Parse the \c --display-dimensions argument

   The argument must have the form \c TID:wxh, e.g. \c 0:640x480.
*/
static void
parse_arg_display_dimensions(const string s,
                             track_info_c &ti) {
  display_properties_t dprop;

  vector<string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Display dimensions: not given in the form <TID>:<width>x<height>, e.g. 1:640x480 (argument was '%1%').\n")) % s);

  vector<string> dims = split(parts[1], "x", 2);
  int64_t id = 0;
  int w, h;
  if ((dims.size() != 2) || !parse_int(parts[0], id) || !parse_int(dims[0], w) || !parse_int(dims[1], h) || (0 >= w) || (0 >= h))
    mxerror(boost::format(Y("Display dimensions: not given in the form <TID>:<width>x<height>, e.g. 1:640x480 (argument was '%1%').\n")) % s);

  dprop.aspect_ratio        = -1.0;
  dprop.width               = w;
  dprop.height              = h;

  ti.display_properties[id] = dprop;
}

/** \brief Parse the \c --cropping argument

   The argument must have the form \c TID:left,top,right,bottom e.g.
   \c 0:10,5,10,5
*/
static void
parse_arg_cropping(const string &s,
                   track_info_c &ti) {
  pixel_crop_t crop;

  string err_msg   = Y("Cropping parameters: not given in the form <TID>:<left>,<top>,<right>,<bottom> e.g. 0:10,5,10,5 (argument was '%1%').\n");

  vector<string> v = split(s, ":");
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

  ti.pixel_crop_list[id] = crop;
}

/** \brief Parse the \c --stereo-mode argument

   The argument must either be a number between 0 and 3 or
   one of the keywords \c 'none', \c 'left', \c 'right' or \c 'both'.
*/
static void
parse_arg_stereo_mode(const string &s,
                      track_info_c &ti) {
  static const char * const keywords[] = {
    "none", "right", "left", "both", NULL
  };
  string errmsg = Y("Stereo mode parameter: not given in the form <TID>:<n|keyword> where n is a number between 0 and 3 "
                    "or one of the keywords 'none', 'right', 'left', 'both' (argument was '%1%').\n");

  vector<string> v = split(s, ":");
  if (v.size() != 2)
    mxerror(boost::format(errmsg) % s);

  int64_t id = 0;
  if (!parse_int(v[0], id))
    mxerror(boost::format(errmsg) % s);

  int i;
  for (i = 0; NULL != keywords[i]; ++i)
    if (v[1] == keywords[i]) {
      ti.stereo_mode_list[id] = (stereo_mode_e)i;
      return;
    }

  if (!parse_int(v[1], i) || (i < 0) || (STEREO_MODE_BOTH < i))
    mxerror(boost::format(errmsg) % s);

  ti.stereo_mode_list[id] = (stereo_mode_e)i;
}

/** \brief Parse the duration formats to \c --split

  This function is called by ::parse_split if the format specifies
  a duration after which a new file should be started.
*/
static void
parse_arg_split_duration(const string &arg) {
  string s = arg;

  if (starts_with_case(s, "duration:"))
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
parse_arg_split_timecodes(const string &arg) {
  string s = arg;

  if (starts_with_case(s, "timecodes:"))
    s.erase(0, 10);

  vector<string> timecodes = split(s, ",");
  vector<string>::const_iterator timecode;
  mxforeach(timecode, timecodes) {
    int64_t split_after;
    if (!parse_timecode(*timecode, split_after))
      mxerror(boost::format(Y("Invalid time for '--split' in '--split %1%'. Additional error message: %2%.\n")) % arg % timecode_parser_error);
    g_cluster_helper->add_split_point(split_point_t(split_after, split_point_t::SPT_TIMECODE, true));
  }
}

/** \brief Parse the size format to \c --split

  This function is called by ::parse_split if the format specifies
  a size after which a new file should be started.
*/
static void
parse_arg_split_size(const string &arg) {
  string s       = arg;
  string err_msg = Y("Invalid split size in '--split %1%'.\n");

  if (starts_with_case(s, "size:"))
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
parse_arg_split(const string &arg) {
  string err_msg = Y("Invalid format for '--split' in '--split %1%'.\n");

  if (arg.size() < 2)
    mxerror(boost::format(err_msg) % arg);

  string s = arg;

  // HH:MM:SS
  if (starts_with_case(s, "duration:"))
    parse_arg_split_duration(arg);

  else if (starts_with_case(s, "size:"))
    parse_arg_split_size(arg);

  else if (starts_with_case(s, "timecodes:"))
    parse_arg_split_timecodes(arg);

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
}

/** \brief Parse the \c --default-track argument

   The argument must have the form \c TID or \c TID:boolean. The former
   is equivalent to \c TID:1.
*/
static void
parse_arg_default_track(const string &s,
                        track_info_c &ti) {
  bool is_default      = true;
  vector<string> parts = split(s, ":", 2);
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

  ti.default_track_flags[id] = is_default;
}

/** \brief Parse the \c --cues argument

   The argument must have the form \c TID:cuestyle, e.g. \c 0:none.
*/
static void
parse_arg_cues(const string &s,
               track_info_c &ti) {

  // Extract the track number.
  vector<string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid cues option. No track ID specified in '--cues %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '--cues %1%'.\n")) % s);

  if (parts[1].empty())
    mxerror(boost::format(Y("Invalid cues option specified in '--cues %1%'.\n")) % s);

  if (parts[1] == "all")
    ti.cue_creations[id] = CUE_STRATEGY_ALL;
  else if (parts[1] == "iframes")
    ti.cue_creations[id] = CUE_STRATEGY_IFRAMES;
  else if (parts[1] == "none")
    ti.cue_creations[id] = CUE_STRATEGY_NONE;
  else
    mxerror(boost::format(Y("'%1%' is an unsupported argument for --cues.\n")) % s);
}

/** \brief Parse the \c --compression argument

   The argument must have the form \c TID:compression, e.g. \c 0:bz2.
*/
static void
parse_arg_compression(const string &s,
                  track_info_c &ti) {
  // Extract the track number.
  vector<string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid compression option. No track ID specified in '--compression %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '--compression %1%'.\n")) % s);

  if (parts[1].size() == 0)
    mxerror(boost::format(Y("Invalid compression option specified in '--compression %1%'.\n")) % s);

  ti.compression_list[id] = COMPRESSION_UNSPECIFIED;
  parts[1] = downcase(parts[1]);
#ifdef HAVE_LZO
  if ((parts[1] == "lzo") || (parts[1] == "lzo1x"))
    ti.compression_list[id] = COMPRESSION_LZO;
#endif
  if (parts[1] == "zlib")
    ti.compression_list[id] = COMPRESSION_ZLIB;
#ifdef HAVE_BZLIB_H
  if ((parts[1] == "bz2") || (parts[1] == "bzlib"))
    ti.compression_list[id] = COMPRESSION_BZ2;
#endif
  if ((parts[1] == "mpeg4_p2") || (parts[1] == "mpeg4p2"))
    ti.compression_list[id] = COMPRESSION_MPEG4_P2;
  if (parts[1] == "none")
    ti.compression_list[id] = COMPRESSION_NONE;
  if (ti.compression_list[id] == COMPRESSION_UNSPECIFIED)
    mxerror(boost::format(Y("'%1%' is an unsupported argument for --compression. Available compression methods are 'none' and 'zlib'.\n")) % s);
}

/** \brief Parse the argument for a couple of options

   Some options have similar parameter styles. The arguments must have
   the form \c TID:value, e.g. \c 0:XVID.
*/
static void
parse_arg_language(const string &s,
                   map<int64_t, string> &storage,
                   const string &opt,
                   const char *topic,
                   bool check,
                   bool empty_ok = false) {
  // Extract the track number.
  vector<string>parts = split(s, ":", 2);
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
parse_arg_sub_charset(const string &s,
                      track_info_c &ti) {
  // Extract the track number.
  vector<string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid sub charset option. No track ID specified in '--sub-charset %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '--sub-charset %1%'.\n")) % s);

  if (parts[1].empty())
    mxerror(boost::format(Y("Invalid sub charset specified in '--sub-charset %1%'.\n")) % s);

  ti.sub_charsets[id] = parts[1];
}

/** \brief Parse the \c --tags argument

   The argument must have the form \c TID:filename, e.g. \c 0:tags.xml.
*/
static void
parse_arg_tags(const string &s,
               const string &opt,
               track_info_c &ti) {
  // Extract the track number.
  vector<string> parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(boost::format(Y("Invalid tags option. No track ID specified in '%1% %2%'.\n")) % opt % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("Invalid track ID specified in '%1% %2%'.\n")) % opt % s);

  if (parts[1].empty())
    mxerror(boost::format(Y("Invalid tags file name specified in '%1% %2%'.\n")) % opt % s);

  ti.all_tags[id] = parts[1];
}

/** \brief Parse the \c --fourcc argument

   The argument must have the form \c TID:fourcc, e.g. \c 0:XVID.
*/
static void
parse_arg_fourcc(const string &s,
                 const string &opt,
                 track_info_c &ti) {
  string orig          = s;
  vector<string> parts = split(s, ":", 2);
  strip(parts);

  if (parts.size() != 2)
    mxerror(boost::format(Y("FourCC: Missing track ID in '%1% %2%'.\n")) % opt % orig);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("FourCC: Invalid track ID in '%1% %2%'.\n")) % opt % orig);

  if (parts[1].size() != 4)
    mxerror(boost::format(Y("The FourCC must be exactly four characters long in '%1% %2%'.\n")) % opt % orig);

  ti.all_fourccs[id] = parts[1];
}

/** \brief Parse the argument for \c --track-order

   The argument must be a comma separated list of track IDs.
*/
static void
parse_arg_track_order(const string &s) {
  track_order_t to;

  vector<string> parts = split(s, ",");
  strip(parts);

  int i;
  for (i = 0; i < parts.size(); i++) {
    vector<string> pair = split(parts[i].c_str(), ":");

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
parse_arg_append_to(const string &s,
                    track_info_c &ti) {
  g_append_mapping.clear();
  vector<string> entries = split(s, ",");
  strip(entries);

  vector<string>::const_iterator entry;
  mxforeach(entry, entries) {
    append_spec_t mapping;

    if (   (mxsscanf((*entry).c_str(), "" LLD ":" LLD ":" LLD ":" LLD, &mapping.src_file_id, &mapping.src_track_id, &mapping.dst_file_id, &mapping.dst_track_id) != 4)
        || (0 > mapping.src_file_id)
        || (0 > mapping.src_track_id)
        || (0 > mapping.dst_file_id)
        || (0> mapping.dst_track_id))
      mxerror(boost::format(Y("'%1%' is not a valid mapping of file and track IDs in '--append-to %2%'.\n")) % (*entry) % s);

    g_append_mapping.push_back(mapping);
  }
}

static void
parse_arg_append_mode(const string &s,
                      track_info_c &ti) {
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
   'us', 'ns' or 'fps' (see \c parse_number_with_unit).
*/
static void
parse_arg_default_duration(const string &s,
                           track_info_c &ti) {
  vector<string> parts = split(s, ":");
  if (parts.size() != 2)
    mxerror(boost::format(Y("'%1%' is not a valid tuple of track ID and default duration in '--default-duration %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("'%1%' is not a valid track ID in '--default-duration %2%'.\n")) % parts[0] % s);

  ti.default_durations[id] = parse_number_with_unit(parts[1], "default duration", "--default-duration");
}

/** \brief Parse the argument for \c --nalu-size-length

   The argument must be a tuple consisting of a track ID and the NALU size
   length, an integer between 2 and 4 inclusively.
*/
static void
parse_arg_nalu_size_length(const string &s,
                           track_info_c &ti) {
  static bool s_nalu_size_length_3_warning_printed = false;

  vector<string> parts = split(s, ":");
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

  ti.nalu_size_lengths[id] = nalu_size_length;
}

/** \brief Parse the argument for \c --blockadd

   The argument must be a tupel consisting of a track ID and the max number
   of BlockAdditional IDs.
*/
static void
parse_arg_max_blockadd_id(const string &s,
                          track_info_c &ti) {
  vector<string> parts = split(s, ":");
  if (parts.size() != 2)
    mxerror(boost::format(Y("'%1%' is not a valid pair of track ID and block additional in '--blockadd %1%'.\n")) % s);

  int64_t id = 0;
  if (!parse_int(parts[0], id))
    mxerror(boost::format(Y("'%1%' is not a valid track ID in '--blockadd %2%'.\n")) % parts[0] % s);

  int64_t max_blockadd_id = 0;
  if (!parse_int(parts[1], max_blockadd_id) || (max_blockadd_id < 0))
    mxerror(boost::format(Y("'%1%' is not a valid block additional max in '--blockadd %2%'.\n")) % parts[1] % s);

  ti.max_blockadd_ids[id] = max_blockadd_id;
}

/** \brief Parse the argument for \c --aac-is-sbr

   The argument can either be just a number (the track ID) or a tupel
   "trackID:number" where the second number is either "0" or "1". If only
   a track ID is given then "number" is assumed to be "1".
*/
static void
parse_arg_aac_is_sbr(const string &s,
                     track_info_c &ti) {
  vector<string> parts = split(s, ":", 2);

  int64_t id = 0;
  if (!parse_int(parts[0], id) || (id < 0))
    mxerror(boost::format(Y("Invalid track ID specified in '--aac-is-sbr %1%'.\n")) % s);

  if ((parts.size() == 2) && (parts[1] != "0") && (parts[1] != "1"))
    mxerror(boost::format(Y("Invalid boolean specified in '--aac-is-sbr %1%'.\n")) % s);

  ti.all_aac_is_sbr[id] = (1 == parts.size()) || (parts[1] == "1");
}

static void
parse_arg_priority(const string &arg) {
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
parse_arg_previous_segment_uid(const string &param,
                               const string &arg) {
  if (NULL != g_seguid_link_previous.get())
    mxerror(boost::format(Y("The previous UID was already given in '%1% %2%'.\n")) % param % arg);

  try {
    g_seguid_link_previous = bitvalue_cptr(new bitvalue_c(arg, 128));
  } catch (...) {
    mxerror(boost::format(Y("Unknown format for the previous UID in '%1% %2%'.\n")) % param % arg);
  }
}

static void
parse_arg_next_segment_uid(const string &param,
                           const string &arg) {
  if (NULL != g_seguid_link_next.get())
    mxerror(boost::format(Y("The next UID was already given in '%1% %2%'.\n")) % param % arg);

  try {
    g_seguid_link_next = bitvalue_cptr(new bitvalue_c(arg, 128));
  } catch (...) {
    mxerror(boost::format(Y("Unknown format for the next UID in '%1% %2%'.\n")) % param % arg);
  }
}

static void
parse_arg_cluster_length(string arg) {
  int idx = arg.find("ms");
  if (0 <= idx) {
    arg.erase(idx);
    if (!parse_int(arg, g_max_ns_per_cluster) || (100 > g_max_ns_per_cluster) || (32000 < g_max_ns_per_cluster))
      mxerror(boost::format(Y("Cluster length '%1%' out of range (100..32000).\n")) % arg);

    g_max_ns_per_cluster     *= 1000000;
    g_max_blocks_per_cluster  = 65535;

  } else {
    if (!parse_int(arg, g_max_blocks_per_cluster) || (0 > g_max_blocks_per_cluster) || (60000 < g_max_blocks_per_cluster))
      mxerror(boost::format(Y("Cluster length '%1%' out of range (0..60000).\n")) % arg);

    g_max_ns_per_cluster = 32000000000ull;
  }
}

static void
parse_arg_attach_file(attachment_t &attachment,
                      const string &arg,
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
    attachment.data         = counted_ptr<buffer_t>(new buffer_t);
    mm_io_cptr io           = mm_io_cptr(new mm_file_io_c(attachment.name));
    attachment.data->m_size = io->get_size();

    if (0 == attachment.data->m_size)
      mxerror(boost::format(Y("The size of attachment '%1%' is 0.\n")) % attachment.name);

    attachment.data->m_buffer = (unsigned char *)safemalloc(attachment.data->m_size); io->read(attachment.data->m_buffer, attachment.data->m_size);

  } catch (...) {
    mxerror(boost::format(Y("The attachment '%1%' could not be read.\n")) % attachment.name);
  }

  add_attachment(attachment);
  attachment.clear();
}

static void
parse_arg_chapter_language(const string &arg) {
  if (g_chapter_language != "")
    mxerror(boost::format(Y("'--chapter-language' may only be given once in '--chapter-language %1%'.\n")) % arg);

  if (g_chapter_file_name != "")
    mxerror(boost::format(Y("'--chapter-language' must be given before '--chapters' in '--chapter-language %1%'.\n")) % arg);

  int i = map_to_iso639_2_code(arg.c_str());
  if (-1 == i)
    mxerror(boost::format(Y("'%1%' is neither a valid ISO639-2 nor a valid ISO639-1 code in '--chapter-language %1%'. "
                            "See 'mkvmerge --list-languages' for a list of all languages and their respective ISO639-2 codes.\n")) % arg);

  g_chapter_language = iso639_languages[i].iso639_2_code;
}

static void
parse_arg_chapter_charset(const string &arg,
                          track_info_c &ti) {
  if (g_chapter_charset != "")
    mxerror(boost::format(Y("'--chapter-charset' may only be given once in '--chapter-charset %1%'.\n")) % arg);

  if (g_chapter_file_name != "")
    mxerror(boost::format(Y("'--chapter-charset' must be given before '--chapters' in '--chapter-charset %1%'.\n")) % arg);

  g_chapter_charset  = arg;
  ti.chapter_charset = arg;
}

static void
parse_arg_chapters(const string &param,
                   const string &arg) {
  if (g_chapter_file_name != "")
    mxerror(boost::format(Y("Only one chapter file allowed in '%1% %2%'.\n")) % param % arg);

  delete g_kax_chapters;

  g_chapter_file_name = arg;
  g_kax_chapters      = parse_chapters(g_chapter_file_name, 0, -1, 0, g_chapter_language.c_str(), g_chapter_charset.c_str(), false, NULL, &g_tags_from_cue_chapters);
}

static void
parse_arg_segmentinfo(const string &param,
                      const string &arg) {
  if (g_segmentinfo_file_name != "")
    mxerror(boost::format(Y("Only one segment info file allowed in '%1% %2%'.\n")) % param % arg);

  g_segmentinfo_file_name = arg;
  g_kax_info_chap         = parse_segmentinfo(arg, false);

  handle_segmentinfo();
}

static void
parse_arg_timecode_scale(const string &arg) {
  if (TIMECODE_SCALE_MODE_NORMAL != g_timecode_scale_mode)
    mxerror(Y("'--timecode-scale' was used more than once.\n"));

  int64_t temp =0 ;
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
parse_arg_default_language(const string &arg) {
  int i = map_to_iso639_2_code(arg.c_str());
  if (-1 == i)
    mxerror(boost::format(Y("'%1%' is neither a valid ISO639-2 nor a valid ISO639-1 code in '--default-language %1%'. "
                            "See 'mkvmerge --list-languages' for a list of all languages and their respective ISO639-2 codes.\n")) % arg);

  g_default_language = iso639_languages[i].iso639_2_code;
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
parse_args(vector<string> args) {
  handle_common_cli_args(args, "");

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
  vector<string>::const_iterator sit;
  mxforeach(sit, args) {
    const string &this_arg = *sit;
    string next_arg        = ((sit + 1) == args.end()) ? "" : *(sit + 1);

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

  mxinfo(boost::format(Y("%1% built on %2% %3%\n")) % version_info % __DATE__ % __TIME__);

  // Now parse options that are needed right at the beginning.
  mxforeach(sit, args) {
    const string &this_arg = *sit;
    string next_arg        = ((sit + 1) == args.end()) ? "" : *(sit + 1);

    bool no_next_arg       = (sit + 1) == args.end();

    if ((this_arg == "-o") || (this_arg == "--output")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks a file name.\n")) % this_arg);

      if (g_outfile != "")
        mxerror(Y("Only one output file allowed.\n"));

      g_outfile = next_arg;
      sit++;

    } else if (this_arg == "--engage") {
      if (no_next_arg)
        mxerror(Y("'--engage' lacks its argument.\n"));
      engage_hacks(next_arg);
      sit++;
    }
  }

  if (g_outfile.empty()) {
    mxinfo(Y("Error: no output file name was given.\n\n"));
    usage(2);
  }

  track_info_c *ti  = new track_info_c;
  bool inputs_found = false;
  attachment_t attachment;

  mxforeach(sit, args) {
    const string &this_arg = *sit;
    string next_arg        = ((sit + 1) == args.end()) ? "" : *(sit + 1);

    bool no_next_arg       = (sit + 1) == args.end();

    // Ignore the options we took care of in the first step.
    if (   (this_arg == "-o")
        || (this_arg == "--output")
        || (this_arg == "--command-line-charset")
        || (this_arg == "--engage")) {
      sit++;
      continue;
    }

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

    } else if (this_arg == "--cluster-length") {
      if (no_next_arg)
        mxerror(Y("'--cluster-length' lacks the length.\n"));

      parse_arg_cluster_length(next_arg);
      sit++;

    } else if (this_arg == "--no-cues")
      g_write_cues = false;

    else if (this_arg == "--no-clusters-in-meta-seek")
      g_write_meta_seek_for_clusters = false;

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

      parse_arg_chapter_language(next_arg);
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

      cue_to_chapter_name_format = next_arg;
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

    } else if (this_arg == "--no-chapters") {
      ti->no_chapters = true;

    } else if (this_arg == "--no-attachments") {
      ti->no_attachments = true;

    } else if (this_arg == "--no-tags") {
      ti->no_tags = true;

    } else if (this_arg == "--meta-seek-size") {
      mxwarn(Y("The option '--meta-seek-size' is no longer supported. Please read mkvmerge's documentation, especially the section about the MATROSKA FILE LAYOUT.\n"));
      sit++;

    } else if (this_arg == "--timecode-scale") {
      if (no_next_arg)
        mxerror(Y("'--timecode-scale' lacks its argument.\n"));

      parse_arg_timecode_scale(next_arg);
      sit++;
    }

    // Options that apply to the next input file only.
    else if ((this_arg == "-A") || (this_arg == "--noaudio"))
      ti->no_audio = true;

    else if ((this_arg == "-D") || (this_arg == "--novideo"))
      ti->no_video = true;

    else if ((this_arg == "-S") || (this_arg == "--nosubs"))
      ti->no_subs = true;

    else if ((this_arg == "-B") || (this_arg == "--nobuttons"))
      ti->no_buttons = true;

    else if ((this_arg == "-a") || (this_arg == "--atracks")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track number(s).\n")) % this_arg);

      parse_arg_tracks(next_arg, ti->atracks, this_arg);
      sit++;

    } else if ((this_arg == "-d") || (this_arg == "--vtracks")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track number(s).\n")) % this_arg);

      parse_arg_tracks(next_arg, ti->vtracks, this_arg);
      sit++;

    } else if ((this_arg == "-s") || (this_arg == "--stracks")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track number(s).\n")) % this_arg);

      parse_arg_tracks(next_arg, ti->stracks, this_arg);
      sit++;

    } else if ((this_arg == "-b") || (this_arg == "--btracks")) {
      if (no_next_arg)
        mxerror(boost::format(Y("'%1%' lacks the track number(s).\n")) % this_arg);

      parse_arg_tracks(next_arg, ti->btracks, this_arg);
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

    } else if (this_arg == "--language") {
      if (no_next_arg)
        mxerror(Y("'--language' lacks its argument.\n"));

      parse_arg_language(next_arg, ti->languages, "language", "language", true);
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

      parse_arg_language(next_arg, ti->track_names, "track-name", Y("track name"), false, true);
      sit++;

    } else if (this_arg == "--timecodes") {
      if (no_next_arg)
        mxerror(Y("'--timecodes' lacks its argument.\n"));

      parse_arg_language(next_arg, ti->all_ext_timecodes, "timecodes", Y("timecodes"), false);
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

      parse_arg_append_to(next_arg, *ti);
      sit++;

    } else if (this_arg == "--append-mode") {
      if (no_next_arg)
        mxerror(Y("'--append-mode' lacks its argument.\n"));

      parse_arg_append_mode(next_arg, *ti);
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

    } else if (this_arg == "--print-malloc-report")
      s_print_malloc_report = true;

    else if (this_arg.length() == 0)
      mxerror(Y("An empty file name is not valid.\n"));

    // The argument is an input file.
    else {
      if (g_outfile == this_arg)
        mxerror(boost::format(Y("The name of the output file '%1%' and of one of the input files is the same. This would cause mkvmerge to overwrite "
                                "one of your input files. This is most likely not what you want.\n")) % g_outfile);

      if (!ti->atracks.empty() && ti->no_audio)
        mxerror(Y("'-A' and '-a' used on the same source file.\n"));

      if (!ti->vtracks.empty() && ti->no_video)
        mxerror(Y("'-D' and '-d' used on the same source file.\n"));

      if (!ti->stracks.empty() && ti->no_subs)
        mxerror(Y("'-S' and '-s' used on the same source file.\n"));

      if (!ti->btracks.empty() && ti->no_buttons)
        mxerror(Y("'-B' and '-b' used on the same source file.\n"));

      filelist_t file;
      if ('+' == this_arg[0]) {
        file.name = this_arg.substr(1);
        if (file.name.empty())
          mxerror(Y("A single '+' is not a valid command line option. If you want to append a file "
                    "use '+' directly followed by the file name, e.g. '+movie_part_2.avi'."));
        if (g_files.empty())
          mxerror(Y("The first file cannot be appended because there are no files to append to.\n"));

        file.appending = true;

      } else
        file.name = this_arg;

      ti->fname = file.name;

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
    }
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
  set_usage();
  init_input_file_type_list();

  parse_args(command_line_utf8(argc, argv));

  int64_t start = get_current_time_millis();

  create_readers();

  if (g_packetizers.empty() && !g_files.empty())
    mxerror(Y("No streams to output were found. Aborting.\n"));

  create_next_output_file();
  main_loop();
  finish_file(true);

  int64_t duration = (get_current_time_millis() - start + 500) / 1000;

  if (1 != duration)
    mxinfo(boost::format(Y("Muxing took %1% seconds.\n")) % duration);
  else
    mxinfo(Y("Muxing took 1 second.\n"));

  cleanup();

  if (s_print_malloc_report)
    dump_malloc_report();

  mxexit();
}
