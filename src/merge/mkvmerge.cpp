/** \brief command line parsing
 *
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * \file
 * \version $Id$
 *
 * \author Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include "os.h"

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
#include <string>
#include <typeinfo>
#include <vector>

#include <matroska/KaxChapters.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "extern_data.h"
#include "hacks.h"
#include "iso639.h"
#include "mm_io.h"
#include "output_control.h"
#include "tagparser.h"
#include "tag_common.h"

using namespace libmatroska;
using namespace std;

typedef struct {
  const char *ext;
  int type;
  const char *desc;
} file_type_t;

// Specs say that track numbers should start at 1.
static int track_number = 1;

file_type_t file_types[] =
  {{"---", TYPEUNKNOWN, "<unknown>"},
   {"demultiplexers:", -1, ""},
   {"aac ", TYPEAAC, "AAC (Advanced Audio Coding)"},
   {"ac3 ", TYPEAC3, "A/52 (aka AC3)"},
   {"avi ", TYPEAVI, "AVI (Audio/Video Interleaved)"},
   {"dts ", TYPEDTS, "DTS (Digital Theater System)"},
   {"mp2 ", TYPEMP3, "MPEG1 layer II audio (CBR and VBR/ABR)"},
   {"mp3 ", TYPEMP3, "MPEG1 layer III audio (CBR and VBR/ABR)"},
   {"mkv ", TYPEMATROSKA, "general Matroska files"},
   {"ogg ", TYPEOGM, "general OGG media stream, audio/video embedded in OGG"},
   {"mov ", TYPEQTMP4, "Quicktime/MP4 audio and video"},
   {"rm  ", TYPEREAL, "RealMedia audio and video"},
   {"srt ", TYPESRT, "SRT text subtitles"},
   {"ssa ", TYPESSA, "SSA/ASS text subtitles"},
   {"idx ", TYPEVOBSUB, "VobSub subtitles"},
   {"wav ", TYPEWAV, "WAVE (uncompressed PCM)"},
#if defined(HAVE_FLAC_FORMAT_H)
   {"flac", TYPEFLAC, "FLAC lossless audio"},
#endif
   {"tta",  TYPETTA, "TTA lossless audio"},
   {"output modules:", -1, ""},
   {"    ", -1,      "AAC audio"},
   {"    ", -1,      "AC3 audio"},
   {"    ", -1,      "DTS audio"},
#if defined(HAVE_FLAC_FORMAT_H)
   {"    ", -1,      "FLAC"},
#endif
   {"    ", -1,      "MP3 audio"},
   {"    ", -1,      "simple text subtitles"},
   {"    ", -1,      "uncompressed PCM audio"},
   {"    ", -1,      "Video (not MPEG1/2)"},
   {"    ", -1,      "Vorbis audio"},
   {NULL,  -1,      NULL}};

/** \brief Outputs usage information
 */
static void
usage() {
  mxinfo(_(
    "mkvmerge -o out [global options] [options1] <file1> [@optionsfile ...]"
    "\n\n Global options:\n"
    "  -v, --verbose            verbose status\n"
    "  -q, --quiet              suppress status output\n"
    "  -o, --output out         Write to the file 'out'.\n"
    "  --title <title>          Title for this output file.\n"
    "  --global-tags <file>     Read global tags from a XML file.\n"
    "  --command-line-charset   Charset for strings on the command line\n"
    "\n Chapter handling:\n"
    "  --chapters <file>        Read chapter information from the file.\n"
    "  --chapter-language <lng> Set the 'language' element in chapter entries."
    "\n  --chapter-charset <cset> Charset for a simple chapter file.\n"
    "  --cue-chapter-name-format\n"
    "                           Pattern for the conversion from CUE sheet\n"
    "                           entries to chapter names.\n"
    "  --default-language <lng> Use this language for all tracks unless\n"
    "                           overridden with the --language option.\n"
    "\n General output control (advanced global options):\n"
    "  --track-order <FileID1:TID1,FileID2:TID2,FileID3:TID3,...>\n"
    "                           A comma separated list of both file IDs\n"
    "                           and track IDs that controls the order of the\n"
    "                           tracks in the output file.\n"
    "  --cluster-length <n[ms]> Put at most n data blocks into each cluster.\n"
    "                           If the number is postfixed with 'ms' then\n"
    "                           put at most n milliseconds of data into each\n"
    "                           cluster.\n"
    "  --no-cues                Do not write the cue data (the index).\n"
    "  --no-clusters-in-meta-seek\n"
    "                           Do not write meta seek data for clusters.\n"
    "  --disable-lacing         Do not Use lacing.\n"
    "  --enable-durations       Enable block durations for all blocks.\n"
    "  --append-to <SFID1:STID1:DFID1:DTID1,SFID2:STID2:DFID2:DTID2,...>\n"
    "                           A comma separated list of file and track IDs\n"
    "                           that controls which track of a file is\n"
    "                           appended to another track of the preceding\n"
    "                           file.\n"
    "  --timecode-scale <n>     Force the timecode scale factor to n.\n"
    "\n File splitting and linking (more global options):\n"
    "  --split <d[K,M,G]|HH:MM:SS|s>\n"
    "                           Create a new file after d bytes (KB, MB, GB)\n"
    "                           or after a specific time.\n"
    "  --split-max-files <n>    Create at most n files.\n"
    "  --link                   Link splitted files.\n"
    "  --link-to-previous <SID> Link the first file to the given SID.\n"
    "  --link-to-next <SID>     Link the last file to the given SID.\n"
    "\n Attachment support (more global options):\n"
    "  --attachment-description <desc>\n"
    "                           Description for the following attachment.\n"
    "  --attachment-mime-type <mime type>\n"
    "                           Mime type for the following attachment.\n"
    "  --attach-file <file>     Creates a file attachment inside the\n"
    "                           Matroska file.\n"
    "  --attach-file-once <file>\n"
    "                           Creates a file attachment inside the\n"
    "                           firsts Matroska file written.\n"
    "\n Options for each input file:\n"
    "  -a, --atracks <n,m,...>  Copy audio tracks n,m etc. Default: copy all\n"
    "                           audio tracks.\n"
    "  -d, --vtracks <n,m,...>  Copy video tracks n,m etc. Default: copy all\n"
    "                           video tracks.\n"
    "  -s, --stracks <n,m,...>  Copy subtitle tracks n,m etc. Default: copy\n"
    "                           all subtitle tracks.\n"
    "  -A, --noaudio            Don't copy any audio track from this file.\n"
    "  -D, --novideo            Don't copy any video track from this file.\n"
    "  -S, --nosubs             Don't copy any text track from this file.\n"
    "  --no-chapters            Don't keep chapters from a Matroska file.\n"
    "  --no-attachments         Don't keep attachments from a Matroska file.\n"
    "  --no-tags                Don't keep tags from a Matroska file.\n"
    "  -y, --sync <TID:d[,o[/p]]>\n"
    "                           Synchronize, delay the audio track with the\n"
    "                           id TID by d ms. \n"
    "                           d > 0: Pad with silent samples.\n"
    "                           d < 0: Remove samples from the beginning.\n"
    "                           o/p: Adjust the timecodes by o/p to fix\n"
    "                           linear drifts. p defaults to 1000 if\n"
    "                           omitted. Both o and p can be floating point\n"
    "                           numbers.\n"
    "  --delay <Xs|ms|us|ns>    Delay to apply to the packets of the track\n"
    "                           by simply adjusting the timecodes.\n"
    "  --default-track <TID>    Sets the 'default' flag for this track.\n"
    "  --track-name <TID:name>  Sets the name for a track.\n"
    "  --cues <TID:none|iframes|all>\n"
    "                           Create cue (index) entries for this track:\n"
    "                           None at all, only for I frames, for all.\n"
    "  --language <TID:lang>    Sets the language for the track (ISO639-2\n"
    "                           code, see --list-languages).\n"
    "  -t, --tags <TID:file>    Read tags for the track from a XML file.\n"
    "  --aac-is-sbr <TID>       Track with the ID is HE-AAC/AAC+/SBR-AAC.\n"
    "  --timecodes <TID:file>   Read the timecodes to be used from a file.\n"
    "\n Options that only apply to video tracks:\n"
    "  -f, --fourcc <FOURCC>    Forces the FourCC to the specified value.\n"
    "                           Works only for video tracks.\n"
    "  --aspect-ratio <TID:f|a/b>\n"
    "                           Sets the display dimensions by calculating\n"
    "                           width and height for this aspect ratio.\n"
    "  --aspect-ratio-factor <TID:f|a/b>\n"
    "                           First calculates the aspect ratio by multi-\n"
    "                           plying the video's original aspect ratio\n"
    "                           with this factor and calculates the display\n"
    "                           dimensions from this factor.\n"
    "  --display-dimensions <TID:width>x<height>\n"
    "                           Explicitely set the display dimensions.\n"
    "  --cropping <TID:left,top,right,bottom>\n"
    "                           Sets the cropping parameters.\n"
    "\n Options that only apply to text subtitle tracks:\n"
    "  --sub-charset <TID:charset>\n"
    "                           Sets the charset the text subtitles are\n"
    "                           written in for the conversion to UTF-8.\n"
    "\n Options that only apply to VobSub subtitle tracks:\n"
    "  --compression <TID:method>\n"
    "                           Sets the compression method used for the\n"
    "                           specified track ('none' or 'zlib').\n"
    "\n\n Other options:\n"
    "  -i, --identify <file>    Print information about the source file.\n"
    "  -l, --list-types         Lists supported input file types.\n"
    "  --list-languages         Lists all ISO639 languages and their\n"
    "                           ISO639-2 codes.\n"
    "  --priority <priority>    Set the priority mkvmerge runs with.\n"
    "  -h, --help               Show this help.\n"
    "  -V, --version            Show version information.\n"
    "  @optionsfile             Reads additional command line options from\n"
    "                           the specified file (see man page).\n"
    "\n\nPlease read the man page/the HTML documentation to mkvmerge. It\n"
    "explains several details in great length which are not obvious from\n"
    "this listing.\n"
  ));
}

