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
    \version \$Id: r_matroska.h,v 1.2 2003/04/13 15:23:03 mosu Exp $
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
#include "StdIOCallback.h"

using namespace LIBMATROSKA_NAMESPACE;

// default values for Matroska elements
#define MKVD_TIMECODESCALE 1000000 // 1000000 = 1ms

// see http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/*checkout*/matroska/doc/website/specs/codex.html?rev=HEAD&content-type=text/html
#define MKV_MSVCM "V_MS/VFW/FOURCC"
#define MKV_MSACM "A_MS/ACM"

typedef struct {
  u_int32_t tnum;
  
  char *codec_id;
  int ms_compat;

  char type; // 'v' = video, 'a' = audio, 't' = text subs
  
  // Parameters for video tracks
  u_int32_t v_width, v_height;
  float v_frate;
  char v_fourcc[5];

  // Parameters for audio tracks
  u_int32_t a_channels, a_bps;
  float a_sfreq;
  u_int16_t a_formattag;

  void *private_data;
  unsigned int private_size;

  unsigned char *headers[3];
  u_int32_t header_sizes[3];

  int64_t units_processed;

  int ok;

  generic_packetizer_c *packetizer;
} mkv_track_t;

class mkv_reader_c: public generic_reader_c {
private:
  int act_wchar;

  mkv_track_t **tracks;
  int num_tracks;

  int64_t tc_scale;
  u_int32_t cluster_tc;

  StdIOCallback *in;

  EbmlStream *es;
  EbmlElement *saved_l1, *saved_l2, *segment;
     
public:
  mkv_reader_c(track_info_t *nti) throw (error_c);
  virtual ~mkv_reader_c();

  virtual int          read();
  virtual packet_t    *get_packet();

  virtual int          display_priority();
  virtual void         display_progress();
  
  static int           probe_file(FILE *file, int64_t size);
    
private:
  virtual int          demuxing_requested(mkv_track_t *t);
  virtual int          read_headers();
  virtual void         create_packetizers();
  virtual mkv_track_t *new_mkv_track();
  virtual mkv_track_t *find_track_by_num(u_int32_t num, mkv_track_t *c);
  virtual void         verify_tracks();
  virtual int          packets_available();
  virtual void         handle_subtitles(mkv_track_t *t, KaxBlock &block);
};


#endif  // __R_MATROSKA_H
