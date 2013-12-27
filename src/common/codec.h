/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for the various Codec IDs

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_CODEC_H
#define MTX_COMMON_CODEC_H

#include "common/common_pch.h"

#include <ostream>

#include "common/fourcc.h"

// see http://www.matroska.org/technical/specs/codecid/index.html

#define MKV_A_AAC_2MAIN  "A_AAC/MPEG2/MAIN"
#define MKV_A_AAC_2LC    "A_AAC/MPEG2/LC"
#define MKV_A_AAC_2SSR   "A_AAC/MPEG2/SSR"
#define MKV_A_AAC_2SBR   "A_AAC/MPEG2/LC/SBR"
#define MKV_A_AAC_4MAIN  "A_AAC/MPEG4/MAIN"
#define MKV_A_AAC_4LC    "A_AAC/MPEG4/LC"
#define MKV_A_AAC_4SSR   "A_AAC/MPEG4/SSR"
#define MKV_A_AAC_4LTP   "A_AAC/MPEG4/LTP"
#define MKV_A_AAC_4SBR   "A_AAC/MPEG4/LC/SBR"
#define MKV_A_AAC        "A_AAC"
#define MKV_A_AC3        "A_AC3"
#define MKV_A_ALAC       "A_ALAC"
#define MKV_A_EAC3       "A_EAC3"
#define MKV_A_DTS        "A_DTS"
#define MKV_A_MP2        "A_MPEG/L2"
#define MKV_A_MP3        "A_MPEG/L3"
#define MKV_A_PCM        "A_PCM/INT/LIT"
#define MKV_A_PCM_BE     "A_PCM/INT/BIG"
#define MKV_A_PCM_FLOAT  "A_PCM/FLOAT/IEEE"
#define MKV_A_VORBIS     "A_VORBIS"
#define MKV_A_ACM        "A_MS/ACM"
#define MKV_A_OPUS       "A_OPUS"
#define MKV_A_QUICKTIME  "A_QUICKTIME"
#define MKV_A_QDMC       "A_QUICKTIME/QDMC"
#define MKV_A_QDMC2      "A_QUICKTIME/QDM2"
#define MKV_A_REAL_14_4  "A_REAL/14_4"
#define MKV_A_REAL_28_8  "A_REAL/28_8"
#define MKV_A_REAL_COOK  "A_REAL/COOK"
#define MKV_A_REAL_SIPR  "A_REAL/SIPR"
#define MKV_A_REAL_ATRC  "A_REAL/ATRC"
#define MKV_A_FLAC       "A_FLAC"
#define MKV_A_MLP        "A_MLP"
#define MKV_A_TRUEHD     "A_TRUEHD"
#define MKV_A_TTA        "A_TTA1"
#define MKV_A_WAVPACK4   "A_WAVPACK4"

#define MKV_V_MPEG1      "V_MPEG1"
#define MKV_V_MPEG2      "V_MPEG2"
#define MKV_V_MPEG4_SP   "V_MPEG4/ISO/SP"
#define MKV_V_MPEG4_ASP  "V_MPEG4/ISO/ASP"
#define MKV_V_MPEG4_AP   "V_MPEG4/ISO/AP"
#define MKV_V_MPEG4_AVC  "V_MPEG4/ISO/AVC"
#define MKV_V_MSCOMP     "V_MS/VFW/FOURCC"
#define MKV_V_REALV1     "V_REAL/RV10"
#define MKV_V_REALV2     "V_REAL/RV20"
#define MKV_V_REALV3     "V_REAL/RV30"
#define MKV_V_REALV4     "V_REAL/RV40"
#define MKV_V_QUICKTIME  "V_QUICKTIME"
#define MKV_V_THEORA     "V_THEORA"
#define MKV_V_DIRAC      "V_DIRAC"
#define MKV_V_VP8        "V_VP8"
#define MKV_V_VP9        "V_VP9"

