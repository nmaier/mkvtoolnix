/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   retrieves and displays information about a Matroska file

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

// {{{ includes

#include "os.h"

#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(COMP_MSC)
#include <cassert>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <iostream>
#include <typeinfo>

extern "C" {
#include <avilib.h>
}

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>
#include <matroska/KaxVersion.h>

#if !defined(MATROSKA_VERSION)
#define MATROSKA_VERSION 2
#endif

#include "mkvinfo.h"

#include "chapters.h"
#include "checksums.h"
#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "mm_io.h"
#include "xml_element_mapping.h"

using namespace libmatroska;
using namespace std;

// }}}

// {{{ structs, output/helper functions

typedef struct {
  unsigned int tnum, tuid;
  char type;
  int64_t size;
} kax_track_t;

kax_track_t **tracks = NULL;
int num_tracks = 0;
bool use_gui = false;
bool calc_checksums = false;
bool show_summary = false;
bool show_hexdump = false;
int hexdump_max_size = 16;
uint64_t tc_scale = TIMECODE_SCALE;

void
add_track(kax_track_t *s) {
  tracks = (kax_track_t **)saferealloc(tracks, sizeof(kax_track_t *) *
                                       (num_tracks + 1));
  tracks[num_tracks] = s;
  num_tracks++;
}

kax_track_t *
find_track(int tnum) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->tnum == tnum)
      return tracks[i];

  return NULL;
}

kax_track_t *
find_track_by_uid(int tuid) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->tuid == tuid)
      return tracks[i];

  return NULL;
}

void
set_usage() {
  usage_text =
    Y("Usage: mkvinfo [options] inname\n\n"
      " options:\n"
      "  -g, --gui      Start the GUI (and open inname if it was given).\n"
      "  inname         Use 'inname' as the source.\n"
      "  -v, --verbose  Increase verbosity. See the man page for a detailed\n"
      "                 description of what mkvinfo outputs.\n"
      "  -c, --checksum Calculate and display checksums of frame contents.\n"
      "  -s, --summary  Only show summaries of the contents, not each element.\n"
      "  -x, --hexdump  Show the first 16 bytes of each frame as a hex dump.\n"
      "  --output-charset <charset>\n"
      "                 Output messages in this charset\n"
      "  -r, -o, --redirect-output file.ext\n"
      "                 Redirect all messages to this file.\n"
      "  -h, --help     Show this help.\n"
      "  -V, --version  Show version information.\n");

  version_info = (boost::format("mkvinfo v%1% ('%2%')") % VERSION % VERSIONNAME).str();
}

#define UTF2STR(s)                 UTFstring_to_cstrutf8(UTFstring(s))

#define show_error(error)          ui_show_error(error)
#define show_warning(l, f)         _show_element(NULL, NULL, false, l, f)
#define show_unknown_element(e, l) _show_unknown_element(es, e, l)
#define show_element(e, l, s)      _show_element(e, es, false, l, s)

static void _show_element(EbmlElement *l, EbmlStream *es, bool skip, int level, const std::string &info);

static void
_show_unknown_element(EbmlStream *es,
                      EbmlElement *e,
                      int level) {
  int i;
  string element_id;
  for (i = e->Generic().GlobalId.Length - 1; 0 <= i; --i)
    element_id += (boost::format("%|1$02x|") % ((e->Generic().GlobalId.Value >> (i * 8)) & 0xff)).str();

  string s = (boost::format(Y("(Unknown element: %1%; ID: 0x%2% size: %3%)")) % e->Generic().DebugName % element_id % (e->GetSize() + e->HeadSize())).str();
  _show_element(e, es, true, level, s);
}

static void
_show_element(EbmlElement *l,
              EbmlStream *es,
              bool skip,
              int level,
              const std::string &info) {
  if (show_summary)
    return;

  ui_show_element(level, info, NULL != l ? l->GetElementPosition() : -1);

  if ((NULL == l) || !skip)
    return;

  // Dump unknown elements recursively.
  EbmlMaster *m = dynamic_cast<EbmlMaster *>(l);
  if (NULL != m) {
    int i;
    for (i = 0; i < m->ListSize(); i++)
      show_unknown_element((*m)[i], level + 1);
  }

  l->SkipData(*es, l->Generic().Context);
}

inline void
_show_element(EbmlElement *l,
              EbmlStream *es,
              bool skip,
              int level,
              const boost::format &info) {
  _show_element(l, es, skip, level, info.str());
}

static string
create_hexdump(const unsigned char *buf,
               int size) {
  string hex(" hexdump");
  int bmax = MXMAX(size, hexdump_max_size);
  int b;

  for (b = 0; b < bmax; ++b)
    hex += (boost::format(" %|1$02x|") % buf[b]).str();

  return hex;
}

static string
to_hex(const unsigned char *buf,
       int size) {
  string hex;
  int b;
  for (b = 0; b < size; ++b)
    hex += (boost::format(" 0x%|1$02x|") % (int)buf[b]).str();

  return hex;
}

// }}}

// {{{ FUNCTION parse_args

void
parse_args(vector<string> args,
           string &file_name) {
  int i;

  verbose = 0;
  file_name = "";

  use_gui = false;

  handle_common_cli_args(args, "-o");

  // Now parse the rest of the arguments.
  for (i = 0; i < args.size(); i++)
    if ((args[i] == "-g") || (args[i] == "--gui")) {
      if (!ui_graphical_available())
        mxerror("mkvinfo was compiled without GUI support.\n");
      use_gui = true;
    } else if ((args[i] == "-c") || (args[i] == "--checksum"))
      calc_checksums = true;
    else if ((args[i] == "-C") || (args[i] == "--check-mode")) {
      calc_checksums = true;
      verbose = 4;
    } else if ((args[i] == "-s") || (args[i] == "--summary")) {
      calc_checksums = true;
      show_summary = true;
    } else if ((args[i] == "-x") || (args[i] == "--hexdump"))
      show_hexdump = true;
    else if ((args[i] == "-X") || (args[i] == "--full-hexdump")) {
      show_hexdump = true;
      hexdump_max_size = INT_MAX;
    } else if (file_name != "")
      mxerror(Y("Only one input file is allowed.\n"));
    else
      file_name = args[i];
}

// }}}

// {{{ is_global, parse_multicomment/chapter_atom, asctime_r, gmtime_r

#define in_parent(p) \
  (!p->IsFiniteSize() || \
   (in->getFilePointer() < \
    (p->GetElementPosition() + p->HeadSize() + p->GetSize())))

bool
is_global(EbmlStream *es,
          EbmlElement *l,
          int level) {
  if (is_id(l, EbmlVoid)) {
    show_element(l, level, (boost::format(Y("EbmlVoid (size: %1%)")) % (l->ElementSize() - l->HeadSize())).str());
    return true;

  } else if (is_id(l, EbmlCrc32)) {
    show_element(l, level, "EbmlCrc32");
    return true;
  }

  return false;
}

#if defined(COMP_MSC) || defined(COMP_MINGW)
struct tm *
gmtime_r(const time_t *timep,
         struct tm *result) {
  struct tm *aresult;

  aresult = gmtime(timep);
  memcpy(result, aresult, sizeof(struct tm));

  return result;
}

char *
asctime_r(const struct tm *tm,
          char *buf) {
  char *abuf;

  abuf = asctime(tm);
  strcpy(buf, abuf);

  return abuf;
}
#endif

// }}}

struct master_sorter_t {
  int m_index;
  int64_t m_pos;

  inline master_sorter_t(int index, int64_t pos):
    m_index(index), m_pos(pos) { }

  inline bool operator <(const master_sorter_t &cmp) const {
    return m_pos < cmp.m_pos;
  }
};

void
sort_master(EbmlMaster &m) {
  int i;
  vector<EbmlElement *> tmp;
  vector<master_sorter_t> sort_me;

  for (i = 0; m.ListSize() > i; ++i)
    sort_me.push_back(master_sorter_t(i, m[i]->GetElementPosition()));
  sort(sort_me.begin(), sort_me.end());

  for (i = 0; sort_me.size() > i; ++i)
    tmp.push_back(m[sort_me[i].m_index]);
  m.RemoveAll();

  for (i = 0; tmp.size() > i; ++i)
    m.PushElement(*tmp[i]);
}

void
read_master(EbmlMaster *m,
            EbmlStream *es,
            const EbmlSemanticContext &ctx,
            int &upper_lvl_el,
            EbmlElement *&l2) {

  m->Read(*es, ctx, upper_lvl_el, l2, true);
  if (m->ListSize() == 0)
    return;

  sort_master(*m);
}

