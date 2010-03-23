/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   FLAC helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __FLAC_COMMON_H
#define __FLAC_COMMON_H

#include "common/common_pch.h"

#if defined(HAVE_FLAC_FORMAT_H)

#include <FLAC/format.h>

#define FLAC_HEADER_STREAM_INFO      1
#define FLAC_HEADER_VORBIS_COMMENTS  2
#define FLAC_HEADER_CUESHEET         4
#define FLAC_HEADER_APPLICATION      8
#define FLAC_HEADER_SEEKTABLE       16

int flac_get_num_samples(unsigned char *buf, int size, FLAC__StreamMetadata_StreamInfo &stream_info);
int flac_decode_headers(unsigned char *mem, int size, int num_elements, ...);

#endif /* HAVE_FLAC_FORMAT_H */

#endif /* __FLAC_COMMON_H */
