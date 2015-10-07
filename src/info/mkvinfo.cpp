/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   retrieves and displays information about a Matroska file

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

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

#include <avilib.h>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <ebml/EbmlCrc32.h>
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

#include "common/chapters/chapters.h"
#include "common/checksums.h"
#include "common/codec.h"
#include "common/command_line.h"
#include "common/ebml.h"
#include "common/endian.h"
#include "common/hevc.h"
#include "common/kax_file.h"
#include "common/mm_io.h"
#include "common/mm_io_x.h"
#include "common/mpeg4_p10.h"
#include "common/stereo_mode.h"
#include "common/strings/editing.h"
#include "common/strings/formatting.h"
#include "common/translation.h"
#include "common/version.h"
#include "common/xml/ebml_chapters_converter.h"
#include "common/xml/ebml_tags_converter.h"
#include "info/mkvinfo.h"
#include "info/info_cli_parser.h"

using namespace libmatroska;

struct kax_track_t {
  uint64_t tnum, tuid;
  char type;
  int64_t default_duration;
  size_t mkvmerge_track_id;

  kax_track_t();
};

struct track_info_t {
  int64_t m_size, m_min_timecode, m_max_timecode, m_blocks, m_blocks_by_ref_num[3], m_add_duration_for_n_packets;

  track_info_t();
  bool min_timecode_unset();
  bool max_timecode_unset();
};

kax_track_t::kax_track_t()
  : tnum(0)
  , tuid(0)
  , type(' ')
  , default_duration(0)
  , mkvmerge_track_id(0)
{
}
typedef std::shared_ptr<kax_track_t> kax_track_cptr;

track_info_t::track_info_t()
  : m_size(0)
  , m_min_timecode(LLONG_MAX)
  , m_max_timecode(LLONG_MIN)
  , m_blocks(0)
  , m_add_duration_for_n_packets(0)
{
  memset(m_blocks_by_ref_num, 0, sizeof(int64_t) * 3);
}

bool
track_info_t::min_timecode_unset() {
  return LLONG_MAX == m_min_timecode;
}

bool
track_info_t::max_timecode_unset() {
  return LLONG_MIN == m_max_timecode;
}

std::vector<kax_track_cptr> s_tracks;
std::map<unsigned int, kax_track_cptr> s_tracks_by_number;
std::map<unsigned int, track_info_t> s_track_info;
options_c g_options;
static uint64_t s_tc_scale = TIMECODE_SCALE;
std::vector<boost::format> g_common_boost_formats;
size_t s_mkvmerge_track_id = 0;

#define BF_DO(n)                             g_common_boost_formats[n]
#define BF_ADD(s)                            g_common_boost_formats.push_back(boost::format(s))
#define BF_SHOW_UNKNOWN_ELEMENT              BF_DO( 0)
#define BF_EBMLVOID                          BF_DO( 1)
#define BF_FORMAT_BINARY_1                   BF_DO( 2)
#define BF_FORMAT_BINARY_2                   BF_DO( 3)
#define BF_BLOCK_GROUP_BLOCK_BASICS          BF_DO( 4)
#define BF_BLOCK_GROUP_BLOCK_ADLER           BF_DO( 3) // Intentional -- same format.
#define BF_BLOCK_GROUP_BLOCK_FRAME           BF_DO( 5)
#define BF_BLOCK_GROUP_DURATION              BF_DO( 6)
#define BF_BLOCK_GROUP_REFERENCE_1           BF_DO( 7)
#define BF_BLOCK_GROUP_REFERENCE_2           BF_DO( 8)
#define BF_BLOCK_GROUP_REFERENCE_PRIORITY    BF_DO( 9)
#define BF_BLOCK_GROUP_VIRTUAL               BF_DO(10)
#define BF_BLOCK_GROUP_REFERENCE_VIRTUAL     BF_DO(11)
#define BF_BLOCK_GROUP_ADD_ID                BF_DO(12)
#define BF_BLOCK_GROUP_ADDITIONAL            BF_DO(13)
#define BF_BLOCK_GROUP_SLICE_LACE            BF_DO(14)
#define BF_BLOCK_GROUP_SLICE_FRAME           BF_DO(15)
#define BF_BLOCK_GROUP_SLICE_DELAY           BF_DO(16)
#define BF_BLOCK_GROUP_SLICE_DURATION        BF_DO(17)
#define BF_BLOCK_GROUP_SLICE_ADD_ID          BF_DO(18)
#define BF_BLOCK_GROUP_SUMMARY_POSITION      BF_DO(19)
#define BF_BLOCK_GROUP_SUMMARY_WITH_DURATION BF_DO(20)
#define BF_BLOCK_GROUP_SUMMARY_NO_DURATION   BF_DO(21)
#define BF_BLOCK_GROUP_SUMMARY_V2            BF_DO(22)
#define BF_SIMPLE_BLOCK_BASICS               BF_DO(23)
#define BF_SIMPLE_BLOCK_ADLER                BF_DO( 3) // Intentional -- same format.
#define BF_SIMPLE_BLOCK_FRAME                BF_DO(24)
#define BF_SIMPLE_BLOCK_POSITION             BF_DO(19) // Intentional -- same format.
#define BF_SIMPLE_BLOCK_SUMMARY              BF_DO(25)
#define BF_SIMPLE_BLOCK_SUMMARY_V2           BF_DO(26)
#define BF_CLUSTER_TIMECODE                  BF_DO(27)
#define BF_CLUSTER_POSITION                  BF_DO(28)
#define BF_CLUSTER_PREVIOUS_SIZE             BF_DO(29)
#define BF_CODEC_STATE                       BF_DO(30)
#define BF_AT                                BF_DO(31)
#define BF_SIZE                              BF_DO(32)
#define BF_BLOCK_GROUP_DISCARD_PADDING       BF_DO(33)

void
init_common_boost_formats() {
  g_common_boost_formats.clear();
  BF_ADD(Y("(Unknown element: %1%; ID: 0x%2% size: %3%)"));                                                     //  0 -- BF_SHOW_UNKNOWN_ELEMENT
  BF_ADD(Y("EbmlVoid (size: %1%)"));                                                                            //  1 -- BF_EBMLVOID
  BF_ADD(Y("length %1%, data: %2%"));                                                                           //  2 -- BF_FORMAT_BINARY_1
  BF_ADD(Y(" (adler: 0x%|1$08x|)"));                                                                            //  3 -- BF_FORMAT_BINARY_2
  BF_ADD(Y("Block (track number %1%, %2% frame(s), timecode %|3$.3f|s = %4%)"));                                //  4 -- BF_BLOCK_GROUP_BLOCK_SUMMARY
  BF_ADD(Y("Frame with size %1%%2%%3%"));                                                                       //  5 -- BF_BLOCK_GROUP_BLOCK_FRAME
  BF_ADD(Y("Block duration: %1%.%|2$06d|ms"));                                                                  //  6 -- BF_BLOCK_GROUP_DURATION
  BF_ADD(Y("Reference block: -%1%.%|2$06d|ms"));                                                                //  7 -- BF_BLOCK_GROUP_REFERENCE_1
  BF_ADD(Y("Reference block: %1%.%|2$06d|ms"));                                                                 //  8 -- BF_BLOCK_GROUP_REFERENCE_2
  BF_ADD(Y("Reference priority: %1%"));                                                                         //  9 -- BF_BLOCK_GROUP_REFERENCE_PRIORITY
  BF_ADD(Y("Block virtual: %1%"));                                                                              // 10 -- BF_BLOCK_GROUP_VIRTUAL
  BF_ADD(Y("Reference virtual: %1%"));                                                                          // 11 -- BF_BLOCK_GROUP_REFERENCE_VIRTUAL
  BF_ADD(Y("AdditionalID: %1%"));                                                                               // 12 -- BF_BLOCK_GROUP_ADD_ID
  BF_ADD(Y("Block additional: %1%"));                                                                           // 13 -- BF_BLOCK_GROUP_ADDITIONAL
  BF_ADD(Y("Lace number: %1%"));                                                                                // 14 -- BF_BLOCK_GROUP_SLICE_LACE
  BF_ADD(Y("Frame number: %1%"));                                                                               // 15 -- BF_BLOCK_GROUP_SLICE_FRAME
  BF_ADD(Y("Delay: %|1$.3f|ms"));                                                                               // 16 -- BF_BLOCK_GROUP_SLICE_DELAY
  BF_ADD(Y("Duration: %|1$.3f|ms"));                                                                            // 17 -- BF_BLOCK_GROUP_SLICE_DURATION
  BF_ADD(Y("Block additional ID: %1%"));                                                                        // 18 -- BF_BLOCK_GROUP_SLICE_ADD_ID
  BF_ADD(Y(", position %1%"));                                                                                  // 19 -- BF_BLOCK_GROUP_SUMMARY_POSITION
  BF_ADD(Y("%1% frame, track %2%, timecode %3% (%4%), duration %|5$.3f|, size %6%, adler 0x%|7$08x|%8%%9%\n")); // 20 -- BF_BLOCK_GROUP_SUMMARY_WITH_DURATION
  BF_ADD(Y("%1% frame, track %2%, timecode %3% (%4%), size %5%, adler 0x%|6$08x|%7%%8%\n"));                    // 21 -- BF_BLOCK_GROUP_SUMMARY_NO_DURATION
  BF_ADD(Y("[%1% frame for track %2%, timecode %3%]"));                                                         // 22 -- BF_BLOCK_GROUP_SUMMARY_V2
  BF_ADD(Y("SimpleBlock (%1%track number %2%, %3% frame(s), timecode %|4$.3f|s = %5%)"));                       // 23 -- BF_SIMPLE_BLOCK_BASICS
  BF_ADD(Y("Frame with size %1%%2%%3%"));                                                                       // 24 -- BF_SIMPLE_BLOCK_FRAME
  BF_ADD(Y("%1% frame, track %2%, timecode %3% (%4%), size %5%, adler 0x%|6$08x|%7%\n"));                       // 25 -- BF_SIMPLE_BLOCK_SUMMARY
  BF_ADD(Y("[%1% frame for track %2%, timecode %3%]"));                                                         // 26 -- BF_SIMPLE_BLOCK_SUMMARY_V2
  BF_ADD(Y("Cluster timecode: %|1$.3f|s"));                                                                     // 27 -- BF_CLUSTER_TIMECODE
  BF_ADD(Y("Cluster position: %1%"));                                                                           // 28 -- BF_CLUSTER_POSITION
  BF_ADD(Y("Cluster previous size: %1%"));                                                                      // 29 -- BF_CLUSTER_PREVIOUS_SIZE
  BF_ADD(Y("Codec state: %1%"));                                                                                // 30 -- BF_CODEC_STATE
  BF_ADD(Y(" at %1%"));                                                                                         // 31 -- BF_AT
  BF_ADD(Y(" size %1%"));                                                                                       // 32 -- BF_SIZE
  BF_ADD(Y("Discard padding: %|1$.3f|ms (%2%ns)"));                                                             // 33 -- BF_BLOCK_GROUP_DISCARD_PADDING
}