/** \brief Prints information about what has been compiled into mkvmerge
 */
static void
print_capabilities() {
#if defined(HAVE_AVICLASSES)
  mxinfo("AVICLASSES\n");
#endif
#if defined(HAVE_AVILIB0_6_10)
  mxinfo("AVILIB0_6_10\n");
#endif
#if defined(HAVE_BZLIB_H)
  mxinfo("BZ2\n");
#endif
#if defined(HAVE_LZO1X_H)
  mxinfo("LZO\n");
#endif
#if defined(HAVE_FLAC_FORMAT_H)
  mxinfo("FLAC\n");
#endif
}

int64_t
create_track_number(generic_reader_c *reader,
                    int64_t tid) {
  bool found;
  int i, file_num;
  int64_t tnum;

  found = false;
  file_num = -1;
  for (i = 0; i < files.size(); i++)
    if (files[i].reader == reader) {
      found = true;
      file_num = i;
      break;
    }

  if (!found)
    die(_("create_track_number: file_num not found. %s\n"), BUGMSG);

  tnum = -1;
  found = false;
  for (i = 0; i < track_order.size(); i++)
    if ((track_order[i].file_id == file_num) &&
        (track_order[i].track_id == tid)) {
      found = true;
      tnum = i + 1;
      break;
    }
  if (found) {
    found = false;
    for (i = 0; i < packetizers.size(); i++)
      if ((packetizers[i].packetizer != NULL) &&
          (packetizers[i].packetizer->get_track_num() == tnum)) {
        tnum = track_number;
        break;
      }
  } else
    tnum = track_number;

  if (tnum >= track_number)
    track_number = tnum + 1;

  return tnum;
}

/** \brief Identify a file type and its contents
 *
 * This function called for \c --identify. It sets up dummy track info
 * data for the reader, probes the input file, creates the file reader
 * and calls its identify function.
 */
static void
identify(const string &filename) {
  track_info_c ti;
  filelist_t file;

  memset(&file, 0, sizeof(filelist_t));

  file.name = from_utf8_c(cc_local_utf8, filename);
  file.type = get_file_type(file.name);
  ti.fname = file.name;
  
  if (file.type == TYPEUNKNOWN)
    mxerror(_("File %s has unknown type. Please have a look "
              "at the supported file types ('mkvmerge --list-types') and "
              "contact the author Moritz Bunkus <moritz@bunkus.org> if your "
              "file type is supported but not recognized properly.\n"),
            file.name);

  file.appending = false;
  file.pack = NULL;
  file.ti = new track_info_c(ti);

  files.push_back(file);

  verbose = 0;
  identifying = true;
  suppress_warnings = true;
  create_readers();

  files[0].reader->identify();
}

/** \brief Parse tags and add them to the list of all tags
 *
 * Also tests the tags for missing mandatory elements.
 */
void
parse_and_add_tags(const char *file_name) {
  KaxTags *tags;
  KaxTag *tag;
  KaxTagTargets *targets;
  bool found;

  tags = new KaxTags;

  parse_xml_tags(file_name, tags);

  while (tags->ListSize() > 0) {
    tag = (KaxTag *)(*tags)[0];
    targets = &GetChild<KaxTagTargets>(*tag);
    found = true;
    if (FINDFIRST(targets, KaxTagTrackUID) == NULL) {
      *(static_cast<EbmlUInteger *>(&GetChild<KaxTagTrackUID>(*targets))) =
        1;
      found = false;
    }
    fix_mandatory_tag_elements(tag);
    if (!tag->CheckMandatory())
      mxerror(_("Error parsing the tags in '%s': some mandatory "
                "elements are missing.\n"), file_name);
    if (!found)
      targets->Remove(targets->ListSize() - 1);
    tags->Remove(0);
    add_tags(tag);
  }

  delete tags;
}

/** \brief Parse the \c --atracks / \c --vtracks / \c --stracks argument
 *
 * The argument is a comma separated list of track IDs.
 */
static void
parse_tracks(string s,
             vector<int64_t> *tracks,
             const string &opt) {
  int i;
  int64_t tid;
  vector<string> elements;

  tracks->clear();

  elements = split(s, ",");
  strip(elements);
  for (i = 0; i < elements.size(); i++) {
    if (!parse_int(elements[i], tid))
      mxerror(_("Invalid track ID in '%s %s'.\n"), opt.c_str(), s.c_str());
    tracks->push_back(tid);
  }
}

/** \brief Parse the \c --sync argument
 *
 * The argument must have the form <tt>TID:d</tt> or
 * <tt>TID:d,l1/l2</tt>, e.g. <tt>0:200</tt>.  The part before the
 * comma is the displacement in ms. The optional part after comma is
 * the linear factor which defaults to 1 if not given.
 */
