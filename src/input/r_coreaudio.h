/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the CoreAudio reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_INPUT_R_COREAUDIO_H
#define MTX_INPUT_R_COREAUDIO_H

#include "common/common_pch.h"

#include "common/mm_io.h"
#include "common/error.h"
#include "common/samples_timecode_conv.h"

#include "merge/pr_generic.h"

struct coreaudio_chunk_t {
  std::string m_type;
  uint64_t m_position, m_data_position, m_size;
};
typedef std::vector<coreaudio_chunk_t>::iterator coreaudio_chunk_itr;

struct coreaudio_packet_t {
  uint64_t m_position, m_size, m_duration, m_timecode;
};
typedef std::vector<coreaudio_packet_t>::iterator coreaudio_packet_itr;

class coreaudio_reader_c: public generic_reader_c {
private:
  memory_cptr m_magic_cookie;

  std::vector<coreaudio_chunk_t> m_chunks;
  std::vector<coreaudio_packet_t> m_packets;

  coreaudio_packet_itr m_current_packet;

  std::string m_codec;
  bool m_supported;

  double m_sample_rate;
  unsigned int m_flags, m_bytes_per_packet, m_frames_per_packet, m_channels, m_bites_per_sample;

  samples_to_timecode_converter_c m_frames_to_timecode;

  bool m_debug_headers, m_debug_chunks, m_debug_packets;

public:
  coreaudio_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~coreaudio_reader_c();

  virtual std::string const get_format_name(bool translate = true) const {
    return translate ? Y("CoreAudio") : "CoreAudio";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);

  static int probe_file(mm_io_c *in, uint64_t size);

protected:
  void scan_chunks();
  coreaudio_chunk_itr find_chunk(std::string const &type, bool throw_on_error, coreaudio_chunk_itr start);

  coreaudio_chunk_itr
  find_chunk(std::string const &type, bool throw_on_error = false) {
    return find_chunk(type, throw_on_error, m_chunks.begin());
  }

  memory_cptr read_chunk(std::string const &type, bool throw_on_error = true);

  void parse_desc_chunk();
  void parse_pakt_chunk();
  void parse_kuki_chunk();

  void handle_alac_magic_cookie(memory_cptr chunk);

  generic_packetizer_c *create_alac_packetizer();

  void debug_error_and_throw(boost::format const &format) const;
  void dump_headers() const;
};

#endif  // MTX_INPUT_R_COREAUDIO_H