std::string
create_element_text(const std::string &text,
                    int64_t position,
                    int64_t size) {
  std::string additional_text;

  if ((1 < g_options.m_verbose) && (0 <= position))
    additional_text += (BF_AT % position).str();

  if (g_options.m_show_size && (-1 != size)) {
    if (-2 != size)
      additional_text += (BF_SIZE % size).str();
    else
      additional_text += Y(" size is unknown");
  }

  return text + additional_text;
}

void
add_track(kax_track_cptr t) {
  s_tracks.push_back(t);
  s_tracks_by_number[t->tnum] = t;
}

kax_track_t *
find_track(int tnum) {
  return s_tracks_by_number[tnum].get();
}

#define show_error(error)          ui_show_error(error)
#define show_warning(l, f)         _show_element(nullptr, nullptr, false, l, f)
#define show_unknown_element(e, l) _show_unknown_element(es, e, l)
#define show_element(e, l, s)      _show_element(e, es, false, l, s)

static void _show_element(EbmlElement *l, EbmlStream *es, bool skip, int level, const std::string &info);

static void
_show_unknown_element(EbmlStream *es,
                      EbmlElement *e,
                      int level) {
  static boost::format s_bf_show_unknown_element("%|1$02x|");

  int i;
  std::string element_id;
  for (i = EBML_ID_LENGTH(static_cast<const EbmlId &>(*e)) - 1; 0 <= i; --i)
    element_id += (s_bf_show_unknown_element % ((EBML_ID_VALUE(static_cast<const EbmlId &>(*e)) >> (i * 8)) & 0xff)).str();

  std::string s = (BF_SHOW_UNKNOWN_ELEMENT % EBML_NAME(e) % element_id % (e->GetSize() + e->HeadSize())).str();
  _show_element(e, es, true, level, s);
}

static void
_show_element(EbmlElement *l,
              EbmlStream *es,
              bool skip,
              int level,
              const std::string &info) {
  if (g_options.m_show_summary)
    return;

  ui_show_element(level, info,
                    !l                 ? -1
                  :                      static_cast<int64_t>(l->GetElementPosition()),
                    !l                 ? -1
                  : !l->IsFiniteSize() ? -2
                  :                      static_cast<int64_t>(l->GetSizeLength() + EBML_ID_LENGTH(static_cast<const EbmlId &>(*l)) + l->GetSize()));

  if (!l || !skip)
    return;

  // Dump unknown elements recursively.
  auto *m = dynamic_cast<EbmlMaster *>(l);
  if (m)
    for (auto child : *m)
      show_unknown_element(child, level + 1);

  l->SkipData(*es, EBML_CONTEXT(l));
}

inline void
_show_element(EbmlElement *l,
              EbmlStream *es,
              bool skip,
              int level,
              const boost::format &info) {
  _show_element(l, es, skip, level, info.str());
}

static std::string
create_hexdump(const unsigned char *buf,
               int size) {
  static boost::format s_bf_create_hexdump(" %|1$02x|");

  std::string hex(" hexdump");
  int bmax = std::min(size, g_options.m_hexdump_max_size);
  int b;

  for (b = 0; b < bmax; ++b)
    hex += (s_bf_create_hexdump % static_cast<int>(buf[b])).str();

  return hex;
}

std::string
create_codec_dependent_private_info(KaxCodecPrivate &c_priv,
                                    char track_type,
                                    const std::string &codec_id) {
  if ((codec_id == MKV_V_MSCOMP) && ('v' == track_type) && (c_priv.GetSize() >= sizeof(alBITMAPINFOHEADER))) {
    alBITMAPINFOHEADER *bih = reinterpret_cast<alBITMAPINFOHEADER *>(c_priv.GetBuffer());
    unsigned char *fcc      = reinterpret_cast<unsigned char *>(&bih->bi_compression);
    return (boost::format(Y(" (FourCC: %1%%2%%3%%4%, 0x%|5$08x|)"))
            % fcc[0] % fcc[1] % fcc[2] % fcc[3] % get_uint32_le(&bih->bi_compression)).str();

  } else if ((codec_id == MKV_A_ACM) && ('a' == track_type) && (c_priv.GetSize() >= sizeof(alWAVEFORMATEX))) {
    alWAVEFORMATEX *wfe     = reinterpret_cast<alWAVEFORMATEX *>(c_priv.GetBuffer());
    return (boost::format(Y(" (format tag: 0x%|1$04x|)")) % get_uint16_le(&wfe->w_format_tag)).str();

  } else if ((codec_id == MKV_V_MPEG4_AVC) && ('v' == track_type) && (c_priv.GetSize() >= 4)) {
    auto avcc = mpeg4::p10::avcc_c::unpack(memory_cptr{new memory_c(c_priv.GetBuffer(), c_priv.GetSize(), false)});

    return (boost::format(Y(" (h.264 profile: %1% @L%2%.%3%)"))
            % (  avcc.m_profile_idc ==  44 ? "CAVLC 4:4:4 Intra"
               : avcc.m_profile_idc ==  66 ? "Baseline"
               : avcc.m_profile_idc ==  77 ? "Main"
               : avcc.m_profile_idc ==  83 ? "Scalable Baseline"
               : avcc.m_profile_idc ==  86 ? "Scalable High"
               : avcc.m_profile_idc ==  88 ? "Extended"
               : avcc.m_profile_idc == 100 ? "High"
               : avcc.m_profile_idc == 110 ? "High 10"
               : avcc.m_profile_idc == 118 ? "Multiview High"
               : avcc.m_profile_idc == 122 ? "High 4:2:2"
               : avcc.m_profile_idc == 128 ? "Stereo High"
               : avcc.m_profile_idc == 144 ? "High 4:4:4"
               : avcc.m_profile_idc == 244 ? "High 4:4:4 Predictive"
               :                             Y("Unknown"))
            % (avcc.m_level_idc / 10) % (avcc.m_level_idc % 10)).str();
  } else if ((codec_id == MKV_V_MPEGH_HEVC) && ('v' == track_type) && (c_priv.GetSize() >= 4)) {
    auto hevcc = hevc::hevcc_c::unpack(std::make_shared<memory_c>(c_priv.GetBuffer(), c_priv.GetSize(), false));

    return (boost::format(Y(" (HEVC profile: %1% @L%2%.%3%)"))
            % (  hevcc.m_general_profile_idc == 1 ? "Main"
               : hevcc.m_general_profile_idc == 2 ? "Main 10"
               : hevcc.m_general_profile_idc == 3 ? "Main Still Picture"
               :                                    Y("Unknown"))
            % (hevcc.m_general_level_idc / 3 / 10) % (hevcc.m_general_level_idc / 3 % 10)).str();
  }

  return "";
}

#define in_parent(p) \
  (!p->IsFiniteSize() || \
   (in->getFilePointer() < \
    (p->GetElementPosition() + p->HeadSize() + p->GetSize())))

