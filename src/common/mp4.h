/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   common definitions for MP4 files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_MP4_H
#define MTX_COMMON_MP4_H

// Object type identifications.
// See http://gpac.sourceforge.net/tutorial/mediatypes.htm
#define MP4OTI_MPEG4Systems1                   0x01
#define MP4OTI_MPEG4Systems2                   0x02
#define MP4OTI_MPEG4Visual                     0x20
#define MP4OTI_MPEG4Audio                      0x40
#define MP4OTI_MPEG2VisualSimple               0x60
#define MP4OTI_MPEG2VisualMain                 0x61
#define MP4OTI_MPEG2VisualSNR                  0x62
#define MP4OTI_MPEG2VisualSpatial              0x63
#define MP4OTI_MPEG2VisualHigh                 0x64
#define MP4OTI_MPEG2Visual422                  0x65
#define MP4OTI_MPEG2AudioMain                  0x66
#define MP4OTI_MPEG2AudioLowComplexity         0x67
#define MP4OTI_MPEG2AudioScaleableSamplingRate 0x68
#define MP4OTI_MPEG2AudioPart3                 0x69
#define MP4OTI_MPEG1Visual                     0x6A
#define MP4OTI_MPEG1Audio                      0x6B
#define MP4OTI_JPEG                            0x6C
#define MP4OTI_DTS                             0xA9
#define MP4OTI_VOBSUB                          0xE0

#endif
