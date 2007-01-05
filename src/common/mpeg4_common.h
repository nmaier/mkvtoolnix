/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file
   \version $Id$

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MPEG4_COMMON_H
#define __MPEG4_COMMON_H

#include "common_memory.h"

/** Start code for a MPEG-4 part 2 (?) video object plain */
#define MPEGVIDEO_VOP_START_CODE                  0x000001b6
/** Strt code for a MPEG-4 part 2 group of pictures */
#define MPEGVIDEO_GOP_START_CODE                  0x000001b3
/** Start code for a MPEG-1/-2 sequence header */
#define MPEGVIDEO_SEQUENCE_START_CODE             0x000001b3
/** Start code for a MPEG-1 and -2 packet */
#define MPEGVIDEO_PACKET_START_CODE               0x000001ba
#define MPEGVIDEO_SYSTEM_HEADER_START_CODE        0x000001bb
#define MPEGVIDEO_MPEG_PROGRAM_END_CODE           0x000001b9

/** Start code for a MPEG-4 part 2 visual object sequence start marker */
#define MPEGVIDEO_VOS_START_CODE                  0x000001b0
/** Start code for a MPEG-4 part 2 visual object sequence end marker */
#define MPEGVIDEO_VOS_END_CODE                    0x000001b1
/** Start code for a MPEG-4 part 2 visual object marker */
#define MPEGVIDEO_VISUAL_OBJECT_START_CODE        0x000001b5

#define MPEGVIDEO_START_CODE_PREFIX               0x000001
#define MPEGVIDEO_PROGRAM_STREAM_MAP              0xbc
#define MPEGVIDEO_PRIVATE_STREAM_1                0xbd
#define MPEGVIDEO_PADDING_STREAM                  0xbe
#define MPEGVIDEO_PRIVATE_STREAM_2                0xbf
#define MPEGVIDEO_ECM_STREAM                      0xf0
#define MPEGVIDEO_EMM_STREAM                      0xf1
#define MPEGVIDEO_PROGRAM_STREAM_DIRECTORY        0xff
#define MPEGVIDEO_DSMCC_STREAM                    0xf2
#define MPEGVIDEO_ITUTRECH222TYPEE_STREAM         0xf8

#define mpeg_is_start_code(v) \
  ((((v) >> 8) & 0xffffff) == MPEGVIDEO_START_CODE_PREFIX)

/** MPEG-1/-2 frame rate: 24000/1001 frames per second */
#define MPEGVIDEO_FPS_23_976    0x01
/** MPEG-1/-2 frame rate: 24 frames per second */
#define MPEGVIDEO_FPS_24        0x02
/** MPEG-1/-2 frame rate: 25 frames per second */
#define MPEGVIDEO_FPS_25        0x03
/** MPEG-1/-2 frame rate: 30000/1001 frames per second */
#define MPEGVIDEO_FPS_29_97     0x04
/** MPEG-1/-2 frame rate: 30 frames per second */
#define MPEGVIDEO_FPS_30        0x05
/** MPEG-1/-2 frame rate: 50 frames per second */
#define MPEGVIDEO_FPS_50        0x06
/** MPEG-1/-2 frame rate: 60000/1001 frames per second */
#define MPEGVIDEO_FPS_59_94     0x07
/** MPEG-1/-2 frame rate: 60 frames per second */
#define MPEGVIDEO_FPS_60        0x08

/** MPEG-1/-2 aspect ratio 1:1 */
#define MPEGVIDEO_AR_1_1        0x10
/** MPEG-1/-2 aspect ratio 4:3 */
#define MPEGVIDEO_AR_4_3        0x20
/** MPEG-1/-2 aspect ratio 16:9 */
#define MPEGVIDEO_AR_16_9       0x30
/** MPEG-1/-2 aspect ratio 2.21 */
#define MPEGVIDEO_AR_2_21       0x40

#define IS_MPEG4_L2_FOURCC(s) \
  (!strncasecmp((s), "DIVX", 4) || !strncasecmp((s), "XVID", 4) || \
   !strncasecmp((s), "DX5", 3))
#define IS_MPEG4_L2_CODECID(s) \
  (((s) == MKV_V_MPEG4_SP) || ((s) == MKV_V_MPEG4_AP) || \
   ((s) == MKV_V_MPEG4_ASP))

enum mpeg_video_type_e {
  MPEG_VIDEO_NONE = 0,
  MPEG_VIDEO_V1,
  MPEG_VIDEO_V2,
  MPEG_VIDEO_V4_LAYER_2,
  MPEG_VIDEO_V4_LAYER_10
};

enum frame_type_e {
  FRAME_TYPE_I,
  FRAME_TYPE_P,
  FRAME_TYPE_B
};
#define FRAME_TYPE_TO_CHAR(t) \
  (FRAME_TYPE_I == (t) ? 'I' : FRAME_TYPE_P == (t) ? 'P' : 'B')