bool
is_global(EbmlStream *es,
          EbmlElement *l,
          int level) {
  if (Is<EbmlVoid>(l)) {
    show_element(l, level, (BF_EBMLVOID % (l->ElementSize() - l->HeadSize())).str());
    return true;

  } else if (Is<EbmlCrc32>(l)) {
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

void
read_master(EbmlMaster *m,
            EbmlStream *es,
            const EbmlSemanticContext &ctx,
            int &upper_lvl_el,
            EbmlElement *&l2) {

  m->Read(*es, ctx, upper_lvl_el, l2, true);
  if (m->ListSize() == 0)
    return;

  brng::sort(m->GetElementList(), [](EbmlElement const *a, EbmlElement const *b) { return a->GetElementPosition() < b->GetElementPosition(); });
}

std::string
format_binary(EbmlBinary &bin,
              size_t max_len = 16) {
  size_t len         = std::min(max_len, static_cast<size_t>(bin.GetSize()));
  const binary *b    = bin.GetBuffer();
  std::string result = (BF_FORMAT_BINARY_1 % bin.GetSize() % to_hex(b, len)).str();

  if (len < bin.GetSize())
    result += "...";

  if (g_options.m_calc_checksums)
    result += (BF_FORMAT_BINARY_2 % calc_adler32(bin.GetBuffer(), bin.GetSize())).str();

  strip(result);

  return result;
}

inline std::string
format_binary(EbmlBinary *bin,
              size_t max_len = 16) {
  return format_binary(*bin, max_len);
}

void
handle_chaptertranslate(EbmlStream *&es,
                        EbmlElement *&l2) {
  show_element(l2, 2, Y("Chapter Translate"));

  for (auto l3 : *static_cast<EbmlMaster *>(l2))
    if (Is<KaxChapterTranslateEditionUID>(l3))
      show_element(l3, 3, boost::format(Y("Chapter Translate Edition UID: %1%")) % static_cast<KaxChapterTranslateEditionUID *>(l3)->GetValue());

    else if (Is<KaxChapterTranslateCodec>(l3))
      show_element(l3, 3, boost::format(Y("Chapter Translate Codec: %1%"))       % static_cast<KaxChapterTranslateCodec *>(l3)->GetValue());

    else if (Is<KaxChapterTranslateID>(l3))
      show_element(l3, 3, boost::format(Y("Chapter Translate ID: %1%"))          % format_binary(static_cast<EbmlBinary *>(l3)));
}

void
handle_info(EbmlStream *&es,
            int &upper_lvl_el,
            EbmlElement *&l1) {
  // General info about this Matroska file
  show_element(l1, 1, Y("Segment information"));

  upper_lvl_el               = 0;
  EbmlElement *element_found = nullptr;
  auto m1                    = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, EBML_CONTEXT(l1), upper_lvl_el, element_found);

  s_tc_scale = FindChildValue<KaxTimecodeScale, uint64_t>(m1, TIMECODE_SCALE);

  for (auto l2 : *m1)
    if (Is<KaxTimecodeScale>(l2)) {
      s_tc_scale = static_cast<KaxTimecodeScale *>(l2)->GetValue();
      show_element(l2, 2, boost::format(Y("Timecode scale: %1%")) % s_tc_scale);

    } else if (Is<KaxDuration>(l2)) {
      KaxDuration &duration = *static_cast<KaxDuration *>(l2);
      show_element(l2, 2,
                   boost::format(Y("Duration: %|1$.3f|s (%2%)"))
                   % (duration.GetValue() * s_tc_scale / 1000000000.0)
                   % format_timecode(static_cast<uint64_t>(duration.GetValue()) * s_tc_scale, 3));

    } else if (Is<KaxMuxingApp>(l2))
      show_element(l2, 2, boost::format(Y("Muxing application: %1%")) % static_cast<KaxMuxingApp *>(l2)->GetValueUTF8());

    else if (Is<KaxWritingApp>(l2))
      show_element(l2, 2, boost::format(Y("Writing application: %1%")) % static_cast<KaxWritingApp *>(l2)->GetValueUTF8());

    else if (Is<KaxDateUTC>(l2)) {
      struct tm tmutc;
      char buffer[40];
      time_t temptime = static_cast<KaxDateUTC *>(l2)->GetEpochDate();
      if (gmtime_r(&temptime, &tmutc) && asctime_r(&tmutc, buffer)) {
        buffer[strlen(buffer) - 1] = 0;
        show_element(l2, 2, boost::format(Y("Date: %1% UTC"))              % buffer);
      } else
        show_element(l2, 2, boost::format(Y("Date (invalid, value: %1%)")) % temptime);

    } else if (Is<KaxSegmentUID>(l2))
      show_element(l2, 2, boost::format(Y("Segment UID: %1%"))             % to_hex(static_cast<KaxSegmentUID *>(l2)));

    else if (Is<KaxSegmentFamily>(l2))
      show_element(l2, 2, boost::format(Y("Family UID: %1%"))              % to_hex(static_cast<KaxSegmentFamily *>(l2)));

    else if (Is<KaxChapterTranslate>(l2))
      handle_chaptertranslate(es, l2);

    else if (Is<KaxPrevUID>(l2))
      show_element(l2, 2, boost::format(Y("Previous segment UID: %1%"))    % to_hex(static_cast<KaxPrevUID *>(l2)));

    else if (Is<KaxPrevFilename>(l2))
      show_element(l2, 2, boost::format(Y("Previous filename: %1%"))       % static_cast<KaxPrevFilename *>(l2)->GetValueUTF8());

    else if (Is<KaxNextUID>(l2))
      show_element(l2, 2, boost::format(Y("Next segment UID: %1%"))        % to_hex(static_cast<KaxNextUID *>(l2)));

    else if (Is<KaxNextFilename>(l2))
      show_element(l2, 2, boost::format(Y("Next filename: %1%"))           % static_cast<KaxNextFilename *>(l2)->GetValueUTF8());

    else if (Is<KaxSegmentFilename>(l2))
      show_element(l2, 2, boost::format(Y("Segment filename: %1%"))        % static_cast<KaxSegmentFilename *>(l2)->GetValueUTF8());

    else if (Is<KaxTitle>(l2))
      show_element(l2, 2, boost::format(Y("Title: %1%"))                   % static_cast<KaxTitle *>(l2)->GetValueUTF8());
}

void
handle_audio_track(EbmlStream *&es,
                   EbmlElement *&l3,
                   std::vector<std::string> &summary) {
  show_element(l3, 3, "Audio track");

  for (auto l4 : *static_cast<EbmlMaster *>(l3))
    if (Is<KaxAudioSamplingFreq>(l4)) {
      KaxAudioSamplingFreq &freq = *static_cast<KaxAudioSamplingFreq *>(l4);
      show_element(l4, 4, boost::format(Y("Sampling frequency: %1%")) % freq.GetValue());
      summary.push_back((boost::format(Y("sampling freq: %1%")) % freq.GetValue()).str());

    } else if (Is<KaxAudioOutputSamplingFreq>(l4)) {
      KaxAudioOutputSamplingFreq &ofreq = *static_cast<KaxAudioOutputSamplingFreq *>(l4);
      show_element(l4, 4, boost::format(Y("Output sampling frequency: %1%")) % ofreq.GetValue());
      summary.push_back((boost::format(Y("output sampling freq: %1%")) % ofreq.GetValue()).str());

    } else if (Is<KaxAudioChannels>(l4)) {
      KaxAudioChannels &channels = *static_cast<KaxAudioChannels *>(l4);
      show_element(l4, 4, boost::format(Y("Channels: %1%")) % channels.GetValue());
      summary.push_back((boost::format(Y("channels: %1%")) % channels.GetValue()).str());

#if MATROSKA_VERSION >= 2
    } else if (Is<KaxAudioPosition>(l4)) {
      KaxAudioPosition &positions = *static_cast<KaxAudioPosition *>(l4);
      show_element(l4, 4, boost::format(Y("Channel positions: %1%")) % format_binary(positions));
#endif

    } else if (Is<KaxAudioBitDepth>(l4)) {
      KaxAudioBitDepth &bps = *static_cast<KaxAudioBitDepth *>(l4);
      show_element(l4, 4, boost::format(Y("Bit depth: %1%")) % bps.GetValue());
      summary.push_back((boost::format(Y("bits per sample: %1%")) % bps.GetValue()).str());

    } else if (!is_global(es, l4, 4))
      show_unknown_element(l4, 4);
}

void
handle_video_track(EbmlStream *&es,
                   EbmlElement *&l3,
                   std::vector<std::string> &summary) {
  show_element(l3, 3, Y("Video track"));

  for (auto l4 : *static_cast<EbmlMaster *>(l3))
    if (Is<KaxVideoPixelWidth>(l4)) {
      KaxVideoPixelWidth &width = *static_cast<KaxVideoPixelWidth *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel width: %1%")) % width.GetValue());
      summary.push_back((boost::format(Y("pixel width: %1%")) % width.GetValue()).str());

    } else if (Is<KaxVideoPixelHeight>(l4)) {
      KaxVideoPixelHeight &height = *static_cast<KaxVideoPixelHeight *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel height: %1%")) % height.GetValue());
      summary.push_back((boost::format(Y("pixel height: %1%")) % height.GetValue()).str());

    } else if (Is<KaxVideoDisplayWidth>(l4)) {
      KaxVideoDisplayWidth &width = *static_cast<KaxVideoDisplayWidth *>(l4);
      show_element(l4, 4, boost::format(Y("Display width: %1%")) % width.GetValue());
      summary.push_back((boost::format(Y("display width: %1%")) % width.GetValue()).str());

    } else if (Is<KaxVideoDisplayHeight>(l4)) {
      KaxVideoDisplayHeight &height = *static_cast<KaxVideoDisplayHeight *>(l4);
      show_element(l4, 4, boost::format(Y("Display height: %1%")) % height.GetValue());
      summary.push_back((boost::format(Y("display height: %1%")) % height.GetValue()).str());

    } else if (Is<KaxVideoPixelCropLeft>(l4)) {
      KaxVideoPixelCropLeft &left = *static_cast<KaxVideoPixelCropLeft *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel crop left: %1%")) % left.GetValue());
      summary.push_back((boost::format(Y("pixel crop left: %1%")) % left.GetValue()).str());

    } else if (Is<KaxVideoPixelCropTop>(l4)) {
      KaxVideoPixelCropTop &top = *static_cast<KaxVideoPixelCropTop *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel crop top: %1%")) % top.GetValue());
      summary.push_back((boost::format(Y("pixel crop top: %1%")) % top.GetValue()).str());

    } else if (Is<KaxVideoPixelCropRight>(l4)) {
      KaxVideoPixelCropRight &right = *static_cast<KaxVideoPixelCropRight *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel crop right: %1%")) % right.GetValue());
      summary.push_back((boost::format(Y("pixel crop right: %1%")) % right.GetValue()).str());

    } else if (Is<KaxVideoPixelCropBottom>(l4)) {
      KaxVideoPixelCropBottom &bottom = *static_cast<KaxVideoPixelCropBottom *>(l4);
      show_element(l4, 4, boost::format(Y("Pixel crop bottom: %1%")) % bottom.GetValue());
      summary.push_back((boost::format(Y("pixel crop bottom: %1%")) % bottom.GetValue()).str());

#if MATROSKA_VERSION >= 2
    } else if (Is<KaxVideoDisplayUnit>(l4)) {
      auto unit = static_cast<KaxVideoDisplayUnit *>(l4)->GetValue();
      show_element(l4, 4,
                   boost::format(Y("Display unit: %1%%2%"))
                   % unit
                   % (  0 == unit ? Y(" (pixels)")
                      : 1 == unit ? Y(" (centimeters)")
                      : 2 == unit ? Y(" (inches)")
                      : 3 == unit ? Y(" (aspect ratio)")
                      :               ""));

    } else if (Is<KaxVideoGamma>(l4))
      show_element(l4, 4, boost::format(Y("Gamma: %1%")) % static_cast<KaxVideoGamma *>(l4)->GetValue());

    else if (Is<KaxVideoFlagInterlaced>(l4))
      show_element(l4, 4, boost::format(Y("Interlaced: %1%")) % static_cast<KaxVideoFlagInterlaced *>(l4)->GetValue());

    else if (Is<KaxVideoStereoMode>(l4)) {
      auto stereo_mode = static_cast<KaxVideoStereoMode *>(l4)->GetValue();
      show_element(l4, 4,
                   boost::format(Y("Stereo mode: %1% (%2%)"))
                   % stereo_mode
                   % stereo_mode_c::translate(static_cast<stereo_mode_c::mode>(stereo_mode)));

    } else if (Is<KaxVideoAspectRatio>(l4)) {
      auto ar_type = static_cast<KaxVideoAspectRatio *>(l4)->GetValue();
      show_element(l4, 4,
                   boost::format(Y("Aspect ratio type: %1%%2%"))
                   % ar_type
                   % (  0 == ar_type ? Y(" (free resizing)")
                      : 1 == ar_type ? Y(" (keep aspect ratio)")
                      : 2 == ar_type ? Y(" (fixed)")
                      :                  ""));
#endif
    } else if (Is<KaxVideoColourSpace>(l4))
      show_element(l4, 4, boost::format(Y("Colour space: %1%")) % format_binary(static_cast<KaxVideoColourSpace *>(l4)));

    else if (Is<KaxVideoFrameRate>(l4))
      show_element(l4, 4, boost::format(Y("Frame rate: %1%")) % static_cast<KaxVideoFrameRate *>(l4)->GetValue());

    else if (!is_global(es, l4, 4))
      show_unknown_element(l4, 4);
}

void
handle_content_encodings(EbmlStream *&es,
                         EbmlElement *&l3) {
  show_element(l3, 3, Y("Content encodings"));

  for (auto l4 : *static_cast<EbmlMaster *>(l3))
    if (Is<KaxContentEncoding>(l4)) {
      show_element(l4, 4, Y("Content encoding"));

      for (auto l5 : *static_cast<EbmlMaster *>(l4))
        if (Is<KaxContentEncodingOrder>(l5))
          show_element(l5, 5, boost::format(Y("Order: %1%")) % static_cast<KaxContentEncodingOrder *>(l5)->GetValue());

        else if (Is<KaxContentEncodingScope>(l5)) {
          std::vector<std::string> scope;
          auto ce_scope = static_cast<KaxContentEncodingScope *>(l5)->GetValue();

          if ((ce_scope & 0x01) == 0x01)
            scope.push_back(Y("1: all frames"));
          if ((ce_scope & 0x02) == 0x02)
            scope.push_back(Y("2: codec private data"));
          if ((ce_scope & 0xfc) != 0x00)
            scope.push_back(Y("rest: unknown"));
          if (scope.empty())
            scope.push_back(Y("unknown"));
          show_element(l5, 5, boost::format(Y("Scope: %1% (%2%)")) % ce_scope % join(", ", scope));

        } else if (Is<KaxContentEncodingType>(l5)) {
          auto ce_type = static_cast<KaxContentEncodingType *>(l5)->GetValue();
          show_element(l5, 5,
                       boost::format(Y("Type: %1% (%2%)"))
                       % ce_type
                       % (  0 == ce_type ? Y("compression")
                          : 1 == ce_type ? Y("encryption")
                          :                Y("unknown")));

        } else if (Is<KaxContentCompression>(l5)) {
          show_element(l5, 5, Y("Content compression"));

          for (auto l6 : *static_cast<EbmlMaster *>(l5))
            if (Is<KaxContentCompAlgo>(l6)) {
              auto c_algo = static_cast<KaxContentCompAlgo *>(l6)->GetValue();
              show_element(l6, 6,
                           boost::format(Y("Algorithm: %1% (%2%)"))
                           % c_algo
                           % (  0 == c_algo ?   "ZLIB"
                              : 1 == c_algo ?   "bzLib"
                              : 2 == c_algo ?   "lzo1x"
                              : 3 == c_algo ? Y("header removal")
                              :               Y("unknown")));

            } else if (Is<KaxContentCompSettings>(l6))
              show_element(l6, 6, boost::format(Y("Settings: %1%")) % format_binary(static_cast<KaxContentCompSettings *>(l6)));

            else if (!is_global(es, l6, 6))
              show_unknown_element(l6, 6);
        } else if (Is<KaxContentEncryption>(l5)) {
          show_element(l5, 5, Y("Content encryption"));

          for (auto l6 : *static_cast<EbmlMaster *>(l5))
            if (Is<KaxContentEncAlgo>(l6)) {
              auto e_algo = static_cast<KaxContentEncAlgo *>(l6)->GetValue();
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

            } else if (Is<KaxContentEncKeyID>(l6))
              show_element(l6, 6, boost::format(Y("Encryption key ID: %1%")) % format_binary(static_cast<KaxContentEncKeyID *>(l6)));

            else if (Is<KaxContentSigAlgo>(l6)) {
              auto s_algo = static_cast<KaxContentSigAlgo *>(l6)->GetValue();
              show_element(l6, 6,
                           boost::format(Y("Signature algorithm: %1% (%2%)"))
                           % s_algo
                           % (  0 == s_algo ? Y("no signature algorithm")
                              : 1 == s_algo ? Y("RSA")
                              :               Y("unknown")));

            } else if (Is<KaxContentSigHashAlgo>(l6)) {
              auto s_halgo = static_cast<KaxContentSigHashAlgo *>(l6)->GetValue();
              show_element(l6, 6,
                           boost::format(Y("Signature hash algorithm: %1% (%2%)"))
                           % s_halgo
                           % (  0 == s_halgo ? Y("no signature hash algorithm")
                              : 1 == s_halgo ? Y("SHA1-160")
                              : 2 == s_halgo ? Y("MD5")
                              :                Y("unknown")));

            } else if (Is<KaxContentSigKeyID>(l6))
              show_element(l6, 6, boost::format(Y("Signature key ID: %1%")) % format_binary(static_cast<KaxContentSigKeyID *>(l6)));

            else if (Is<KaxContentSignature>(l6))
              show_element(l6, 6, boost::format(Y("Signature: %1%")) % format_binary(static_cast<KaxContentSignature *>(l6)));

            else if (!is_global(es, l6, 6))
              show_unknown_element(l6, 6);

        } else if (!is_global(es, l5, 5))
          show_unknown_element(l5, 5);

    } else if (!is_global(es, l4, 4))
      show_unknown_element(l4, 4);
}

void
handle_tracks(EbmlStream *&es,
              int &upper_lvl_el,
              EbmlElement *&l1) {
  // Yep, we've found our KaxTracks element. Now find all tracks
  // contained in this segment.
  show_element(l1, 1, Y("Segment tracks"));

  size_t s_mkvmerge_track_id = 0;
  upper_lvl_el               = 0;
  EbmlElement *element_found = nullptr;
  auto m1                    = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, EBML_CONTEXT(l1), upper_lvl_el, element_found);

  for (auto l2 : *m1)
    if (Is<KaxTrackEntry>(l2)) {
      // We actually found a track entry :) We're happy now.
      show_element(l2, 2, Y("A track"));

      std::vector<std::string> summary;
      std::string kax_codec_id, fourcc_buffer;
      auto track = std::make_shared<kax_track_t>();

      for (auto l3 : *static_cast<EbmlMaster *>(l2))
        // Now evaluate the data belonging to this track
        if (Is<KaxTrackAudio>(l3))
          handle_audio_track(es, l3, summary);

        else if (Is<KaxTrackVideo>(l3))
          handle_video_track(es, l3, summary);

        else if (Is<KaxTrackNumber>(l3)) {
          track->tnum = static_cast<KaxTrackNumber *>(l3)->GetValue();

          auto existing_track = find_track(track->tnum);
          size_t track_id     = s_mkvmerge_track_id;
          if (!existing_track) {
            track->mkvmerge_track_id = s_mkvmerge_track_id;
            ++s_mkvmerge_track_id;
            add_track(track);

          } else
            track_id = existing_track->mkvmerge_track_id;

          show_element(l3, 3, boost::format(Y("Track number: %1% (track ID for mkvmerge & mkvextract: %2%)")) % track->tnum % track_id);
          summary.push_back((boost::format(Y("mkvmerge/mkvextract track ID: %1%"))                            % track_id).str());

        } else if (Is<KaxTrackUID>(l3)) {
          track->tuid = static_cast<KaxTrackUID *>(l3)->GetValue();
          show_element(l3, 3, boost::format(Y("Track UID: %1%"))                                              % track->tuid);

        } else if (Is<KaxTrackType>(l3)) {
          auto ttype  = static_cast<KaxTrackType *>(l3)->GetValue();
          track->type = track_audio    == ttype ? 'a'
                      : track_video    == ttype ? 'v'
                      : track_subtitle == ttype ? 's'
                      : track_buttons  == ttype ? 'b'
                      :                           '?';
          show_element(l3, 3,
                       boost::format(Y("Track type: %1%"))
                       % (  'a' == track->type ? "audio"
                          : 'v' == track->type ? "video"
                          : 's' == track->type ? "subtitles"
                          : 'b' == track->type ? "buttons"
                          :                      "unknown"));

#if MATROSKA_VERSION >= 2
        } else if (Is<KaxTrackFlagEnabled>(l3))
          show_element(l3, 3, boost::format(Y("Enabled: %1%"))                % static_cast<KaxTrackFlagEnabled *>(l3)->GetValue());
#endif

        else if (Is<KaxTrackName>(l3))
          show_element(l3, 3, boost::format(Y("Name: %1%"))                   % static_cast<KaxTrackName *>(l3)->GetValueUTF8());

        else if (Is<KaxCodecID>(l3)) {
          kax_codec_id = static_cast<KaxCodecID *>(l3)->GetValue();
          show_element(l3, 3, boost::format(Y("Codec ID: %1%"))               % kax_codec_id);

        } else if (Is<KaxCodecPrivate>(l3)) {
          KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate *>(l3);
          fourcc_buffer = create_codec_dependent_private_info(c_priv, track->type, kax_codec_id);

          if (g_options.m_calc_checksums && !g_options.m_show_summary)
            fourcc_buffer += (boost::format(Y(" (adler: 0x%|1$08x|)"))        % calc_adler32(c_priv.GetBuffer(), c_priv.GetSize())).str();

          if (g_options.m_show_hexdump)
            fourcc_buffer += create_hexdump(c_priv.GetBuffer(), c_priv.GetSize());

          show_element(l3, 3, boost::format(Y("CodecPrivate, length %1%%2%")) % c_priv.GetSize() % fourcc_buffer);

        } else if (Is<KaxCodecName>(l3))
          show_element(l3, 3, boost::format(Y("Codec name: %1%"))             % static_cast<KaxCodecName *>(l3)->GetValueUTF8());

#if MATROSKA_VERSION >= 2
        else if (Is<KaxCodecSettings>(l3))
          show_element(l3, 3, boost::format(Y("Codec settings: %1%"))         % static_cast<KaxCodecSettings *>(l3)->GetValueUTF8());

        else if (Is<KaxCodecInfoURL>(l3))
          show_element(l3, 3, boost::format(Y("Codec info URL: %1%"))         % static_cast<KaxCodecInfoURL *>(l3)->GetValue());

        else if (Is<KaxCodecDownloadURL>(l3))
          show_element(l3, 3, boost::format(Y("Codec download URL: %1%"))     % static_cast<KaxCodecDownloadURL *>(l3)->GetValue());

        else if (Is<KaxCodecDecodeAll>(l3))
          show_element(l3, 3, boost::format(Y("Codec decode all: %1%"))       % static_cast<KaxCodecDecodeAll *>(l3)->GetValue());

        else if (Is<KaxTrackOverlay>(l3))
          show_element(l3, 3, boost::format(Y("Track overlay: %1%"))          % static_cast<KaxTrackOverlay *>(l3)->GetValue());
#endif // MATROSKA_VERSION >= 2

        else if (Is<KaxTrackMinCache>(l3))
          show_element(l3, 3, boost::format(Y("MinCache: %1%"))               % static_cast<KaxTrackMinCache *>(l3)->GetValue());

        else if (Is<KaxTrackMaxCache>(l3))
          show_element(l3, 3, boost::format(Y("MaxCache: %1%"))               % static_cast<KaxTrackMaxCache *>(l3)->GetValue());

        else if (Is<KaxTrackDefaultDuration>(l3)) {
          track->default_duration = static_cast<KaxTrackDefaultDuration *>(l3)->GetValue();
          show_element(l3, 3,
                       boost::format(Y("Default duration: %|1$.3f|ms (%|2$.3f| frames/fields per second for a video track)"))
                       % (static_cast<double>(track->default_duration) / 1000000.0)
                       % (1000000000.0 / static_cast<double>(track->default_duration)));
          summary.push_back((boost::format(Y("default duration: %|1$.3f|ms (%|2$.3f| frames/fields per second for a video track)"))
                             % (static_cast<double>(track->default_duration) / 1000000.0)
                             % (1000000000.0 / static_cast<double>(track->default_duration))
                             ).str());

        } else if (Is<KaxTrackFlagLacing>(l3))
          show_element(l3, 3, boost::format(Y("Lacing flag: %1%"))          % static_cast<KaxTrackFlagLacing *>(l3)->GetValue());

        else if (Is<KaxTrackFlagDefault>(l3))
          show_element(l3, 3, boost::format(Y("Default flag: %1%"))         % static_cast<KaxTrackFlagDefault *>(l3)->GetValue());

        else if (Is<KaxTrackFlagForced>(l3))
          show_element(l3, 3, boost::format(Y("Forced flag: %1%"))          % static_cast<KaxTrackFlagForced *>(l3)->GetValue());

        else if (Is<KaxTrackLanguage>(l3)) {
          auto language = static_cast<KaxTrackLanguage *>(l3)->GetValue();
          show_element(l3, 3, boost::format(Y("Language: %1%"))             % language);
          summary.push_back((boost::format(Y("language: %1%"))              % language).str());

        } else if (Is<KaxTrackTimecodeScale>(l3))
          show_element(l3, 3, boost::format(Y("Timecode scale: %1%"))       % static_cast<KaxTrackTimecodeScale *>(l3)->GetValue());

        else if (Is<KaxMaxBlockAdditionID>(l3))
          show_element(l3, 3, boost::format(Y("Max BlockAddition ID: %1%")) % static_cast<KaxMaxBlockAdditionID *>(l3)->GetValue());

        else if (Is<KaxContentEncodings>(l3))
          handle_content_encodings(es, l3);

        else if (Is<KaxCodecDelay>(l3)) {
          auto value = static_cast<KaxCodecDelay *>(l3)->GetValue();
          show_element(l3, 3, boost::format(Y("Codec delay: %|1$.3f|ms (%2%ns)")) % (static_cast<double>(value) / 1000000.0) % value);

        } else if (Is<KaxSeekPreRoll>(l3)) {
          auto value = static_cast<KaxSeekPreRoll *>(l3)->GetValue();
          show_element(l3, 3, boost::format(Y("Seek pre-roll: %|1$.3f|ms (%2%ns)")) % (static_cast<double>(value) / 1000000.0) % value);

        } else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      if (g_options.m_show_summary)
        mxinfo(boost::format(Y("Track %1%: %2%, codec ID: %3%%4%%5%%6%\n"))
               % track->tnum
               % (  'a' == track->type ? Y("audio")
                  : 'v' == track->type ? Y("video")
                  : 's' == track->type ? Y("subtitles")
                  : 'b' == track->type ? Y("buttons")
                  :                      Y("unknown"))
               % kax_codec_id
               % fourcc_buffer
               % (summary.empty() ? "" : ", ")
               % join(", ", summary));

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);
}

