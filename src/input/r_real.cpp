/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   RealMedia demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxTrackVideo.h>

#include "common/bit_cursor.h"
#include "common/ebml.h"
#include "common/endian.h"
#include "common/error.h"
#include "input/r_real.h"
#include "merge/output_control.h"
#include "output/p_aac.h"
#include "output/p_ac3.h"
#include "output/p_passthrough.h"
#include "output/p_realaudio.h"
#include "output/p_video.h"

/*
   Description of the RealMedia file format:
   http://www.pcisys.net/~melanson/codecs/rmff.htm
*/

extern "C" {

static void *
mm_io_file_open(const char *path,
                int) {
  try {
    return reinterpret_cast<mm_io_c *>(const_cast<char *>(path));
  } catch(...) {
    return NULL;
  }
}

static int
mm_io_file_close(void *) {
  return 0;
}

static int64_t
mm_io_file_tell(void *file) {
  return NULL != file ? static_cast<mm_io_c *>(file)->getFilePointer() : -1;
}

static int64_t
mm_io_file_seek(void *file,
                int64_t offset,
                int whence) {
  if (NULL == file)
    return -1;

  seek_mode smode = SEEK_END == whence ? seek_end
                  : SEEK_CUR == whence ? seek_current
                  :                      seek_beginning;
  return static_cast<mm_io_c *>(file)->setFilePointer2(offset, smode) ? 0 : -1;
}

static int64_t
mm_io_file_read(void *file,
                void *buffer,
                int64_t bytes) {
  return NULL == file ? -1 : static_cast<mm_io_c *>(file)->read(buffer, bytes);
}

static int64_t
mm_io_file_write(void *file,
                 const void *buffer,
                 int64_t bytes) {
  return NULL == file ? -1 : static_cast<mm_io_c *>(file)->write(buffer, bytes);
}

}

mb_file_io_t mm_io_file_io = {
  mm_io_file_open,
  mm_io_file_close,
  mm_io_file_read,
  mm_io_file_write,
  mm_io_file_tell,
  mm_io_file_seek
};

int
real_reader_c::probe_file(mm_io_c *in,
                          uint64_t size) {
  unsigned char data[4];

  if (4 > size)
    return 0;

  try {
    in->setFilePointer(0, seek_beginning);
    if (in->read(data, 4) != 4)
      return 0;
    in->setFilePointer(0, seek_beginning);

  } catch (...) {
    return 0;
  }

  if(strncasecmp((char *)data, ".RMF", 4))
    return 0;

  return 1;
}

real_reader_c::real_reader_c(const track_info_c &ti,
                             const mm_io_cptr &in)
  : generic_reader_c(ti, in)
{
}

void
real_reader_c::read_headers() {
  file = rmff_open_file_with_io(reinterpret_cast<const char *>(m_in.get_object()), RMFF_OPEN_MODE_READING, &mm_io_file_io);
  if (NULL == file) {
    if (RMFF_ERR_NOT_RMFF == rmff_last_error)
      throw mtx::input::invalid_format_x();
    else
      throw mtx::input::open_x();
  }
  m_in->setFilePointer(0, seek_beginning);

  done = false;

  show_demuxer_info();

  parse_headers();
  get_information_from_data();
}

real_reader_c::~real_reader_c() {
  size_t i;

  for (i = 0; i < demuxers.size(); i++) {
    real_demuxer_cptr &demuxer = demuxers[i];

    safefree(demuxer->private_data);
    safefree(demuxer->extra_data);
  }
  demuxers.clear();
  m_ti.m_private_data = NULL;
  rmff_close_file(file);
}