string
format_binary(EbmlBinary &bin,
              int max_len = 10) {
  int len         = MXMAX(max_len, bin.GetSize());
  const binary *b = bin.GetBuffer();
  string result   = (boost::format(Y("length %1%, data: %2%")) % bin.GetSize() % to_hex(b, MXMAX(max_len, bin.GetSize()))).str();

  if (len < bin.GetSize())
    result += "...";

  if (calc_checksums)
    result += (boost::format(Y(" (adler: 0x%|1$08x|)")) % calc_adler32(bin.GetBuffer(), bin.GetSize())).str();

  strip(result);

  return result;
}

#define handle(f) handle_##f(in, es, upper_lvl_el, l1, l2, l3, l4, l5, l6)
#define def_handle(f) handle_##f(mm_io_c *&in, \
                                 EbmlStream *&es, \
                                 int &upper_lvl_el, \
                                 EbmlElement *&l1, \
                                 EbmlElement *&l2, \
                                 EbmlElement *&l3, \
                                 EbmlElement *&l4, \
                                 EbmlElement *&l5, \
                                 EbmlElement *&l6)
#define handle2(f, args...) \
  handle_##f(in, es, upper_lvl_el, l1, l2, l3, l4, l5, l6, ##args)
#define def_handle2(f, args...) handle_##f(mm_io_c *&in, \
                                           EbmlStream *&es, \
                                           int &upper_lvl_el, \
                                           EbmlElement *&l1, \
                                           EbmlElement *&l2, \
                                           EbmlElement *&l3, \
                                           EbmlElement *&l4, \
                                           EbmlElement *&l5, \
                                           EbmlElement *&l6, \
                                           ##args)


void
def_handle(chaptertranslate) {
  EbmlMaster *m2;
  int i2;

  show_element(l2, 2, Y("Chapter Translate"));

  m2 = static_cast<EbmlMaster *>(l2);
  for (i2 = 0; i2 < m2->ListSize(); i2++) {
    l3 = (*m2)[i2];

    if (is_id(l3, KaxChapterTranslateEditionUID)) {
      KaxChapterTranslateEditionUID &translate_edition_uid = *static_cast<KaxChapterTranslateEditionUID *>(l3);
      show_element(l3, 3, boost::format(Y("Chapter Translate Edition UID: %1%")) % uint64(translate_edition_uid));

    } else if (is_id(l3, KaxChapterTranslateCodec)) {
      KaxChapterTranslateCodec &translate_codec = *static_cast<KaxChapterTranslateCodec *>(l3);
      show_element(l3, 3, boost::format(Y("Chapter Translate Codec: %1%")) % uint64(translate_codec));

    } else if (is_id(l3, KaxChapterTranslateID)) {
      KaxChapterTranslateID &translate_id = *static_cast<KaxChapterTranslateID *>(l3);
      show_element(l3, 3, boost::format(Y("Chapter Translate ID: %1%")) % format_binary(*static_cast<EbmlBinary *>(&translate_id)));
    }
  }
}

void
def_handle(info) {
  // General info about this Matroska file
  show_element(l1, 1, Y("Segment information"));

  upper_lvl_el   = 0;
  EbmlMaster *m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  int i1;
  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxTimecodeScale)) {
      KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
      tc_scale = uint64(ktc_scale);
      show_element(l2, 2, boost::format(Y("Timecode scale: %1%")) % tc_scale);

    } else if (is_id(l2, KaxDuration)) {
      KaxDuration &duration = *static_cast<KaxDuration *>(l2);
      show_element(l2, 2,
                   boost::format(Y("Duration: %|1$.3f|s (%2%)"))
                   % (double(duration) * tc_scale / 1000000000.0)
                   % format_timecode((uint64_t)(double(duration) * tc_scale), 3));

    } else if (is_id(l2, KaxMuxingApp)) {
      KaxMuxingApp &muxingapp = *static_cast<KaxMuxingApp *>(l2);
      show_element(l2, 2, boost::format(Y("Muxing application: %1%")) % UTF2STR(muxingapp));

    } else if (is_id(l2, KaxWritingApp)) {
      KaxWritingApp &writingapp = *static_cast<KaxWritingApp *>(l2);
      show_element(l2, 2, boost::format(Y("Writing application: %1%")) % UTF2STR(writingapp));

    } else if (is_id(l2, KaxDateUTC)) {
      struct tm tmutc;
      time_t temptime;
      char buffer[40];
      KaxDateUTC &dateutc = *static_cast<KaxDateUTC *>(l2);
      temptime = dateutc.GetEpochDate();
      if ((gmtime_r(&temptime, &tmutc) != NULL) &&
          (asctime_r(&tmutc, buffer) != NULL)) {
        buffer[strlen(buffer) - 1] = 0;
        show_element(l2, 2, boost::format(Y("Date: %1% UTC")) % buffer);
      } else
        show_element(l2, 2, boost::format(Y("Date (invalid, value: %1%)")) % temptime);

    } else if (is_id(l2, KaxSegmentUID)) {
      KaxSegmentUID &uid = *static_cast<KaxSegmentUID *>(l2);
      show_element(l2, 2, boost::format(Y("Segment UID:%1%")) % to_hex(uid.GetBuffer(), uid.GetSize()));

    } else if (is_id(l2, KaxSegmentFamily)) {
      KaxSegmentFamily &family = *static_cast<KaxSegmentFamily *>(l2);
      show_element(l2, 2, boost::format(Y("Family UID:%1%")) % to_hex(family.GetBuffer(), family.GetSize()));

    } else if (is_id(l2, KaxChapterTranslate))
      handle(chaptertranslate);

    else if (is_id(l2, KaxPrevUID)) {
      KaxPrevUID &uid = *static_cast<KaxPrevUID *>(l2);
      show_element(l2, 2, boost::format(Y("Previous segment UID:%1%")) % to_hex(uid.GetBuffer(), uid.GetSize()));

    } else if (is_id(l2, KaxPrevFilename)) {
      KaxPrevFilename &filename = *static_cast<KaxPrevFilename *>(l2);
      show_element(l2, 2, boost::format(Y("Previous filename: %1%")) % UTF2STR(filename));

    } else if (is_id(l2, KaxNextUID)) {
      KaxNextUID &uid = *static_cast<KaxNextUID *>(l2);
      show_element(l2, 2, boost::format(Y("Next segment UID:%1%")) % to_hex(uid.GetBuffer(), uid.GetSize()));

    } else if (is_id(l2, KaxNextFilename)) {
      KaxNextFilename &filename = *static_cast<KaxNextFilename *>(l2);
      show_element(l2, 2, boost::format(Y("Next filename: %1%")) % UTF2STR(filename));

    } else if (is_id(l2, KaxSegmentFilename)) {
      KaxSegmentFilename &filename =
        *static_cast<KaxSegmentFilename *>(l2);
      show_element(l2, 2, boost::format(Y("Segment filename: %1%")) % UTF2STR(filename));

    } else if (is_id(l2, KaxTitle)) {
      KaxTitle &title = *static_cast<KaxTitle *>(l2);
      show_element(l2, 2, boost::format(Y("Title: %1%")) % UTF2STR(title));

    }
  }
}