void
handle_seek_head(EbmlStream *&es,
                 int &upper_lvl_el,
                 EbmlElement *&l1) {
  if ((g_options.m_verbose < 2) && !g_options.m_use_gui) {
    show_element(l1, 1, Y("Seek head (subentries will be skipped)"));
    return;
  }

  show_element(l1, 1, Y("Seek head"));

  upper_lvl_el               = 0;
  EbmlElement *element_found = nullptr;
  auto m1                    = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, EBML_CONTEXT(l1), upper_lvl_el, element_found);

  for (auto l2 : *m1)
    if (Is<KaxSeek>(l2)) {
      show_element(l2, 2, Y("Seek entry"));

      for (auto l3 : *static_cast<EbmlMaster *>(l2))
        if (Is<KaxSeekID>(l3)) {
          KaxSeekID &seek_id = static_cast<KaxSeekID &>(*l3);
          EbmlId id(seek_id.GetBuffer(), seek_id.GetSize());

          show_element(l3, 3,
                       boost::format(Y("Seek ID: %1% (%2%)"))
                       % to_hex(seek_id)
                       % (  Is<KaxInfo>(id)        ? "KaxInfo"
                          : Is<KaxCluster>(id)     ? "KaxCluster"
                          : Is<KaxTracks>(id)      ? "KaxTracks"
                          : Is<KaxCues>(id)        ? "KaxCues"
                          : Is<KaxAttachments>(id) ? "KaxAttachments"
                          : Is<KaxChapters>(id)    ? "KaxChapters"
                          : Is<KaxTags>(id)        ? "KaxTags"
                          : Is<KaxSeekHead>(id)    ? "KaxSeekHead"
                          :                          "unknown"));

        } else if (Is<KaxSeekPosition>(l3))
          show_element(l3, 3, boost::format(Y("Seek position: %1%")) % static_cast<KaxSeekPosition *>(l3)->GetValue());

        else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);
}

