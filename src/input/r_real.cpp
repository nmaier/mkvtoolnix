/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   RealMedia demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <matroska/KaxTrackVideo.h>

#include "bit_cursor.h"
#include "common.h"
#include "error.h"
#include "p_aac.h"
#include "p_ac3.h"
#include "p_passthrough.h"
#include "p_realaudio.h"
#include "p_video.h"
#include "output_control.h"
#include "r_real.h"

#define PFX "real_reader: "

/*
   Description of the RealMedia file format:
   http://www.pcisys.net/~melanson/codecs/rmff.htm
*/

extern "C" {

static void *
mm_io_file_open(const char *path,
                int mode) {
  try {
    open_mode omode;

    if (MB_OPEN_MODE_READING == mode)
      omode = MODE_READ;
    else
      omode = MODE_CREATE;

    return new mm_file_io_c(path, omode);
  } catch(...) {
    return NULL;
  }
}

static int
mm_io_file_close(void *file) {
  if (NULL != file)
    delete static_cast<mm_file_io_c *>(file);
  return 0;
}

static int64_t
mm_io_file_tell(void *file) {
  if (NULL != file)
    return static_cast<mm_file_io_c *>(file)->getFilePointer();
  return -1;
}

static int64_t
mm_io_file_seek(void *file,
                int64_t offset,
                int whence) {
  seek_mode smode;

  if (NULL == file)
    return -1;
  if (SEEK_END == whence)
    smode = seek_end;
  else if (SEEK_CUR == whence)
    smode = seek_current;
  else
    smode = seek_beginning;
  if (!static_cast<mm_file_io_c *>(file)->setFilePointer2(offset, smode))
    return -1;
  return 0;
}

static int64_t
mm_io_file_read(void *file,
                void *buffer,
                int64_t bytes) {
  if (NULL == file)
    return -1;
  return static_cast<mm_file_io_c *>(file)->read(buffer, bytes);
}

static int64_t
mm_io_file_write(void *file,
                 const void *buffer,
                 int64_t bytes) {
  if (NULL == file)
    return -1;
  return static_cast<mm_file_io_c *>(file)->write(buffer, bytes);
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
real_reader_c::probe_file(mm_io_c *io,
                          int64_t size) {
  unsigned char data[4];

  if (size < 4)
    return 0;

  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(data, 4) != 4)
      return 0;
    io->setFilePointer(0, seek_beginning);

  } catch (...) {
    return 0;
  }

  if(strncasecmp((char *)data, ".RMF", 4))
    return 0;

  return 1;
}

real_reader_c::real_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {

  file = rmff_open_file_with_io(ti.fname.c_str(), RMFF_OPEN_MODE_READING,
                                &mm_io_file_io);
  if (file == NULL) {
    if (rmff_last_error == RMFF_ERR_NOT_RMFF)
      throw error_c(PFX "Source is not a valid RealMedia file.");
    else
      throw error_c(PFX "Could not read the source file.");
  }
  file->io->seek(file->handle, 0, SEEK_END);
  file_size = file->io->tell(file->handle);
  file->io->seek(file->handle, 0, SEEK_SET);

  done = false;

  if (verbose)
    mxinfo(FMT_FN "Using the RealMedia demultiplexer.\n", ti.fname.c_str());

  parse_headers();
  get_information_from_data();
}

real_reader_c::~real_reader_c() {
  real_demuxer_t *demuxer;
  int i, j;

  for (i = 0; i < demuxers.size(); i++) {
    demuxer = demuxers[i];
    if (demuxer->segments != NULL) {
      for (j = 0; j < demuxer->segments->size(); j++)
        safefree((*demuxer->segments)[j].data);
      delete demuxer->segments;
    }
    safefree(demuxer->private_data);
    safefree(demuxer->extra_data);
    safefree(demuxer);
  }
  demuxers.clear();
  ti.private_data = NULL;
  rmff_close_file(file);
}