void
def_handle2(audio_track,
            vector<string> &summary) {
  EbmlMaster *m3;
  int i3;

  show_element(l3, 3, "Audio track");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxAudioSamplingFreq)) {
      KaxAudioSamplingFreq &freq = *static_cast<KaxAudioSamplingFreq *>(l4);
      show_element(l4, 4, boost::format(Y("Sampling frequency: %1%")) % float(freq));
      summary.push_back((boost::format(Y("sampling freq: %1%")) % float(freq)).str());

    } else if (is_id(l4, KaxAudioOutputSamplingFreq)) {
      KaxAudioOutputSamplingFreq &ofreq = *static_cast<KaxAudioOutputSamplingFreq *>(l4);
      show_element(l4, 4, boost::format(Y("Output sampling frequency: %1%")) % float(ofreq));
      summary.push_back((boost::format(Y("output sampling freq: %1%")) % float(ofreq)).str());

    } else if (is_id(l4, KaxAudioChannels)) {
      KaxAudioChannels &channels = *static_cast<KaxAudioChannels *>(l4);
      show_element(l4, 4, boost::format(Y("Channels: %1%")) % uint64(channels));
      summary.push_back((boost::format(Y("channels: %1%")) % uint64(channels)).str());

#if MATROSKA_VERSION >= 2
    } else if (is_id(l4, KaxAudioPosition)) {
      KaxAudioPosition &positions = *static_cast<KaxAudioPosition *>(l4);
      show_element(l4, 4, boost::format(Y("Channel positions: %1%")) % format_binary(positions));
#endif

    } else if (is_id(l4, KaxAudioBitDepth)) {
      KaxAudioBitDepth &bps = *static_cast<KaxAudioBitDepth *>(l4);
      show_element(l4, 4, boost::format(Y("Bit depth: %1%")) % uint64(bps));
      summary.push_back((boost::format(Y("bits per sample: %1%")) % uint64(bps)).str());

    } else if (!is_global(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle2(video_track,
            vector<string> &summary) {
  show_element(l3, 3, Y("Video track"));

  EbmlMaster *m3 = static_cast<EbmlMaster *>(l3);
  int i3;
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxVideoPixelWidth)) {
      KaxVideoPixelWidth &width = *static_cast<KaxVideoPixelWidth *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel width: %1%")) % uint64(width));
      summary.push_back((boost::format(Y("pixel width: %1%")) % uint64(width)).str());

    } else if (is_id(l4, KaxVideoPixelHeight)) {
      KaxVideoPixelHeight &height = *static_cast<KaxVideoPixelHeight *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel height: %1%")) % uint64(height));
      summary.push_back((boost::format(Y("pixel height: %1%")) % uint64(height)).str());

    } else if (is_id(l4, KaxVideoDisplayWidth)) {
      KaxVideoDisplayWidth &width = *static_cast<KaxVideoDisplayWidth *>(l4);
      show_element(l4, 4, boost::format(Y("Display width: %1%")) % uint64(width));
      summary.push_back((boost::format(Y("display width: %1%")) % uint64(width)).str());

    } else if (is_id(l4, KaxVideoDisplayHeight)) {
      KaxVideoDisplayHeight &height = *static_cast<KaxVideoDisplayHeight *>(l4);
      show_element(l4, 4, boost::format(Y("Display height: %1%")) % uint64(height));
      summary.push_back((boost::format(Y("display height: %1%")) % uint64(height)).str());

    } else if (is_id(l4, KaxVideoPixelCropLeft)) {
      KaxVideoPixelCropLeft &left = *static_cast<KaxVideoPixelCropLeft *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel crop left: %1%")) % uint64(left));
      summary.push_back((boost::format(Y("pixel crop left: %1%")) % uint64(left)).str());

    } else if (is_id(l4, KaxVideoPixelCropTop)) {
      KaxVideoPixelCropTop &top = *static_cast<KaxVideoPixelCropTop *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel crop top: %1%")) % uint64(top));
      summary.push_back((boost::format(Y("pixel crop top: %1%")) % uint64(top)).str());

    } else if (is_id(l4, KaxVideoPixelCropRight)) {
      KaxVideoPixelCropRight &right = *static_cast<KaxVideoPixelCropRight *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel crop right: %1%")) % uint64(right));
      summary.push_back((boost::format(Y("pixel crop right: %1%")) % uint64(right)).str());

    } else if (is_id(l4, KaxVideoPixelCropBottom)) {
      KaxVideoPixelCropBottom &bottom = *static_cast<KaxVideoPixelCropBottom *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel crop bottom: %1%")) % uint64(bottom));
      summary.push_back((boost::format(Y("pixel crop bottom: %1%")) % uint64(bottom)).str());

#if MATROSKA_VERSION >= 2
    } else if (is_id(l4, KaxVideoDisplayUnit)) {
      KaxVideoDisplayUnit &unit = *static_cast<KaxVideoDisplayUnit *>(l4);
      show_element(l4, 4,
                   boost::format(Y("Display unit: %1%%2%"))
                   % uint64(unit)
                   % (  uint16(unit) == 0 ? Y(" (pixels)")
                      : uint16(unit) == 1 ? Y(" (centimeters)")
                      : uint16(unit) == 2 ? Y(" (inches)")
                      :                     ""));

    } else if (is_id(l4, KaxVideoGamma)) {
      KaxVideoGamma &gamma = *static_cast<KaxVideoGamma *>(l4);
      show_element(l4, 4, boost::format(Y("Gamma: %1%")) % float(gamma));

    } else if (is_id(l4, KaxVideoFlagInterlaced)) {
      KaxVideoFlagInterlaced &f_interlaced = *static_cast<KaxVideoFlagInterlaced *>(l4);
      show_element(l4, 4, boost::format(Y("Interlaced: %1%")) % uint64(f_interlaced));

    } else if (is_id(l4, KaxVideoStereoMode)) {
      KaxVideoStereoMode &stereo = *static_cast<KaxVideoStereoMode *>(l4);
      show_element(l4, 4,
                   boost::format(Y("Stereo mode: %1%%2%"))
                   % uint64(stereo)
                   % (  uint8(stereo) == 0 ? Y(" (mono)")
                      : uint8(stereo) == 1 ? Y(" (right eye)")
                      : uint8(stereo) == 2 ? Y(" (left eye)")
                      : uint8(stereo) == 3 ? Y(" (both eyes)")
                      :                      ""));

    } else if (is_id(l4, KaxVideoAspectRatio)) {
      KaxVideoAspectRatio &ar_type = *static_cast<KaxVideoAspectRatio *>(l4);
      show_element(l4, 4,
                   boost::format(Y("Aspect ratio type: %1%%2%"))
                   % uint64(ar_type)
                   % (  uint8(ar_type) == 0 ? Y(" (free resizing)")
                      : uint8(ar_type) == 1 ? Y(" (keep aspect ratio)")
                      : uint8(ar_type) == 2 ? Y(" (fixed)")
                      :                       ""));
#endif
    } else if (is_id(l4, KaxVideoColourSpace)) {
      KaxVideoColourSpace &cspace = *static_cast<KaxVideoColourSpace *>(l4);
      show_element(l4, 4, boost::format(Y("Colour space: %1%")) % format_binary(cspace));

    } else if (is_id(l4, KaxVideoFrameRate)) {
      KaxVideoFrameRate &framerate = *static_cast<KaxVideoFrameRate *>(l4);
      show_element(l4, 4, boost::format(Y("Frame rate: %1%")) % float(framerate));

    } else if (!is_global(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(content_encodings) {
  show_element(l3, 3, Y("Content encodings"));

  EbmlMaster *m3 = static_cast<EbmlMaster *>(l3);
  int i3;
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxContentEncoding)) {
      show_element(l4, 4, Y("Content encoding"));

      EbmlMaster *m4 = static_cast<EbmlMaster *>(l4);
      int i4;
      for (i4 = 0; i4 < m4->ListSize(); i4++) {
        l5 = (*m4)[i4];

        if (is_id(l5, KaxContentEncodingOrder)) {
          KaxContentEncodingOrder &ce_order = *static_cast<KaxContentEncodingOrder *>(l5);
          show_element(l5, 5, boost::format(Y("Order: %1%")) % uint64(ce_order));

        } else if (is_id(l5,  KaxContentEncodingScope)) {
          std::vector<std::string> scope;
          KaxContentEncodingScope &ce_scope = *static_cast<KaxContentEncodingScope *>(l5);

          if ((uint32(ce_scope) & 0x01) == 0x01)
            scope.push_back(Y("1: all frames"));
          if ((uint32(ce_scope) & 0x02) == 0x02)
            scope.push_back(Y("2: codec private data"));
          if ((uint32(ce_scope) & 0xfc) != 0x00)
            scope.push_back(Y("rest: unknown"));
          if (scope.empty())
            scope.push_back(Y("unknown"));
          show_element(l5, 5, boost::format(Y("Scope: %1% (%2%)")) % uint64(ce_scope) % join(", ", scope));

        } else if (is_id(l5,  KaxContentEncodingType)) {
          uint64_t ce_type = uint64(*static_cast<KaxContentEncodingType *>(l5));
          show_element(l5, 5,
                       boost::format(Y("Type: %1% (%2%)"))
                       % ce_type
                       % (  0 == ce_type ? Y("compression")
                          : 1 == ce_type ? Y("encryption")
                          :                Y("unknown")));

        } else if (is_id(l5, KaxContentCompression)) {
          show_element(l5, 5, Y("Content compression"));

          EbmlMaster *m5 = static_cast<EbmlMaster *>(l5);
          int i5;
          for (i5 = 0; i5 < m5->ListSize(); i5++) {
            l6 = (*m5)[i5];

            if (is_id(l6, KaxContentCompAlgo)) {
              uint64_t c_algo = uint64(*static_cast<KaxContentCompAlgo *>(l6));
              show_element(l6, 6,
                           boost::format(Y("Algorithm: %1% (%2%)"))
                           % c_algo
                           % (  0 == c_algo ?   "ZLIB"
                              : 1 == c_algo ?   "bzLib"
                              : 2 == c_algo ?   "lzo1x"
                              : 3 == c_algo ? Y("header removal")
                              :               Y("unknown")));

            } else if (is_id(l6, KaxContentCompSettings)) {
              KaxContentCompSettings &c_settings = *static_cast<KaxContentCompSettings *>(l6);
              show_element(l6, 6, boost::format(Y("Settings: %1%")) % format_binary(c_settings));

            } else if (!is_global(es, l6, 6))
              show_unknown_element(l6, 6);
          }

        } else if (is_id(l5, KaxContentEncryption)) {
          show_element(l5, 5, Y("Content encryption"));

          EbmlMaster *m5 = static_cast<EbmlMaster *>(l5);
          int i5;
          for (i5 = 0; i5 < m5->ListSize(); i5++) {
            l6 = (*m5)[i5];

            if (is_id(l6, KaxContentEncAlgo)) {
              uint64_t e_algo = uint64(*static_cast<KaxContentEncAlgo *>(l6));
              show_element(l6, 6,
                           boost::format(Y("Encryption algorithm: %1% (%2%)"))
                           % e_algo
                           % (  0 == e_algo ? Y("no encryption")
                              : 1 == e_algo ?   "DES"
                              : 2 == e_algo ?   "3DES"
                              : 3 == e_algo ?   "Twofish"
                              : 4 == e_algo ?   "Blowfish"
                              : 5 == e_algo ?   "AES"
                              :               Y("unknown")));

            } else if (is_id(l6, KaxContentEncKeyID)) {
              KaxContentEncKeyID &e_keyid = *static_cast<KaxContentEncKeyID *>(l6);
              show_element(l6, 6, boost::format(Y("Encryption key ID: %1%")) % format_binary(e_keyid));

            } else if (is_id(l6, KaxContentSigAlgo)) {
              uint64_t s_algo = uint64(*static_cast<KaxContentSigAlgo *>(l6));
              show_element(l6, 6,
                           boost::format(Y("Signature algorithm: %1% (%2%)"))
                           % s_algo
                           % (  0 == s_algo ? Y("no signature algorithm")
                              : 1 == s_algo ? Y("RSA")
                              :               Y("unknown")));

            } else if (is_id(l6, KaxContentSigHashAlgo)) {
              uint64_t s_halgo = uint64(*static_cast<KaxContentSigHashAlgo *>(l6));
              show_element(l6, 6,
                           boost::format(Y("Signature hash algorithm: %1% (%2%)"))
                           % s_halgo
                           % (  0 == s_halgo ? Y("no signature hash algorithm")
                              : 1 == s_halgo ? Y("SHA1-160")
                              : 2 == s_halgo ? Y("MD5")
                              :                Y("unknown")));

            } else if (is_id(l6, KaxContentSigKeyID)) {
              KaxContentSigKeyID &s_keyid = *static_cast<KaxContentSigKeyID *>(l6);
              show_element(l6, 6, boost::format(Y("Signature key ID: %1%")) % format_binary(s_keyid));

            } else if (is_id(l6, KaxContentSignature)) {
              KaxContentSignature &sig = *static_cast<KaxContentSignature *>(l6);
              show_element(l6, 6, boost::format(Y("Signature: %1%")) % format_binary(sig));

            } else if (!is_global(es, l6, 6))
              show_unknown_element(l6, 6);

          }

        } else if (!is_global(es, l5, 5))
          show_unknown_element(l5, 5);

      }

    } else if (!is_global(es, l4, 4))
      show_unknown_element(l4, 4);

  }
}