static void
parse_sync(string s,
           audio_sync_t &async,
           const string &opt) {
  vector<string> parts;
  string orig, linear, div;
  int idx;
  double d1, d2;

  // Extract the track number.
  orig = s;
  parts = split(s, ":", 2);
  if (parts.size() != 2)
    mxerror(_("Invalid sync option. No track ID specified in '%s %s'.\n"),
            opt.c_str(), s.c_str());

  if (!parse_int(parts[0], async.id))
    mxerror(_("Invalid track ID specified in '%s %s'.\n"), opt.c_str(),
            s.c_str());

  s = parts[1];
  if (s.size() == 0)
    mxerror(_("Invalid sync option specified in '%s %s'.\n"), opt.c_str(),
            orig.c_str());

  // Now parse the actual sync values.
  idx = s.find(',');
  if (idx >= 0) {
    linear = s.substr(idx + 1);
    s.erase(idx);
    idx = linear.find('/'); 
    if (idx < 0)
      async.linear = strtod(linear.c_str(), NULL) / 1000.0;
    else {
      div = linear.substr(idx + 1);
      linear.erase(idx);
      d1 = strtod(linear.c_str(), NULL);
      d2 = strtod(div.c_str(), NULL);
      if (d2 == 0.0)
        mxerror(_("Invalid sync option specified in '%s %s'. The divisor is "
                  "zero.\n"), opt.c_str(), orig.c_str());

      async.linear = d1 / d2;
    }
    if (async.linear <= 0.0)
      mxerror(_("Invalid sync option specified in '%s %s'. The linear sync "
                "value may not be smaller than zero.\n"), opt.c_str(),
              orig.c_str());

  } else
    async.linear = 1.0;
  async.displacement = atoi(s.c_str());
}

/** \brief Parse the \c --aspect-ratio argument
 *
 * The argument must have the form \c TID:w/h or \c TID:float, e.g. \c 0:16/9
 */
static void
parse_aspect_ratio(const string &s,
                   const string &opt,
                   track_info_c &ti,
                   bool is_factor) {
  int idx;
  float w, h;
  string div;
  vector<string> parts;
  display_properties_t dprop;
  const char *msg;

  if (is_factor)
    msg = _("Aspect ratio factor");
  else
    msg = _("Aspect ratio");
  dprop.ar_factor = is_factor;

  parts = split(s, ":", 2);
  if (parts.size() != 2)
    mxerror(_("%s: missing track ID in '%s %s'.\n"), msg, opt.c_str(),
            s.c_str());
  if (!parse_int(parts[0], dprop.id))
    mxerror(_("%s: invalid track ID in '%s %s'.\n"), msg, opt.c_str(),
            s.c_str());
  dprop.width = -1;
  dprop.height = -1;

  idx = parts[1].find('/');
  if (idx < 0)
    idx = parts[1].find(':');
  if (idx < 0) {
    dprop.aspect_ratio = strtod(parts[1].c_str(), NULL);
    ti.display_properties->push_back(dprop);
    return;
  }

  div = parts[1].substr(idx + 1);
  parts[1].erase(idx);
  if (parts[1].size() == 0)
    mxerror(_("%s: missing dividend in '%s %s'.\n"), msg, opt.c_str(),
            s.c_str());

  if (div.size() == 0)
    mxerror(_("%s: missing divisor in '%s %s'.\n"), msg, opt.c_str(),
            s.c_str());

  w = strtod(parts[1].c_str(), NULL);
  h = strtod(div.c_str(), NULL);
  if (h == 0.0)
    mxerror(_("%s: divisor is 0 in '%s %s'.\n"), msg, opt.c_str(),
            s.c_str());

  dprop.aspect_ratio = w / h;
  ti.display_properties->push_back(dprop);
}

/** \brief Parse the \c --display-dimensions argument
 *
 * The argument must have the form \c TID:wxh, e.g. \c 0:640x480.
 */
static void
parse_display_dimensions(const string s,
                         track_info_c &ti) {
  vector<string> parts, dims;
  int w, h;
  display_properties_t dprop;

  parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(_("Display dimensions: not given in the form "
              "<TID>:<width>x<height>, e.g. 1:640x480 (argument was '%s').\n"),
            s.c_str());

  dims = split(parts[1], "x", 2);
  if ((dims.size() != 2) || !parse_int(parts[0], dprop.id) ||
      !parse_int(dims[0], w) || !parse_int(dims[1], h) ||
      (w <= 0) || (h <= 0))
    mxerror(_("Display dimensions: not given in the form "
              "<TID>:<width>x<height>, e.g. 1:640x480 (argument was '%s').\n"),
            s.c_str());

  dprop.aspect_ratio = -1.0;
  dprop.width = w;
  dprop.height = h;
  ti.display_properties->push_back(dprop);
}

/** \brief Parse the \c --cropping argument
 *
 * The argument must have the form \c TID:left,top,right,bottom e.g.
 * \c 0:10,5,10,5
 */
static void
parse_cropping(const string &s,
               track_info_c &ti) {
  pixel_crop_t crop;
  vector<string> v;

  v = split(s, ":");
  if (v.size() != 2)
    mxerror(_("Cropping parameters: not given in the form "
              "<TID>:<left>,<top>,<right>,<bottom> e.g. 0:10,5,10,5 "
              "(argument was '%s').\n"), s.c_str());
  if (!parse_int(v[0], crop.id))
    mxerror(_("Cropping parameters: not given in the form "
              "<TID>:<left>,<top>,<right>,<bottom> e.g. 0:10,5,10,5 "
              "(argument was '%s').\n"), s.c_str());

  v = split(v[1], ",");
  if (v.size() != 4)
    mxerror(_("Cropping parameters: not given in the form "
              "<TID>:<left>,<top>,<right>,<bottom> e.g. 0:10,5,10,5 "
              "(argument was '%s').\n"), s.c_str());
  if (!parse_int(v[0], crop.left) ||
      !parse_int(v[1], crop.top) ||
      !parse_int(v[2], crop.right) ||
      !parse_int(v[3], crop.bottom))
    mxerror(_("Cropping parameters: not given in the form "
              "<TID>:<left>,<top>,<right>,<bottom> e.g. 0:10,5,10,5 "
              "(argument was '%s').\n"), s.c_str());

  ti.pixel_crop_list->push_back(crop);
}

/** \brief Parse the \c --split argument
 *
 * The \c --split option takes several formats.
 *
 * \arg size based: If only a number is given or the number is
 * postfixed with '<tt>K</tt>', '<tt>M</tt>' or '<tt>G</tt>' this is
 * interpreted as the size after which to split.
 *
 * \arg time based: If a number postfixed with '<tt>s</tt>' or in a
 * format matching '<tt>HH:MM:SS</tt>' or '<tt>HH:MM:SS.mmm</tt>' is
 * given then this is interpreted as the time after which to split.
 */
static void
parse_split(const string &arg) {
  int64_t modifier;
  char mod;
  string s;

  if (arg.size() < 2)
    mxerror(_("Invalid format for '--split' in '--split %s'.\n"), arg.c_str());

  s = arg;

  // HH:MM:SS
  if (((s.size() == 8) || (s.size() == 12)) &&
      (s[2] == ':') && (s[5] == ':') &&
      isdigit(s[0]) && isdigit(s[1]) && isdigit(s[3]) &&
      isdigit(s[4]) && isdigit(s[6]) && isdigit(s[7])) {
    int secs, mins, hours, msecs;

    if (s.size() == 12) {
      if ((s[8] != '.') || !parse_int(s.substr(9), msecs))
        mxerror(_("Invalid time for '--split' in '--split %s'.\n"),
                arg.c_str());
      s.erase(8);
    } else
      msecs = 0;

    mxsscanf(s.c_str(), "%d:%d:%d", &hours, &mins, &secs);
    split_after = secs + mins * 60 + hours * 3600;
    if ((hours < 0) || (mins < 0) || (mins > 59) ||
        (secs < 0) || (secs > 59) || (split_after < 10))
      mxerror(_("Invalid time for '--split' in '--split %s'.\n"),
              arg.c_str());

    split_after = split_after * 1000 + msecs;
    split_by_time = true;
    return;
  }

  mod = tolower(s[s.size() - 1]);

  // Number of seconds: e.g. 1000s or 4254S
  if (mod == 's') {
    s.erase(s.size() - 1);
    if (!parse_int(s, split_after))
      mxerror(_("Invalid time for '--split' in '--split %s'.\n"),
              arg.c_str());

    split_after *= 1000;
    split_by_time = true;
    return;
  }

  // Size in bytes/KB/MB/GB
  modifier = 1;
  if (mod == 'k')
    modifier = 1024;
  else if (mod == 'm')
    modifier = 1024 * 1024;
  else if (mod == 'g')
    modifier = 1024 * 1024 * 1024;
  else if (!isdigit(mod))
    mxerror(_("Invalid split size in '--split %s'.\n"), arg.c_str());
  if (modifier != 1)
    s.erase(s.size() - 1);
  if (!parse_int(s, split_after))
    mxerror(_("Invalid split size in '--split %s'.\n"), arg.c_str());

  split_after *= modifier;
  if (split_after <= (1024 * 1024))
    mxerror(_("Invalid split size in '--split %s': The size is too small.\n"),
            arg.c_str());

  split_by_time = false;
}

