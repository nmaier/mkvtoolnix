/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for the various Codec IDs

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/codec.h"
#include "common/mp4.h"

std::vector<codec_c> codec_c::ms_codecs;

void
codec_c::initialize() {
  if (!ms_codecs.empty())
    return;
  ms_codecs.emplace_back("MPEG-1/2",                CT_V_MPEG12,    "mpeg|mpg[12]|m[12]v.|mpgv|mp[12]v|h262|V_MPEG[12]");
  ms_codecs.emplace_back("MPEG-4p2",                CT_V_MPEG4_P2,  "3iv2|xvi[dx]|divx|dx50|fmp4|mp4v|V_MPEG4/ISO/(?:SP|AP|ASP)");
  ms_codecs.emplace_back("MPEG-4p10/AVC/h.264",     CT_V_MPEG4_P10, "avc.|[hx]264|V_MPEG4/ISO/AVC");
  ms_codecs.emplace_back("RealVideo",               CT_V_REAL,      "rv[1234]\\d|V_REAL/RV\\d+");
  ms_codecs.emplace_back("Theora",                  CT_V_THEORA,    "theo|thra|V_THEORA");
  ms_codecs.emplace_back("Dirac",                   CT_V_DIRAC,     "drac|V_DIRAC");
  ms_codecs.emplace_back("VP8",                     CT_V_VP8,       "vp8\\d|V_VP8");
  ms_codecs.emplace_back("VP9",                     CT_V_VP9,       "vp9\\d|V_VP9");
  ms_codecs.emplace_back("Sorenson",                CT_V_SVQ,       "svq[i0-9]");
  ms_codecs.emplace_back("VC1",                     CT_V_VC1,       "wvc1|vc-1");

  ms_codecs.emplace_back("AAC",                     CT_A_AAC,       "mp4a|aac.|raac|racp|A_AAC.*",           std::vector<uint16_t>{ 0x00ffu, 0x706du });
  ms_codecs.emplace_back("AC3/EAC3",                CT_A_AC3,       "ac3.|ac-3|sac3|eac3|a52[\\sb]|dnet|A_E?AC3", 0x2000u);
  ms_codecs.emplace_back("ALAC",                    CT_A_ALAC,      "alac|A_ALAC");
  ms_codecs.emplace_back("DTS",                     CT_A_DTS,       "dts[\\sbcehl]|A_DTS",                   0x2001u);
  ms_codecs.emplace_back("MP2",                     CT_A_MP2,       "mp2.|\\.mp[12]|mp2a|A_MPEG/L2",         0x0050);
  ms_codecs.emplace_back("MP3",                     CT_A_MP3,       "mp3.|\\.mp3|LAME|mpga|A_MPEG/L3",       0x0055);
  ms_codecs.emplace_back("PCM",                     CT_A_PCM,       "twos|sowt|A_PCM/(?:INT|FLOAT)/.+",      std::vector<uint16_t>{ 0x0001u, 0x0003u });
  ms_codecs.emplace_back("Vorbis",                  CT_A_VORBIS,    "vor[1b]|A_VORBIS",                      std::vector<uint16_t>{ 0x566fu, 0xfffeu });
  ms_codecs.emplace_back("Opus",                    CT_A_OPUS,      "opus|A_OPUS(?:/EXPERIMENTAL)?");
  ms_codecs.emplace_back("QDMC",                    CT_A_QDMC,      "qdm2|A_QUICKTIME/QDM[2C]");
  ms_codecs.emplace_back("FLAC",                    CT_A_FLAC,      "flac|A_FLAC");
  ms_codecs.emplace_back("MLP",                     CT_A_MLP,       "mlp\\s|A_MLP");
  ms_codecs.emplace_back("TrueHD",                  CT_A_TRUEHD,    "trhd|A_TRUEHD");
  ms_codecs.emplace_back("TrueAudio",               CT_A_TTA,       "tta1|A_TTA1?");
  ms_codecs.emplace_back("WavPack4",                CT_A_WAVPACK4,  "wvpk|A_WAVPACK4");

  ms_codecs.emplace_back("SubRip/SRT",              CT_S_SRT,       "S_TEXT/(?:UTF8|ASCII)");
  ms_codecs.emplace_back("SubStationAlpha",         CT_S_SSA_ASS,   "ssa\\s|ass\\s|S_TEXT/(?:SSA|ASS)");
  ms_codecs.emplace_back("UniversalSubtitleFormat", CT_S_USF,       "usf\\s|S_TEXT/USF");
  ms_codecs.emplace_back("Kate",                    CT_S_KATE,      "kate|S_KATE");
  ms_codecs.emplace_back("VobSub",                  CT_S_VOBSUB,    "S_VOBSUB(?:/ZLIB)?");
  ms_codecs.emplace_back("PGS",                     CT_S_PGS,       "S_HDMV/PGS");

  ms_codecs.emplace_back("VobButton",               CT_B_VOBBTN,    "B_VOBBTN");
}

