/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definitions for the OGG media stream reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
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
#include "theora_common.h"

#define OGM_STREAM_TYPE_UNKNOWN    0
#define OGM_STREAM_TYPE_VORBIS     1
#define OGM_STREAM_TYPE_VIDEO      2
#define OGM_STREAM_TYPE_PCM        3
#define OGM_STREAM_TYPE_MP3        4
#define OGM_STREAM_TYPE_AC3        5
#define OGM_STREAM_TYPE_TEXT       6
#define OGM_STREAM_TYPE_FLAC       7
#define OGM_STREAM_TYPE_AAC        8
#define OGM_STREAM_TYPE_THEORA     9
#define OGM_STREAM_TYPE_VIDEO_AVC 10

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
  flac_header_extractor_c(const string &file_name, int64_t _sid);
  ~flac_header_extractor_c();
  bool extract();
  bool read_page();
};
#endif

struct ogm_demuxer_t {
  ogg_stream_state os;
  int ptzr;
  int stype, serialno, eos;
  int units_processed, vorbis_rate;
  int num_header_packets, num_non_header_packets;
  bool headers_read;
  string language, title;
  vector<memory_cptr> packet_data, nh_packet_data;
#if defined(HAVE_FLAC_FORMAT_H)
  flac_header_extractor_c *fhe;
  int flac_header_packets, channels, bits_per_sample;
#endif
  int64_t first_granulepos, last_granulepos, last_keyframe_number, default_duration;
  bool in_use;

  theora_identification_header_t theora;

  bool is_avc;

  ogm_demuxer_t():
    ptzr(-1), stype(0), serialno(0), eos(0), units_processed(0),
    vorbis_rate(0), num_header_packets(2), num_non_header_packets(0), headers_read(false),
    first_granulepos(0), last_granulepos(0), last_keyframe_number(-1), default_duration(0),
    in_use(false), is_avc(false) {
    memset(&os, 0, sizeof(ogg_stream_state));
  }
};

class ogm_reader_c: public generic_reader_c {
private:
  ogg_sync_state oy;
  mm_io_c *io;
  vector<ogm_demuxer_t *> sdemuxers;
  int bos_pages_read;
  int64_t file_size;

public:
  ogm_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~ogm_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  virtual int get_progress();

  static int probe_file(mm_io_c *io, int64_t size);

private:
  virtual ogm_demuxer_t *find_demuxer(int serialno);
  virtual int read_page(ogg_page *);
  virtual void handle_new_stream(ogg_page *);
  virtual void handle_new_stream_and_packets(ogg_page *);
  virtual void process_page(ogg_page *);
  virtual int packet_available();
  virtual int read_headers();
  virtual void process_header_page(ogg_page *pg);
  virtual void process_header_packets(ogm_demuxer_t *dmx);
  virtual void handle_stream_comments();
  virtual memory_cptr extract_avcc(ogm_demuxer_t *dmx, int64_t tid);
};


#endif  // __R_OGM_H