void
handle_cues(EbmlStream *&es,
            int &upper_lvl_el,
            EbmlElement *&l1) {
  if (g_options.m_verbose < 2) {
    show_element(l1, 1, Y("Cues (subentries will be skipped)"));
    return;
  }

  show_element(l1, 1, "Cues");

  upper_lvl_el               = 0;
  EbmlElement *element_found = nullptr;
  auto m1                    = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, EBML_CONTEXT(l1), upper_lvl_el, element_found);

  for (auto l2 : *m1)
    if (Is<KaxCuePoint>(l2)) {
      show_element(l2, 2, Y("Cue point"));

      for (auto l3 : *static_cast<EbmlMaster *>(l2))
        if (Is<KaxCueTime>(l3))
          show_element(l3, 3, boost::format(Y("Cue time: %|1$.3f|s")) % (s_tc_scale * static_cast<double>(static_cast<KaxCueTime *>(l3)->GetValue()) / 1000000000.0));

        else if (Is<KaxCueTrackPositions>(l3)) {
          show_element(l3, 3, Y("Cue track positions"));

          for (auto l4 : *static_cast<EbmlMaster *>(l3))
            if (Is<KaxCueTrack>(l4))
              show_element(l4, 4, boost::format(Y("Cue track: %1%"))            % static_cast<KaxCueTrack *>(l4)->GetValue());

            else if (Is<KaxCueClusterPosition>(l4))
              show_element(l4, 4, boost::format(Y("Cue cluster position: %1%")) % static_cast<KaxCueClusterPosition *>(l4)->GetValue());

            else if (Is<KaxCueRelativePosition>(l4))
              show_element(l4, 4, boost::format(Y("Cue relative position: %1%")) % static_cast<KaxCueRelativePosition *>(l4)->GetValue());

            else if (Is<KaxCueDuration>(l4))
              show_element(l4, 4, boost::format(Y("Cue duration: %1%"))         % format_timecode(static_cast<KaxCueDuration *>(l4)->GetValue() * s_tc_scale));

            else if (Is<KaxCueBlockNumber>(l4))
              show_element(l4, 4, boost::format(Y("Cue block number: %1%"))     % static_cast<KaxCueBlockNumber *>(l4)->GetValue());

#if MATROSKA_VERSION >= 2
            else if (Is<KaxCueCodecState>(l4))
              show_element(l4, 4, boost::format(Y("Cue codec state: %1%"))      % static_cast<KaxCueCodecState *>(l4)->GetValue());

            else if (Is<KaxCueReference>(l4)) {
              show_element(l4, 4, Y("Cue reference"));

              for (auto l5 : *static_cast<EbmlMaster *>(l4))
                if (Is<KaxCueRefTime>(l5))
                  show_element(l5, 5, boost::format(Y("Cue ref time: %|1$.3f|s"))  % s_tc_scale % (static_cast<KaxCueRefTime *>(l5)->GetValue() / 1000000000.0));

                else if (Is<KaxCueRefCluster>(l5))
                  show_element(l5, 5, boost::format(Y("Cue ref cluster: %1%"))     % static_cast<KaxCueRefCluster *>(l5)->GetValue());

                else if (Is<KaxCueRefNumber>(l5))
                  show_element(l5, 5, boost::format(Y("Cue ref number: %1%"))      % static_cast<KaxCueRefNumber *>(l5)->GetValue());

                else if (Is<KaxCueRefCodecState>(l5))
                  show_element(l5, 5, boost::format(Y("Cue ref codec state: %1%")) % static_cast<KaxCueRefCodecState *>(l5)->GetValue());

                else if (!is_global(es, l5, 5))
                  show_unknown_element(l5, 5);

#endif // MATROSKA_VERSION >= 2

            } else if (!is_global(es, l4, 4))
              show_unknown_element(l4, 4);

        } else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);
}