void
real_reader_c::parse_headers() {

  if (rmff_read_headers(file) != RMFF_ERR_OK)
    return;

  int ndx;
  for (ndx = 0; ndx < file->num_tracks; ndx++) {
    rmff_track_t *track = file->tracks[ndx];

    if ((RMFF_TRACK_TYPE_UNKNOWN == track->type) || (get_uint32_be(&track->mdpr_header.type_specific_size) == 0))
      continue;
    if ((RMFF_TRACK_TYPE_VIDEO == track->type) && !demuxing_requested('v', track->id))
      continue;
    if ((RMFF_TRACK_TYPE_AUDIO == track->type) && !demuxing_requested('a', track->id))
      continue;
    if ((NULL == track->mdpr_header.mime_type)
        ||
        (   strcmp(track->mdpr_header.mime_type, "audio/x-pn-realaudio")
         && strcmp(track->mdpr_header.mime_type, "video/x-pn-realvideo")))
      continue;

    unsigned char *ts_data = track->mdpr_header.type_specific_data;
    uint32_t ts_size       = get_uint32_be(&track->mdpr_header.type_specific_size);

    real_demuxer_cptr dmx(new real_demuxer_t(track));

    if (RMFF_TRACK_TYPE_VIDEO == track->type) {
      dmx->rvp = (real_video_props_t *)track->mdpr_header.type_specific_data;

      memcpy(dmx->fourcc, &dmx->rvp->fourcc2, 4);
      dmx->fourcc[4]    = 0;
      dmx->width        = get_uint16_be(&dmx->rvp->width);
      dmx->height       = get_uint16_be(&dmx->rvp->height);
      uint32_t i        = get_uint32_be(&dmx->rvp->fps);
      dmx->fps          = (float)((i & 0xffff0000) >> 16) + ((float)(i & 0x0000ffff)) / 65536.0;
      dmx->private_data = (unsigned char *)safememdup(ts_data, ts_size);
      dmx->private_size = ts_size;

      demuxers.push_back(dmx);

    } else if (RMFF_TRACK_TYPE_AUDIO == track->type) {
      bool ok     = true;

      dmx->ra4p   = (real_audio_v4_props_t *)track->mdpr_header.type_specific_data;
      dmx->ra5p   = (real_audio_v5_props_t *)track->mdpr_header.type_specific_data;

      int version = get_uint16_be(&dmx->ra4p->version1);

      if (3 == version) {
        dmx->samples_per_second = 8000;
        dmx->channels           = 1;
        dmx->bits_per_sample    = 16;
        dmx->extra_data_size    = 0;
        strcpy(dmx->fourcc, "14_4");

      } else if (4 == version) {
        dmx->samples_per_second  = get_uint16_be(&dmx->ra4p->sample_rate);
        dmx->channels            = get_uint16_be(&dmx->ra4p->channels);
        dmx->bits_per_sample     = get_uint16_be(&dmx->ra4p->sample_size);

        unsigned char *p         = (unsigned char *)(dmx->ra4p + 1);
        int slen                 = p[0];
        p                       += (slen + 1);
        slen                     = p[0];
        p++;

        if (4 != slen) {
          mxwarn(boost::format(Y("real_reader: Couldn't find RealAudio FourCC for id %1% (description length: %2%) Skipping track.\n")) % track->id % slen);
          ok = false;

        } else {
          memcpy(dmx->fourcc, p, 4);
          dmx->fourcc[4]  = 0;
          p              += 4;

          if (ts_size > static_cast<unsigned int>(p - ts_data)) {
            dmx->extra_data_size = ts_size - (p - ts_data);
            dmx->extra_data      = (unsigned char *)safememdup(p, dmx->extra_data_size);
          }
        }

      } else if (5 == version) {
        dmx->samples_per_second = get_uint16_be(&dmx->ra5p->sample_rate);
        dmx->channels           = get_uint16_be(&dmx->ra5p->channels);
        dmx->bits_per_sample    = get_uint16_be(&dmx->ra5p->sample_size);

        memcpy(dmx->fourcc, &dmx->ra5p->fourcc3, 4);
        dmx->fourcc[4] = 0;

        if ((sizeof(real_audio_v5_props_t) + 4) < ts_size) {
          dmx->extra_data_size = ts_size - 4 - sizeof(real_audio_v5_props_t);
          dmx->extra_data      = (unsigned char *)safememdup((unsigned char *)dmx->ra5p + 4 + sizeof(real_audio_v5_props_t), dmx->extra_data_size);
        }

      } else {
        mxwarn(boost::format(Y("real_reader: Only audio header versions 3, 4 and 5 are supported. Track ID %1% uses version %2% and will be skipped.\n"))
               % track->id % version);
        ok = false;
      }

      mxverb(2, boost::format("real_reader: extra_data_size: %1%\n") % dmx->extra_data_size);

      if (ok) {
        dmx->private_data = (unsigned char *)safememdup(ts_data, ts_size);
        dmx->private_size = ts_size;

        demuxers.push_back(dmx);
      }
    }
  }
}

