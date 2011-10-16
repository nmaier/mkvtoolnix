/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the Matroska reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <s.lhomme@free.fr>.
*/

#ifndef __R_MATROSKA_H
#define __R_MATROSKA_H

#include "common/common_pch.h"

#include <boost/logic/tribool.hpp>

#include "common/compression.h"
#include "common/error.h"
#include "common/kax_file.h"
#include "common/mm_io.h"
#include "common/mpeg4_p10.h"
#include "merge/pr_generic.h"

#include <ebml/EbmlUnicodeString.h>

#include <matroska/KaxBlock.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxContentEncoding.h>

using namespace libebml;
using namespace libmatroska;

struct kax_track_t {
  uint64_t tnum, tuid;

  std::string codec_id;
  bool ms_compat;

  char type; // 'v' = video, 'a' = audio, 't' = text subs, 'b' = buttons
  char sub_type;                // 't' = text, 'v' = VobSub
  bool passthrough;             // No special packetizer available.

  uint32_t min_cache, max_cache, max_blockadd_id;
  bool lacing_flag;
  uint64_t default_duration;

  // Parameters for video tracks
  uint64_t v_width, v_height, v_dwidth, v_dheight;
  unsigned int v_display_unit;
  uint64_t v_pcleft, v_pctop, v_pcright, v_pcbottom;
  stereo_mode_c::mode v_stereo_mode;
  float v_frate;
  char v_fourcc[5];
  bool v_bframes;

  // Parameters for audio tracks
  uint64_t a_channels, a_bps, a_formattag;
  float a_sfreq, a_osfreq;

  void *private_data;
  unsigned int private_size;

  unsigned char *headers[3];
  uint32_t header_sizes[3];

  boost::logic::tribool default_track, forced_track;
  std::string language;

  int64_t units_processed;

  std::string track_name;

  bool ok;

  int64_t previous_timecode;

  content_decoder_c content_decoder;

  KaxTags *tags;

  int ptzr;
  generic_packetizer_c *ptzr_ptr;
  bool headers_set;

  bool ignore_duration_hack;

  std::vector<memory_cptr> first_frames_data;

  kax_track_t()
    : tnum(0)
    , tuid(0)
    , ms_compat(false)
    , type(' ')
    , sub_type(' ')
    , passthrough(false)
    , min_cache(0)
    , max_cache(0)
    , max_blockadd_id(0)
    , lacing_flag(true)
    , default_duration(0)
    , v_width(0)
    , v_height(0)
    , v_dwidth(0)
    , v_dheight(0)
    , v_display_unit(0)
    , v_pcleft(0)
    , v_pctop(0)
    , v_pcright(0)
    , v_pcbottom(0)
    , v_stereo_mode(stereo_mode_c::unspecified)
    , v_frate(0.0)
    , v_bframes(false)
    , a_channels(0)
    , a_bps(0)
    , a_formattag(0)
    , a_sfreq(8000.0)
    , a_osfreq(0.0)
    , private_data(NULL)
    , private_size(0)
    , default_track(true)
    , forced_track(boost::logic::indeterminate)
    , language("eng")
    , units_processed(0)
    , ok(false)
    , previous_timecode(0)
    , tags(NULL)
    , ptzr(-1)
    , ptzr_ptr(NULL)
    , headers_set(false)
    , ignore_duration_hack(false)
  {
    memset(v_fourcc, 0, 5);
    memset(headers, 0, 3 * sizeof(unsigned char *));
    memset(header_sizes, 0, 3 * sizeof(uint32_t));
  }

  ~kax_track_t() {
    safefree(private_data);
    if (NULL != tags)
      delete tags;
  }

  void handle_packetizer_display_dimensions();
  void handle_packetizer_pixel_cropping();
  void handle_packetizer_stereo_mode();
  void handle_packetizer_pixel_dimensions();
  void handle_packetizer_default_duration();
  void fix_display_dimension_parameters();
};
typedef counted_ptr<kax_track_t> kax_track_cptr;

class kax_reader_c: public generic_reader_c {
private:
  enum deferred_l1_type_e {
    dl1t_unknown,
    dl1t_attachments,
    dl1t_chapters,
    dl1t_tags,
    dl1t_tracks,
  };

  std::vector<kax_track_cptr> m_tracks;
  std::map<generic_packetizer_c *, kax_track_t *> m_ptzr_to_track_map;

  int64_t m_tc_scale;

  mm_io_cptr m_in;
  kax_file_cptr m_in_file;
  int64_t m_file_size;