void
def_handle(tracks) {
  // Yep, we've found our KaxTracks element. Now find all tracks
  // contained in this segment.
  show_element(l1, 1, Y("Segment tracks"));

  upper_lvl_el = 0;
  EbmlMaster *m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  int i1;
  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxTrackEntry)) {
      // We actually found a track entry :) We're happy now.
      show_element(l2, 2, Y("A track"));

      vector<string> summary;
      char    kax_track_type   = '?';
      int64_t kax_track_number = -1;
      string  kax_codec_id;
      string  fourcc_buffer;
      bool    ms_compat        = false;

      EbmlMaster *m2 = static_cast<EbmlMaster *>(l2);
      int i2;
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        // Now evaluate the data belonging to this track
        if (is_id(l3, KaxTrackAudio))
          handle2(audio_track, summary);

        else if (is_id(l3, KaxTrackVideo))
          handle2(video_track, summary);

        else if (is_id(l3, KaxTrackNumber)) {
          KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
          kax_track_number     = uint64(tnum);

          show_element(l3, 3, boost::format(Y("Track number: %1%")) % uint64(tnum));
          if (find_track(uint64(tnum)) != NULL)
            show_warning(3, boost::format(Y("Warning: There's more than one track with the number %1%.")) % uint64(tnum));

        } else if (is_id(l3, KaxTrackUID)) {
          KaxTrackUID &tuid = *static_cast<KaxTrackUID *>(l3);
          show_element(l3, 3, boost::format(Y("Track UID: %1%")) % uint64(tuid));
          if (find_track_by_uid(uint64(tuid)) != NULL)
            show_warning(3, boost::format(Y("Warning: There's more than one track with the UID %1%.")) % uint64(tuid));

        } else if (is_id(l3, KaxTrackType)) {
          KaxTrackType &ttype = *static_cast<KaxTrackType *>(l3);

          switch (uint8(ttype)) {
            case track_audio:
              kax_track_type = 'a';
              break;
            case track_video:
              kax_track_type = 'v';
              break;
            case track_subtitle:
              kax_track_type = 's';
              break;
            case track_buttons:
              kax_track_type = 'b';
              break;
            default:
              kax_track_type = '?';
              break;
          }
          show_element(l3, 3,
                       boost::format(Y("Track type: %1%"))
                       % (  'a' == kax_track_type ? "audio"
                          : 'v' == kax_track_type ? "video"
                          : 's' == kax_track_type ? "subtitles"
                          : 'b' == kax_track_type ? "buttons"
                          :                         "unknown"));

#if MATROSKA_VERSION >= 2
        } else if (is_id(l3, KaxTrackFlagEnabled)) {
          KaxTrackFlagEnabled &fenabled = *static_cast<KaxTrackFlagEnabled *>(l3);
          show_element(l3, 3, boost::format(Y("Enabled: %1%")) % uint64(fenabled));
#endif

        } else if (is_id(l3, KaxTrackName)) {
          KaxTrackName &name = *static_cast<KaxTrackName *>(l3);
          show_element(l3, 3, boost::format(Y("Name: %1%")) % UTF2STR(name));

        } else if (is_id(l3, KaxCodecID)) {
          KaxCodecID &codec_id = *static_cast<KaxCodecID *>(l3);
          kax_codec_id         = string(codec_id);

          show_element(l3, 3, boost::format(Y("Codec ID: %1%")) % kax_codec_id);
          if (   ((kax_codec_id == MKV_V_MSCOMP) && ('v' == kax_track_type))
              || ((kax_codec_id == MKV_A_ACM)    && ('a' == kax_track_type)))
            ms_compat = true;

        } else if (is_id(l3, KaxCodecPrivate)) {
          fourcc_buffer = "";
          KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate *>(l3);
          if (ms_compat && ('v' == kax_track_type) && (c_priv.GetSize() >= sizeof(alBITMAPINFOHEADER))) {
            alBITMAPINFOHEADER *bih = (alBITMAPINFOHEADER *)c_priv.GetBuffer();
            unsigned char *fcc      = (unsigned char *)&bih->bi_compression;
            fourcc_buffer           = (boost::format(Y(" (FourCC: %1%%2%%3%%4%, 0x%|5$08x|)"))
                                       % fcc[0] % fcc[1] % fcc[2] % fcc[3] % get_uint32_le(&bih->bi_compression)).str();
          } else if (ms_compat && ('a' == kax_track_type) && (c_priv.GetSize() >= sizeof(alWAVEFORMATEX))) {
            alWAVEFORMATEX *wfe     = (alWAVEFORMATEX *)c_priv.GetBuffer();
            fourcc_buffer           = (boost::format(Y(" (format tag: 0x%|1$04x|)")) % get_uint16_le(&wfe->w_format_tag)).str();
          }

          if (calc_checksums && !show_summary)
            fourcc_buffer += (boost::format(Y(" (adler: 0x%|1$08x|)")) % calc_adler32(c_priv.GetBuffer(), c_priv.GetSize())).str();

          if (show_hexdump)
            fourcc_buffer += create_hexdump(c_priv.GetBuffer(), c_priv.GetSize());

          show_element(l3, 3, boost::format(Y("CodecPrivate, length %1%%2%")) % c_priv.GetSize() % fourcc_buffer);

        } else if (is_id(l3, KaxCodecName)) {
          KaxCodecName &c_name = *static_cast<KaxCodecName *>(l3);
          show_element(l3, 3, boost::format(Y("Codec name: %1%")) % UTF2STR(c_name));

#if MATROSKA_VERSION >= 2
        } else if (is_id(l3, KaxCodecSettings)) {
          KaxCodecSettings &c_sets = *static_cast<KaxCodecSettings *>(l3);
          show_element(l3, 3, boost::format(Y("Codec settings: %1%")) % UTF2STR(c_sets));

        } else if (is_id(l3, KaxCodecInfoURL)) {
          KaxCodecInfoURL &c_infourl = *static_cast<KaxCodecInfoURL *>(l3);
          show_element(l3, 3, boost::format(Y("Codec info URL: %1%")) % string(c_infourl));

        } else if (is_id(l3, KaxCodecDownloadURL)) {
          KaxCodecDownloadURL &c_downloadurl = *static_cast<KaxCodecDownloadURL *>(l3);
          show_element(l3, 3, boost::format(Y("Codec download URL: %1%")) % string(c_downloadurl));

        } else if (is_id(l3, KaxCodecDecodeAll)) {
          KaxCodecDecodeAll &c_decodeall =
            *static_cast<KaxCodecDecodeAll *>(l3);
          show_element(l3, 3, boost::format(Y("Codec decode all: %1%"))% uint64(c_decodeall));

        } else if (is_id(l3, KaxTrackOverlay)) {
          KaxTrackOverlay &overlay = *static_cast<KaxTrackOverlay *>(l3);
          show_element(l3, 3, boost::format(Y("Track overlay: %1%")) % uint64(overlay));
#endif // MATROSKA_VERSION >= 2

        } else if (is_id(l3, KaxTrackMinCache)) {
          KaxTrackMinCache &min_cache = *static_cast<KaxTrackMinCache *>(l3);
          show_element(l3, 3, boost::format(Y("MinCache: %1%")) % uint64(min_cache));

        } else if (is_id(l3, KaxTrackMaxCache)) {
          KaxTrackMaxCache &max_cache = *static_cast<KaxTrackMaxCache *>(l3);
          show_element(l3, 3, boost::format(Y("MaxCache: %1%")) % uint64(max_cache));

        } else if (is_id(l3, KaxTrackDefaultDuration)) {
          KaxTrackDefaultDuration &def_duration = *static_cast<KaxTrackDefaultDuration *>(l3);
          show_element(l3, 3,
                       boost::format(Y("Default duration: %|1$.3f|ms (%|2$.3f| fps for a video track)"))
                       % ((float)uint64(def_duration) / 1000000.0)
                       % (1000000000.0 / (float)uint64(def_duration)));
          summary.push_back((boost::format(Y("default duration: %|1$.3f|ms (%|2$.3f| fps for a video track)"))
                             % ((float)uint64(def_duration) / 1000000.0)
                             % (1000000000.0 / (float)uint64(def_duration))
                             ).str());

        } else if (is_id(l3, KaxTrackFlagLacing)) {
          KaxTrackFlagLacing &f_lacing = *static_cast<KaxTrackFlagLacing *>(l3);
          show_element(l3, 3, boost::format(Y("Lacing flag: %1%")) % uint64(f_lacing));

        } else if (is_id(l3, KaxTrackFlagDefault)) {
          KaxTrackFlagDefault &f_default = *static_cast<KaxTrackFlagDefault *>(l3);
          show_element(l3, 3, boost::format(Y("Default flag: %1%")) % uint64(f_default));

        } else if (is_id(l3, KaxTrackFlagForced)) {
          KaxTrackFlagForced &f_forced = *static_cast<KaxTrackFlagForced *>(l3);
          show_element(l3, 3, boost::format(Y("Forced flag: %1%")) % uint64(f_forced));

        } else if (is_id(l3, KaxTrackLanguage)) {
          KaxTrackLanguage &language = *static_cast<KaxTrackLanguage *>(l3);
          show_element(l3, 3, boost::format(Y("Language: %1%")) % string(language));
          summary.push_back((boost::format(Y("language: %1%")) % string(language)).str());

        } else if (is_id(l3, KaxTrackTimecodeScale)) {
          KaxTrackTimecodeScale &ttc_scale = *static_cast<KaxTrackTimecodeScale *>(l3);
          show_element(l3, 3, boost::format(Y("Timecode scale: %1%")) % float(ttc_scale));

        } else if (is_id(l3, KaxMaxBlockAdditionID)) {
          KaxMaxBlockAdditionID &max_block_add_id = *static_cast<KaxMaxBlockAdditionID *>(l3);
          show_element(l3, 3, boost::format(Y("Max BlockAddition ID: %1%")) % uint64(max_block_add_id));

        } else if (is_id(l3, KaxContentEncodings))
          handle(content_encodings);

        else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      }

      if (show_summary)
        mxinfo(boost::format(Y("Track %1%: %2%, codec ID: %3%%4%%5%%6%\n"))
               % kax_track_number
               % (  'a' == kax_track_type ? Y("audio")
                  : 'v' == kax_track_type ? Y("video")
                  : 's' == kax_track_type ? Y("subtitles")
                  : 'b' == kax_track_type ? Y("butons")
                  :                         Y("unknown"))
               % kax_codec_id
               % fourcc_buffer
               % (summary.empty() ? "" : ", ")
               % join(", ", summary));

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);
  }
}