void
handle_attachments(EbmlStream *&es,
                   int &upper_lvl_el,
                   EbmlElement *&l1) {
  show_element(l1, 1, Y("Attachments"));

  upper_lvl_el               = 0;
  EbmlElement *element_found = nullptr;
  auto m1                    = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, EBML_CONTEXT(l1), upper_lvl_el, element_found);

  for (auto l2 : *m1)
    if (Is<KaxAttached>(l2)) {
      show_element(l2, 2, Y("Attached"));

      for (auto l3 : *static_cast<EbmlMaster *>(l2))
        if (Is<KaxFileDescription>(l3))
          show_element(l3, 3, boost::format(Y("File description: %1%")) % static_cast<KaxFileDescription *>(l3)->GetValueUTF8());

         else if (Is<KaxFileName>(l3))
          show_element(l3, 3, boost::format(Y("File name: %1%"))        % static_cast<KaxFileName *>(l3)->GetValueUTF8());

         else if (Is<KaxMimeType>(l3))
           show_element(l3, 3, boost::format(Y("Mime type: %1%"))       % static_cast<KaxMimeType *>(l3)->GetValue());

         else if (Is<KaxFileData>(l3))
          show_element(l3, 3, boost::format(Y("File data, size: %1%"))  % static_cast<KaxFileData *>(l3)->GetSize());

         else if (Is<KaxFileUID>(l3))
           show_element(l3, 3, boost::format(Y("File UID: %1%"))        % static_cast<KaxFileUID *>(l3)->GetValue());

         else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);
}

void
handle_silent_track(EbmlStream *&es,
                    EbmlElement *&l2) {
  show_element(l2, 2, "Silent Tracks");

  for (auto l3 : *static_cast<EbmlMaster *>(l2))
    if (Is<KaxClusterSilentTrackNumber>(l3))
      show_element(l3, 3, boost::format(Y("Silent Track Number: %1%")) % static_cast<KaxClusterSilentTrackNumber *>(l3)->GetValue());

    else if (!is_global(es, l3, 3))
      show_unknown_element(l3, 3);
}

void
handle_block_group(EbmlStream *&es,
                   EbmlElement *&l2,
                   KaxCluster *&cluster) {
  show_element(l2, 2, Y("Block group"));

  std::vector<int> frame_sizes;
  std::vector<uint32_t> frame_adlers;
  std::vector<std::string> frame_hexdumps;

  bool bref_found     = false;
  bool fref_found     = false;

  int64_t lf_timecode = 0;
  int64_t lf_tnum     = 0;
  int64_t frame_pos   = 0;

  float bduration     = -1.0;

  for (auto l3 : *static_cast<EbmlMaster *>(l2))
    if (Is<KaxBlock>(l3)) {
      KaxBlock &block = *static_cast<KaxBlock *>(l3);
      block.SetParent(*cluster);
      show_element(l3, 3,
                   BF_BLOCK_GROUP_BLOCK_BASICS
                   % block.TrackNum()
                   % block.NumberFrames()
                   % (static_cast<double>(block.GlobalTimecode()) / 1000000000.0)
                   % format_timecode(block.GlobalTimecode(), 3));

      lf_timecode = block.GlobalTimecode();
      lf_tnum     = block.TrackNum();
      bduration   = -1.0;
      frame_pos   = block.GetElementPosition() + block.ElementSize();

      for (size_t i = 0; i < block.NumberFrames(); ++i) {
        auto &data = block.GetBuffer(i);
        auto adler = calc_adler32(data.Buffer(), data.Size());

        std::string adler_str;
        if (g_options.m_calc_checksums)
          adler_str = (BF_BLOCK_GROUP_BLOCK_ADLER % adler).str();

        std::string hex;
        if (g_options.m_show_hexdump)
          hex = create_hexdump(data.Buffer(), data.Size());

        show_element(nullptr, 4, BF_BLOCK_GROUP_BLOCK_FRAME % data.Size() % adler_str % hex);

        frame_sizes.push_back(data.Size());
        frame_adlers.push_back(adler);
        frame_hexdumps.push_back(hex);
        frame_pos -= data.Size();
      }

    } else if (Is<KaxBlockDuration>(l3)) {
      auto duration = static_cast<KaxBlockDuration *>(l3)->GetValue();
      bduration     = static_cast<double>(duration) * s_tc_scale / 1000000.0;
      show_element(l3, 3, BF_BLOCK_GROUP_DURATION % (duration * s_tc_scale / 1000000) % (duration * s_tc_scale % 1000000));

    } else if (Is<KaxReferenceBlock>(l3)) {
      int64_t reference = static_cast<KaxReferenceBlock *>(l3)->GetValue() * s_tc_scale;

      if (0 >= reference) {
        bref_found  = true;
        reference  *= -1;
        show_element(l3, 3, BF_BLOCK_GROUP_REFERENCE_1 % (reference / 1000000) % (reference % 1000000));

      } else if (0 < reference) {
        fref_found = true;
        show_element(l3, 3, BF_BLOCK_GROUP_REFERENCE_2 % (reference / 1000000) % (reference % 1000000));
      }

    } else if (Is<KaxReferencePriority>(l3))
      show_element(l3, 3, BF_BLOCK_GROUP_REFERENCE_PRIORITY % static_cast<KaxReferencePriority *>(l3)->GetValue());

#if MATROSKA_VERSION >= 2
    else if (Is<KaxBlockVirtual>(l3))
      show_element(l3, 3, BF_BLOCK_GROUP_VIRTUAL            % format_binary(static_cast<KaxBlockVirtual *>(l3)));

    else if (Is<KaxReferenceVirtual>(l3))
      show_element(l3, 3, BF_BLOCK_GROUP_REFERENCE_VIRTUAL  % static_cast<KaxReferenceVirtual *>(l3)->GetValue());

    else if (Is<KaxCodecState>(l3))
      show_element(l3, 3, BF_CODEC_STATE                    % format_binary(static_cast<KaxCodecState *>(l3)));

    else if (Is<KaxDiscardPadding>(l3)) {
      auto value = static_cast<KaxDiscardPadding *>(l3)->GetValue();
      show_element(l3, 3, BF_BLOCK_GROUP_DISCARD_PADDING    % (static_cast<double>(value) / 1000000.0) % value);
    }

#endif // MATROSKA_VERSION >= 2
    else if (Is<KaxBlockAdditions>(l3)) {
      show_element(l3, 3, Y("Additions"));

      for (auto l4 : *static_cast<EbmlMaster *>(l3))
        if (Is<KaxBlockMore>(l4)) {
          show_element(l4, 4, Y("More"));

          for (auto l5 : *static_cast<EbmlMaster *>(l4))
            if (Is<KaxBlockAddID>(l5))
              show_element(l5, 5, BF_BLOCK_GROUP_ADD_ID     % static_cast<KaxBlockAddID *>(l5)->GetValue());

            else if (Is<KaxBlockAdditional>(l5))
              show_element(l5, 5, BF_BLOCK_GROUP_ADDITIONAL % format_binary(static_cast<KaxBlockAdditional *>(l5)));

            else if (!is_global(es, l5, 5))
              show_unknown_element(l5, 5);

        } else if (!is_global(es, l4, 4))
          show_unknown_element(l4, 4);

    } else if (Is<KaxSlices>(l3)) {
      show_element(l3, 3, Y("Slices"));

      for (auto l4 : *static_cast<EbmlMaster *>(l3))
        if (Is<KaxTimeSlice>(l4)) {
          show_element(l4, 4, Y("Time slice"));

          for (auto l5 : *static_cast<EbmlMaster *>(l4))
            if (Is<KaxSliceLaceNumber>(l5))
              show_element(l5, 5, BF_BLOCK_GROUP_SLICE_LACE     % static_cast<KaxSliceLaceNumber *>(l5)->GetValue());

            else if (Is<KaxSliceFrameNumber>(l5))
              show_element(l5, 5, BF_BLOCK_GROUP_SLICE_FRAME    % static_cast<KaxSliceFrameNumber *>(l5)->GetValue());

            else if (Is<KaxSliceDelay>(l5))
              show_element(l5, 5, BF_BLOCK_GROUP_SLICE_DELAY    % (static_cast<double>(static_cast<KaxSliceDelay *>(l5)->GetValue()) * s_tc_scale / 1000000.0));

            else if (Is<KaxSliceDuration>(l5))
              show_element(l5, 5, BF_BLOCK_GROUP_SLICE_DURATION % (static_cast<double>(static_cast<KaxSliceDuration *>(l5)->GetValue()) * s_tc_scale / 1000000.0));

            else if (Is<KaxSliceBlockAddID>(l5))
              show_element(l5, 5, BF_BLOCK_GROUP_SLICE_ADD_ID   % static_cast<KaxSliceBlockAddID *>(l5)->GetValue());

            else if (!is_global(es, l5, 5))
              show_unknown_element(l5, 5);

        } else if (!is_global(es, l4, 4))
          show_unknown_element(l4, 4);

    } else if (!is_global(es, l3, 3))
      show_unknown_element(l3, 3);

  if (g_options.m_show_summary) {
    std::string position;
    size_t fidx;

    for (fidx = 0; fidx < frame_sizes.size(); fidx++) {
      if (1 <= g_options.m_verbose) {
        position   = (BF_BLOCK_GROUP_SUMMARY_POSITION % frame_pos).str();
        frame_pos += frame_sizes[fidx];
      }

      if (bduration != -1.0)
        mxinfo(BF_BLOCK_GROUP_SUMMARY_WITH_DURATION
               % (bref_found && fref_found ? 'B' : bref_found ? 'P' : !fref_found ? 'I' : 'P')
               % lf_tnum
               % (lf_timecode / 1000000)
               % format_timecode(lf_timecode, 3)
               % bduration
               % frame_sizes[fidx]
               % frame_adlers[fidx]
               % frame_hexdumps[fidx]
               % position);
      else
        mxinfo(BF_BLOCK_GROUP_SUMMARY_NO_DURATION
               % (bref_found && fref_found ? 'B' : bref_found ? 'P' : !fref_found ? 'I' : 'P')
               % lf_tnum
               % (lf_timecode / 1000000)
               % format_timecode(lf_timecode, 3)
               % frame_sizes[fidx]
               % frame_adlers[fidx]
               % frame_hexdumps[fidx]
               % position);
    }

  } else if (g_options.m_verbose > 2)
    show_element(nullptr, 2,
                 BF_BLOCK_GROUP_SUMMARY_V2
                 % (bref_found && fref_found ? 'B' : bref_found ? 'P' : !fref_found ? 'I' : 'P')
                 % lf_tnum
                 % (lf_timecode / 1000000));

  track_info_t &tinfo = s_track_info[lf_tnum];

  tinfo.m_blocks                                                                                 += frame_sizes.size();
  tinfo.m_blocks_by_ref_num[bref_found && fref_found ? 2 : bref_found ? 1 : !fref_found ? 0 : 1] += frame_sizes.size();
  tinfo.m_min_timecode                                                                             = std::min(tinfo.m_min_timecode, lf_timecode);
  tinfo.m_size                                                                                    += boost::accumulate(frame_sizes, 0);

  if (!tinfo.max_timecode_unset() && (tinfo.m_max_timecode >= lf_timecode))
    return;

  tinfo.m_max_timecode = lf_timecode;

  if (-1 == bduration)
    tinfo.m_add_duration_for_n_packets  = frame_sizes.size();
  else {
    tinfo.m_max_timecode               += bduration * 1000000.0;
    tinfo.m_add_duration_for_n_packets  = 0;
  }
}