/** Pointers to MPEG4 video frames and their data

   MPEG4 video can be stored in a "packed" format, e.g. in AVI. This means
   that one AVI chunk may contain more than one video frame. This is
   usually the case with B frames due to limitations in how AVI and
   Windows' media frameworks work. With ::mpeg4_find_frame_types
   such packed frames can be analyzed. The results are stored in these
   structures: one structure for one frame in the analyzed chunk.
*/
struct video_frame_t {
  /** The beginning of the frame data. This is a pointer into an existing
      buffer handed over to ::mpeg4_find_frame_types. */
  unsigned char *data;
  /** The size of the frame in bytes. */
  int size;
  /** The position of the frame in the original buffer. */
  int pos;
  /** The frame type: \c 'I', \c 'P' or \c 'B'. */
  frame_type_e type;
  /** Private data. */
  unsigned char *priv;
  /** The timecode of the frame in \c ns. */
  int64_t timecode;
  /** The duration of the frame in \c ns. */
  int64_t duration;
  /** The frame's backward reference in \c ns relative to its
      \link video_frame_t::timecode timecode \endlink.
      This value is only set for P and B frames. */
  int64_t bref;
  /** The frame's forward reference in \c ns relative to its
      \link video_frame_t::timecode timecode \endlink.
      This value is only set for B frames. */
  int64_t fref;

  video_frame_t():
    data(NULL), size(0), pos(0), type(FRAME_TYPE_I), priv(NULL),
    timecode(0), duration(0), bref(0), fref(0) {};
};

namespace mpeg1_2 {
  int MTX_DLL_API extract_fps_idx(const unsigned char *buffer,
                                  int buffer_size);
  double MTX_DLL_API get_fps(int idx);
  bool MTX_DLL_API extract_ar(const unsigned char *buffer,
                              int buffer_size, float &ar);
};

namespace mpeg4 {
  namespace p2 {
    bool MTX_DLL_API is_fourcc(const void *fourcc);

    bool MTX_DLL_API extract_par(const unsigned char *buffer,
                                 int buffer_size,
                                 uint32_t &par_num, uint32_t &par_den);
    bool MTX_DLL_API extract_size(const unsigned char *buffer,
                                  int buffer_size,
                                  uint32_t &width, uint32_t &height);
    void MTX_DLL_API find_frame_types(const unsigned char *buffer,
                                      int buffer_size,
                                      vector<video_frame_t> &frames);
    memory_c * MTX_DLL_API parse_config_data(const unsigned char *buffer,
                                             int buffer_size);
  };

  namespace p10 {

#define NALU_TYPE_NON_IDR_SLICE  0x01
#define NALU_TYPE_DP_A_SLICE     0x02
#define NALU_TYPE_DP_B_SLICE     0x03
#define NALU_TYPE_DP_C_SLICE     0x04
#define NALU_TYPE_IDR_SLICE      0x05
#define NALU_TYPE_SEI            0x06
#define NALU_TYPE_SEQ_PARAM      0x07
#define NALU_TYPE_PIC_PARAM      0x08
#define NALU_TYPE_ACCESS_UNIT    0x09
#define NALU_TYPE_END_OF_SEQ     0x0a
#define NALU_TYPE_END_OF_STREAM  0x0b
#define NALU_TYPE_FILLER_DATA    0x0c

#define AVC_SLICE_TYPE_P   0
#define AVC_SLICE_TYPE_B   1
#define AVC_SLICE_TYPE_I   2
#define AVC_SLICE_TYPE_SP  3
#define AVC_SLICE_TYPE_SI  4
#define AVC_SLICE_TYPE2_P  5
#define AVC_SLICE_TYPE2_B  6
#define AVC_SLICE_TYPE2_I  7
#define AVC_SLICE_TYPE2_SP 8
#define AVC_SLICE_TYPE2_SI 9

    struct sps_info_t {
      unsigned id;

      unsigned profile_idc;
      unsigned profile_compat;
      unsigned level_idc;
      unsigned log2_max_frame_num;
      unsigned pic_order_cnt_type;
      unsigned log2_max_pic_order_cnt_lsb;
      int offset_for_non_ref_pic;
      int offset_for_top_to_bottom_field;
      unsigned num_ref_frames_in_pic_order_cnt_cycle;
      bool delta_pic_order_always_zero_flag;
      bool frame_mbs_only;

      // vui:
      bool vui_present, ar_found;
      unsigned par_num, par_den;

      // timing_info:
      bool timing_info_present;
      unsigned num_units_in_tick, time_scale;
      bool fixed_frame_rate;

      unsigned crop_left, crop_top, crop_right, crop_bottom;
      unsigned width, height;

      sps_info_t() {
        memset(this, 0, sizeof(*this));
      }
    };

    struct pps_info_t {
      unsigned id;
      unsigned sps_id;

      bool pic_order_present;

      pps_info_t() {
        memset(this, 0, sizeof(*this));
      }
    };

