/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_ogm.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_ogm.h,v 1.10 2003/04/13 15:23:03 mosu Exp $
    \brief class definitions for the OGG media stream reader
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __R_OGM_H
#define __R_OGM_H

#include <stdio.h>

#include <ogg/ogg.h>

#include "common.h"
#include "pr_generic.h"

#define OGM_STREAM_TYPE_UNKNOWN 0
#define OGM_STREAM_TYPE_VORBIS  1
#define OGM_STREAM_TYPE_VIDEO   2
#define OGM_STREAM_TYPE_PCM     3
#define OGM_STREAM_TYPE_MP3     4
#define OGM_STREAM_TYPE_AC3     5
#define OGM_STREAM_TYPE_TEXT    6

typedef struct {
  ogg_stream_state      os;
  generic_packetizer_c *packetizer;
  int                   sid;
  int                   stype;
  int                   eos;
  int                   serial;
  int                   units_processed;
  int                   num_packets;
  int                   packet_sizes[3];
  unsigned char        *packet_data[3];
  int                   vorbis_rate;
} ogm_demuxer_t;

class ogm_reader_c: public generic_reader_c {
private:
  ogg_sync_state    oy;
  FILE             *file;
  int               act_wchar;
  ogm_demuxer_t   **sdemuxers;
  int               num_sdemuxers;
  int               nastreams, nvstreams, ntstreams, numstreams;
  char            **comments;
  int               bos_pages_read;
     
public:
  ogm_reader_c(track_info_t *nti) throw (error_c);
  virtual ~ogm_reader_c();

  virtual int            read();
  virtual packet_t      *get_packet();

  virtual int            display_priority();
  virtual void           display_progress();
  
  static int             probe_file(FILE *file, int64_t size);
    
private:
  virtual ogm_demuxer_t *find_demuxer(int serialno);
  virtual int            demuxing_requested(unsigned char *, int);
  virtual int            read_page(ogg_page *);
  virtual void           add_new_demuxer(ogm_demuxer_t *);
  virtual void           handle_new_stream(ogg_page *);
  virtual void           process_page(ogg_page *);
  virtual int            packet_available();
  virtual int            read_headers();
  virtual void           process_header_page(ogg_page *);
  virtual void           create_packetizers();
  virtual void           free_demuxer(int);
};


#endif  // __R_OGM_H