void
handle_simple_block(EbmlStream *&es,
                    EbmlElement *&l2,
                    KaxCluster *&cluster) {
  std::vector<int> frame_sizes;
  std::vector<uint32_t> frame_adlers;

  KaxSimpleBlock &block = *static_cast<KaxSimpleBlock *>(l2);
  block.SetParent(*cluster);

  int64_t frame_pos   = block.GetElementPosition() + block.ElementSize();
  uint64_t timecode   = block.GlobalTimecode() / 1000000;
  track_info_t &tinfo = s_track_info[block.TrackNum()];

  std::string info;
  if (block.IsKeyframe())
    info = Y("key, ");
  if (block.IsDiscardable())
    info += Y("discardable, ");

  show_element(l2, 2,
               BF_SIMPLE_BLOCK_BASICS
               % info
               % block.TrackNum()
               % block.NumberFrames()
               % ((float)timecode / 1000.0)
               % format_timecode(block.GlobalTimecode(), 3));

  int i;
  for (i = 0; i < (int)block.NumberFrames(); i++) {
    DataBuffer &data = block.GetBuffer(i);
    uint32_t adler   = calc_adler32(data.Buffer(), data.Size());

    std::string adler_str;
    if (g_options.m_calc_checksums)
      adler_str = (BF_SIMPLE_BLOCK_ADLER % adler).str();

    std::string hex;
    if (g_options.m_show_hexdump)
      hex = create_hexdump(data.Buffer(), data.Size());

    show_element(nullptr, 3, BF_SIMPLE_BLOCK_FRAME % data.Size() % adler_str % hex);

    frame_sizes.push_back(data.Size());
    frame_adlers.push_back(adler);
    frame_pos -= data.Size();
  }

  if (g_options.m_show_summary) {
    std::string position;
    size_t fidx;

    for (fidx = 0; fidx < frame_sizes.size(); fidx++) {
      if (1 <= g_options.m_verbose) {
        position   = (BF_SIMPLE_BLOCK_POSITION % frame_pos).str();
        frame_pos += frame_sizes[fidx];
      }

      mxinfo(BF_SIMPLE_BLOCK_SUMMARY
             % (block.IsKeyframe() ? 'I' : block.IsDiscardable() ? 'B' : 'P')
             % block.TrackNum()
             % timecode
             % format_timecode(block.GlobalTimecode(), 3)
             % frame_sizes[fidx]
             % frame_adlers[fidx]
             % position);
    }

  } else if (g_options.m_verbose > 2)
    show_element(nullptr, 2,
                 BF_SIMPLE_BLOCK_SUMMARY_V2
                 % (block.IsKeyframe() ? 'I' : block.IsDiscardable() ? 'B' : 'P')
                 % block.TrackNum()
                 % timecode);

  tinfo.m_blocks                                                                    += block.NumberFrames();
  tinfo.m_blocks_by_ref_num[block.IsKeyframe() ? 0 : block.IsDiscardable() ? 2 : 1] += block.NumberFrames();
  tinfo.m_min_timecode                                                                = std::min(tinfo.m_min_timecode, static_cast<int64_t>(block.GlobalTimecode()));
  tinfo.m_max_timecode                                                                = std::max(tinfo.m_min_timecode, static_cast<int64_t>(block.GlobalTimecode()));
  tinfo.m_add_duration_for_n_packets                                                  = block.NumberFrames();
  tinfo.m_size                                                                       += boost::accumulate(frame_sizes, 0);
}

void
handle_cluster(EbmlStream *&es,
               int &upper_lvl_el,
               EbmlElement *&l1,
               int64_t file_size) {
  auto cluster = static_cast<KaxCluster *>(l1);

  if (g_options.m_use_gui)
    ui_show_progress(100 * cluster->GetElementPosition() / file_size, Y("Parsing file"));

  upper_lvl_el               = 0;
  EbmlElement *element_found = nullptr;
  auto m1                    = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, EBML_CONTEXT(l1), upper_lvl_el, element_found);

  cluster->InitTimecode(FindChildValue<KaxClusterTimecode>(m1), s_tc_scale);

  for (auto l2 : *m1)
    if (Is<KaxClusterTimecode>(l2))
      show_element(l2, 2, BF_CLUSTER_TIMECODE      % (static_cast<double>(static_cast<KaxClusterTimecode *>(l2)->GetValue()) * s_tc_scale / 1000000000.0));

    else if (Is<KaxClusterPosition>(l2))
      show_element(l2, 2, BF_CLUSTER_POSITION      % static_cast<KaxClusterPosition *>(l2)->GetValue());

    else if (Is<KaxClusterPrevSize>(l2))
      show_element(l2, 2, BF_CLUSTER_PREVIOUS_SIZE % static_cast<KaxClusterPrevSize *>(l2)->GetValue());

    else if (Is<KaxClusterSilentTracks>(l2))
      handle_silent_track(es, l2);

    else if (Is<KaxBlockGroup>(l2))
      handle_block_group(es, l2, cluster);

    else if (Is<KaxSimpleBlock>(l2))
      handle_simple_block(es, l2, cluster);

    else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);
}