void
def_handle(seek_head) {
  if ((verbose < 2) && !use_gui) {
    show_element(l1, 1, Y("Seek head (subentries will be skipped)"));
    return;
  }

  show_element(l1, 1, Y("Seek head"));

  upper_lvl_el   = 0;
  EbmlMaster *m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  int i1;
  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxSeek)) {
      show_element(l2, 2, Y("Seek entry"));

      EbmlMaster *m2 = static_cast<EbmlMaster *>(l2);
      int i2;
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        if (is_id(l3, KaxSeekID)) {
          KaxSeekID &seek_id = static_cast<KaxSeekID &>(*l3);
          EbmlId id(seek_id.GetBuffer(), seek_id.GetSize());

          show_element(l3, 3,
                       boost::format(Y("Seek ID:%1% (%2%)"))
                       % to_hex(seek_id.GetBuffer(), seek_id.GetSize())
                       % (  KaxInfo::ClassInfos.GlobalId        == id ? "KaxInfo"
                          : KaxCluster::ClassInfos.GlobalId     == id ? "KaxCluster"
                          : KaxTracks::ClassInfos.GlobalId      == id ? "KaxTracks"
                          : KaxCues::ClassInfos.GlobalId        == id ? "KaxCues"
                          : KaxAttachments::ClassInfos.GlobalId == id ? "KaxAttachments"
                          : KaxChapters::ClassInfos.GlobalId    == id ? "KaxChapters"
                          : KaxTags::ClassInfos.GlobalId        == id ? "KaxTags"
                          : KaxSeekHead::ClassInfos.GlobalId    == id ? "KaxSeekHead"
                          :                                             "unknown"));

        } else if (is_id(l3, KaxSeekPosition)) {
          KaxSeekPosition &seek_pos = static_cast<KaxSeekPosition &>(*l3);
          show_element(l3, 3, boost::format(Y("Seek position: %1%")) % uint64(seek_pos));

        } else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      } // while (l3 != NULL)


    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