/** \brief Parse the \c --delay argument
 *
 * time based: A number that must be postfixed with <tt>s</tt>,
 * <tt>ms</tt>, <tt>us</tt> or <tt>ns</tt> to specify seconds,
 * milliseconds, microseconds and nanoseconds respectively.
 */
static void
parse_delay(const string &s,
            audio_sync_t &async) {
  string unit;
  vector<string> parts;
  int unit_idx;
  int64_t mult;

  // Extract the track number.
  parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(_("Invalid delay option. No track ID specified in "
              "'--delay %s'.\n"), s.c_str());

  if (!parse_int(parts[0], async.id))
    mxerror(_("Invalid track ID specified in '--delay %s'.\n"), s.c_str());

  for (unit_idx = 0; isdigit(parts[1][unit_idx]); unit_idx++)
    ;
  if (unit_idx == 0)
    mxerror(_("Missing delay given in '--delay %s'.\n"), s.c_str());
  unit = parts[1].substr(unit_idx);
  parts[1].erase(unit_idx);
  mult = 1;
  if (unit == "s")
    mult = 1000000000;
  else if (unit == "ms")
    mult = 1000000;
  else if (unit == "us")
    mult = 1000;
  else if (unit == "ns")
    mxerror(_("Invalid unit given in '--delay %s'.\n"), s.c_str());

  if (!parse_int(parts[1], async.displacement))
    mxerror(_("Invalid delay given in '--delay %s'.\n"), s.c_str());
  async.displacement *= mult;
}

/** \brief Parse the \c --cues argument
 *
 * The argument must have the form \c TID:cuestyle, e.g. \c 0:none.
 */
static void
parse_cues(const string &s,
           cue_creation_t &cues) {
  vector<string> parts;

  // Extract the track number.
  parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(_("Invalid cues option. No track ID specified in '--cues %s'.\n"),
            s.c_str());

  if (!parse_int(parts[0], cues.id))
    mxerror(_("Invalid track ID specified in '--cues %s'.\n"), s.c_str());

  if (parts[1].size() == 0)
    mxerror(_("Invalid cues option specified in '--cues %s'.\n"),
            s.c_str());

  if (parts[1] == "all")
    cues.cues = CUES_ALL;
  else if (parts[1] == "iframes")
    cues.cues = CUES_IFRAMES;
  else if (parts[1] == "none")
    cues.cues = CUES_NONE;
  else
    mxerror(_("'%s' is an unsupported argument for --cues.\n"), s.c_str());
}

/** \brief Parse the \c --compression argument
 *
 * The argument must have the form \c TID:compression, e.g. \c 0:bz2.
 */
static void
parse_compression(const string &s,
                  cue_creation_t &compression) {
  vector<string> parts;

  // Extract the track number.
  parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(_("Invalid compression option. No track ID specified in "
              "'--compression %s'.\n"), s.c_str());

  if (!parse_int(parts[0], compression.id))
    mxerror(_("Invalid track ID specified in '--compression %s'.\n"),
            s.c_str());

  if (parts[1].size() == 0)
    mxerror(_("Invalid compression option specified in '--compression %s'.\n"),
            s.c_str());

  compression.cues = -1;
  parts[1] = downcase(parts[1]);
#ifdef HAVE_LZO1X_H
  if ((parts[1] == "lzo") || (parts[1] == "lzo1x"))
    compression.cues = COMPRESSION_LZO;
#endif
  if (parts[1] == "zlib")
    compression.cues = COMPRESSION_ZLIB;
#ifdef HAVE_BZLIB_H
  if ((parts[1] == "bz2") || (parts[1] == "bzlib"))
    compression.cues = COMPRESSION_BZ2;
#endif
  if (parts[1] == "none")
    compression.cues = COMPRESSION_NONE;
  if (compression.cues == -1)
    mxerror(_("'%s' is an unsupported argument for --compression. Available "
              "compression methods are 'none' and 'zlib'.\n"), s.c_str());
}

/** \brief Parse the argument for a couple of options
 *
 * Some options have similar parameter styles. The arguments must have
 * the form \c TID:value, e.g. \c 0:XVID.
 */
static void
parse_language(const string &s,
               language_t &lang,
               const string &opt,
               const char *topic,
               bool check) {
  vector<string> parts;

  // Extract the track number.
  parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(_("No track ID specified in '--%s' %s'.\n"), opt.c_str(),
            s.c_str());

  if (!parse_int(parts[0], lang.id))
    mxerror(_("Invalid track ID specified in '--%s %s'.\n"), opt.c_str(),
            s.c_str());

  if (check) {
    if (parts[1].size() == 0)
      mxerror(_("Invalid %s specified in '--%s %s'.\n"), topic, opt.c_str(),
              s.c_str());

    if (!is_valid_iso639_2_code(parts[1].c_str())) {
      const char *iso639_2;

      iso639_2 = map_iso639_1_to_iso639_2(parts[1].c_str());
      if (iso639_2 == NULL)
        mxerror(_("'%s' is neither a valid ISO639-2 nor a valid ISO639-1 code."
                  " See 'mkvmerge --list-languages' for a list of all "
                  "languages and their respective ISO639-2 codes.\n"),
                parts[1].c_str());
      parts[1] = iso639_2;
    }
  }

  lang.language = safestrdup(parts[1]);
}

/** \brief Parse the \c --subtitle-charset argument
 *
 * The argument must have the form \c TID:charset, e.g. \c 0:ISO8859-15.
 */
static void
parse_sub_charset(const string &s,
                  language_t &sub_charset) {
  vector<string> parts;

  // Extract the track number.
  parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(_("Invalid sub charset option. No track ID specified in "
              "'--sub-charset %s'.\n"), s.c_str());

  if (!parse_int(parts[0], sub_charset.id))
    mxerror(_("Invalid track ID specified in '--sub-charset %s'.\n"),
            s.c_str());

  if (parts[1].size() == 0)
    mxerror(_("Invalid sub charset specified in '--sub-charset %s'.\n"),
            s.c_str());

  sub_charset.language = safestrdup(parts[1]);
}

/** \brief Parse the \c --tags argument
 *
 * The argument must have the form \c TID:filename, e.g. \c 0:tags.xml.
 */
static void
parse_tags(const string &s,
           tags_t &tags,
           const string &opt) {
  vector<string> parts;

  // Extract the track number.
  parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(_("Invalid tags option. No track ID specified in '%s %s'.\n"),
            opt.c_str(), s.c_str());

  if (!parse_int(parts[0], tags.id))
    mxerror(_("Invalid track ID specified in '%s %s'.\n"), opt.c_str(),
            s.c_str());

  if (parts[1].size() == 0)
    mxerror(_("Invalid tags file name specified in '%s %s'.\n"), opt.c_str(),
            s.c_str());

  tags.file_name = from_utf8_c(cc_local_utf8, parts[1]);
}

