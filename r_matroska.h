/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_matroska.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_matroska.h,v 1.7 2003/05/02 21:49:42 mosu Exp $
    \brief class definitions for the Matroska reader
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __R_MATROSKA_H
#define __R_MATROSKA_H

#include <stdio.h>

#include "common.h"
#include "pr_generic.h"
#include "error.h"

#include "KaxBlock.h"
#include "KaxCluster.h"
#include "StdIOCallback.h"

using namespace LIBMATROSKA_NAMESPACE;

// default values for Matroska elements
#define MKVD_TIMECODESCALE 1000000 // 1000000 = 1ms

typedef struct {
  uint32_t tnum;
  
  char *codec_id;
  int ms_compat;

  char type; // 'v' = video, 'a' = audio, 't' = text subs
  
  // Parameters for video tracks
  uint32_t v_width, v_height;
  float v_frate;
  char v_fourcc[5];

  // Parameters for audio tracks
  uint32_t a_channels, a_bps;
  float a_sfreq;
  uint16_t a_formattag;

  void *private_data;
  unsigned int private_size;

  unsigned char *headers[3];
  uint32_t header_sizes[3];

  int64_t units_processed;

  int ok;

  generic_packetizer_c *packetizer;
} mkv_track_t;

typedef struct {
  unsigned char *data;
  int length;
} buffer_t;

class mkv_reader_c: public generic_reader_c {
private:
  int act_wchar;

  mkv_track_t **tracks;
  int num_tracks;

  int64_t tc_scale;
  uint32_t cluster_tc;

  StdIOCallback *in;

  EbmlStream *es;
  EbmlElement *saved_l1, *saved_l2, *segment;

  KaxCluster *cluster;

  buffer_t **buffers;
  int num_buffers;

  float segment_duration, last_timecode;

  int64_t block_timecode, block_duration, block_ref1, block_ref2;
  mkv_track_t *block_track;
     
public:
  mkv_reader_c(track_info_t *nti) throw (error_c);
  virtual ~mkv_reader_c();

  virtual int read();
  virtual packet_t *get_packet();

  virtual int display_priority();
  virtual void display_progress();
  virtual void set_headers();
  
  static int probe_file(FILE *file, int64_t size);
    
private:
  virtual int demuxing_requested(mkv_track_t *t);
  virtual int read_headers();
  virtual void create_packetizers();
  virtual mkv_track_t *new_mkv_track();
  virtual mkv_track_t *find_track_by_num(uint32_t num, mkv_track_t *c = NULL);
  virtual void verify_tracks();
  virtual int packets_available();
  virtual void add_buffer(DataBuffer &dbuffer);
  virtual void free_buffers();
  virtual void handle_blocks();
};


#endif  // __R_MATROSKA_H