void
def_handle(cues) {
  if (verbose < 2) {
    show_element(l1, 1, Y("Cues (subentries will be skipped)"));
    return;
  }

  show_element(l1, 1, "Cues");

  upper_lvl_el   = 0;
  EbmlMaster *m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  int i1;
  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxCuePoint)) {
      show_element(l2, 2, Y("Cue point"));

      EbmlMaster *m2 = static_cast<EbmlMaster *>(l2);
      int i2;
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        if (is_id(l3, KaxCueTime)) {
          KaxCueTime &cue_time = *static_cast<KaxCueTime *>(l3);
          show_element(l3, 3, boost::format(Y("Cue time: %|1$.3f|s")) % (tc_scale * ((float)uint64(cue_time)) / 1000000000.0));

        } else if (is_id(l3, KaxCueTrackPositions)) {
          show_element(l3, 3, Y("Cue track positions"));

          EbmlMaster *m3 = static_cast<EbmlMaster *>(l3);
          int i3;
          for (i3 = 0; i3 < m3->ListSize(); i3++) {
            l4 = (*m3)[i3];

            if (is_id(l4, KaxCueTrack)) {
              KaxCueTrack &cue_track = *static_cast<KaxCueTrack *>(l4);
              show_element(l4, 4, boost::format(Y("Cue track: %1%")) % uint64(cue_track));

            } else if (is_id(l4, KaxCueClusterPosition)) {
              KaxCueClusterPosition &cue_cp = *static_cast<KaxCueClusterPosition *>(l4);
              show_element(l4, 4, boost::format(Y("Cue cluster position: %1%")) % uint64(cue_cp));

            } else if (is_id(l4, KaxCueBlockNumber)) {
              KaxCueBlockNumber &cue_bn = *static_cast<KaxCueBlockNumber *>(l4);
              show_element(l4, 4, boost::format(Y("Cue block number: %1%")) % uint64(cue_bn));

#if MATROSKA_VERSION >= 2
            } else if (is_id(l4, KaxCueCodecState)) {
              KaxCueCodecState &cue_cs = *static_cast<KaxCueCodecState *>(l4);
              show_element(l4, 4, boost::format(Y("Cue codec state: %1%")) % uint64(cue_cs));

            } else if (is_id(l4, KaxCueReference)) {
              show_element(l4, 4, Y("Cue reference"));

              EbmlMaster *m4 = static_cast<EbmlMaster *>(l4);
              int i4;
              for (i4 = 0; i4 < m4->ListSize(); i4++) {
                l5 = (*m4)[i4];

                if (is_id(l5, KaxCueRefTime)) {
                  KaxCueRefTime &cue_rt = *static_cast<KaxCueRefTime *>(l5);
                  show_element(l5, 5, boost::format(Y("Cue ref time: %|1$.3f|s")) % tc_scale % (((float)uint64(cue_rt)) / 1000000000.0));

                } else if (is_id(l5, KaxCueRefCluster)) {
                  KaxCueRefCluster &cue_rc = *static_cast<KaxCueRefCluster *>(l5);
                  show_element(l5, 5, boost::format(Y("Cue ref cluster: %1%")) % uint64(cue_rc));

                } else if (is_id(l5, KaxCueRefNumber)) {
                  KaxCueRefNumber &cue_rn = *static_cast<KaxCueRefNumber *>(l5);
                  show_element(l5, 5, boost::format(Y("Cue ref number: %1%")) % uint64(cue_rn));

                } else if (is_id(l5, KaxCueRefCodecState)) {
                  KaxCueRefCodecState &cue_rcs = *static_cast<KaxCueRefCodecState *>(l5);
                  show_element(l5, 5, boost::format(Y("Cue ref codec state: %1%")) % uint64(cue_rcs));

                } else if (!is_global(es, l5, 5))
                  show_unknown_element(l5, 5);

              } // while (l5 != NULL)
#endif // MATROSKA_VERSION >= 2

            } else if (!is_global(es, l4, 4))
              show_unknown_element(l4, 4);

          } // while (l4 != NULL)

        } else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      } // while (l3 != NULL)

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

void
def_handle(attachments) {
  show_element(l1, 1, Y("Attachments"));

  upper_lvl_el   = 0;
  EbmlMaster *m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  int i1;
  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxAttached)) {
      show_element(l2, 2, Y("Attached"));

      EbmlMaster *m2 = static_cast<EbmlMaster *>(l2);
      int i2;
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        if (is_id(l3, KaxFileDescription)) {
          KaxFileDescription &f_desc = *static_cast<KaxFileDescription *>(l3);
          show_element(l3, 3, boost::format(Y("File description: %1%")) % UTF2STR(f_desc));

        } else if (is_id(l3, KaxFileName)) {
          KaxFileName &f_name = *static_cast<KaxFileName *>(l3);
          show_element(l3, 3, boost::format(Y("File name: %1%")) % UTF2STR(f_name));

        } else if (is_id(l3, KaxMimeType)) {
          KaxMimeType &mime_type = *static_cast<KaxMimeType *>(l3);
          show_element(l3, 3, boost::format(Y("Mime type: %1%")) % string(mime_type));

        } else if (is_id(l3, KaxFileData)) {
          KaxFileData &f_data = *static_cast<KaxFileData *>(l3);
          show_element(l3, 3, boost::format(Y("File data, size: %1%")) % f_data.GetSize());

        } else if (is_id(l3, KaxFileUID)) {
          KaxFileUID &f_uid = *static_cast<KaxFileUID *>(l3);
          show_element(l3, 3, boost::format(Y("File UID: %1%")) % uint64(f_uid));

        } else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      } // while (l3 != NULL)

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

void
def_handle2(silent_track,
            KaxCluster *&cluster) {
  show_element(l2, 2, "Silent Tracks");
  EbmlMaster *m2 = static_cast<EbmlMaster *>(l2);

  int i2;
  for (i2 = 0; i2 < m2->ListSize(); i2++) {
    l3 = (*m2)[i2];

    if (is_id(l3, KaxClusterSilentTrackNumber)) {
      KaxClusterSilentTrackNumber &c_silent = *static_cast<KaxClusterSilentTrackNumber *>(l3);
      show_element(l3, 3, boost::format(Y("Silent Track Number: %1%")) % uint64(c_silent));

    } else if (!is_global(es, l3, 3))
      show_unknown_element(l3, 3);
  }
}

