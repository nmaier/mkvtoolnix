/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   File type enum

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_FILE_TYPES_H
#define MTX_COMMON_FILE_TYPES_H

#include "common/common_pch.h"

/* file types */
enum file_type_e {
  FILE_TYPE_IS_UNKNOWN = 0,
  FILE_TYPE_AAC,
  FILE_TYPE_AC3,
  FILE_TYPE_ASF,
  FILE_TYPE_AVC_ES,
  FILE_TYPE_AVI,
  FILE_TYPE_CDXA,
  FILE_TYPE_CHAPTERS,
  FILE_TYPE_COREAUDIO,
  FILE_TYPE_DIRAC,
  FILE_TYPE_DTS,
  FILE_TYPE_DV,
  FILE_TYPE_FLAC,
  FILE_TYPE_FLV,
  FILE_TYPE_HEVC_ES,
  FILE_TYPE_HDSUB,
  FILE_TYPE_IVF,
  FILE_TYPE_MATROSKA,
  FILE_TYPE_MICRODVD,
  FILE_TYPE_MP3,
  FILE_TYPE_MPEG_ES,
  FILE_TYPE_MPEG_PS,
  FILE_TYPE_MPEG_TS,
  FILE_TYPE_OGM,
  FILE_TYPE_PGSSUP,
  FILE_TYPE_QTMP4,
  FILE_TYPE_REAL,
  FILE_TYPE_SRT,
  FILE_TYPE_SSA,
  FILE_TYPE_TRUEHD,
  FILE_TYPE_TTA,
  FILE_TYPE_USF,
  FILE_TYPE_VC1,
  FILE_TYPE_VOBBTN,
  FILE_TYPE_VOBSUB,
  FILE_TYPE_WAV,
  FILE_TYPE_WAVPACK4,
  FILE_TYPE_MAX = FILE_TYPE_WAVPACK4
};

struct file_type_t {
  std::string title, extensions;

  file_type_t(const std::string &p_title, const std::string &p_extensions)
    : title(p_title)
    , extensions(p_extensions)
  {
  }

  static std::vector<file_type_t> &get_supported();
};

#endif // MTX_COMMON_FILE_TYPES_H
