/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mpeg4_common.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief MPEG4 video helper functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MPEG4_COMMON_H
#define __MPEG4_COMMON_H

#define VOP_START_CODE 0x000001b6

typedef struct {
  unsigned char *data;
  int size, pos;
  char type;
  unsigned char *priv;
  int64_t timecode, duration, bref, fref;
} video_frame_t;

bool MTX_DLL_API mpeg4_extract_par(const unsigned char *buffer, int size,
                                   uint32_t &par_num, uint32_t &par_den);
void MTX_DLL_API mpeg4_find_frame_types(unsigned char *buf, int size,
                                        vector<video_frame_t> &frames);

#endif /* __MPEG4_COMMON_H */