void
def_handle2(block_group,
            KaxCluster *&cluster) {
  show_element(l2, 2, Y("Block group"));

  vector<int> frame_sizes;
  vector<uint32_t> frame_adlers;
  vector<string> frame_hexdumps;

  bool bref_found     = false;
  bool fref_found     = false;

  int64_t lf_timecode = 0;
  int64_t lf_tnum     = 0;

  float bduration     = -1.0;

  EbmlMaster *m2      = static_cast<EbmlMaster *>(l2);

  int i2;
  for (i2 = 0; i2 < m2->ListSize(); i2++) {
    l3 = (*m2)[i2];

    if (is_id(l3, KaxBlock)) {
      KaxBlock &block = *static_cast<KaxBlock *>(l3);
      block.SetParent(*cluster);
      show_element(l3, 3,
                   boost::format(Y("Block (track number %1%, %2% frame(s), timecode %|3$.3f|s = %4%)"))
                   % block.TrackNum()
                   % block.NumberFrames()
                   % ((float)block.GlobalTimecode() / 1000000000.0)
                   % format_timecode(block.GlobalTimecode()));

      lf_timecode = block.GlobalTimecode() / 1000000;
      lf_tnum     = block.TrackNum();
      bduration   = -1.0;

      int i;
      for (i = 0; i < (int)block.NumberFrames(); i++) {
        DataBuffer &data = block.GetBuffer(i);
        uint32_t adler   = calc_adler32(data.Buffer(), data.Size());

        string adler_str;
        if (calc_checksums)
          adler_str = (boost::format(Y(" (adler: 0x%|1$08x|)")) % adler).str();

        string hex;
        if (show_hexdump)
          hex = create_hexdump(data.Buffer(), data.Size());

        show_element(NULL, 4, boost::format(Y("Frame with size %1%%2%%3%")) % data.Size() % adler_str % hex);

        frame_sizes.push_back(data.Size());
        frame_adlers.push_back(adler);
        frame_hexdumps.push_back(hex);
      }

    } else if (is_id(l3, KaxBlockDuration)) {
      KaxBlockDuration &duration = *static_cast<KaxBlockDuration *>(l3);
      bduration = ((float)uint64(duration)) * tc_scale / 1000000.0;
      show_element(l3, 3, boost::format(Y("Block duration: %1%.%|2$06d|ms")) % (uint64(duration) * tc_scale / 1000000) % (uint64(duration) * tc_scale % 1000000));

    } else if (is_id(l3, KaxReferenceBlock)) {
      KaxReferenceBlock &k_reference = *static_cast<KaxReferenceBlock *>(l3);
      int reference                  = int64(k_reference) * tc_scale;

      if (0 >= reference) {
        bref_found  = true;
        reference  *= -1;
        show_element(l3, 3, boost::format(Y("Reference block: -%1%.%|2$06d|ms")) % (reference / 1000000) % (reference % 1000000));

      } else if (0 < reference) {
        fref_found = true;
        show_element(l3, 3, boost::format(Y("Reference block: %1%.%|2$06d|ms")) % (reference / 1000000) % (reference % 1000000));
      }

    } else if (is_id(l3, KaxReferencePriority)) {
      KaxReferencePriority &priority = *static_cast<KaxReferencePriority *>(l3);
      show_element(l3, 3, boost::format(Y("Reference priority: %1%")) % uint64(priority));

#if MATROSKA_VERSION >= 2
    } else if (is_id(l3, KaxBlockVirtual)) {
      KaxBlockVirtual &bvirt = *static_cast<KaxBlockVirtual *>(l3);
      show_element(l3, 3, boost::format(Y("Block virtual: %1%")) % format_binary(bvirt));

    } else if (is_id(l3, KaxReferenceVirtual)) {
      KaxReferenceVirtual &ref_virt = *static_cast<KaxReferenceVirtual *>(l3);
      show_element(l3, 3, boost::format(Y("Reference virtual: %1%")) % int64(ref_virt));

#endif // MATROSKA_VERSION >= 2
    } else if (is_id(l3, KaxBlockAdditions)) {
      show_element(l3, 3, Y("Additions"));

      EbmlMaster *m3 = static_cast<EbmlMaster *>(l3);
      int i3;
      for (i3 = 0; i3 < m3->ListSize(); i3++) {
        l4 = (*m3)[i3];

        if (is_id(l4, KaxBlockMore)) {
          show_element(l4, 4, Y("More"));

          EbmlMaster *m4 = static_cast<EbmlMaster *>(l4);
          int i4;
          for (i4 = 0; i4 < m4->ListSize(); i4++) {
            l5 = (*m4)[i4];

            if (is_id(l5, KaxBlockAddID)) {
              KaxBlockAddID &add_id = *static_cast<KaxBlockAddID *>(l5);
              show_element(l5, 5, boost::format(Y("AdditionalID: %1%")) % uint64(add_id));

            } else if (is_id(l5, KaxBlockAdditional)) {
              KaxBlockAdditional &block = *static_cast<KaxBlockAdditional *>(l5);
              show_element(l5, 5, boost::format(Y("Block additional: %1%")) % format_binary(block));

            } else if (!is_global(es, l5, 5))
              show_unknown_element(l5, 5);

          } // while (l5 != NULL)

        } else if (!is_global(es, l4, 4))
          show_unknown_element(l4, 4);

      } // while (l4 != NULL)

    } else if (is_id(l3, KaxSlices)) {
      show_element(l3, 3, Y("Slices"));

      EbmlMaster *m3 = static_cast<EbmlMaster *>(l3);
      int i3;
      for (i3 = 0; i3 < m3->ListSize(); i3++) {
        l4 = (*m3)[i3];

        if (is_id(l4, KaxTimeSlice)) {
          show_element(l4, 4, Y("Time slice"));

          EbmlMaster *m4 = static_cast<EbmlMaster *>(l4);
          int i4;
          for (i4 = 0; i4 < m4->ListSize(); i4++) {
            l5 = (*m4)[i4];

            if (is_id(l5, KaxSliceLaceNumber)) {
              KaxSliceLaceNumber &slace_number = *static_cast<KaxSliceLaceNumber *>(l5);
              show_element(l5, 5, boost::format(Y("Lace number: %1%")) % uint64(slace_number));

            } else if (is_id(l5, KaxSliceFrameNumber)) {
              KaxSliceFrameNumber &sframe_number = *static_cast<KaxSliceFrameNumber *>(l5);
              show_element(l5, 5, boost::format(Y("Frame number: %1%")) % uint64(sframe_number));

            } else if (is_id(l5, KaxSliceDelay)) {
              KaxSliceDelay &sdelay = *static_cast<KaxSliceDelay *>(l5);
              show_element(l5, 5, boost::format(Y("Delay: %|1$.3f|ms")) % (((float)uint64(sdelay)) * tc_scale / 1000000.0));

            } else if (is_id(l5, KaxSliceDuration)) {
              KaxSliceDuration &sduration = *static_cast<KaxSliceDuration *>(l5);
              show_element(l5, 5, boost::format(Y("Duration: %|1$.3f|ms")) % (((float)uint64(sduration)) * tc_scale / 1000000.0));

            } else if (is_id(l5, KaxSliceBlockAddID)) {
              KaxSliceBlockAddID &sbaid = *static_cast<KaxSliceBlockAddID *>(l5);
              show_element(l5, 5, boost::format(Y("Block additional ID: %1%")) % uint64(sbaid));

            } else if (!is_global(es, l5, 5))
              show_unknown_element(l5, 5);

          } // while (l5 != NULL)

        } else if (!is_global(es, l4, 4))
          show_unknown_element(l4, 4);

      } // while (l4 != NULL)

    } else if (!is_global(es, l3, 3))
      show_unknown_element(l3, 3);

  } // while (l3 != NULL)

  if (show_summary) {
    int fidx;

    for (fidx = 0; fidx < frame_sizes.size(); fidx++) {
      if (bduration != -1.0)
        mxinfo(boost::format(Y("%1% frame, track %2%, timecode %3% (%4%), duration %|5$.3f|, size %6%, adler 0x%|7$08x|%8%\n"))
               % (bref_found && fref_found ? 'B' : bref_found ? 'P' : !fref_found ? 'I' : 'P')
               % lf_tnum
               % lf_timecode
               % format_timecode(lf_timecode)
               % bduration
               % frame_sizes[fidx]
               % frame_adlers[fidx]
               % frame_hexdumps[fidx]);
      else
        mxinfo(boost::format(Y("%1% frame, track %2%, timecode %3% (%4%), size %5%, adler 0x%|6$08x|%7%\n"))
               % (bref_found && fref_found ? 'B' : bref_found ? 'P' : !fref_found ? 'I' : 'P')
               % lf_tnum
               % lf_timecode
               % format_timecode(lf_timecode)
               % frame_sizes[fidx]
               % frame_adlers[fidx]
               % frame_hexdumps[fidx]);
    }

  } else if (verbose > 2)
    show_element(NULL, 2,
                 boost::format(Y("[%1% frame for track %2%, timecode %3%]"))
                 % (bref_found && fref_found ? 'B' : bref_found ? 'P' : !fref_found ? 'I' : 'P')
                 % lf_tnum
                 % lf_timecode);
}

void
def_handle2(simple_block,
            KaxCluster *&cluster) {
  vector<int> frame_sizes;
  vector<uint32_t> frame_adlers;

  KaxSimpleBlock &block = *static_cast<KaxSimpleBlock *>(l2);
  block.SetParent(*cluster);

  uint64_t timecode = block.GlobalTimecode() / 1000000;

  string info;
  if (block.IsKeyframe())
    info = Y("key, ");
  if (block.IsDiscardable())
    info += Y("discardable, ");

  show_element(l2, 2,
               boost::format(Y("SimpleBlock (%1%track number %2%, %3% frame(s), timecode %|4$.3f|s = %5%)"))
               % info
               % block.TrackNum()
               % block.NumberFrames()
               % ((float)timecode / 1000.0)
               % format_timecode(block.GlobalTimecode()));

  int i;
  for (i = 0; i < (int)block.NumberFrames(); i++) {
    DataBuffer &data = block.GetBuffer(i);
    uint32_t adler   = calc_adler32(data.Buffer(), data.Size());

    string adler_str;
    if (calc_checksums)
      adler_str = (boost::format(Y(" (adler: 0x%|1$08x|)")) % adler).str();

    string hex;
    if (show_hexdump)
      hex = create_hexdump(data.Buffer(), data.Size());

    show_element(NULL, 4, boost::format(Y("Frame with size %1%%2%%3%")) % data.Size() % adler_str % hex);

    frame_sizes.push_back(data.Size());
    frame_adlers.push_back(adler);
  }

  if (show_summary) {
    int fidx;

    for (fidx = 0; fidx < frame_sizes.size(); fidx++)
      mxinfo(boost::format(Y("%1% frame, track %2%, timecode %3% (%4%), size %5%, adler 0x%|6$08x|\n"))
             % (block.IsKeyframe() ? 'I' : block.IsDiscardable() ? 'B' : 'P')
             % block.TrackNum()
             % timecode
             % format_timecode(timecode)
             % frame_sizes[fidx]
             % frame_adlers[fidx]);

  } else if (verbose > 2)
    show_element(NULL, 2,
                 boost::format(Y("[%1% frame for track %2%, timecode %3%]"))
                 % (block.IsKeyframe() ? 'I' : block.IsDiscardable() ? 'B' : 'P')
                 % block.TrackNum()
                 % timecode);
}

void
def_handle2(cluster,
            KaxCluster *&cluster,
            int64_t file_size) {
  cluster = (KaxCluster *)l1;

  if (use_gui)
    ui_show_progress(100 * cluster->GetElementPosition() / file_size, Y("Parsing file"));

  upper_lvl_el   = 0;
  EbmlMaster *m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  int i1;
  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxClusterTimecode)) {
      KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
      int64_t cluster_tc     = uint64(ctc);
      cluster->InitTimecode(cluster_tc, tc_scale);

      show_element(l2, 2, boost::format(Y("Cluster timecode: %|1$.3f|s")) % ((float)cluster_tc * (float)tc_scale / 1000000000.0));

    } else if (is_id(l2, KaxClusterPosition)) {
      KaxClusterPosition &c_pos = *static_cast<KaxClusterPosition *>(l2);
      show_element(l2, 2, boost::format(Y("Cluster position: %1%")) % uint64(c_pos));

    } else if (is_id(l2, KaxClusterPrevSize)) {
      KaxClusterPrevSize &c_psize = *static_cast<KaxClusterPrevSize *>(l2);
      show_element(l2, 2, boost::format(Y("Cluster previous size: %1%")) % uint64(c_psize));

    } else if (is_id(l2, KaxClusterSilentTracks))
      handle2(silent_track, cluster);

    else if (is_id(l2, KaxBlockGroup))
      handle2(block_group, cluster);

    else if (is_id(l2, KaxSimpleBlock))
      handle2(simple_block, cluster);

    else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

