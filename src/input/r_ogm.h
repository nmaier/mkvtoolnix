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
#if defined(HAVE_FLAC_FORMAT_H)
#include <FLAC/stream_decoder.h>
#endif

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
#define OGM_STREAM_TYPE_FLAC    7
#define OGM_STREAM_TYPE_AAC     8

#if defined(HAVE_FLAC_FORMAT_H)
class flac_header_extractor_c {
public:
  FLAC__StreamDecoder *decoder;
  bool metadata_parsed;
  int channels, sample_rate, bits_per_sample;
  mm_io_c *file;
  ogg_stream_state os;
  ogg_sync_state oy;
  ogg_page og;
  int64_t sid, num_packets, num_header_packets;
  bool done;

public:
  flac_header_extractor_c(const char *file_name, int64_t nsid);
  ~flac_header_extractor_c();
  bool extract();
  bool read_page();
};
#endif

struct ogm_demuxer_t {
  ogg_stream_state os;
  generic_packetizer_c *packetizer;
  bool headers_set;
  int sid, stype, serial, eos;
  int units_processed, vorbis_rate;
  bool headers_read;
  vector<unsigned char *> packet_data, nh_packet_data;
  vector<int> packet_sizes, nh_packet_sizes;
#if defined(HAVE_FLAC_FORMAT_H)
  flac_header_extractor_c *fhe;
  int flac_header_packets, channels, bits_per_sample;
  int64_t last_granulepos;
#endif

  ogm_demuxer_t():
    packetizer(NULL), sid(0), stype(0), serial(0), eos(0), units_processed(0),
    vorbis_rate(0), headers_read(false) {
    memset(&os, 0, sizeof(ogg_stream_state));
  }
  ~ogm_demuxer_t() {
    uint32_t i;
    for (i = 0; i < packet_data.size(); i++)
      safefree(packet_data[i]);
  }
};

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
  ogm_reader_c(track_info_c *nti) throw (error_c);
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
  virtual void create_packetizer(int64_t tid);
  virtual void free_demuxer(int);
  virtual void flush_packetizers();
};


#endif  // __R_OGM_H