/** \brief Parse the \c --fourcc argument
 *
 * The argument must have the form \c TID:fourcc, e.g. \c 0:XVID.
 */
static void
parse_fourcc(const string &s,
             const string &opt,
             track_info_c &ti) {
  vector<string> parts;
  string orig = s;
  fourcc_t fourcc;

  parts = split(s, ":", 2);
  strip(parts);
  if (parts.size() != 2)
    mxerror(_("FourCC: Missing track ID in '%s %s'.\n"), opt.c_str(),
            orig.c_str());
  if (!parse_int(parts[0], fourcc.id))
    mxerror(_("FourCC: Invalid track ID in '%s %s'.\n"), opt.c_str(),
            orig.c_str());
  if (parts[1].size() != 4)
    mxerror(_("The FourCC must be exactly four characters long in '%s %s'."
              "\n"), opt.c_str(), orig.c_str());
  memcpy(fourcc.fourcc, parts[1].c_str(), 4);
  fourcc.fourcc[4] = 0;
  ti.all_fourccs->push_back(fourcc);
}

/** \brief Parse the argument for \c --track-order
 *
 * The argument must be a comma separated list of track IDs.
 */
static void
parse_track_order(const string &s) {
  vector<string> parts, pair;
  uint32_t i;
  track_order_t to;

  parts = split(s, ",");
  strip(parts);
  for (i = 0; i < parts.size(); i++) {
    pair = split(parts[i].c_str(), ":");
    if (pair.size() != 2)
      mxerror(_("'%s' is not a valid pair of file ID and track ID in "
                "'--track-order %s'.\n"), parts[i].c_str(), s.c_str());
    if (!parse_int(pair[0], to.file_id))
      mxerror(_("'%s' is not a valid file ID in '--track-order %s'.\n"),
              pair[0].c_str(), s.c_str());
    if (!parse_int(pair[1], to.track_id))
      mxerror(_("'%s' is not a valid file ID in '--track-order %s'.\n"),
              pair[1].c_str(), s.c_str());
    track_order.push_back(to);
  }
}

static void
parse_append_to(const string &s,
                track_info_c &ti) {
  vector<string> entries;
  vector<string>::const_iterator entry;
  append_spec_t mapping;

  append_mapping.clear();
  entries = split(s, ",");
  strip(entries);
  foreach(entry, entries) {
    if ((mxsscanf((*entry).c_str(), "%lld:%lld:%lld:%lld",
                  &mapping.src_file_id, &mapping.src_track_id,
                  &mapping.dst_file_id, &mapping.dst_track_id) != 4) ||
        (mapping.src_file_id < 0) || (mapping.src_track_id < 0) ||
        (mapping.dst_file_id < 0) || (mapping.dst_track_id < 0))
      mxerror(_("'%s' is not a valid mapping of file and track IDs in "
                "'--append-to %s'.\n"), (*entry).c_str(), s.c_str());
    append_mapping.push_back(mapping);
  }
}

/** \brief Sets the priority mkvmerge runs with
 *
 * Depending on the OS different functions are used. On Unix like systems
 * the process is being nice'd if priority is negative ( = less important).
 * Only the super user can increase the priority, but you shouldn't do
 * such work as root anyway.
 * On Windows SetPriorityClass is used.
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
guess_mime_type(string file_name) {
  vector<string> extensions;
  int i, k;

  i = file_name.rfind('.');
  if (i < 0)
    mxerror(_("No MIME type has been set for the attachment '%s', and "
              "the file name contains no extension. Therefore the MIME "
              "type could not be guessed automatically.\n"),
            file_name.c_str());
  file_name.erase(0, i + 1);

  for (i = 0; mime_types[i].name != NULL; i++) {
    if (mime_types[i].extensions[0] == 0)
      continue;

    extensions = split(mime_types[i].extensions, " ");
    for (k = 0; k < extensions.size(); k++)
      if (!strcasecmp(extensions[k].c_str(), file_name.c_str())) {
        mxinfo(_("Automatic MIME type recognition for '%s': %s\n"),
               file_name.c_str(), mime_types[i].name);
        return mime_types[i].name;
      }
  }

  mxerror(_("No MIME type has been set for the attachment '%s', and "
            "it could not be guessed based on its extension.\n"),
          file_name.c_str());
  return "";
}

/** \brief Parses and handles command line arguments
 *
 * Also probes input files for their type and creates the appropriate
 * file reader.
 *
 * Options are parsed in several passes because some options must be
 * handled/known before others. The first pass finds
 * '<tt>--identify</tt>'. The second pass handles options that only
 * print some information and exit right afterwards
 * (e.g. '<tt>--version</tt>' or '<tt>--list-types</tt>'). The third
 * pass looks for '<tt>--output-file</tt>'. The fourth pass handles
 * everything else.
 */