void
real_reader_c::parse_headers() {
  uint32_t ts_size, ndx, i;
  unsigned char *ts_data;
  real_demuxer_t *dmx;
  rmff_track_t *track;

  if (rmff_read_headers(file) == RMFF_ERR_OK) {
    for (ndx = 0; ndx < file->num_tracks; ndx++) {
      track = file->tracks[ndx];
      if ((track->type == RMFF_TRACK_TYPE_UNKNOWN) ||
          (get_uint32_be(&track->mdpr_header.type_specific_size) == 0))
        continue;
      if ((track->type == RMFF_TRACK_TYPE_VIDEO) &&
          !demuxing_requested('v', track->id))
        continue;
      if ((track->type == RMFF_TRACK_TYPE_AUDIO) &&
          !demuxing_requested('a', track->id))
        continue;
      if ((track->mdpr_header.mime_type == NULL) ||
          (strcmp(track->mdpr_header.mime_type, "audio/x-pn-realaudio") &&
           strcmp(track->mdpr_header.mime_type, "video/x-pn-realvideo")))
        continue;

      ts_data = track->mdpr_header.type_specific_data;
      ts_size = get_uint32_be(&track->mdpr_header.type_specific_size);

      dmx = (real_demuxer_t *)safemalloc(sizeof(real_demuxer_t));
      memset(dmx, 0, sizeof(real_demuxer_t));
      dmx->track = track;
      dmx->ptzr = -1;

      if (track->type == RMFF_TRACK_TYPE_VIDEO) {
        dmx->rvp = (real_video_props_t *)track->mdpr_header.type_specific_data;

        memcpy(dmx->fourcc, &dmx->rvp->fourcc2, 4);
        dmx->fourcc[4] = 0;
        dmx->width = get_uint16_be(&dmx->rvp->width);
        dmx->height = get_uint16_be(&dmx->rvp->height);
        i = get_uint32_be(&dmx->rvp->fps);
        dmx->fps = (float)((i & 0xffff0000) >> 16) +
          ((float)(i & 0x0000ffff)) / 65536.0;
        dmx->private_data = (unsigned char *)safememdup(ts_data, ts_size);
        dmx->private_size = ts_size;

        demuxers.push_back(dmx);

      } else if (track->type == RMFF_TRACK_TYPE_AUDIO) {
        bool ok;

        ok = true;

        dmx->ra4p = (real_audio_v4_props_t *)
          track->mdpr_header.type_specific_data;
        dmx->ra5p = (real_audio_v5_props_t *)
          track->mdpr_header.type_specific_data;

        if (get_uint16_be(&dmx->ra4p->version1) == 4) {
          int slen;
          unsigned char *p;

          dmx->samples_per_second = get_uint16_be(&dmx->ra4p->sample_rate);
          dmx->channels = get_uint16_be(&dmx->ra4p->channels);
          dmx->bits_per_sample = get_uint16_be(&dmx->ra4p->sample_size);

          p = (unsigned char *)(dmx->ra4p + 1);
          slen = p[0];
          p += (slen + 1);
          slen = p[0];
          p++;
          if (slen != 4) {
            mxwarn(PFX "Couldn't find RealAudio"
                   " FourCC for id %u (description length: %d) Skipping "
                   "track.\n", track->id, slen);
            ok = false;
          } else {
            memcpy(dmx->fourcc, p, 4);
            dmx->fourcc[4] = 0;
            p += 4;
            if (ts_size > (p - ts_data)) {
              dmx->extra_data_size = ts_size - (p - ts_data);
              dmx->extra_data =
                (unsigned char *)safememdup(p, dmx->extra_data_size);
            }
          }

        } else {

          dmx->samples_per_second = get_uint16_be(&dmx->ra5p->sample_rate);
          dmx->channels = get_uint16_be(&dmx->ra5p->channels);
          dmx->bits_per_sample = get_uint16_be(&dmx->ra5p->sample_size);

          memcpy(dmx->fourcc, &dmx->ra5p->fourcc3, 4);
          dmx->fourcc[4] = 0;
          if (ts_size > (sizeof(real_audio_v5_props_t) + 4)) {
            dmx->extra_data_size = ts_size - 4 - sizeof(real_audio_v5_props_t);
            dmx->extra_data =
              (unsigned char *)safememdup((unsigned char *)dmx->ra5p + 4 +
                                          sizeof(real_audio_v5_props_t),
                                          dmx->extra_data_size);
          }
        }
        mxverb(2, PFX "extra_data_size: %d\n", dmx->extra_data_size);

        if (ok) {
          dmx->private_data = (unsigned char *)safememdup(ts_data, ts_size);
          dmx->private_size = ts_size;

          demuxers.push_back(dmx);
        } else
          free(dmx);
      }
    }
  }
}