void
handle_elements_rec(EbmlStream *es,
                    int level,
                    EbmlElement *e,
                    mtx::xml::ebml_converter_c const &converter) {
  static boost::format s_bf_handle_elements_rec("%1%: %2%");
  static std::vector<std::string> const s_output_as_timecode{ "ChapterTimeStart", "ChapterTimeEnd" };

  std::string elt_name = converter.get_tag_name(*e);

  if (dynamic_cast<EbmlMaster *>(e)) {
    show_element(e, level, elt_name);
    for (auto child : *static_cast<EbmlMaster *>(e))
      handle_elements_rec(es, level + 1, child, converter);

  } else if (dynamic_cast<EbmlUInteger *>(e)) {
    if (brng::find(s_output_as_timecode, elt_name) != s_output_as_timecode.end())
      show_element(e, level, s_bf_handle_elements_rec % elt_name % format_timecode(static_cast<EbmlUInteger *>(e)->GetValue()));
    else
      show_element(e, level, s_bf_handle_elements_rec % elt_name % static_cast<EbmlUInteger *>(e)->GetValue());

  } else if (dynamic_cast<EbmlSInteger *>(e))
    show_element(e, level, s_bf_handle_elements_rec   % elt_name % static_cast<EbmlSInteger *>(e)->GetValue());

  else if (dynamic_cast<EbmlString *>(e))
    show_element(e, level, s_bf_handle_elements_rec   % elt_name % static_cast<EbmlString *>(e)->GetValue());

  else if (dynamic_cast<EbmlUnicodeString *>(e))
    show_element(e, level, s_bf_handle_elements_rec   % elt_name % static_cast<EbmlUnicodeString *>(e)->GetValueUTF8());

  else if (dynamic_cast<EbmlBinary *>(e))
    show_element(e, level, s_bf_handle_elements_rec   % elt_name % format_binary(static_cast<EbmlBinary *>(e)));

  else
    assert(false);
}

void
handle_chapters(EbmlStream *&es,
                int &upper_lvl_el,
                EbmlElement *&l1) {
  show_element(l1, 1, Y("Chapters"));

  upper_lvl_el               = 0;
  EbmlMaster *m1             = static_cast<EbmlMaster *>(l1);
  EbmlElement *element_found = nullptr;
  read_master(m1, es, EBML_CONTEXT(l1), upper_lvl_el, element_found);

  mtx::xml::ebml_chapters_converter_c converter;
  for (auto l2 : *static_cast<EbmlMaster *>(l1))
    handle_elements_rec(es, 2, l2, converter);
}

void
handle_tags(EbmlStream *&es,
            int &upper_lvl_el,
            EbmlElement *&l1) {
  show_element(l1, 1, Y("Tags"));

  upper_lvl_el               = 0;
  EbmlMaster *m1             = static_cast<EbmlMaster *>(l1);
  EbmlElement *element_found = nullptr;
  read_master(m1, es, EBML_CONTEXT(l1), upper_lvl_el, element_found);

  mtx::xml::ebml_tags_converter_c converter;
  for (auto l2 : *static_cast<EbmlMaster *>(l1))
    handle_elements_rec(es, 2, l2, converter);
}

void
handle_ebml_head(EbmlElement *l0,
                 mm_io_cptr in,
                 EbmlStream *es) {
  show_element(l0, 0, Y("EBML head"));

  while (in_parent(l0)) {
    int upper_lvl_el = 0;
    EbmlElement *e   = es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true);

    if (!e)
      return;

    e->ReadData(*in);

    if (Is<EVersion>(e))
      show_element(e, 1, boost::format(Y("EBML version: %1%"))             % static_cast<EbmlUInteger *>(e)->GetValue());

    else if (Is<EReadVersion>(e))
      show_element(e, 1, boost::format(Y("EBML read version: %1%"))        % static_cast<EbmlUInteger *>(e)->GetValue());

    else if (Is<EMaxIdLength>(e))
      show_element(e, 1, boost::format(Y("EBML maximum ID length: %1%"))   % static_cast<EbmlUInteger *>(e)->GetValue());

    else if (Is<EMaxSizeLength>(e))
      show_element(e, 1, boost::format(Y("EBML maximum size length: %1%")) % static_cast<EbmlUInteger *>(e)->GetValue());

    else if (Is<EDocType>(e))
      show_element(e, 1, boost::format(Y("Doc type: %1%"))                 % std::string(*static_cast<EbmlString *>(e)));

    else if (Is<EDocTypeVersion>(e))
      show_element(e, 1, boost::format(Y("Doc type version: %1%"))         % static_cast<EbmlUInteger *>(e)->GetValue());

    else if (Is<EDocTypeReadVersion>(e))
      show_element(e, 1, boost::format(Y("Doc type read version: %1%"))    % static_cast<EbmlUInteger *>(e)->GetValue());

    else
      show_unknown_element(e, 1);

    e->SkipData(*es, EBML_CONTEXT(e));
    delete e;
  }
}

void
display_track_info() {
  if (!g_options.m_show_track_info)
    return;

  for (auto &track : s_tracks) {
    track_info_t &tinfo  = s_track_info[track->tnum];

    if (tinfo.min_timecode_unset())
      tinfo.m_min_timecode = 0;
    if (tinfo.max_timecode_unset())
      tinfo.m_max_timecode = tinfo.m_min_timecode;

    int64_t duration  = tinfo.m_max_timecode - tinfo.m_min_timecode;
    duration         += tinfo.m_add_duration_for_n_packets * track->default_duration;

    mxinfo(boost::format(Y("Statistics for track number %1%: number of blocks: %2%; size in bytes: %3%; duration in seconds: %4%; approximate bitrate in bits/second: %5%\n"))
           % track->tnum
           % tinfo.m_blocks
           % tinfo.m_size
           % (duration / 1000000000.0)
           % static_cast<uint64_t>(duration == 0 ? 0 : tinfo.m_size * 8000000000.0 / duration));
  }
}

bool
process_file(const std::string &file_name) {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = nullptr, *l1 = nullptr;

  s_tc_scale = TIMECODE_SCALE;
  s_tracks.clear();
  s_tracks_by_number.clear();
  s_track_info.clear();

  // open input file
  mm_io_cptr in;
  try {
    in = mm_file_io_c::open(file_name);
  } catch (mtx::mm_io::exception &ex) {
    show_error((boost::format(Y("Error: Couldn't open input file %1% (%2%).\n")) % file_name % ex).str());
    return false;
  }

  in->setFilePointer(0, seek_end);
  uint64_t file_size = in->getFilePointer();
  in->setFilePointer(0, seek_beginning);

  try {
    EbmlStream *es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EBML_INFO(EbmlHead), 0xFFFFFFFFL);
    if (!l0) {
      show_error(Y("No EBML head found."));
      delete es;

      return false;
    }

    handle_ebml_head(l0, in, es);
    l0->SkipData(*es, EBML_CONTEXT(l0));
    delete l0;

    while (1) {
      // NEXT element must be a segment
      l0 = es->FindNextID(EBML_INFO(KaxSegment), 0xFFFFFFFFFFFFFFFFLL);
      if (!l0) {
        show_error(Y("No segment/level 0 element found."));
        return false;
      }

      if (Is<KaxSegment>(l0)) {
        if (!l0->IsFiniteSize())
          show_element(l0, 0, Y("Segment, size unknown"));
        else
          show_element(l0, 0, boost::format(Y("Segment, size %1%")) % l0->GetSize());
        break;
      }

      show_element(l0, 0, boost::format(Y("Next level 0 element is not a segment but %1%")) % typeid(*l0).name());

      l0->SkipData(*es, EBML_CONTEXT(l0));
      delete l0;
    }

    kax_file_cptr kax_file = kax_file_cptr(new kax_file_c(in));

    // Prevent reporting "first timecode after resync":
    kax_file->set_timecode_scale(-1);

    while ((l1 = kax_file->read_next_level1_element())) {
      std::shared_ptr<EbmlElement> af_l1(l1);

      if (Is<KaxInfo>(l1))
        handle_info(es, upper_lvl_el, l1);

      else if (Is<KaxTracks>(l1))
        handle_tracks(es, upper_lvl_el, l1);

      else if (Is<KaxSeekHead>(l1))
        handle_seek_head(es, upper_lvl_el, l1);

      else if (Is<KaxCluster>(l1)) {
        show_element(l1, 1, Y("Cluster"));
        if ((g_options.m_verbose == 0) && !g_options.m_show_summary) {
          delete l0;
          delete es;

          return true;
        }
        handle_cluster(es, upper_lvl_el, l1, file_size);

      } else if (Is<KaxCues>(l1))
        handle_cues(es, upper_lvl_el, l1);

        // Weee! Attachments!
      else if (Is<KaxAttachments>(l1))
        handle_attachments(es, upper_lvl_el, l1);

      else if (Is<KaxChapters>(l1))
        handle_chapters(es, upper_lvl_el, l1);

        // Let's handle some TAGS.
      else if (Is<KaxTags>(l1))
        handle_tags(es, upper_lvl_el, l1);

      else if (!is_global(es, l1, 1))
        show_unknown_element(l1, 1);

      if (!in->setFilePointer2(l1->GetElementPosition() + kax_file->get_element_size(l1)))
        break;
      if (!in_parent(l0))
        break;
    } // while (l1)

    delete l0;
    delete es;

    if (!g_options.m_use_gui && g_options.m_show_track_info)
      display_track_info();

    return true;
  } catch (...) {
    show_error(Y("Caught exception"));
    return false;
  }
}

void
setup(char const *argv0,
      std::string const &locale) {
  mtx_common_init("mkvinfo", argv0);

  init_locales(locale);
  init_common_boost_formats();

  version_info = get_version_info("mkvinfo", vif_full);
}

int
console_main() {
  set_process_priority(-1);

  if (g_options.m_file_name.empty())
    mxerror(Y("No file name given.\n"));

  return process_file(g_options.m_file_name.c_str()) ? 0 : 1;
}

int
main(int argc,
     char **argv) {
  setup(argv[0]);

  g_options = info_cli_parser_c(command_line_utf8(argc, argv)).run();

  init_common_boost_formats();

  if (g_options.m_use_gui)
    return ui_run(argc, argv);
  else
    return console_main();
}
