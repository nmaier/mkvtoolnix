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

#include "common/os.h"

#include <stdio.h>

#include <map>
#include <vector>

#include "common/common.h"
#include "common/compression.h"
#include "common/error.h"
#include "common/mm_io.h"
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
  uint64_t v_pcleft, v_pctop, v_pcright, v_pcbottom;
  stereo_mode_e v_stereo_mode;
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

  bool default_track;
  boost::logic::tribool forced_track;
  std::string language;

  int64_t units_processed;

  std::string track_name;

  bool ok;

  int64_t previous_timecode;

  content_decoder_c content_decoder;

  KaxTags *tags;

  int ptzr;
  bool headers_set;

  bool ignore_duration_hack;

  memory_cptr first_frame_data;

  kax_track_t()
    : tnum(0)
    , tuid(0)
    , ms_compat(false)
    , type(' ')
    , sub_type(' ')
    , passthrough(false)
    , min_cache(0)
    , max_cache(0)
    , lacing_flag(false)
    , default_duration(0)
    , v_width(0)
    , v_height(0)
    , v_dwidth(0)
    , v_dheight(0)
    , v_pcleft(0)
    , v_pctop(0)
    , v_pcright(0)
    , v_pcbottom(0)
    , v_stereo_mode(STEREO_MODE_UNSPECIFIED)
    , v_frate(0.0)
    , v_bframes(false)
    , a_channels(0)
    , a_bps(0)
    , a_formattag(0)
    , a_sfreq(0.0)
    , a_osfreq(0.0)
    , private_data(NULL)
    , private_size(0)
    , default_track(false)
    , forced_track(boost::logic::indeterminate)
    , language("eng")
    , units_processed(0)
    , ok(false)
    , previous_timecode(0)
    , tags(NULL)
    , ptzr(0)
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
};

class kax_reader_c: public generic_reader_c {
private:
  int act_wchar;

  std::vector<kax_track_t *> tracks;
  std::map<generic_packetizer_c *, kax_track_t *> ptzr_to_track_map;

  int64_t tc_scale;
  int64_t cluster_tc;

  mm_io_c *in;
  int64_t file_size;

  EbmlStream *es;
  EbmlElement *saved_l1, *saved_l2, *segment;
  KaxCluster *cluster;

  int64_t segment_duration;
  int64_t last_timecode, first_timecode;
  std::string title;

  std::vector<int64_t> handled_tags, handled_attachments, handled_chapters;

  std::string writing_app, muxing_app;
  int64_t writing_app_ver;

  int64_t m_attachment_id;

public:
  kax_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~kax_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);

  virtual int get_progress();
  virtual void set_headers();
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *io, int64_t size);

protected:
  virtual void init_passthrough_packetizer(kax_track_t *t);
  virtual void set_packetizer_headers(kax_track_t *t);
  virtual void read_first_frame(kax_track_t *t);
  virtual kax_track_t *new_kax_track();
  virtual kax_track_t *find_track_by_num(uint64_t num, kax_track_t *c = NULL);
  virtual kax_track_t *find_track_by_uid(uint64_t uid, kax_track_t *c = NULL);
  virtual void verify_tracks();
  virtual int packets_available();
  virtual void handle_attachments(mm_io_c *io, EbmlElement *l0, int64_t pos);
  virtual void handle_chapters(mm_io_c *io, EbmlElement *l0, int64_t pos);
  virtual void handle_tags(mm_io_c *io, EbmlElement *l0, int64_t pos);

  virtual void create_video_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_audio_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_subtitle_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_button_packetizer(kax_track_t *t, track_info_c &nti);
  virtual void create_mpeg4_p10_es_video_packetizer(kax_track_t *t, track_info_c &nti);

  virtual void read_headers_info(EbmlElement *&l1, EbmlElement *&l2, int &upper_lvl_el);
  virtual void read_headers_info_writing_app(KaxWritingApp *&kwriting_app);
  virtual void read_headers_track_audio(kax_track_t *&track, KaxTrackAudio *&ktaudio);
  virtual void read_headers_track_video(kax_track_t *&track, KaxTrackVideo *&ktvideo);
  virtual void read_headers_tracks(EbmlElement *&l1, EbmlElement *&l2, int &upper_lvl_el);
  virtual void read_headers_seek_head(EbmlElement *&l0, EbmlElement *&l1,
                                      std::vector<int64_t> &deferred_tags, std::vector<int64_t> &deferred_chapters, std::vector<int64_t> &deferred_attachments);
  virtual int  read_headers();
};

#endif  // __R_MATROSKA_H
