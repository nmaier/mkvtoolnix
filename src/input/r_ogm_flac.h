/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id: r_ogm.h 3578 2007-08-16 16:30:05Z mosu $

   class definitions for the OGG media stream reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_OGM_FLAC_H
#define __R_OGM_FLAC_H

#include "os.h"

#if defined(HAVE_FLAC_FORMAT_H)

#include <ogg/ogg.h>
#include <FLAC/stream_decoder.h>

#include "mm_io.h"

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

class ogm_a_flac_demuxer_c: public ogm_demuxer_c {
public:
  int flac_header_packets, sample_rate, channels, bits_per_sample;

public:
  ogm_a_flac_demuxer_c(ogm_reader_c *p_reader);

  virtual const char *get_type() {
    return "audio";
  };

  virtual string get_codec() {
    return "FLAC";
  };

  virtual void initialize();

  virtual generic_packetizer_c *create_packetizer(track_info_c &ti);

  virtual void process_page(int64_t granulepos);
  virtual void process_header_page();
};

#endif  // HAVE_FLAC_FORMAT_H

#endif  // __R_OGM_FLAC_H
