/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   definition of global variables found in mkvmerge.cpp
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __MKVMERGE_H
#define __MKVMERGE_H

#include "pr_generic.h"

enum display_priority_e {
  DISPLAYPRIORITY_LOW,
  DISPLAYPRIORITY_MEDIUM,
  DISPLAYPRIORITY_HIGH
};

/* file types */
enum file_type_e {
  FILE_TYPE_IS_UNKNOWN = 0,
  FILE_TYPE_OGM,
  FILE_TYPE_AVI,
  FILE_TYPE_WAV,
  FILE_TYPE_SRT,
  FILE_TYPE_MP3,
  FILE_TYPE_AC3,
  FILE_TYPE_CHAPTERS,
  FILE_TYPE_MICRODVD,
  FILE_TYPE_VOBSUB,
  FILE_TYPE_MATROSKA,
  FILE_TYPE_DTS,
  FILE_TYPE_AAC,
  FILE_TYPE_SSA,
  FILE_TYPE_REAL,
  FILE_TYPE_QTMP4,
  FILE_TYPE_FLAC,
  FILE_TYPE_TTA,
  FILE_TYPE_MPEG,
  FILE_TYPE_VOBBTN,
  FILE_TYPE_MAX = FILE_TYPE_VOBBTN
};

int64_t create_track_number(generic_reader_c *reader, int64_t tid);

#endif // __MKVMERGE_H
