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
    \version $Id$
    \brief class definitions for the OGG media stream reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_OGM_H
#define __R_OGM_H

#include "os.h"

#include <stdio.h>

#include <ogg/ogg.h>

#include <vector>

#include "mm_io.h"
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
  ogg_stream_state os;
  generic_packetizer_c *packetizer;
  int sid, stype, serial, eos;
  int units_processed, num_packets, vorbis_rate, packet_sizes[3];
  unsigned char *packet_data[3];
} ogm_demuxer_t;

class ogm_reader_c: public generic_reader_c {
private:
  ogg_sync_state oy;
  mm_io_c *mm_io;
  int act_wchar, num_sdemuxers, nastreams, nvstreams, ntstreams, numstreams;
  ogm_demuxer_t   **sdemuxers;
  char **comments;
  int bos_pages_read;
  int64_t file_size;

public:
  ogm_reader_c(track_info_t *nti) throw (error_c);
  virtual ~ogm_reader_c();

  virtual int read(generic_packetizer_c *ptzr);
  virtual void set_headers();
  virtual void identify();

  virtual int display_priority();
  virtual void display_progress(bool final = false);

  static int probe_file(mm_io_c *mm_io, int64_t size);

private:
  virtual ogm_demuxer_t *find_demuxer(int serialno);
  virtual int read_page(ogg_page *);
  virtual void add_new_demuxer(ogm_demuxer_t *);
  virtual void handle_new_stream(ogg_page *);
  virtual void process_page(ogg_page *);
  virtual int packet_available();
  virtual int read_headers();
  virtual void process_header_page(ogg_page *);
  virtual void create_packetizers();
  virtual void free_demuxer(int);
};


#endif  // __R_OGM_H
