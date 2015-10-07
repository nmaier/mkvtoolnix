/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions of chapter physical equivalent values

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_CHAPTERS_PHYSICAL_H
#define MTX_COMMON_CHAPTERS_PHYSICAL_H

// see http://www.matroska.org/technical/specs/index.html#physical
#define CHAPTER_PHYSEQUIV_SET          70
#define CHAPTER_PHYSEQUIV_PACKAGE      70

#define CHAPTER_PHYSEQUIV_CD           60
#define CHAPTER_PHYSEQUIV_12INCH       60
#define CHAPTER_PHYSEQUIV_10INCH       60
#define CHAPTER_PHYSEQUIV_7INCH        60
#define CHAPTER_PHYSEQUIV_TAPE         60
#define CHAPTER_PHYSEQUIV_MINIDISC     60
#define CHAPTER_PHYSEQUIV_DAT          60
#define CHAPTER_PHYSEQUIV_DVD          60
#define CHAPTER_PHYSEQUIV_VHS          60
#define CHAPTER_PHYSEQUIV_LASERDISC    60

#define CHAPTER_PHYSEQUIV_SIDE         50

#define CHAPTER_PHYSEQUIV_LAYER        40

#define CHAPTER_PHYSEQUIV_SESSION      30

#define CHAPTER_PHYSEQUIV_TRACK        20

#define CHAPTER_PHYSEQUIV_INDEX        10

#endif // MTX_COMMON_CHAPTERS_PHYSICAL_H