void
real_reader_c::create_video_packetizer(real_demuxer_cptr dmx) {
  std::string codec_id = (boost::format("V_REAL/%1%") % dmx->fourcc).str();
  dmx->ptzr = add_packetizer(new video_packetizer_c(this, m_ti, codec_id.c_str(), 0.0, dmx->width, dmx->height));

  if (strcmp(dmx->fourcc, "RV40"))
    dmx->rv_dimensions = true;

  show_packetizer_info(dmx->track->id, PTZR(dmx->ptzr));
}

void
real_reader_c::create_dnet_audio_packetizer(real_demuxer_cptr dmx) {
  dmx->ptzr = add_packetizer(new ac3_bs_packetizer_c(this, m_ti, dmx->samples_per_second, dmx->channels, dmx->bsid));
  show_packetizer_info(dmx->track->id, PTZR(dmx->ptzr));
}

void
real_reader_c::create_aac_audio_packetizer(real_demuxer_cptr dmx) {
  int channels, sample_rate;
  int detected_profile;

  int64_t tid            = dmx->track->id;
  int profile            = -1;
  int output_sample_rate = 0;
  bool sbr               = false;
  bool extra_data_parsed = false;

  if (4 < dmx->extra_data_size) {
    uint32_t extra_len = get_uint32_be(dmx->extra_data);
    mxverb(2, boost::format("real_reader: extra_len: %1%\n") % extra_len);

    if ((4 + extra_len) <= dmx->extra_data_size) {
      extra_data_parsed = true;
      if (!parse_aac_data(&dmx->extra_data[4 + 1], extra_len - 1, profile, channels, sample_rate, output_sample_rate, sbr))
        mxerror_tid(m_ti.m_fname, tid, Y("This AAC track does not contain valid headers. Could not parse the AAC information.\n"));
      mxverb(2,
             boost::format("real_reader: 1. profile: %1%, channels: %2%, sample_rate: %3%, output_sample_rate: %4%, sbr: %5%\n")
             % profile % channels % sample_rate % output_sample_rate % sbr);
      if (sbr)
        profile = AAC_PROFILE_SBR;
    }
  }

  if (-1 == profile) {
    channels    = dmx->channels;
    sample_rate = dmx->samples_per_second;
    if (!strcasecmp(dmx->fourcc, "racp") || (44100 > sample_rate)) {
      output_sample_rate = 2 * sample_rate;
      sbr                = true;
    }

  } else {
    dmx->channels           = channels;
    dmx->samples_per_second = sample_rate;
  }

  detected_profile = profile;
  if (sbr)
    profile = AAC_PROFILE_SBR;

  if (   (map_has_key(m_ti.m_all_aac_is_sbr, tid) && m_ti.m_all_aac_is_sbr[tid])
      || (map_has_key(m_ti.m_all_aac_is_sbr, -1)  && m_ti.m_all_aac_is_sbr[-1]))
    profile = AAC_PROFILE_SBR;

  if ((-1 != detected_profile)
      &&
      (   (map_has_key(m_ti.m_all_aac_is_sbr, tid) && !m_ti.m_all_aac_is_sbr[tid])
       || (map_has_key(m_ti.m_all_aac_is_sbr, -1)  && !m_ti.m_all_aac_is_sbr[-1])))
    profile = detected_profile;

  mxverb(2,
         boost::format("real_reader: 2. profile: %1%, channels: %2%, sample_rate: %3%, output_sample_rate: %4%, sbr: %5%\n")
         % profile % channels % sample_rate % output_sample_rate % sbr);

  m_ti.m_private_data = NULL;
  m_ti.m_private_size = 0;
  dmx->is_aac         = true;
  dmx->ptzr           = add_packetizer(new aac_packetizer_c(this, m_ti, AAC_ID_MPEG4, profile, sample_rate, channels, false, true));

  show_packetizer_info(tid, PTZR(dmx->ptzr));

  if (AAC_PROFILE_SBR == profile)
    PTZR(dmx->ptzr)->set_audio_output_sampling_freq(output_sample_rate);

  else if (!extra_data_parsed)
    mxwarn(boost::format(Y("RealMedia files may contain HE-AAC / AAC+ / SBR AAC audio. In some cases this can NOT be detected automatically. "
                           "Therefore you have to specifiy '--aac-is-sbr %1%' manually for this input file if the file actually contains SBR AAC. "
                           "The file will be muxed in the WRONG way otherwise. Also read mkvmerge's documentation.\n")) % tid);

  // AAC packetizers might need the timecode of the first packet in order
  // to fill in stuff. Let's misuse ref_timecode for that.
  dmx->ref_timecode = -1;
}