codec_c const
codec_c::look_up(std::string const &fourcc_or_codec_id) {
  initialize();

  auto itr = brng::find_if(ms_codecs, [&fourcc_or_codec_id](codec_c const &c) { return c.matches(fourcc_or_codec_id); });

  return itr == ms_codecs.end() ? codec_c{} : *itr;
}

codec_c const
codec_c::look_up(codec_type_e type) {
  initialize();

  auto itr = brng::find_if(ms_codecs, [type](codec_c const &c) { return c.m_type == type; });

  return itr == ms_codecs.end() ? codec_c{} : *itr;
}

codec_c const
codec_c::look_up(char const *fourcc_or_codec_id) {
  return look_up(std::string{fourcc_or_codec_id});
}

codec_c const
codec_c::look_up(fourcc_c const &fourcc) {
  return look_up(fourcc.str());
}

codec_c const
codec_c::look_up_audio_format(uint16_t audio_format) {
  initialize();

  auto itr = brng::find_if(ms_codecs, [audio_format](codec_c const &c) { return brng::find(c.m_audio_formats, audio_format) != c.m_audio_formats.end(); });

  return itr == ms_codecs.end() ? codec_c{} : *itr;
}

codec_c const
codec_c::look_up_object_type_id(unsigned int object_type_id) {
  return look_up(  (   (MP4OTI_MPEG4Audio                      == object_type_id)
                    || (MP4OTI_MPEG2AudioMain                  == object_type_id)
                    || (MP4OTI_MPEG2AudioLowComplexity         == object_type_id)
                    || (MP4OTI_MPEG2AudioScaleableSamplingRate == object_type_id)) ? CT_A_AAC
                 : MP4OTI_MPEG1Audio                           == object_type_id   ? CT_A_MP2
                 : MP4OTI_MPEG2AudioPart3                      == object_type_id   ? CT_A_MP3
                 : MP4OTI_DTS                                  == object_type_id   ? CT_A_DTS
                 : (   (MP4OTI_MPEG2VisualSimple               == object_type_id)
                    || (MP4OTI_MPEG2VisualMain                 == object_type_id)
                    || (MP4OTI_MPEG2VisualSNR                  == object_type_id)
                    || (MP4OTI_MPEG2VisualSpatial              == object_type_id)
                    || (MP4OTI_MPEG2VisualHigh                 == object_type_id)
                    || (MP4OTI_MPEG2Visual422                  == object_type_id)
                    || (MP4OTI_MPEG1Visual                     == object_type_id)) ? CT_V_MPEG12
                 : MP4OTI_MPEG4Visual                          == object_type_id   ? CT_V_MPEG4_P2
                 : MP4OTI_VOBSUB                               == object_type_id   ? CT_S_VOBSUB
                 :                                                                   CT_UNKNOWN);
}

std::string const
codec_c::get_name(std::string const &fourcc_or_codec_id,
                  std::string const &fallback) {
  auto codec = look_up(fourcc_or_codec_id);
  return codec ? codec.m_name : fallback;
}

std::string const
codec_c::get_name(codec_type_e type,
                  std::string const &fallback) {
  auto codec = look_up(type);
  return codec ? codec.m_name : fallback;
}