  counted_ptr<EbmlStream> m_es;

  int64_t m_segment_duration, m_last_timecode, m_first_timecode;
  std::string m_title;

  typedef std::map<deferred_l1_type_e, std::vector<int64_t> > deferred_positions_t;
  deferred_positions_t m_deferred_l1_positions, m_handled_l1_positions;

  std::string m_writing_app, m_muxing_app;
  int64_t m_writing_app_ver;

  int64_t m_attachment_id;

  counted_ptr<KaxTags> m_tags;

  file_status_e m_file_status;

public:
  kax_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~kax_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("Matroska") : "Matroska";
  }

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);

  virtual int get_progress();
  virtual void set_headers();
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *io, uint64_t size);

protected:
  virtual void set_track_packetizer(kax_track_t *t, generic_packetizer_c *ptzr);
  virtual void init_passthrough_packetizer(kax_track_t *t);
  virtual void set_packetizer_headers(kax_track_t *t);
  virtual void read_first_frames(kax_track_t *t, unsigned num_wanted = 1);
  virtual kax_track_t *find_track_by_num(uint64_t num, kax_track_t *c = NULL);
  virtual kax_track_t *find_track_by_uid(uint64_t uid, kax_track_t *c = NULL);

  virtual bool verify_acm_audio_track(kax_track_t *t);
  virtual bool verify_flac_audio_track(kax_track_t *t);
  virtual bool verify_vorbis_audio_track(kax_track_t *t);
  virtual void verify_audio_track(kax_track_t *t);
  virtual bool verify_mscomp_video_track(kax_track_t *t);
  virtual bool verify_theora_video_track(kax_track_t *t);
  virtual void verify_video_track(kax_track_t *t);
  virtual bool verify_kate_subtitle_track(kax_track_t *t);
  virtual bool verify_vobsub_subtitle_track(kax_track_t *t);
  virtual void verify_subtitle_track(kax_track_t *t);
  virtual void verify_button_track(kax_track_t *t);
  virtual void verify_tracks();

  virtual int packets_available();
  virtual void handle_attachments(mm_io_c *io, EbmlElement *l0, int64_t pos);
  virtual void handle_chapters(mm_io_c *io, EbmlElement *l0, int64_t pos);
  virtual void handle_tags(mm_io_c *io, EbmlElement *l0, int64_t pos);
  virtual void process_global_tags();

  virtual bool unlace_vorbis_private_data(kax_track_t *t, unsigned char *buffer, int size);

  virtual void create_video_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_audio_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_subtitle_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_button_packetizer(kax_track_t *t, track_info_c &nti);

  virtual void create_aac_audio_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_ac3_audio_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_dts_audio_packetizer(kax_track_t *t, track_info_c &nti);
#if defined(HAVE_FLAC_FORMAT_H)
  virtual void create_flac_audio_packetizer(kax_track_t *t, track_info_c &nti);
#endif
  virtual void create_mp3_audio_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_pcm_audio_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_tta_audio_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_vorbis_audio_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_wavpack_audio_packetizer(kax_track_t *t, track_info_c &nti);

  virtual void create_vc1_video_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_mpeg4_p10_video_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_mpeg4_p10_es_video_packetizer(kax_track_t *t, track_info_c &nti);

  virtual void read_headers_info(EbmlElement *&l1, EbmlElement *&l2, int &upper_lvl_el);
  virtual void read_headers_info_writing_app(KaxWritingApp *&kwriting_app);
  virtual void read_headers_track_audio(kax_track_t *track, KaxTrackAudio *ktaudio);
  virtual void read_headers_track_video(kax_track_t *track, KaxTrackVideo *ktvideo);
  virtual void read_headers_tracks(mm_io_c *io, EbmlElement *l0, int64_t position);
  virtual void read_headers_seek_head(EbmlElement *l0, EbmlElement *l1);
  virtual bool read_headers();

  virtual void process_simple_block(KaxCluster *cluster, KaxSimpleBlock *block_simple);
  virtual void process_block_group(KaxCluster *cluster, KaxBlockGroup *block_group);

  virtual mpeg4::p10::avc_es_parser_cptr parse_first_mpeg4_p10_frame(kax_track_t *t, track_info_c &nti);

  void init_l1_position_storage(deferred_positions_t &storage);
  virtual bool has_deferred_element_been_processed(deferred_l1_type_e type, int64_t position);
};

#endif  // __R_MATROSKA_H