void
real_reader_c::create_packetizer(int64_t tid) {
  int i;
  real_demuxer_t *dmx;
  rmff_track_t *track;

  dmx = find_demuxer(tid);
  if (dmx == NULL)
    return;

  if (dmx->ptzr == -1) {
    track = dmx->track;
    ti.id = track->id;
    ti.private_data = dmx->private_data;
    ti.private_size = dmx->private_size;

    if (track->type == RMFF_TRACK_TYPE_VIDEO) {
      char buffer[20];

      mxprints(buffer, "V_REAL/%s", dmx->fourcc);
      dmx->ptzr =
        add_packetizer(new video_packetizer_c(this, buffer, dmx->fps,
                                              dmx->width, dmx->height, ti));
      if ((dmx->fourcc[0] != 'R') || (dmx->fourcc[1] != 'V') ||
          (dmx->fourcc[2] != '4') || (dmx->fourcc[3] != '0'))
        dmx->rv_dimensions = true;

      mxinfo(FMT_TID "Using the video output module (FourCC: %s).\n",
             ti.fname.c_str(), (int64_t)track->id, dmx->fourcc);

    } else {
      ra_packetizer_c *ptzr;

      if (!strncmp(dmx->fourcc, "dnet", 4)) {
        dmx->ptzr =
          add_packetizer(new ac3_bs_packetizer_c(this, dmx->samples_per_second,
                                                 dmx->channels, dmx->bsid,
                                                 ti));
        mxinfo(FMT_TID "Using the AC3 output module (FourCC: %s).\n",
               ti.fname.c_str(), (int64_t)track->id, dmx->fourcc);

      } else if (!strcasecmp(dmx->fourcc, "raac") ||
                 !strcasecmp(dmx->fourcc, "racp")) {
        int profile, channels, sample_rate, output_sample_rate;
        uint32_t extra_len;
        bool sbr, extra_data_parsed;

        profile = -1;
        output_sample_rate = 0;
        sbr = false;
        extra_data_parsed = false;
        if (dmx->extra_data_size > 4) {
          extra_len = get_uint32_be(dmx->extra_data);
          mxverb(2, PFX "extra_len: %u\n", extra_len);
          if (dmx->extra_data_size >= (4 + extra_len)) {
            extra_data_parsed = true;
            parse_aac_data(&dmx->extra_data[4 + 1], extra_len - 1,
                           profile, channels, sample_rate, output_sample_rate,
                           sbr);
            mxverb(2, PFX "1. profile: %d, channels: %d, "
                   "sample_rate: %d, output_sample_rate: %d, sbr: %d\n",
                   profile, channels, sample_rate, output_sample_rate,
                   (int)sbr);
            if (sbr)
              profile = AAC_PROFILE_SBR;
          }
        }
        if (profile == -1) {
          channels = dmx->channels;
          sample_rate = dmx->samples_per_second;
          if (!strcasecmp(dmx->fourcc, "racp") || (sample_rate < 44100)) {
            output_sample_rate = 2 * sample_rate;
            sbr = true;
          }
        } else {
          dmx->channels = channels;
          dmx->samples_per_second = sample_rate;
        }
        if (sbr)
          profile = AAC_PROFILE_SBR;
        for (i = 0; i < (int)ti.aac_is_sbr.size(); i++)
          if ((ti.aac_is_sbr[i] == -1) || (ti.aac_is_sbr[i] == track->id)) {
            profile = AAC_PROFILE_SBR;
            break;
          }
        mxverb(2, PFX "2. profile: %d, channels: %d, sample_rate: "
               "%d, output_sample_rate: %d, sbr: %d\n", profile, channels,
               sample_rate, output_sample_rate, (int)sbr);
        ti.private_data = NULL;
        ti.private_size = 0;
        dmx->is_aac = true;
        dmx->ptzr =
          add_packetizer(new aac_packetizer_c(this, AAC_ID_MPEG4, profile,
                                              sample_rate, channels, ti, false,
                                              true));
        mxinfo(FMT_TID "Using the AAC output module (FourCC: %s).\n",
               ti.fname.c_str(), (int64_t)track->id, dmx->fourcc);
        if (profile == AAC_PROFILE_SBR)
          PTZR(dmx->ptzr)->set_audio_output_sampling_freq(output_sample_rate);
        else if (!extra_data_parsed)
          mxwarn("RealMedia files may contain HE-AAC / AAC+ / SBR AAC audio. "
                 "In some cases this can NOT be detected automatically. "
                 "Therefore you have to specifiy '--aac-is-sbr %u' manually "
                 "for this input file if the file actually contains SBR AAC. "
                 "The file will be muxed in the WRONG way otherwise. Also "
                 "read mkvmerge's documentation.\n", track->id);

        // AAC packetizers might need the timecode of the first packet in order
        // to fill in stuff. Let's misuse ref_timecode for that.
        dmx->ref_timecode = -1;

      } else {
        ptzr = new ra_packetizer_c(this, dmx->samples_per_second,
                                   dmx->channels, dmx->bits_per_sample,
                                   (dmx->fourcc[0] << 24) |
                                   (dmx->fourcc[1] << 16) |
                                   (dmx->fourcc[2] << 8) | dmx->fourcc[3],
                                   dmx->private_data, dmx->private_size,
                                   ti);
        dmx->ptzr = add_packetizer(ptzr);
        dmx->segments = new vector<rv_segment_t>;

        mxinfo(FMT_TID "Using the RealAudio output module (FourCC: %s).\n",
               ti.fname.c_str(), (int64_t)track->id, dmx->fourcc);
      }
    }
  }
}