void
handle_elements_rec(EbmlStream *es,
                    int level,
                    int parent_idx,
                    EbmlElement *e,
                    const parser_element_t *mapping) {
  bool found = false;
  int elt_idx;
  for (elt_idx = 0; NULL != mapping[elt_idx].name; ++elt_idx)
    if (e->Generic().GlobalId == mapping[elt_idx].id) {
      found = true;
      break;
    }

  if (!found) {
    show_element(e, level, boost::format(Y("(Unknown element: %1%)")) % e->Generic().DebugName);
    return;
  }

  string elt_name = mapping[elt_idx].name;
  EbmlMaster *m;

  switch (mapping[elt_idx].type) {
    case EBMLT_MASTER:
      show_element(e, level, elt_name);
      m = dynamic_cast<EbmlMaster *>(e);
      assert(m != NULL);

      int i;
      for (i = 0; m->ListSize() > i; ++i)
        handle_elements_rec(es, level + 1, elt_idx, (*m)[i], mapping);
      break;

    case EBMLT_UINT:
    case EBMLT_BOOL:
      show_element(e, level, boost::format("%1%: %2%") % elt_name % uint64(*dynamic_cast<EbmlUInteger *>(e)));
      break;

    case EBMLT_STRING:
      show_element(e, level, boost::format("%1%: %2%") % elt_name % string(*dynamic_cast<EbmlString *>(e)));
      break;

    case EBMLT_USTRING:
      show_element(e, level, boost::format("%1%: %2%") % elt_name % UTFstring_to_cstr(UTFstring(*static_cast<EbmlUnicodeString *>(e)).c_str()));
      break;

    case EBMLT_TIME:
      show_element(e, level, boost::format("%1%: %2%") % elt_name % format_timecode(uint64(*dynamic_cast<EbmlUInteger *>(e))));
      break;

    case EBMLT_BINARY:
      show_element(e, level, boost::format("%1%: %2%") % elt_name % format_binary(*static_cast<EbmlBinary *>(e)));
      break;

    default:
      assert(false);
  }
}

void
def_handle(chapters) {
  show_element(l1, 1, Y("Chapters"));

  upper_lvl_el   = 0;
  EbmlMaster *m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  int i1;
  for (i1 = 0; i1 < m1->ListSize(); i1++)
    handle_elements_rec(es, 2, 0, (*m1)[i1], chapter_elements);
}

void
def_handle(tags) {
  show_element(l1, 1, Y("Tags"));

  upper_lvl_el   = 0;
  EbmlMaster *m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  int i1;
  for (i1 = 0; i1 < m1->ListSize(); i1++)
    handle_elements_rec(es, 2, 0, (*m1)[i1], tag_elements);
}

void
handle_ebml_head(EbmlElement *l0,
                 mm_io_c *in,
                 EbmlStream *es) {
  show_element(l0, 0, Y("EBML head"));

  while (in_parent(l0)) {
    int upper_lvl_el = 0;
    EbmlElement *e   = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

    if (NULL == e)
      return;

    e->ReadData(*in);

    if (is_id(e, EVersion))
      show_element(e, 1, boost::format(Y("EBML version: %1%")) % uint64(*static_cast<EbmlUInteger *>(e)));

    else if (is_id(e, EReadVersion))
      show_element(e, 1, boost::format(Y("EBML read version: %1%")) % uint64(*static_cast<EbmlUInteger *>(e)));

    else if (is_id(e, EMaxIdLength))
      show_element(e, 1, boost::format(Y("EBML maximum ID length: %1%")) % uint64(*static_cast<EbmlUInteger *>(e)));

    else if (is_id(e, EDocType))
      show_element(e, 1, boost::format(Y("Doc type: %1%")) % string(*static_cast<EbmlString *>(e)));

    else if (is_id(e, EDocTypeVersion))
      show_element(e, 1, boost::format(Y("Doc type version: %1%")) % uint64(*static_cast<EbmlUInteger *>(e)));

    else if (is_id(e, EDocTypeReadVersion))
      show_element(e, 1, boost::format(Y("Doc type read version: %1%")) % uint64(*static_cast<EbmlUInteger *>(e)));

    else
      show_unknown_element(e, 1);

    e->SkipData(*es, e->Generic().Context);
    delete e;
  }
}

bool
process_file(const string &file_name) {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  EbmlElement *l5 = NULL, *l6 = NULL;
  KaxCluster *cluster;

  // open input file
  mm_io_c *in;
  try {
    in = new mm_file_io_c(file_name);
  } catch (...) {
    show_error((boost::format(Y("Error: Couldn't open input file %1% (%2%).\n")) % file_name % strerror(errno)).str());
    return false;
  }

  in->setFilePointer(0, seek_end);
  uint64_t file_size = in->getFilePointer();
  in->setFilePointer(0, seek_beginning);

  try {
    EbmlStream *es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (NULL == l0) {
      show_error(Y("No EBML head found."));
      delete es;

      return false;
    }

    handle_ebml_head(l0, in, es);
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;

    while (1) {
      // Next element must be a segment
      l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
      if (NULL == l0) {
        show_error(Y("No segment/level 0 element found."));
        return false;
      }

      if (is_id(l0, KaxSegment)) {
        if (l0->GetSize() == -1)
          show_element(l0, 0, Y("Segment, size unknown"));
        else
          show_element(l0, 0, boost::format(Y("Segment, size %1%")) % l0->GetSize());
        break;
      }

      show_element(l0, 0, boost::format(Y("Next level 0 element is not a segment but %1%")) % typeid(*l0).name());

      l0->SkipData(*es, l0->Generic().Context);
      delete l0;
    }

    upper_lvl_el = 0;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

    while ((NULL != l1) && (0 >= upper_lvl_el)) {
      if (is_id(l1, KaxInfo))
        handle(info);

      else if (is_id(l1, KaxTracks))
        handle(tracks);

      else if (is_id(l1, KaxSeekHead))
        handle(seek_head);

      else if (is_id(l1, KaxCluster)) {
        show_element(l1, 1, Y("Cluster"));
        if ((verbose == 0) && !show_summary) {
          delete l1;
          delete l0;
          delete es;
          delete in;

          return true;
        }
        handle2(cluster, cluster, file_size);

      } else if (is_id(l1, KaxCues))
        handle(cues);

        // Weee! Attachments!
      else if (is_id(l1, KaxAttachments))
        handle(attachments);

      else if (is_id(l1, KaxChapters))
        handle(chapters);

        // Let's handle some TAGS.
      else if (is_id(l1, KaxTags))
        handle(tags);

      else if (!is_global(es, l1, 1))
        show_unknown_element(l1, 1);

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (0 < upper_lvl_el) {
        upper_lvl_el--;
        if (0 < upper_lvl_el)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (0 > upper_lvl_el) {
        upper_lvl_el++;
        if (0 > upper_lvl_el)
          break;

      }

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    delete l0;
    delete es;
    delete in;

    return true;
  } catch (...) {
    show_error(Y("Caught exception"));
    delete in;

    return false;
  }
}

void
setup() {
  init_stdio();
  set_usage();

#if defined(HAVE_LIBINTL_H)
  if (setlocale(LC_MESSAGES, "") == NULL)
    mxerror("Could not set the locale properly. Check the LANG, LC_ALL and LC_MESSAGES environment variables.\n");
  bindtextdomain("mkvtoolnix", MTX_LOCALE_DIR);
  textdomain("mkvtoolnix");
#endif

  mm_file_io_c::setup();
  cc_local_utf8 = utf8_init("");
  init_cc_stdio();

  xml_element_map_init();
}

void
cleanup() {
  utf8_done();
}

int
console_main(vector<string> args) {
  string file_name;
  bool ok;

#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  nice(2);
#endif

  parse_args(args, file_name);
  if (file_name == "") {
    usage();
    mxexit(0);
  }
  ok = process_file(file_name.c_str());
  cleanup();

  if (ok)
    return 0;
  else
    return 1;
}

int
main(int argc,
     char **argv) {
  vector<string> args;
  string initial_file;

  setup();

  args = command_line_utf8(argc, argv);
  parse_args(args, initial_file);

  if (use_gui)
    return ui_run(argc, argv);
  else
    return console_main(args);
}
