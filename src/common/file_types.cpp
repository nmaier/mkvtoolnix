/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   File type enum

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/file_types.h"

static std::vector<file_type_t> s_supported_file_types;

std::vector<file_type_t> &
file_type_t::get_supported() {
  if (!s_supported_file_types.empty())
    return s_supported_file_types;

  s_supported_file_types.push_back(file_type_t(Y("A/52 (aka AC3)"),                      "ac3 eac3"));
  s_supported_file_types.push_back(file_type_t(Y("AAC (Advanced Audio Coding)"),         "aac m4a mp4"));
  s_supported_file_types.push_back(file_type_t(Y("AVC/h.264 elementary streams"),        "264 avc h264 x264"));
  s_supported_file_types.push_back(file_type_t(Y("AVI (Audio/Video Interleaved)"),       "avi"));
  s_supported_file_types.push_back(file_type_t(Y("Dirac"),                               "drc"));
  s_supported_file_types.push_back(file_type_t(Y("Dolby TrueHD"),                        "thd thd+ac3 truehd true-hd"));
  s_supported_file_types.push_back(file_type_t(Y("DTS/DTS-HD (Digital Theater System)"), "dts dtshd dts-hd"));
#if defined(HAVE_FLAC_FORMAT_H)
  s_supported_file_types.push_back(file_type_t(Y("FLAC (Free Lossless Audio Codec)"),    "flac ogg"));
#endif
  s_supported_file_types.push_back(file_type_t(Y("IVF with VP8 video files"),            "ivf"));
  s_supported_file_types.push_back(file_type_t(Y("MP4 audio/video files"),               "mp4 m4v"));
  s_supported_file_types.push_back(file_type_t(Y("MPEG audio files"),                    "mp2 mp3"));
  s_supported_file_types.push_back(file_type_t(Y("MPEG program streams"),                "mpg mpeg m2v mpv evo evob vob"));
  s_supported_file_types.push_back(file_type_t(Y("MPEG transport streams"),              "ts m2ts"));
  s_supported_file_types.push_back(file_type_t(Y("MPEG video elementary streams"),       "m1v m2v mpv"));
  s_supported_file_types.push_back(file_type_t(Y("Matroska audio/video files"),          "mka mks mkv mk3d webm webmv webma"));
  s_supported_file_types.push_back(file_type_t(Y("PGS/SUP subtitles"),                   "sup"));
  s_supported_file_types.push_back(file_type_t(Y("QuickTime audio/video files"),         "mov"));
  s_supported_file_types.push_back(file_type_t(Y("Ogg/OGM audio/video files"),           "ogg ogm"));
  s_supported_file_types.push_back(file_type_t(Y("RealMedia audio/video files"),         "ra ram rm rmvb rv"));
  s_supported_file_types.push_back(file_type_t(Y("SRT text subtitles"),                  "srt"));
  s_supported_file_types.push_back(file_type_t(Y("SSA/ASS text subtitles"),              "ass ssa"));
  s_supported_file_types.push_back(file_type_t(Y("TTA (The lossless True Audio codec)"), "tta"));
  s_supported_file_types.push_back(file_type_t(Y("USF text subtitles"),                  "usf xml"));
  s_supported_file_types.push_back(file_type_t(Y("VC1 elementary streams"),              "vc1"));
  s_supported_file_types.push_back(file_type_t(Y("VobButtons"),                          "btn"));
  s_supported_file_types.push_back(file_type_t(Y("VobSub subtitles"),                    "idx"));
  s_supported_file_types.push_back(file_type_t(Y("WAVE (uncompressed PCM audio)"),       "wav"));
  s_supported_file_types.push_back(file_type_t(Y("WAVPACK v4 audio"),                    "wv"));
  s_supported_file_types.push_back(file_type_t(Y("WebM audio/video files"),              "webm webmv webma"));

  return s_supported_file_types;
}