void
real_reader_c::create_audio_packetizer(real_demuxer_cptr dmx) {
  if (!strncmp(dmx->fourcc, "dnet", 4))
    create_dnet_audio_packetizer(dmx);

  else if (!strcasecmp(dmx->fourcc, "raac") || !strcasecmp(dmx->fourcc, "racp"))
    create_aac_audio_packetizer(dmx);

  else {
    if (!strcasecmp(dmx->fourcc, "COOK"))
      dmx->cook_audio_fix = true;

    dmx->ptzr = add_packetizer(new ra_packetizer_c(this, m_ti, dmx->samples_per_second, dmx->channels, dmx->bits_per_sample, get_uint32_be(dmx->fourcc),
                                                   dmx->private_data, dmx->private_size));

    show_packetizer_info(dmx->track->id, PTZR(dmx->ptzr));
  }
}

void
real_reader_c::create_packetizer(int64_t tid) {

  real_demuxer_cptr dmx = find_demuxer(tid);
  if (!dmx.is_set())
    return;

  if (-1 != dmx->ptzr)
    return;

  rmff_track_t *track = dmx->track;
  m_ti.m_id           = track->id;
  m_ti.m_private_data = dmx->private_data;
  m_ti.m_private_size = dmx->private_size;

  if (RMFF_TRACK_TYPE_VIDEO == track->type)
    create_video_packetizer(dmx);
  else
    create_audio_packetizer(dmx);
}

void
real_reader_c::create_packetizers() {
  uint32_t i;

  for (i = 0; i < demuxers.size(); i++)
    create_packetizer(demuxers[i]->track->id);
}

real_demuxer_cptr
real_reader_c::find_demuxer(unsigned int id) {
  size_t i;

  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->track->id == id)
      return demuxers[i];

  return real_demuxer_cptr(NULL);
}

file_status_e
real_reader_c::finish() {
  size_t i;

  for (i = 0; i < demuxers.size(); i++) {
    real_demuxer_cptr dmx = demuxers[i];
    if (dmx.is_set() && (NULL != dmx->track) && (dmx->track->type == RMFF_TRACK_TYPE_AUDIO) && !dmx->segments.empty())
      deliver_audio_frames(dmx, dmx->last_timecode / dmx->num_packets);
  }

  done = true;

  return flush_packetizers();
}

