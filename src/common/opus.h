/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for Opus data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_OPUS_H
#define MTX_COMMON_OPUS_H

#include "common/common_pch.h"

#include "common/strings/formatting.h"
#include "common/timecode.h"

namespace mtx { namespace opus {

class exception: public mtx::exception {
public:
  virtual const char *what() const throw() {
    return "generic Opus error";
  }
};

class decode_error: public exception {
protected:
  std::string m_message;
public:
  explicit decode_error(std::string const &message)  : m_message(message)       { }
  explicit decode_error(boost::format const &message): m_message(message.str()) { }
  virtual ~decode_error() throw() { }

  virtual char const *what() const throw() {
    return m_message.c_str();
  }
};

struct PACKED_STRUCTURE packed_id_header_t {
  char     identifier[8]; // "OpusHead"
  uint8_t  version;       // 1
  uint8_t  channels;
  uint16_t pre_skip;
  uint32_t input_sample_rate;
  uint16_t output_gain;
  uint8_t  channel_map;
};

struct id_header_t {
  unsigned int channels, pre_skip, input_sample_rate, output_gain;

  static id_header_t decode(memory_cptr const &mem);
};

struct toc_t {
  unsigned int config, frame_count;
  bool stereo;
  timecode_c frame_duration, packet_duration;

  static toc_t decode(memory_cptr const &mem);
};

inline std::ostream &
operator <<(std::ostream &out,
            id_header_t const &h) {
  out << "id_header_t{channels: " << h.channels
      << " pre_skip: "            << h.pre_skip
      << " input_sample_rate: "   << h.input_sample_rate
      << " output_gain: "         << h.output_gain
      << "}";
  return out;
}

inline std::ostream &
operator <<(std::ostream &out,
            toc_t const &t) {
  auto flags = out.flags();
  out << std::boolalpha
      << "toc_t{config: "     << t.config
      << " frame_count: "     << t.frame_count
      << " stereo: "          << t.stereo
      << " frame_duration: "  << t.frame_duration
      << " packet_duration: " << t.packet_duration
      << "}";
  out.flags(flags);

  return out;
}

}}

#endif // MTX_COMMON_OPUS_H
