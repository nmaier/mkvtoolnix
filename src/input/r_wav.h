/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the WAV reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Peter Niemayer <niemayer@isg.de>.
*/

#ifndef __R_WAV_H
#define __R_WAV_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/dts.h"
#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

#include <avilib.h>

class wav_reader_c;

class wav_demuxer_c {
public:
  wav_reader_c         *m_reader;
  wave_header          *m_wheader;
  generic_packetizer_c *m_ptzr;
  track_info_c         &m_ti;

public:
  wav_demuxer_c(wav_reader_c *reader, wave_header *wheader);
  virtual ~wav_demuxer_c() {};

  virtual int64_t get_preferred_input_size() = 0;
  virtual unsigned char *get_buffer() = 0;
  virtual void process(int64_t len) = 0;

  virtual generic_packetizer_c *create_packetizer() = 0;

  virtual std::string get_codec() = 0;

  virtual bool probe(mm_io_cptr &io) = 0;
};

typedef counted_ptr<wav_demuxer_c> wav_demuxer_cptr;

struct wav_chunk_t {
  int64_t pos;
  char id[4];
  int64_t len;
};

class wav_reader_c: public generic_reader_c {
private:
  mm_io_cptr m_io;
  struct wave_header m_wheader;
  int64_t m_bytes_processed, m_bytes_in_data_chunks, m_remaining_bytes_in_current_data_chunk;

  std::vector<wav_chunk_t> m_chunks;
  int m_cur_data_chunk_idx;

  wav_demuxer_cptr m_demuxer;

  uint32_t m_format_tag;

public:
  wav_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~wav_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("WAV") : "WAV";
  }

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);

  virtual int get_progress();

  static int probe_file(mm_io_c *io, uint64_t size);

protected:
  void scan_chunks();
  int find_chunk(const char *id, int start_idx = 0, bool allow_empty = true);

  void dump_headers();

  void parse_file();
  void create_demuxer();
};

#endif // __R_WAV_H