#define MKV_S_TEXTUTF8   "S_TEXT/UTF8"
#define MKV_S_TEXTSSA    "S_TEXT/SSA"
#define MKV_S_TEXTASS    "S_TEXT/ASS"
#define MKV_S_TEXTUSF    "S_TEXT/USF"
#define MKV_S_TEXTASCII  "S_TEXT/ASCII"
#define MKV_S_VOBSUB     "S_VOBSUB"
#define MKV_S_VOBSUBZLIB "S_VOBSUB/ZLIB"
#define MKV_S_KATE       "S_KATE"
#define MKV_S_HDMV_PGS   "S_HDMV/PGS"

#define MKV_B_VOBBTN     "B_VOBBTN"

enum codec_type_e {
    CT_UNKNOWN  = 0
  , CT_V_MPEG12 = 0x1000
  , CT_V_MPEG4_P2
  , CT_V_MPEG4_P10
  , CT_V_REAL
  , CT_V_THEORA
  , CT_V_DIRAC
  , CT_V_VP8
  , CT_V_VP9
  , CT_V_SVQ
  , CT_V_VC1

  , CT_A_AAC = 0x2000
  , CT_A_AC3
  , CT_A_ALAC
  , CT_A_DTS
  , CT_A_MP2
  , CT_A_MP3
  , CT_A_PCM
  , CT_A_VORBIS
  , CT_A_OPUS
  , CT_A_QDMC
  , CT_A_FLAC
  , CT_A_MLP
  , CT_A_TRUEHD
  , CT_A_TTA
  , CT_A_WAVPACK4

  , CT_S_SRT = 0x3000
  , CT_S_SSA_ASS
  , CT_S_USF
  , CT_S_VOBSUB
  , CT_S_KATE
  , CT_S_PGS

  , CT_B_VOBBTN = 0x4000
};

class codec_c {
private:
  static std::vector<codec_c> ms_codecs;

protected:
  boost::regex m_match_re;
  std::string m_name;
  codec_type_e m_type;
  std::vector<uint16_t> m_audio_formats;

public:
  codec_c()
    : m_type{CT_UNKNOWN}
  {
  }

  codec_c(std::string const &name, codec_type_e type, std::string const &match_re, uint16_t audio_format = 0u)
    : m_match_re{(boost::format("(?:%1%)") % match_re).str(), boost::regex::perl | boost::regex::icase}
    , m_name{name}
    , m_type{type}
  {
    if (audio_format)
      m_audio_formats.push_back(audio_format);
  }

  codec_c(std::string const &name, codec_type_e type, std::string const &match_re, std::vector<uint16_t> audio_formats)
    : m_match_re{(boost::format("(?:%1%)") % match_re).str(), boost::regex::perl | boost::regex::icase}
    , m_name{name}
    , m_type{type}
    , m_audio_formats{audio_formats}
  {
  }

  bool matches(std::string const &fourcc_or_codec_id) const {
    return boost::regex_match(fourcc_or_codec_id, m_match_re);
  }

  bool valid() const {
    return m_type != CT_UNKNOWN;
  }

  operator bool() const {
    return valid();
  }

  bool is(codec_type_e type) const {
    return type == m_type;
  }

  std::string const get_name(std::string fallback = "") const {
    if (!valid())
      return fallback;
    return m_name;
  }

  codec_type_e get_type() const {
    return m_type;
  }

private:
  static void initialize();

public:                         // static
  static codec_c const look_up(std::string const &fourcc_or_codec_id);
  static codec_c const look_up(char const *fourcc_or_codec_id);
  static codec_c const look_up(fourcc_c const &fourcc);
  static codec_c const look_up(codec_type_e type);
  static codec_c const look_up_audio_format(uint16_t audio_format);
  static codec_c const look_up_object_type_id(unsigned int object_type_id);

  static std::string const get_name(std::string const &fourcc_or_codec_id, std::string const &fallback);
  static std::string const get_name(codec_type_e type, std::string const &fallback);
};

inline std::ostream &
operator <<(std::ostream &out,
            codec_c const &codec) {
  if (codec)
    out << (boost::format("%1% (0x%|2$04x|)") % codec.get_name() % codec.get_type());
  else
    out << "<invalid-codec>";

  return out;
}

#endif // MTX_COMMON_CODEC_H
