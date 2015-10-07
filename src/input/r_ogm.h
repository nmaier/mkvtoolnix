/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the OGG media stream reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_R_OGM_H
#define MTX_R_OGM_H

#include "common/common_pch.h"

#include <ogg/ogg.h>

#include "common/codec.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "common/theora.h"
#include "common/kate.h"

class ogm_reader_c;

class ogm_demuxer_c {
public:
  ogm_reader_c *reader;
  track_info_c &m_ti;

  ogg_stream_state os;
  int ptzr;
  int64_t track_id;

  codec_c codec;
  bool ms_compat;
  int serialno, eos;
  unsigned int units_processed, num_header_packets, num_non_header_packets;
  bool headers_read;
  std::string language, title;
  std::vector<memory_cptr> packet_data, nh_packet_data;
  int64_t first_granulepos, last_granulepos, default_duration;
  bool in_use;

  int display_width, display_height;

public:
  ogm_demuxer_c(ogm_reader_c *p_reader);

  virtual ~ogm_demuxer_c();

  virtual const char *get_type() {
    return "unknown";
  };
  virtual std::string get_codec() {
    return codec.get_name("unknown");
  };

  virtual void initialize() {
  };
  virtual generic_packetizer_c *create_packetizer() {
    return nullptr;
  };
  virtual void process_page(int64_t granulepos);
  virtual void process_header_page();

  virtual void get_duration_and_len(ogg_packet &op, int64_t &duration, int &duration_len);
  virtual bool is_header_packet(ogg_packet &op) {
    return op.packet[0] & 1;
  };
};

typedef std::shared_ptr<ogm_demuxer_c> ogm_demuxer_cptr;

class ogm_reader_c: public generic_reader_c {
private:
  ogg_sync_state oy;
  std::vector<ogm_demuxer_cptr> sdemuxers;
  int bos_pages_read;

public:
  ogm_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~ogm_reader_c();

  virtual translatable_string_c get_format_name() const {
    return YT("Ogg/OGM");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *in, uint64_t size);

private:
  virtual ogm_demuxer_cptr find_demuxer(int serialno);
  virtual int read_page(ogg_page *);
  virtual void handle_new_stream(ogg_page *);
  virtual void handle_new_stream_and_packets(ogg_page *);
  virtual void process_page(ogg_page *);
  virtual int packet_available();
  virtual int read_headers_internal();
  virtual void process_header_page(ogg_page *pg);
  virtual void process_header_packets(ogm_demuxer_cptr dmx);
  virtual void handle_stream_comments();
};

#endif  // MTX_R_OGM_H