static void
parse_args(vector<string> &args) {
  const char *process_priorities[5] = {"lowest", "lower", "normal", "higher",
                                       "highest"};
  track_info_c *ti;
  int i;
  filelist_t file;
  vector<string>::const_iterator sit;
  string this_arg, next_arg;
  bool no_next_arg;
  audio_sync_t async;
  cue_creation_t cues;
  int64_t id;
  language_t lang;
  attachment_t attachment;
  tags_t tags;
  mm_io_c *io;

  ti = new track_info_c;
  memset(&attachment, 0, sizeof(attachment_t));
  memset(&tags, 0, sizeof(tags_t));

  // Check if only information about the file is wanted. In this mode only
  // two parameters are allowed: the --identify switch and the file.
  if (((args.size() == 2) || (args.size() == 3)) &&
      ((args[0] == "-i") || (args[0] == "--identify") ||
       (args[0] == "--identify-verbose") || (args[0] == "-I"))) {
    if ((args[0] == "--identify-verbose") || (args[0] == "-I"))
      identify_verbose = true;
    if (args.size() == 3)
      verbose = 3;
    identify(args[1]);
    mxexit();
  }

  // First parse options that either just print some infos and then exit.
  foreach(sit, args) {
    this_arg = *sit;
    if ((sit + 1) == args.end())
      next_arg = "";
    else
      next_arg = *(sit + 1);
    no_next_arg = (sit + 1) == args.end();

    if ((this_arg == "-V") || (this_arg == "--version")) {
      mxinfo(VERSIONINFO " built on " __DATE__ " " __TIME__ "\n");
      mxexit(0);

    } else if ((this_arg == "-h") || (this_arg == "-?") ||
               (this_arg == "--help")) {
      usage();
      mxexit(0);

    } else if ((this_arg == "-l") || (this_arg == "--list-types")) {
      mxinfo(_("Known file types:\n  ext  description\n"
               "  ---  --------------------------\n"));
      for (i = 1; file_types[i].ext; i++)
        mxinfo("  %s  %s\n", file_types[i].ext, file_types[i].desc);
      mxexit(0);

    } else if ((this_arg == "--list-languages")) {
      list_iso639_languages();
      mxexit(0);

    } else if ((this_arg == "-i") || (this_arg == "--identify"))
      mxerror(_("'%s' can only be used with a file name. "
                "No other options are allowed if '%s' is used.\n"),
              this_arg.c_str(), this_arg.c_str());

    else if ((this_arg == "--capabilities")) {
      print_capabilities();
      mxexit(0);

    }

  }

  mxinfo(VERSIONINFO " built on " __DATE__ " " __TIME__ "\n");

  // Now parse options that are needed right at the beginning.
  foreach(sit, args) {
    this_arg = *sit;
    if ((sit + 1) == args.end())
      next_arg = "";
    else
      next_arg = *(sit + 1);
    no_next_arg = (sit + 1) == args.end();

    if ((this_arg == "-o") || (this_arg == "--output")) {
      if (no_next_arg)
        mxerror(_("'%s' lacks a file name.\n"), this_arg.c_str());

      if (outfile != "")
        mxerror(_("Only one output file allowed.\n"));

      outfile = next_arg;
      sit++;

    } else if ((this_arg == "--engage")) {
      if (no_next_arg)
        mxerror(_("'--engage' lacks its argument.\n"));
      engage_hacks(next_arg);
      sit++;
    }
  }

  if (outfile == "") {
    mxinfo(_("Error: no output file name was given.\n\n"));
    usage();
    mxexit(2);
  }

  foreach(sit, args) {
    this_arg = *sit;
    if ((sit + 1) == args.end())
      next_arg = "";
    else
      next_arg = *(sit + 1);
    no_next_arg = (sit + 1) == args.end();

    // Ignore the options we took care of in the first step.
    if ((this_arg == "-o") || (this_arg == "--output") ||
        (this_arg == "--command-line-charset") ||
        (this_arg == "--engage")) {
      sit++;
      continue;
    }

    // Global options
    if ((this_arg == "--priority")) {
      bool found;

      if (no_next_arg)
        mxerror(_("'--priority' lacks its argument.\n"));
      found = false;
      for (i = 0; i < 5; i++)
        if ((next_arg == process_priorities[i])) {
          set_process_priority(i - 2);
          found = true;
          break;
        }
      if (!found)
        mxerror(_("'%s' is not a valid priority class.\n"), next_arg.c_str());
      sit++;

    } else if ((this_arg == "-q"))
      verbose = 0;

    else if ((this_arg == "-v") || (this_arg == "--verbose"))
      verbose++;

    else if ((this_arg == "--title")) {
      if (no_next_arg)
        mxerror(_("'--title' lacks the title.\n"));

      segment_title = next_arg;
      segment_title_set = true;
      sit++;

    } else if ((this_arg == "--split")) {
      if ((no_next_arg) || (next_arg[0] == 0))
        mxerror(_("'--split' lacks the size.\n"));

      parse_split(next_arg);
      sit++;

    } else if ((this_arg == "--split-max-files")) {
      if ((no_next_arg) || (next_arg[0] == 0))
        mxerror(_("'--split-max-files' lacks the number of files.\n"));

      if (!parse_int(next_arg, split_max_num_files) ||
          (split_max_num_files < 2))
        mxerror(_("Wrong argument to '--split-max-files'.\n"));

      sit++;

    } else if ((this_arg == "--link")) {
      no_linking = false;

    } else if ((this_arg == "--link-to-previous")) {
      if ((no_next_arg) || (next_arg[0] == 0))
        mxerror(_("'--link-to-previous' lacks the next UID.\n"));

      if (seguid_link_previous != NULL)
        mxerror(_("The previous UID was already given in '%s %s'.\n"),
                this_arg.c_str(), next_arg.c_str());

      try {
        seguid_link_previous = new bitvalue_c(next_arg, 128);
      } catch (exception &ex) {
        mxerror(_("Unknown format for the previous UID in '%s %s'.\n"),
                this_arg.c_str(), next_arg.c_str());
      }

      sit++;

    } else if ((this_arg == "--link-to-next")) {
      if ((no_next_arg) || (next_arg[0] == 0))
        mxerror(_("'--link-to-next' lacks the previous UID.\n"));

      if (seguid_link_next != NULL)
        mxerror(_("The next UID was already given in '%s %s'.\n"),
                this_arg.c_str(), next_arg.c_str());

      try {
        seguid_link_next = new bitvalue_c(next_arg, 128);
      } catch (exception &ex) {
        mxerror(_("Unknown format for the next UID in '%s %s'.\n"),
                this_arg.c_str(), next_arg.c_str());
      }

      sit++;

    } else if ((this_arg == "--cluster-length")) {
      int idx;

      if (no_next_arg)
        mxerror(_("'--cluster-length' lacks the length.\n"));

      idx = next_arg.find("ms");
      if (idx >= 0) {
        next_arg.erase(idx);
        if (!parse_int(next_arg, max_ns_per_cluster) ||
            (max_ns_per_cluster < 100) ||
            (max_ns_per_cluster > 32000))
          mxerror(_("Cluster length '%s' out of range (100..32000).\n"),
                  next_arg.c_str());
        max_ns_per_cluster *= 1000000;

        max_blocks_per_cluster = 65535;
      } else {
        if (!parse_int(next_arg, max_blocks_per_cluster) ||
            (max_blocks_per_cluster < 0) ||
            (max_blocks_per_cluster > 60000))
          mxerror(_("Cluster length '%s' out of range (0..60000).\n"),
                  next_arg.c_str());

        max_ns_per_cluster = 32000000000ull;
      }

      sit++;

    } else if ((this_arg == "--no-cues"))
      write_cues = false;

    else if ((this_arg == "--no-clusters-in-meta-seek"))
      write_meta_seek_for_clusters = false;

    else if ((this_arg == "--disable-lacing"))
      no_lacing = true;

    else if ((this_arg == "--enable-durations"))
      use_durations = true;

    else if ((this_arg == "--attachment-description")) {
      if (no_next_arg)
        mxerror(_("'--attachment-description' lacks the description.\n"));

      if (attachment.description != NULL)
        mxwarn(_("More than one description was given for a single attachment."
                 "\n"));
      safefree(attachment.description);
      attachment.description = safestrdup(next_arg);
      sit++;

    } else if ((this_arg == "--attachment-mime-type")) {
      if (no_next_arg)
        mxerror(_("'--attachment-mime-type' lacks the MIME type.\n"));

      if (attachment.mime_type != NULL)
        mxwarn(_("More than one MIME type was given for a single attachment. "
                 "'%s' will be discarded and '%s' used instead.\n"),
               attachment.mime_type, next_arg.c_str());
      safefree(attachment.mime_type);
      attachment.mime_type = safestrdup(next_arg);
      sit++;

    } else if ((this_arg == "--attach-file") ||
               (this_arg == "--attach-file-once")) {
      if (no_next_arg)
        mxerror(_("'%s' lacks the file name.\n"), this_arg.c_str());

      attachment.name = from_utf8_c(cc_local_utf8, next_arg);
      if (attachment.mime_type == NULL)
        attachment.mime_type = safestrdup(guess_mime_type(next_arg));

      if ((this_arg == "--attach-file"))
        attachment.to_all_files = true;
      try {
        io = new mm_file_io_c(attachment.name);
        attachment.size = io->get_size();
        delete io;
        if (attachment.size == 0)
          throw exception();
      } catch (...) {
        mxerror(_("The attachment '%s' could not be read, or its "
                  "size is 0.\n"), attachment.name);
      }

      attachments.push_back(attachment);
      memset(&attachment, 0, sizeof(attachment_t));

      sit++;

    } else if ((this_arg == "--global-tags")) {
      if (no_next_arg)
        mxerror(_("'--global-tags' lacks the file name.\n"));

      parse_and_add_tags(from_utf8(cc_local_utf8, next_arg).c_str());
      sit++;

    } else if ((this_arg == "--chapter-language")) {
      if (no_next_arg)
        mxerror(_("'--chapter-language' lacks the language.\n"));

      if (chapter_language != "")
        mxerror(_("'--chapter-language' may only be given once in '"
                  "--chapter-language %s'.\n"), next_arg.c_str());

      if (chapter_file_name != "")
        mxerror(_("'--chapter-language' must be given before '--chapters' in "
                  "'--chapter-language %s'.\n"), next_arg.c_str());

      if (!is_valid_iso639_2_code(next_arg.c_str()))
        mxerror(_("'%s' is not a valid ISO639-2 language code. Run "
                  "'mkvmerge --list-languages' for a complete list of all "
                  "languages and their respective ISO639-2 codes.\n"),
                next_arg.c_str());

      chapter_language = next_arg;
      sit++;

    } else if ((this_arg == "--chapter-charset")) {
      if (no_next_arg)
        mxerror(_("'--chapter-charset' lacks the charset.\n"));

      if (chapter_charset != "")
        mxerror(_("'--chapter-charset' may only be given once in '"
                  "--chapter-charset %s'.\n"), next_arg.c_str());

      if (chapter_file_name != "")
        mxerror(_("'--chapter-charset' must be given before '--chapters' in "
                  "'--chapter-charset %s'.\n"), next_arg.c_str());

      chapter_charset = next_arg;
      sit++;

    } else if ((this_arg == "--cue-chapter-name-format")) {
      if (no_next_arg)
        mxerror(_("'--cue-chapter-name-format' lacks the format.\n"));
      if (chapter_file_name != "")
        mxerror(_("'--cue-chapter-name-format' must be given before "
                  "'--chapters'.\n"));

      cue_to_chapter_name_format = next_arg;
      sit++;

    } else if ((this_arg == "--chapters")) {
      if (no_next_arg)
        mxerror(_("'--chapters' lacks the file name.\n"));

      if (chapter_file_name != "")
        mxerror(_("Only one chapter file allowed in '%s %s'.\n"),
                this_arg.c_str(), next_arg.c_str());

      chapter_file_name = from_utf8(cc_local_utf8, next_arg);
      if (kax_chapters != NULL)
        delete kax_chapters;
      kax_chapters = parse_chapters(chapter_file_name.c_str(), 0, -1, 0,
                                    chapter_language.c_str(),
                                    chapter_charset.c_str(), false,
                                    false, &tags_from_cue_chapters);
      sit++;

    } else if ((this_arg == "--no-chapters")) {
      ti->no_chapters = true;

    } else if ((this_arg == "--no-attachments")) {
      ti->no_attachments = true;

    } else if ((this_arg == "--no-tags")) {
      ti->no_tags = true;

    } else if ((this_arg == "--meta-seek-size")) {
      mxwarn(_("The option '--meta-seek-size' is no longer supported. Please "
               "read mkvmerge's documentation, especially the section about "
               "the MATROSKA FILE LAYOUT."));
      sit++;

    } else if ((this_arg == "--timecode-scale")) {
      int64_t temp;

      if (no_next_arg)
        mxerror(_("'--timecode-scale' lacks its argument.\n"));
      if (timecode_scale_mode != timecode_scale_mode_normal)
        mxerror(_("'--timecode-scale' was used more than once.\n"));

      if (!parse_int(next_arg, temp))
        mxerror(_("The argument to '--timecode-scale' must be a number.\n"));

      if (temp == -1)
        timecode_scale_mode = timecode_scale_mode_auto;
      else {
        if ((temp > 10000000) || (temp < 1000))
          mxerror(_("The given timecode scale factor is outside the valid "
                    "range (1000...10000000 or -1 for 'sample precision "
                    "even if a video track is present').\n"));

        timecode_scale = temp;
        timecode_scale_mode = timecode_scale_mode_fixed;
      }
      sit++;
    }


    // Options that apply to the next input file only.
    else if ((this_arg == "-A") || (this_arg == "--noaudio"))
      ti->no_audio = true;

    else if ((this_arg == "-D") || (this_arg == "--novideo"))
      ti->no_video = true;

    else if ((this_arg == "-S") || (this_arg == "--nosubs"))
      ti->no_subs = true;

    else if ((this_arg == "-a") || (this_arg == "--atracks")) {
      if (no_next_arg)
        mxerror(_("'%s' lacks the stream number(s).\n"), this_arg.c_str());

      parse_tracks(next_arg, ti->atracks, this_arg);
      sit++;

    } else if ((this_arg == "-d") || (this_arg == "--vtracks")) {
      if (no_next_arg)
        mxerror(_("'%s' lacks the stream number(s).\n"), this_arg.c_str());

      parse_tracks(next_arg, ti->vtracks, this_arg);
      sit++;

    } else if ((this_arg == "-s") || (this_arg == "--stracks")) {
      if (no_next_arg)
        mxerror(_("'%s' lacks the stream number(s).\n"), this_arg.c_str());

      parse_tracks(next_arg, ti->stracks, this_arg);
      sit++;

    } else if ((this_arg == "-f") || (this_arg == "--fourcc")) {
      if (no_next_arg)
        mxerror(_("'%s' lacks the FourCC.\n"), this_arg.c_str());

      parse_fourcc(next_arg, this_arg, *ti);
      sit++;

    } else if ((this_arg == "--aspect-ratio")) {
      if (no_next_arg)
        mxerror(_("'--aspect-ratio' lacks the aspect ratio.\n"));

      parse_aspect_ratio(next_arg, this_arg, *ti, false);
      sit++;

    } else if ((this_arg == "--aspect-ratio-factor")) {
      if (no_next_arg)
        mxerror(_("'--aspect-ratio-factor' lacks the aspect ratio factor.\n"));

      parse_aspect_ratio(next_arg, this_arg, *ti, true);
      sit++;

    } else if ((this_arg == "--display-dimensions")) {
      if (no_next_arg)
        mxerror(_("'--display-dimensions' lacks the dimensions.\n"));

      parse_display_dimensions(next_arg, *ti);
      sit++;

    } else if ((this_arg == "--cropping")) {
      if (no_next_arg)
        mxerror(_("'--cropping' lacks the crop parameters.\n"));

      parse_cropping(next_arg, *ti);
      sit++;

    } else if ((this_arg == "-y") || (this_arg == "--sync")) {
      if (no_next_arg)
        mxerror(_("'%s' lacks the audio delay.\n"), this_arg.c_str());

      parse_sync(next_arg, async, this_arg);
      ti->audio_syncs->push_back(async);
      sit++;

    } else if ((this_arg == "--cues")) {
      if (no_next_arg)
        mxerror(_("'--cues' lacks its argument.\n"));

      parse_cues(next_arg, cues);
      ti->cue_creations->push_back(cues);
      sit++;

    } else if ((this_arg == "--delay")) {
      if (no_next_arg)
        mxerror(_("'--delay' lacks the delay to apply.\n"));

      parse_delay(next_arg, async);
      ti->packet_delays->push_back(async);
      sit++;

    } else if ((this_arg == "--default-track")) {
      if (no_next_arg)
        mxerror(_("'--default-track' lacks the track ID.\n"));

      if (!parse_int(next_arg, id))
        mxerror(_("Invalid track ID specified in '%s %s'.\n"),
                this_arg.c_str(), next_arg.c_str());

      ti->default_track_flags->push_back(id);
      sit++;

    } else if ((this_arg == "--language")) {
      if (no_next_arg)
        mxerror(_("'--language' lacks its argument.\n"));

      parse_language(next_arg, lang, "language", "language", true);
      ti->languages->push_back(lang);
      sit++;

    } else if ((this_arg == "--default-language")) {
      if (no_next_arg)
        mxerror(_("'--default-language' lacks its argument.\n"));

      if (!is_valid_iso639_2_code(next_arg.c_str()))
        mxerror(_("'%s' is not a valid ISO639-2 language code in "
                  "'--default-language %s'. Run 'mkvmerge --list-languages' "
                  "for a list of all languages and their respective "
                  "ISO639-2 codes.\n"), next_arg.c_str(), next_arg.c_str());
      default_language = next_arg;
      sit++;

    } else if ((this_arg == "--sub-charset")) {
      if (no_next_arg)
        mxerror(_("'--sub-charset' lacks its argument.\n"));

      parse_sub_charset(next_arg, lang);
      ti->sub_charsets->push_back(lang);
      sit++;

    } else if ((this_arg == "-t") || (this_arg == "--tags")) {
      if (no_next_arg)
        mxerror(_("'%s' lacks its argument.\n"), this_arg.c_str());

      parse_tags(next_arg, tags, this_arg);
      ti->all_tags->push_back(tags);
      sit++;

    } else if ((this_arg == "--aac-is-sbr")) {
      if (no_next_arg)
        mxerror(_("'%s' lacks the track ID.\n"), this_arg.c_str());

      if (!parse_int(next_arg, id) || (id < 0))
        mxerror(_("Invalid track ID specified in '%s %s'.\n"),
                this_arg.c_str(), next_arg.c_str());

      ti->aac_is_sbr->push_back(id);
      sit++;

    } else if ((this_arg == "--compression")) {
      if (no_next_arg)
        mxerror(_("'--compression' lacks its argument.\n"));

      parse_compression(next_arg, cues);
      ti->compression_list->push_back(cues);
      sit++;

    } else if ((this_arg == "--track-name")) {
      if (no_next_arg)
        mxerror(_("'--track-name' lacks its argument.\n"));

      parse_language(next_arg, lang, "track-name", "track name", false);
      ti->track_names->push_back(lang);
      sit++;

    } else if ((this_arg == "--timecodes")) {
      string s;

      if (no_next_arg)
        mxerror(_("'--timecodes' lacks its argument.\n"));

      s = from_utf8(cc_local_utf8, next_arg);
      parse_language(s, lang, "timecodes", "timecodes", false);
      ti->all_ext_timecodes->push_back(lang);
      sit++;

    } else if ((this_arg == "--track-order")) {
      if (no_next_arg)
        mxerror(_("'--track-order' lacks its argument.\n"));
      if (track_order.size() > 0)
        mxerror(_("'--track-order' may only be given once.\n"));

      parse_track_order(next_arg);
      sit++;

    } else if ((this_arg == "--append-to")) {
      if (no_next_arg)
        mxerror(_("'--append-to' lacks its argument.\n"));

      parse_append_to(next_arg, *ti);
      sit++;

    }

    else if (this_arg.length() == 0)
      mxerror("An empty file name is not valid.\n");

    // The argument is an input file.
    else {
      if (outfile == this_arg)
        mxerror(_("The name of the output file '%s' and of one of the input "
                  "files is the same. This would cause mkvmerge to overwrite "
                  "one of your input files. This is most likely not what you "
                  "want.\n"), outfile.c_str());

      if ((ti->atracks->size() != 0) && ti->no_audio)
        mxerror(_("'-A' and '-a' used on the same source file.\n"));

      if ((ti->vtracks->size() != 0) && ti->no_video)
        mxerror(_("'-D' and '-d' used on the same source file.\n"));

      if ((ti->stracks->size() != 0) && ti->no_subs)
        mxerror(_("'-S' and '-s' used on the same source file.\n"));

      memset(&file, 0, sizeof(filelist_t));

      if (this_arg[0] == '+') {
        this_arg.erase(0, 1);
        if (this_arg.size() == 0)
          mxerror(_("A single '+' is not a valid command line option. If you "
                    "want to append a file use '+' directly followed by the "
                    "file name, e.g. '+movie_part_2.avi'."));
        if (files.size() == 0)
          mxerror("The first file cannot be appended because there are no "
                  "files to append to.\n");
        file.appending = true;
      }
      file.name = from_utf8_c(cc_local_utf8, this_arg);
      file.type = get_file_type(file.name);
      ti->fname = from_utf8_c(cc_local_utf8, this_arg);

      if (file.type == TYPEUNKNOWN)
        mxerror(_("The file '%s' has unknown type. Please have a look "
                  "at the supported file types ('mkvmerge --list-types') and "
                  "contact the author Moritz Bunkus <moritz@bunkus.org> if "
                  "your file type is supported but not recognized "
                  "properly.\n"), file.name);

      if (file.type != TYPECHAPTERS) {
        file.pack = NULL;
        file.ti = ti;
        file.done = false;

        files.push_back(file);
      } else {
        delete ti;
        safefree(file.name);
        memset(&file, 0, sizeof(filelist_t));
      }

      ti = new track_info_c;
    }
  }

  if (files.size() == 0) {
    usage();
    mxexit();
  }

  if (no_linking &&
      ((seguid_link_previous != NULL) || (seguid_link_next != NULL))) {
    mxwarn(_("'--link' must be used if '--link-to-previous' or "
             "'--link-to-next' are used as well. File linking will be turned "
             "on now.\n"));
    no_linking = false;
  }
  if ((split_after <= 0) && !no_linking)
    mxwarn(_("'--link', '--link-to-previous' and '--link-to-next' are "
             "only useful in combination with '--split'.\n"));

  delete ti;
  safefree(attachment.description);
  safefree(attachment.mime_type);
}