file_status_e
real_reader_c::read(generic_packetizer_c *,
                    bool) {
  if (done)
    return flush_packetizers();

  int size = rmff_get_next_frame_size(file);
  if (0 >= size) {
    if (file->num_packets_read < file->num_packets_in_chunk)
      mxwarn_fn(m_ti.m_fname, boost::format(Y("File contains fewer frames than expected or is corrupt after frame %1%.\n")) % file->num_packets_read);
    return finish();
  }

  unsigned char *chunk = (unsigned char *)safemalloc(size);
  memory_c mem(chunk, size, true);
  rmff_frame_t *frame = rmff_read_next_frame(file, chunk);

  if (NULL == frame) {
    if (file->num_packets_read < file->num_packets_in_chunk)
      mxwarn_fn(m_ti.m_fname, boost::format(Y("File contains fewer frames than expected or is corrupt after frame %1%.\n")) % file->num_packets_read);
    return finish();
  }

  int64_t timecode      = (int64_t)frame->timecode * 1000000ll;
  real_demuxer_cptr dmx = find_demuxer(frame->id);

  if (!dmx.is_set() || (-1 == dmx->ptzr)) {
    rmff_release_frame(frame);
    return FILE_STATUS_MOREDATA;
  }

  if (dmx->cook_audio_fix && dmx->first_frame && ((frame->flags & RMFF_FRAME_FLAG_KEYFRAME) != RMFF_FRAME_FLAG_KEYFRAME))
    dmx->force_keyframe_flag = true;

  if (dmx->force_keyframe_flag && ((frame->flags & RMFF_FRAME_FLAG_KEYFRAME) == RMFF_FRAME_FLAG_KEYFRAME))
    dmx->force_keyframe_flag = false;

  if (dmx->force_keyframe_flag)
    frame->flags |= RMFF_FRAME_FLAG_KEYFRAME;

  if (RMFF_TRACK_TYPE_VIDEO == dmx->track->type)
    assemble_video_packet(dmx, frame);

  else if (dmx->is_aac) {
    // If the first AAC packet does not start at 0 then let the AAC
    // packetizer adjust its data accordingly.
    if (dmx->first_frame) {
      dmx->ref_timecode = timecode;
      PTZR(dmx->ptzr)->set_displacement_maybe(timecode);
    }

    deliver_aac_frames(dmx, mem);

  } else
    queue_audio_frames(dmx, mem, timecode, frame->flags);

  rmff_release_frame(frame);

  dmx->first_frame = false;

  return FILE_STATUS_MOREDATA;
}

void
real_reader_c::queue_one_audio_frame(real_demuxer_cptr dmx,
                                     memory_c &mem,
                                     uint64_t timecode,
                                     uint32_t flags) {
  rv_segment_cptr segment(new rv_segment_t);

  segment->data  = memory_cptr(new memory_c(mem));
  segment->flags = flags;
  dmx->segments.push_back(segment);

  dmx->last_timecode = timecode;

  mxverb_tid(2, m_ti.m_fname, dmx->track->id, boost::format("enqueueing one length %1% timecode %2% flags 0x%|3$08x|\n") % mem.get_size() % timecode % flags);
}

void
real_reader_c::queue_audio_frames(real_demuxer_cptr dmx,
                                  memory_c &mem,
                                  uint64_t timecode,
                                  uint32_t flags) {
  // Enqueue the packets if no packets are in the queue or if the current
  // packet's timecode is the same as the timecode of those before.
  if (dmx->segments.empty() || (dmx->last_timecode == timecode)) {
    queue_one_audio_frame(dmx, mem, timecode, flags);
    return;
  }

  // This timecode is different. So let's push the packets out.
  deliver_audio_frames(dmx, (timecode - dmx->last_timecode) / dmx->segments.size());

  // Enqueue this packet.
  queue_one_audio_frame(dmx, mem, timecode, flags);
}

void
real_reader_c::deliver_audio_frames(real_demuxer_cptr dmx,
                                    uint64_t duration) {
  uint32_t i;

  if (dmx->segments.empty() || (-1 == dmx->ptzr))
    return;

  for (i = 0; i < dmx->segments.size(); i++) {
    rv_segment_cptr segment = dmx->segments[i];
    mxverb_tid(2, m_ti.m_fname, dmx->track->id,
               boost::format("delivering audio length %1% timecode %2% flags 0x%|3$08x| duration %4%\n")
               % segment->data->get_size() % dmx->last_timecode % segment->flags % duration);

    PTZR(dmx->ptzr)->process(new packet_t(segment->data, dmx->last_timecode, duration,
                                          (segment->flags & RMFF_FRAME_FLAG_KEYFRAME) == RMFF_FRAME_FLAG_KEYFRAME ? -1 : dmx->ref_timecode));
    if ((segment->flags & 2) == 2)
      dmx->ref_timecode = dmx->last_timecode;
  }

  dmx->num_packets += dmx->segments.size();
  dmx->segments.clear();
}