void
real_reader_c::create_packetizers() {
  uint32_t i;

  for (i = 0; i < demuxers.size(); i++)
    create_packetizer(demuxers[i]->track->id);
}

real_demuxer_t *
real_reader_c::find_demuxer(int id) {
  int i;

  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->track->id == id)
      return demuxers[i];

  return NULL;
}

file_status_e
real_reader_c::finish() {
  int i;
  int64_t dur;
  real_demuxer_t *dmx;

  for (i = 0; i < demuxers.size(); i++) {
    dmx = demuxers[i];
    if ((dmx->track->type == RMFF_TRACK_TYPE_AUDIO) &&
        (dmx->segments != NULL)) {
      dur = dmx->last_timecode / dmx->num_packets;
      deliver_audio_frames(dmx, dur);
    }
  }

  done = true;

  flush_packetizers();

  return FILE_STATUS_DONE;
}

file_status_e
real_reader_c::read(generic_packetizer_c *,
                    bool) {
  int size;
  unsigned char *chunk;
  real_demuxer_t *dmx;
  int64_t timecode;
  rmff_frame_t *frame;

  if (done)
    return FILE_STATUS_DONE;

  size = rmff_get_next_frame_size(file);
  if (size <= 0) {
    if (file->num_packets_read < file->num_packets_in_chunk)
      mxwarn(PFX "%s: File contains fewer frames than expected or is "
             "corrupt after frame %u.\n", ti.fname.c_str(),
             file->num_packets_read);
    return finish();
  }

  chunk = (unsigned char *)safemalloc(size);
  memory_c mem(chunk, size, true);
  frame = rmff_read_next_frame(file, chunk);
  if (frame == NULL) {
    if (file->num_packets_read < file->num_packets_in_chunk)
      mxwarn(PFX "%s: File contains fewer frames than expected or is "
             "corrupt after frame %u.\n", ti.fname.c_str(),
             file->num_packets_read);
    return finish();
  }

  timecode = (int64_t)frame->timecode * 1000000ll;
  dmx = find_demuxer(frame->id);
  if (dmx == NULL) {
    rmff_release_frame(frame);
    return FILE_STATUS_MOREDATA;
  }

  if (dmx->track->type == RMFF_TRACK_TYPE_VIDEO)
    assemble_video_packet(dmx, frame);

  else if (dmx->is_aac) {
    // If the first AAC packet does not start at 0 then let the AAC
    // packetizer adjust its data accordingly.
    if (dmx->ref_timecode == -1) {
      dmx->ref_timecode = timecode;
      PTZR(dmx->ptzr)->set_displacement_maybe(timecode);
    }

    deliver_aac_frames(dmx, mem);

  } else
    queue_audio_frames(dmx, mem, timecode, frame->flags);

  rmff_release_frame(frame);

  return FILE_STATUS_MOREDATA;
}

