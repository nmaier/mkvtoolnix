/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_DIRAC_COMMON_H
#define __MTX_COMMON_DIRAC_COMMON_H

#include "common/os.h"

#include "common/smart_pointers.h"

#define DIRAC_SYNC_WORD            0x42424344 // 'BBCD'
#define DIRAC_UNIT_SEQUENCE_HEADER 0x00
#define DIRAC_UNIT_END_OF_SEQUENCE 0x10
#define DIRAC_UNIT_AUXILIARY_DATA  0x20
#define DIRAC_UNIT_PADDING         0x30

namespace dirac {
  inline bool is_auxiliary_data(int type) {
    return (type & 0xf8) == 0x20;
  }

  inline bool is_picture(int type) {
    return (type & 0x08) == 0x08;
  }

  struct sequence_header_t {
    unsigned int major_version;
    unsigned int minor_version;
    unsigned int profile;
    unsigned int level;
    unsigned int base_video_format;
    unsigned int pixel_width;
    unsigned int pixel_height;
    unsigned int chroma_format;
    bool         interlaced;
    bool         top_field_first;
    unsigned int frame_rate_numerator;
    unsigned int frame_rate_denominator;
    unsigned int aspect_ratio_numerator;
    unsigned int aspect_ratio_denominator;
    unsigned int clean_width;
    unsigned int clean_height;
    unsigned int left_offset;
    unsigned int top_offset;

    sequence_header_t();
  };

  struct frame_t {
    memory_cptr data;
    int64_t     timecode;
    int64_t     duration;
    bool        contains_sequence_header;

    frame_t();
    void init();
  };
  typedef counted_ptr<frame_t> frame_cptr;

  bool parse_sequence_header(const unsigned char *buf, int size, sequence_header_t &seqhdr);

  class es_parser_c {
  protected:
    int64_t m_stream_pos;

    bool m_seqhdr_found;
    bool m_seqhdr_changed;
    sequence_header_t m_seqhdr;
    memory_cptr m_raw_seqhdr;

    memory_cptr m_unparsed_buffer;

    std::deque<memory_cptr> m_pre_frame_extra_data;
    std::deque<memory_cptr> m_post_frame_extra_data;

    std::deque<frame_cptr> m_frames;
    frame_cptr m_current_frame;

    std::deque<int64_t> m_timecodes;
    int64_t m_previous_timecode;
    int64_t m_num_timecodes;
    int64_t m_num_repeated_fields;

    bool m_default_duration_forced;
    int64_t m_default_duration;

  public:
    es_parser_c();
    virtual ~es_parser_c();

    virtual void add_bytes(unsigned char *buf, size_t size);
    virtual void add_bytes(memory_cptr &buf) {
      add_bytes(buf->get_buffer(), buf->get_size());
    };

    virtual void flush();

    virtual bool is_sequence_header_available() {
      return m_seqhdr_found;
    }

    virtual bool has_sequence_header_changed() {
      return m_seqhdr_changed;
    }

    virtual bool are_headers_available() {
      return m_seqhdr_found;
    }

    virtual void get_sequence_header(sequence_header_t &seqhdr) {
      if (m_seqhdr_found)
        memcpy(&seqhdr, &m_seqhdr, sizeof(sequence_header_t));
    }

    virtual memory_cptr get_raw_sequence_header() {
      if (m_seqhdr_found)
        return memory_cptr(m_raw_seqhdr->clone());
      else
        return memory_cptr(nullptr);
    }

    virtual void handle_unit(memory_cptr packet);

    virtual bool is_frame_available() {
      return !m_frames.empty();
    }

    virtual frame_cptr get_frame() {
      frame_cptr frame;

      if (!m_frames.empty()) {
        frame = m_frames.front();
        m_frames.pop_front();
      }

      return frame;
    }

    virtual void add_timecode(int64_t timecode) {
      m_timecodes.push_back(timecode);
    }

    virtual void set_default_duration(int64_t default_duration) {
      m_default_duration        = default_duration;
      m_default_duration_forced = true;
    }

    virtual int64_t get_default_duration() {
      return m_default_duration;
    }

  protected:
    virtual void handle_auxiliary_data_unit(memory_cptr packet);
    virtual void handle_end_of_sequence_unit(memory_cptr packet);
    virtual void handle_padding_unit(memory_cptr packet);
    virtual void handle_picture_unit(memory_cptr packet);
    virtual void handle_sequence_header_unit(memory_cptr packet);
    virtual void handle_unknown_unit(memory_cptr packet);

    virtual int64_t get_next_timecode();
    virtual int64_t peek_next_calculated_timecode();

    virtual void add_pre_frame_extra_data(memory_cptr packet);
    virtual void add_post_frame_extra_data(memory_cptr packet);
    virtual void combine_extra_data_with_packet();

    virtual void flush_frame();
  };
};

#endif  // __MTX_COMMON_DIRAC_COMMON_H
