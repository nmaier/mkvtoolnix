/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  mp3_common.h
  common routines for MP3 handling

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on Xiph.org's 'oggmerge' found in their CVS repository
  See http://www.xiph.org

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __MP3_COMMON_H
#define __MP3_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

extern int mp3_tabsel[2][16];
extern long mp3_freqs[9];

typedef struct {
  int lsf;
  int mpeg25;
  int mode;
  int error_protection;
  int stereo;
  int ssize;
  int bitrate_index;
  int sampling_frequency;
  int padding;
  int framesize;
} mp3_header_t;

int find_mp3_header(char *buf, int size, unsigned long *_header);
void decode_mp3_header(unsigned long header, mp3_header_t *h);

#ifdef __cplusplus
}
#endif


#endif