void
real_reader_c::queue_one_audio_frame(real_demuxer_t *dmx,
                                     memory_c &mem,
                                     uint64_t timecode,
                                     uint32_t flags) {
  rv_segment_t segment;

  segment.data = mem.data;
  segment.size = mem.size;
  mem.lock();
  segment.flags = flags;
  dmx->segments->push_back(segment);
  dmx->last_timecode = timecode;
  mxverb(2, "enqueueing one for %u/'%s' length %u timecode %llu flags "
         "0x%08x\n", dmx->track->id, ti.fname.c_str(), mem.size, timecode,
         flags);
}

void
real_reader_c::queue_audio_frames(real_demuxer_t *dmx,
                                  memory_c &mem,
                                  uint64_t timecode,
                                  uint32_t flags) {
  // Enqueue the packets if no packets are in the queue or if the current
  // packet's timecode is the same as the timecode of those before.
  if ((dmx->segments->size() == 0) ||
      (dmx->last_timecode == timecode)) {
    queue_one_audio_frame(dmx, mem, timecode, flags);
    return;
  }

  // This timecode is different. So let's push the packets out.
  deliver_audio_frames(dmx, (timecode - dmx->last_timecode) /
                       dmx->segments->size());
  // Enqueue this packet.
  queue_one_audio_frame(dmx, mem, timecode, flags);
}

void
real_reader_c::deliver_audio_frames(real_demuxer_t *dmx,
                                    uint64_t duration) {
  uint32_t i;
  rv_segment_t segment;

  if (dmx->segments->size() == 0)
    return;

  for (i = 0; i < dmx->segments->size(); i++) {
    segment = (*dmx->segments)[i];
    mxverb(2, "delivering audio for %u/'%s' length %llu timecode %llu flags "
           "0x%08x duration %llu\n", dmx->track->id, ti.fname.c_str(),
           segment.size, dmx->last_timecode, (uint32_t)segment.flags,
           duration);
    memory_c mem(segment.data, segment.size, true);
    PTZR(dmx->ptzr)->process(mem, dmx->last_timecode, duration,
                             (segment.flags & 2) == 2 ? -1 :
                             dmx->ref_timecode);
    if ((segment.flags & 2) == 2)
      dmx->ref_timecode = dmx->last_timecode;
  }
  dmx->num_packets += dmx->segments->size();
  dmx->segments->clear();
}

