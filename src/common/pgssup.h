/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for PGS/SUP subtitles

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_PGSSUP_H
#define __MTX_COMMON_PGSSUP_H

#define PGSSUP_FILE_MAGIC           0x5047 // "PG" big endian
#define PGSSUP_PALETTE_SEGMENT        0x14
#define PGSSUP_PICTURE_SEGMENT        0x15
#define PGSSUP_PRESENTATION_SEGMENT   0x16
#define PGSSUP_WINDOW_SEGMENT         0x17
#define PGSSUP_DISPLAY_SEGMENT        0x80

#endif  // __MTX_COMMON_PGSSUP_H