/** \brief Reads command line arguments from a file
 *
 * Each line contains exactly one command line argument or a
 * comment. Arguments are converted to UTF-8 and appended to the array
 * \c args.
 */
static void
read_args_from_file(vector<string> &args,
                    char *filename) {
  mm_text_io_c *mm_io;
  string buffer;
  bool skip_next;

  mm_io = NULL;
  try {
    mm_io = new mm_text_io_c(new mm_file_io_c(filename));
  } catch (exception &ex) {
    mxerror(_("The file '%s' could not be opened for reading command line "
              "arguments."), filename);
  }

  skip_next = false;
  while (!mm_io->eof() && mm_io->getline2(buffer)) {
    if (skip_next) {
      skip_next = false;
      continue;
    }
    strip(buffer);

    if (buffer == "#EMPTY#") {
      args.push_back("");
      continue;
    }

    if ((buffer[0] == '#') || (buffer[0] == 0))
      continue;

    if (buffer == "--command-line-charset") {
      skip_next = true;
      continue;
    }
    args.push_back(buffer);
  }

  delete mm_io;
}

/** \brief Expand the command line parameters
 *
 * Takes each command line paramter, converts it to UTF-8, and reads more
 * commands from command files if the argument starts with '@'. Puts all
 * arguments into a new array.
 */