void
real_reader_c::deliver_aac_frames(real_demuxer_t *dmx,
                                  memory_c &mem) {
  uint32_t num_sub_packets, data_idx, i, sub_length, len_check;
  uint32_t length;
  unsigned char *chunk;

  chunk = mem.data;
  length = mem.size;
  if (length < 2) {
    mxwarn(PFX "Short AAC audio packet for track ID %u of "
           "'%s' (length: %u < 2)\n", dmx->track->id, ti.fname.c_str(),
           length);
    return;
  }
  num_sub_packets = chunk[1] >> 4;
  mxverb(2, PFX "num_sub_packets = %u\n", num_sub_packets);
  if (length < (2 + num_sub_packets * 2)) {
    mxwarn(PFX "Short AAC audio packet for track ID %u of "
           "'%s' (length: %u < %u)\n", dmx->track->id, ti.fname.c_str(),
           length, 2 + num_sub_packets * 2);
    return;
  }
  len_check = 2 + num_sub_packets * 2;
  for (i = 0; i < num_sub_packets; i++) {
    sub_length = get_uint16_be(&chunk[2 + i * 2]);
    mxverb(2, PFX "%u: length %u\n", i, sub_length);
    len_check += sub_length;
  }
  if (len_check != length) {
    mxwarn(PFX "Inconsistent AAC audio packet for track ID %u of "
           "'%s' (length: %u != len_check %u)\n", dmx->track->id,
           ti.fname.c_str(), length, len_check);
    return;
  }
  data_idx = 2 + num_sub_packets * 2;
  for (i = 0; i < num_sub_packets; i++) {
    sub_length = get_uint16_be(&chunk[2 + i * 2]);
    memory_c mem(&chunk[data_idx], sub_length, false);
    PTZR(dmx->ptzr)->process(mem);
    data_idx += sub_length;
  }
}

int
real_reader_c::get_progress() {
  return 100 * file->num_packets_read / file->num_packets_in_chunk;
}

void
real_reader_c::identify() {
  int i;
  real_demuxer_t *demuxer;

  mxinfo("File '%s': container: RealMedia\n", ti.fname.c_str());
  for (i = 0; i < demuxers.size(); i++) {
    demuxer = demuxers[i];
    if (!strcasecmp(demuxer->fourcc, "raac") ||
        !strcasecmp(demuxer->fourcc, "racp"))
      mxinfo("Track ID %d: audio (AAC)\n", demuxer->track->id);
    else
      mxinfo("Track ID %d: %s (%s)\n", demuxer->track->id,
             demuxer->track->type == RMFF_TRACK_TYPE_AUDIO ? "audio" : "video",
             demuxer->fourcc);
  }
}