void
real_reader_c::deliver_aac_frames(real_demuxer_cptr dmx,
                                  memory_c &mem) {
  unsigned char *chunk = mem.get_buffer();
  int length           = mem.get_size();
  if (2 > length) {
    mxwarn_tid(m_ti.m_fname, dmx->track->id, boost::format(Y("Short AAC audio packet (length: %1% < 2)\n")) % length);
    return;
  }

  int num_sub_packets = chunk[1] >> 4;
  mxverb(2, boost::format("real_reader: num_sub_packets = %1%\n") % num_sub_packets);
  if ((2 + num_sub_packets * 2) > length) {
    mxwarn_tid(m_ti.m_fname, dmx->track->id, boost::format(Y("Short AAC audio packet (length: %1% < %2%)\n")) % length % (2 + num_sub_packets * 2));
    return;
  }

  int i, len_check = 2 + num_sub_packets * 2;
  for (i = 0; i < num_sub_packets; i++) {
    int sub_length  = get_uint16_be(&chunk[2 + i * 2]);
    len_check      += sub_length;

    mxverb(2, boost::format("real_reader: %1%: length %2%\n") % i % sub_length);
  }

  if (len_check != length) {
    mxwarn_tid(m_ti.m_fname, dmx->track->id, boost::format(Y("Inconsistent AAC audio packet (length: %1% != len_check %2%)\n")) % length % len_check);
    return;
  }

  int data_idx = 2 + num_sub_packets * 2;
  for (i = 0; i < num_sub_packets; i++) {
    int sub_length = get_uint16_be(&chunk[2 + i * 2]);
    PTZR(dmx->ptzr)->process(new packet_t(new memory_c(&chunk[data_idx], sub_length, false)));
    data_idx += sub_length;
  }
}

int
real_reader_c::get_progress() {
  return 100 * file->num_packets_read / file->num_packets_in_chunk;
}

void
real_reader_c::identify() {
  id_result_container();

  size_t i;
  for (i = 0; i < demuxers.size(); i++) {
    real_demuxer_cptr demuxer = demuxers[i];

    std::string type, codec;
    if (!strcasecmp(demuxer->fourcc, "raac") || !strcasecmp(demuxer->fourcc, "racp")) {
      type  = ID_RESULT_TRACK_AUDIO;
      codec = "AAC";
    } else {
      type  = RMFF_TRACK_TYPE_AUDIO == demuxer->track->type ? ID_RESULT_TRACK_AUDIO : ID_RESULT_TRACK_VIDEO;
      codec = demuxer->fourcc;
    }

    id_result_track(demuxer->track->id, type, codec);
  }
}

void
real_reader_c::assemble_video_packet(real_demuxer_cptr dmx,
                                     rmff_frame_t *frame) {
  int result = rmff_assemble_packed_video_frame(dmx->track, frame);
  if (0 > result) {
    mxwarn_tid(m_ti.m_fname, dmx->track->id, boost::format(Y("Video packet assembly failed. Error code: %1% (%2%)\n")) % rmff_last_error % rmff_last_error_msg);
    return;
  }

  rmff_frame_t *assembled = rmff_get_packed_video_frame(dmx->track);
  while (NULL != assembled) {
    if (!dmx->rv_dimensions)
      set_dimensions(dmx, assembled->data, assembled->size);

    packet_t *packet = new packet_t(new memory_c(assembled->data, assembled->size, true), (int64_t)assembled->timecode * 1000000, 0,
                                    (assembled->flags & RMFF_FRAME_FLAG_KEYFRAME) == RMFF_FRAME_FLAG_KEYFRAME ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC, VFT_NOBFRAME);
    PTZR(dmx->ptzr)->process(packet);

    assembled->allocated_by_rmff = 0;
    rmff_release_frame(assembled);
    assembled = rmff_get_packed_video_frame(dmx->track);
  }
}