    struct slice_info_t {
      unsigned char nalu_type;
      unsigned char nal_ref_idc;
      unsigned char type;
      unsigned char pps_id;
      unsigned frame_num;
      bool field_pic_flag, bottom_field_flag;
      unsigned idr_pic_id;
      unsigned pic_order_cnt_lsb;
      int delta_pic_order_cnt_bottom;
      int delta_pic_order_cnt[2];

      int sps;
      int pps;

      slice_info_t() {
        memset(this, 0, sizeof(*this));
      }
    };

    void MTX_DLL_API nalu_to_rbsp(memory_cptr &buffer);
    void MTX_DLL_API rbsp_to_nalu(memory_cptr &buffer);

    bool MTX_DLL_API parse_sps(memory_cptr &buffer, sps_info_t &sps);
    bool MTX_DLL_API parse_pps(memory_cptr &buffer, pps_info_t &pps);

    bool MTX_DLL_API extract_par(uint8_t *&buffer, int &buffer_size,
                                 uint32_t &par_num, uint32_t &par_den);
    bool MTX_DLL_API is_avc_fourcc(const char *fourcc);

    struct avc_frame_t {
      memory_cptr m_data;
      int64_t m_start, m_end, m_ref1, m_ref2;
      bool m_keyframe;
      slice_info_t m_si;

      avc_frame_t() {
        clear();
      };

      avc_frame_t(const avc_frame_t &f) {
        *this = f;
      };

      avc_frame_t &operator =(const avc_frame_t &f) {
        m_data = f.m_data;
        m_start = f.m_start;
        m_end = f.m_end;
        m_ref1 = f.m_ref1;
        m_ref2 = f.m_ref2;
        m_keyframe = f.m_keyframe;
        m_si = f.m_si;

        return *this;
      };

      void clear() {
        m_start = 0;
        m_end = 0;
        m_ref1 = 0;
        m_ref2 = 0;
        m_keyframe = false;
        memset(&m_si, 0, sizeof(m_si));
        m_data = memory_cptr(NULL);
      };
    };

    class MTX_DLL_API nalu_size_length_error_c {
    protected:
      int m_required_length;

    public:
      nalu_size_length_error_c(int required_length):
        m_required_length(required_length) {
      };

      int get_required_length() {
        return m_required_length;
      };
    };

    class MTX_DLL_API avc_es_parser_c {
    protected:
      int m_nalu_size_length;

      bool m_avcc_ready;
      memory_cptr m_avcc;

      int64_t m_default_duration;
      int m_frame_number;

      deque<avc_frame_t> m_frames, m_frames_out;
      deque<int64_t> m_timecodes;

      bool m_generate_timecodes;

      deque<memory_cptr> m_sps_list, m_pps_list, m_extra_data;
      vector<sps_info_t> m_sps_info_list;
      vector<pps_info_t> m_pps_info_list;

      memory_cptr m_unparsed_buffer;

      avc_frame_t m_incomplete_frame;
      bool m_have_incomplete_frame;

      bool m_ignore_nalu_size_length_errors;

    public:
      avc_es_parser_c();

      void enable_timecode_generation(int64_t default_duration) {
        m_default_duration = default_duration;
        m_generate_timecodes = true;
      };

      void add_bytes(unsigned char *buf, int size);
      void add_bytes(memory_cptr &buf) {
        add_bytes(buf->get(), buf->get_size());
      };

      void flush();

      bool frame_available() {
        return !m_frames_out.empty();
      };

      avc_frame_t get_frame() {
        assert(!m_frames_out.empty());

        avc_frame_t frame(*m_frames_out.begin());
        m_frames_out.erase(m_frames_out.begin(), m_frames_out.begin() + 1);

        return frame;
      };

      memory_cptr get_avcc() {
        if (NULL == m_avcc.get())
          create_avcc();
        return m_avcc;
      };

      int get_width() {
        assert(!m_sps_info_list.empty());
        return m_sps_info_list.begin()->width;
      };

      int get_height() {
        assert(!m_sps_info_list.empty());
        return m_sps_info_list.begin()->height;
      };

      void handle_nalu(memory_cptr nalu);

      void add_timecode(int64_t timecode);

      bool headers_parsed() {
        return m_avcc_ready;
      };

      void set_nalu_size_length(int nalu_size_length) {
        m_nalu_size_length = nalu_size_length;
      };

      void dump_info();

    protected:
      bool parse_slice(memory_cptr &buffer, slice_info_t &si);
      void handle_sps_nalu(memory_cptr &nalu);
      void handle_pps_nalu(memory_cptr &nalu);
      void handle_slice_nalu(memory_cptr &nalu);
      void cleanup();
      void default_cleanup();
      bool flush_decision(slice_info_t &si, slice_info_t &ref);
      void flush_incomplete_frame();
      void write_nalu_size(unsigned char *buffer, int size,
                           int nalu_size_length = -1);
      memory_cptr create_nalu_with_size(const memory_cptr &src,
                                        bool add_extra_data = false);
      void create_avcc();
    };
  };
};

#endif /* __MPEG4_COMMON_H */