void
real_reader_c::assemble_video_packet(real_demuxer_t *dmx,
                                     rmff_frame_t *frame) {
  int result;
  rmff_frame_t *assembled;

  result = rmff_assemble_packed_video_frame(dmx->track, frame);
  if (result < 0) {
    mxwarn(PFX "Video packet assembly failed. Error code: %d (%s)\n",
           rmff_last_error, rmff_last_error_msg);
    return;
  }
  assembled = rmff_get_packed_video_frame(dmx->track);
  while (assembled != NULL) {
    memory_c mem(assembled->data, assembled->size, true);
    if (!dmx->rv_dimensions)
      set_dimensions(dmx, assembled->data, assembled->size);
    PTZR(dmx->ptzr)->process(mem, (int64_t)assembled->timecode * 1000000, -1,
                             (assembled->flags & RMFF_FRAME_FLAG_KEYFRAME) ==
                             RMFF_FRAME_FLAG_KEYFRAME ? VFT_IFRAME :
                             VFT_PFRAMEAUTOMATIC);
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
  const uint32_t cw[8] = {160, 176, 240, 320, 352, 640, 704, 0};
  const uint32_t ch1[8] = {120, 132, 144, 240, 288, 480, 0, 0};
  const uint32_t ch2[4] = {180, 360, 576, 0};
  uint32_t w, h, c, v;
  bit_cursor_c bc(buf, size);

  bc.skip_bits(13);
  bc.skip_bits(13);
  if (!bc.get_bits(3, v))
    return false;

  w = cw[v];
  if (w == 0) {
    do {
      if (!bc.get_bits(8, c))
        return false;
      w += (c << 2);
    } while (c == 255);
  }

  if (!bc.get_bits(3, c))
    return false;
  h = ch1[c];
  if (h == 0) {
    if (!bc.get_bits(1, v))
      return false;
    c = ((c << 1) | v) & 3;
    h = ch2[c];
    if (h == 0) {
      do {
        if (!bc.get_bits(8, c))
          return false;
        h += (c << 2);
      } while (c == 255);
    }
  }

  width = w;
  height = h;

  return true;
}

void
real_reader_c::set_dimensions(real_demuxer_t *dmx,
                              unsigned char *buffer,
                              int size) {
  uint32_t width, height, disp_width, disp_height;
  unsigned char *ptr;
  KaxTrackEntry *track_entry;

  ptr = buffer;
  ptr += 1 + 2 * 4 * (*ptr + 1);
  if ((ptr + 10) >= (buffer + size))
    return;
  buffer = ptr;

  if (get_rv_dimensions(buffer, size, width, height)) {
    if ((dmx->width != width) || (dmx->height != height)) {
      if (!ti.aspect_ratio_given && !ti.display_dimensions_given) {
        disp_width = dmx->width;
        disp_height = dmx->height;

        dmx->width = width;
        dmx->height = height;

      } else if (ti.display_dimensions_given) {
        disp_width = ti.display_width;
        disp_height = ti.display_height;

        dmx->width = width;
        dmx->height = height;

      } else {                  // ti.aspect_ratio_given == true
        dmx->width = width;
        dmx->height = height;

        if (ti.aspect_ratio > ((float)width / (float)height)) {
          disp_width = (uint32_t)(height * ti.aspect_ratio);
          disp_height = height;

        } else {
          disp_width = width;
          disp_height = (uint32_t)(width / ti.aspect_ratio);
        }

      }

      track_entry = PTZR(dmx->ptzr)->get_track_entry();
      KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);

      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelWidth>(video))) = width;
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelHeight>(video))) = height;

      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoDisplayWidth>(video))) = disp_width;
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoDisplayHeight>(video))) = disp_height;

      rerender_track_headers();
    }

    dmx->rv_dimensions = true;
  }
}

void
real_reader_c::get_information_from_data() {
  int i;
  real_demuxer_t *dmx;
  bool information_found;
  rmff_frame_t *frame;
  int64_t old_pos;

  old_pos = file->io->tell(file->handle);
  information_found = true;
  for (i = 0; i < demuxers.size(); i++) {
    dmx = demuxers[i];
    if (!strcasecmp(dmx->fourcc, "DNET")) {
      dmx->bsid = -1;
      information_found = false;
    }
  }

  while (!information_found) {
    frame = rmff_read_next_frame(file, NULL);
    dmx = find_demuxer(frame->id);

    if (dmx == NULL) {
      rmff_release_frame(frame);
      continue;
    }

    if (!strcasecmp(dmx->fourcc, "DNET"))
      dmx->bsid = frame->data[4] >> 3;

    rmff_release_frame(frame);

    information_found = true;
    for (i = 0; i < demuxers.size(); i++) {
      dmx = demuxers[i];
      if (!strcasecmp(dmx->fourcc, "DNET") && (dmx->bsid == -1))
        information_found = false;

    }
  }

  file->io->seek(file->handle, old_pos, SEEK_SET);
  file->num_packets_read = 0;
}

void
real_reader_c::flush_packetizers() {
  uint32_t i;

  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->ptzr != -1)
      PTZR(demuxers[i]->ptzr)->flush();
}

void
real_reader_c::add_available_track_ids() {
  int i;

  for (i = 0; i < demuxers.size(); i++)
    available_track_ids.push_back(demuxers[i]->track->id);
}