bool
real_reader_c::get_rv_dimensions(unsigned char *buf,
                                 int size,
                                 uint32_t &width,
                                 uint32_t &height) {
  static const uint32_t cw[8]  = { 160, 176, 240, 320, 352, 640, 704, 0 };
  static const uint32_t ch1[8] = { 120, 132, 144, 240, 288, 480,   0, 0 };
  static const uint32_t ch2[4] = { 180, 360, 576,   0 };
  bit_cursor_c bc(buf, size);

  try {
    bc.skip_bits(13);
    bc.skip_bits(13);
    int v = bc.get_bits(3);

    int w = cw[v];
    if (0 == w) {
      int c;
      do {
        c = bc.get_bits(8);
        w += (c << 2);
      } while (c == 255);
    }

    int c = bc.get_bits(3);
    int h = ch1[c];
    if (0 == h) {
      v = bc.get_bits(1);
      c = ((c << 1) | v) & 3;
      h = ch2[c];
      if (0 == h) {
        do {
          c  = bc.get_bits(8);
          h += (c << 2);
        } while (c == 255);
      }
    }

    width  = w;
    height = h;

    return true;

  } catch (...) {
    return false;
  }
}

void
real_reader_c::set_dimensions(real_demuxer_cptr dmx,
                              unsigned char *buffer,
                              int size) {
  unsigned char *ptr  = buffer;
  ptr                += 1 + 2 * 4 * (*ptr + 1);

  if ((ptr + 10) >= (buffer + size))
    return;

  buffer = ptr;

  uint32_t width, height;
  if (!get_rv_dimensions(buffer, size, width, height))
    return;

  if ((dmx->width != width) || (dmx->height != height)) {
    uint32_t disp_width  = 0;
    uint32_t disp_height = 0;

    if (!m_ti.display_dimensions_or_aspect_ratio_set()) {
      disp_width  = dmx->width;
      disp_height = dmx->height;

      dmx->width  = width;
      dmx->height = height;

    } else if (m_ti.m_display_dimensions_given) {
      disp_width  = m_ti.m_display_width;
      disp_height = m_ti.m_display_height;

      dmx->width  = width;
      dmx->height = height;

    } else if (m_ti.m_aspect_ratio_given) {
      dmx->width  = width;
      dmx->height = height;

      if (((float)width / (float)height) < m_ti.m_aspect_ratio) {
        disp_width  = (uint32_t)(height * m_ti.m_aspect_ratio);
        disp_height = height;

      } else {
        disp_width  = width;
        disp_height = (uint32_t)(width / m_ti.m_aspect_ratio);
      }

    }

    KaxTrackVideo &video                                     = GetChild<KaxTrackVideo>(*PTZR(dmx->ptzr)->get_track_entry());
    GetChildAs<KaxVideoPixelWidth,  EbmlUInteger>(video)     = width;
    GetChildAs<KaxVideoPixelHeight, EbmlUInteger>(video)     = height;

    if ((0 != disp_width) && (0 != disp_height)) {
      GetChildAs<KaxVideoDisplayWidth,  EbmlUInteger>(video) = disp_width;
      GetChildAs<KaxVideoDisplayHeight, EbmlUInteger>(video) = disp_height;
    }

    rerender_track_headers();
  }

  dmx->rv_dimensions = true;
}

void
real_reader_c::get_information_from_data() {
  int64_t old_pos        = m_in->getFilePointer();
  bool information_found = true;

  size_t i;
  for (i = 0; i < demuxers.size(); i++) {
    real_demuxer_cptr dmx = demuxers[i];
    if (!strcasecmp(dmx->fourcc, "DNET")) {
      dmx->bsid         = -1;
      information_found = false;
    }
  }

  while (!information_found) {
    rmff_frame_t *frame   = rmff_read_next_frame(file, NULL);
    real_demuxer_cptr dmx = find_demuxer(frame->id);

    if (!dmx.is_set()) {
      rmff_release_frame(frame);
      continue;
    }

    if (!strcasecmp(dmx->fourcc, "DNET"))
      dmx->bsid = frame->data[4] >> 3;

    rmff_release_frame(frame);

    information_found = true;
    for (i = 0; i < demuxers.size(); i++) {
      dmx = demuxers[i];
      if (!strcasecmp(dmx->fourcc, "DNET") && (-1 == dmx->bsid))
        information_found = false;

    }
  }

  m_in->setFilePointer(old_pos, seek_beginning);
  file->num_packets_read = 0;
}

void
real_reader_c::add_available_track_ids() {
  size_t i;

  for (i = 0; i < demuxers.size(); i++)
    add_available_track_id(demuxers[i]->track->id);
}
