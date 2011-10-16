/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the OGG media stream reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_OGM_FLAC_H
#define __R_OGM_FLAC_H

#include "common/common_pch.h"

#if defined(HAVE_FLAC_FORMAT_H)

#include <ogg/ogg.h>
#include <FLAC/stream_decoder.h>

#include "common/mm_io.h"

enum oggflac_mode_e {
  ofm_pre_1_1_1,
  ofm_post_1_1_1,
};

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
  oggflac_mode_e mode;

public:
  flac_header_extractor_c(const std::string &file_name, int64_t p_sid, oggflac_mode_e p_mode);
  ~flac_header_extractor_c();
  bool extract();
  bool read_page();
};

class ogm_a_flac_demuxer_c: public ogm_demuxer_c {
public:
  unsigned int flac_header_packets, sample_rate, channels, bits_per_sample;
  oggflac_mode_e mode;

public:
  ogm_a_flac_demuxer_c(ogm_reader_c *p_reader, oggflac_mode_e p_mode);

  virtual const char *get_type() {
    return "audio";
  };

  virtual std::string get_codec() {
    return "FLAC";
  };

  virtual void initialize();

  virtual generic_packetizer_c *create_packetizer();

  virtual void process_page(int64_t granulepos);
  virtual void process_header_page();
};

#endif  // HAVE_FLAC_FORMAT_H

#endif  // __R_OGM_FLAC_H
