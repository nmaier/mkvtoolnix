/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file
   \version $Id: vc1_common.h 3855 2008-08-24 19:44:49Z mosu $

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __VC1_COMMON_H
#define __VC1_COMMON_H

#include "os.h"

#define VC1_PROFILE_SIMPLE    0x00000000
#define VC1_PROFILE_MAIN      0x00000001
#define VC1_PROFILE_COMPLEX   0x00000002
#define VC1_PROFILE_ADVANCED  0x00000003

#define VC1_MARKER_ENDOFSEQ   0x0000010a
#define VC1_MARKER_SLICE      0x0000010b
#define VC1_MARKER_FIELD      0x0000010c
#define VC1_MARKER_FRAME      0x0000010d
#define VC1_MARKER_ENTRYPOINT 0x0000010e
#define VC1_MARKER_SEQHDR     0x0000010f

namespace vc1 {
  enum frame_type_e {
    FRAME_TYPE_I,
    FRAME_TYPE_P,
    FRAME_TYPE_B,
    FRAME_TYPE_BI,
    FRAME_TYPE_P_SKIPPED,
  };

  struct sequence_header_t {
    int  profile;
    int  level;
    int  chroma_format;
    int  frame_rtq_postproc;
    int  bit_rtq_postproc;
    bool postproc_flag;
    int  pixel_width;
    int  pixel_height;
    bool broadcast_flag;
    bool interlace_flag;
    bool tf_counter_flag;
    bool f_inter_p_flag;
    bool psf_mode_flag;
    bool display_info_flag;
    int  display_width;
    int  display_height;
    bool aspect_ratio_flag;
    int  aspect_ratio_width;
    int  aspect_ratio_height;
    bool framerate_flag;
    int  framerate_num;
    int  framerate_den;
    int  color_prim;
    int  transfer_char;
    int  matrix_coef;
    bool hrd_param_flag;
    int  hrd_num_leaky_buckets;

    sequence_header_t();
  };

  struct entrypoint_t {
    bool broken_link_flag;
    bool closed_entry_flag;
    bool pan_scan_flag;
    bool refdist_flag;
    bool loop_filter_flag;
    bool fast_uvmc_flag;
    bool extended_mv_flag;
    int  dquant;
    bool vs_transform_flag;
    bool overlap_flag;
    int  quantizer_mode;
    bool coded_dimensions_flag;
    int  coded_width;
    int  coded_height;
    bool extended_dmv_flag;
    bool luma_scaling_flag;
    int  luma_scaling;
    bool chroma_scaling_flag;
    int  chroma_scaling;

    entrypoint_t();
  };

  struct frame_header_t {
    int          fcm;
    frame_type_e frame_type;

    frame_header_t();
  };

  inline bool is_marker(uint32_t value) {
    return (value & 0xffffff00) == 0x00000100;
  }

  bool MTX_DLL_API parse_sequence_header(const unsigned char *buf, int size, sequence_header_t &seqhdr);
  bool MTX_DLL_API parse_entrypoint(const unsigned char *buf, int size, entrypoint_t &entrypoint, sequence_header_t &seqhdr);
  bool MTX_DLL_API parse_frame_header(const unsigned char *buf, int size, frame_header_t &frame_header, sequence_header_t &seqhdr);

  class MTX_DLL_API es_parser_c {
  protected:
    int64_t m_stream_pos;

    bool m_seqhdr_found;
    sequence_header_t m_seqhdr;

    memory_cptr m_unparsed_buffer;

  public:
    es_parser_c();
    virtual ~es_parser_c();

    virtual void add_bytes(unsigned char *buf, int size);
    virtual void add_bytes(memory_cptr &buf) {
      add_bytes(buf->get(), buf->get_size());
    };

    virtual void flush();

    virtual bool is_sequence_header_available() {
      return m_seqhdr_found;
    }

    virtual void get_sequence_header(sequence_header_t &seqhdr) {
      if (m_seqhdr_found)
        memcpy(&seqhdr, &m_seqhdr, sizeof(sequence_header_t));
    }

    virtual void handle_packet(memory_cptr packet);

  protected:
    virtual void handle_entrypoint_packet(memory_cptr packet);
    virtual void handle_field_packet(memory_cptr packet);
    virtual void handle_frame_packet(memory_cptr packet);
    virtual void handle_sequence_header_packet(memory_cptr packet);
    virtual void handle_slice_packet(memory_cptr packet);
    virtual void handle_unknown_packet(uint32_t marker, memory_cptr packet);
  };
};

#endif  // __VC1_COMMON_H