static void
handle_args(int argc,
            char **argv) {
  int i, cc_command_line;
  vector<string> args;

  cc_command_line = cc_local_utf8;

  for (i = 1; i < argc; i++)
    if (argv[i][0] == '@')
      read_args_from_file(args, &argv[i][1]);
    else {
      if (!strcmp(argv[i], "--command-line-charset")) {
        if ((i + 1) == argc)
          mxerror(_("'--command-line-charset' is missing its argument.\n"));
        cc_command_line = utf8_init(argv[i + 1]);
        i++;
      } else
        args.push_back(to_utf8(cc_command_line, argv[i]));
    }

  parse_args(args);
}

/** \brief Initialize global variables
 */
static void
init_globals() {
  memset(default_tracks, 0, sizeof(default_tracks));
  memset(default_tracks_priority, 0, sizeof(default_tracks_priority));
  clear_list_of_unique_uint32(UNIQUE_ALL_IDS);
}

/** \brief Setup and high level program control
 *
 * Calls the functions for setup, handling the command line arguments,
 * creating the readers, the main loop, finishing the current output
 * file and cleaning up.
 */
int
main(int argc,
     char **argv) {
  time_t start, end;

  init_globals();

  setup();

  handle_args(argc, argv);

  if (split_after > 0)
    splitting = true;

  start = time(NULL);

  create_readers();

  if (packetizers.size() == 0)
    mxerror(_("No streams to output were found. Aborting.\n"));

  create_next_output_file();
  main_loop();
  finish_file(true);

  end = time(NULL);
  mxinfo(_("Muxing took %ld second%s.\n"), end - start,
         (end - start) == 1 ? "" : "s");

  cleanup();

  mxexit();
}
