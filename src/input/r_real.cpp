/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_real.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief RealMedia demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <matroska/KaxTrackVideo.h>

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "r_real.h"
#include "p_aac.h"
#include "p_ac3.h"
#include "p_passthrough.h"
#include "p_video.h"

#define PFX "real_reader: "

/*
 * Description of the RealMedia file format:
 * http://www.pcisys.net/~melanson/codecs/rmff.htm
 */

typedef struct {
  uint32_t chunks;              // number of chunks
  uint32_t timecode;            // timecode from packet header
  uint32_t len;                 // length of actual data
  uint32_t chunktab;            // offset to chunk offset array
} dp_hdr_t;

// }}}

// {{{ FUNCTION real_reader_c::probe_file(mm_io_c *io, int64_t size)

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

  } catch (exception &ex) {
    return 0;
  }

  if(strncasecmp((char *)data, ".RMF", 4))
    return 0;

  return 1;
}

// }}}

// {{{ C'TOR

real_reader_c::real_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {

  file = rmff_open_file(ti->fname, RMFF_OPEN_MODE_READING);
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
    mxinfo("Using RealMedia demultiplexer for %s.\n", ti->fname);

  parse_headers();
  get_information_from_data();
  if (!identifying)
    create_packetizers();
}

// }}}

// {{{ D'TOR

real_reader_c::~real_reader_c() {
  real_demuxer_t *demuxer;
  int i, j;

  for (i = 0; i < demuxers.size(); i++) {
    demuxer = demuxers[i];
    if (demuxer->packetizer != NULL)
      delete demuxer->packetizer;
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
  ti->private_data = NULL;
  rmff_close_file(file);
}

// }}}

// {{{ FUNCTION real_reader_c::parse_headers()

void
real_reader_c::parse_headers() {
  uint32_t ts_size, ndx, i;
  unsigned char *ts_data;
  real_demuxer_t *dmx;
  rmff_track_t *track;

  if (rmff_read_headers(file) == RMFF_ERR_OK) {
    for (ndx = 0; ndx < file->num_tracks; ndx++) {
      track = &file->tracks[ndx];
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
          (!strcmp(track->mdpr_header.mime_type, "audio/x-pn-realaudio") &&
           !strcmp(track->mdpr_header.mime_type, "video/x-pn-realvideo")))
        continue;

      ts_data = track->mdpr_header.type_specific_data;
      ts_size = get_uint32_be(&track->mdpr_header.type_specific_size);

      dmx = (real_demuxer_t *)safemalloc(sizeof(real_demuxer_t));
      memset(dmx, 0, sizeof(real_demuxer_t));
      dmx->track = track;

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

// }}}

// {{{ FUNCTION real_reader_c::create_packetizers()

void
real_reader_c::create_packetizer(int64_t tid) {
  int i;
  real_demuxer_t *dmx;
  rmff_track_t *track;
  bool duplicate_data;

  dmx = find_demuxer(tid);
  if (dmx == NULL)
    return;

  if (dmx->packetizer == NULL) {
    track = dmx->track;
    ti->id = track->id;
    ti->private_data = dmx->private_data;
    ti->private_size = dmx->private_size;
    duplicate_data = false;

    if (track->type == RMFF_TRACK_TYPE_VIDEO) {
      char buffer[20];

      mxprints(buffer, "V_REAL/%s", dmx->fourcc);
      dmx->packetizer =
        new video_packetizer_c(this, buffer, dmx->fps, dmx->width, dmx->height,
                               false, ti);
      if ((dmx->fourcc[0] != 'R') || (dmx->fourcc[1] != 'V') ||
          (dmx->fourcc[2] != '4') || (dmx->fourcc[3] != '0'))
        dmx->rv_dimensions = true;

      if (verbose)
        mxinfo("+-> Using video output module for stream %u (FourCC: "
               "%s).\n", track->id, dmx->fourcc);

      dmx->f_merged = false;
      dmx->segments = new vector<rv_segment_t>;

    } else {
      char buffer[20];
      passthrough_packetizer_c *ptzr;

      if (!strncmp(dmx->fourcc, "dnet", 4)) {
        dmx->packetizer =
          new ac3_bs_packetizer_c(this, dmx->samples_per_second, dmx->channels,
                                  dmx->bsid, ti);
        mxverb(1, "+-> Using the AC3 output module for stream "
               "%u (FourCC: %s).\n", track->id, dmx->fourcc);

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
        for (i = 0; i < (int)ti->aac_is_sbr->size(); i++)
          if (((*ti->aac_is_sbr)[i] == -1) ||
              ((*ti->aac_is_sbr)[i] == track->id)) {
            profile = AAC_PROFILE_SBR;
            break;
          }
        mxverb(2, PFX "2. profile: %d, channels: %d, sample_rate: "
               "%d, output_sample_rate: %d, sbr: %d\n", profile, channels,
               sample_rate, output_sample_rate, (int)sbr);
        ti->private_data = NULL;
        ti->private_size = 0;
        dmx->is_aac = true;
        dmx->packetizer =
          new aac_packetizer_c(this, AAC_ID_MPEG4, profile,
                               sample_rate, channels, ti, false, true);
        mxverb(1, "+-> Using the AAC output module for stream "
               "%u (FourCC: %s).\n", track->id, dmx->fourcc);
        if (profile == AAC_PROFILE_SBR)
          dmx->packetizer->
            set_audio_output_sampling_freq(output_sample_rate);
        else if (!extra_data_parsed)
          mxwarn("RealMedia files may contain HE-AAC / AAC+ / SBR AAC audio. "
                 "In some cases this can NOT be detected automatically. "
                 "Therefore you have to specifiy '--aac-is-sbr %u' manually "
                 "for this input file if the file actually contains SBR AAC. "
                 "The file will be muxed in the WRONG way otherwise. Also "
                 "read mkvmerge's documentation.\n", track->id);
        duplicate_data = true;

      } else {
        ptzr = new passthrough_packetizer_c(this, ti);
        dmx->packetizer = ptzr;
        dmx->segments = new vector<rv_segment_t>;

        mxprints(buffer, "A_REAL/%c%c%c%c", toupper(dmx->fourcc[0]),
                 toupper(dmx->fourcc[1]), toupper(dmx->fourcc[2]),
                 toupper(dmx->fourcc[3]));

        ptzr->set_track_type(track_audio);
        ptzr->set_codec_id(buffer);
        ptzr->set_codec_private(dmx->private_data, dmx->private_size);
        ptzr->set_audio_sampling_freq((float)dmx->samples_per_second);
        ptzr->set_audio_channels(dmx->channels);
        ptzr->set_audio_bit_depth(dmx->bits_per_sample);

        if (verbose)
          mxinfo("+-> Using generic audio output module for stream "
                 "%u (FourCC: %s).\n", track->id, dmx->fourcc);
      }
    }

    dmx->packetizer->duplicate_data_on_add(duplicate_data);
  }
}

void
real_reader_c::create_packetizers() {
  uint32_t i;

  for (i = 0; i < ti->track_order->size(); i++)
    create_packetizer((*ti->track_order)[i]);
  for (i = 0; i < demuxers.size(); i++)
    create_packetizer(demuxers[i]->track->id);
}

// }}}

// {{{ FUNCTION real_reader_c::find_demuxer(int id)

real_demuxer_t *
real_reader_c::find_demuxer(int id) {
  int i;

  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->track->id == id)
      return demuxers[i];

  return NULL;
}

int
real_reader_c::finish() {
  int i;
  int64_t dur;
  real_demuxer_t *dmx;

  for (i = 0; i < demuxers.size(); i++) {
    dmx = demuxers[i];
    if ((dmx->track->type == RMFF_TRACK_TYPE_AUDIO) &&
        (dmx->segments != NULL)) {
      dur = dmx->c_timecode / dmx->c_numpackets;
      deliver_audio_frames(dmx, dur);
    }
  }

  done = true;

  flush_packetizers();

  return 0;
}

// }}}

// {{{ FUNCTION real_reader_c::read()

int
real_reader_c::read(generic_packetizer_c *) {
  int size;
  unsigned char *chunk;
  real_demuxer_t *dmx;
  int64_t timecode;
  rmff_frame_t *frame;

  if (done)
    return 0;

  size = rmff_get_next_frame_size(file);
  if (size <= 0) {
    if (file->num_packets_read < file->num_packets_in_chunk)
      mxwarn(PFX "%s: File contains fewer frames than expected or is "
             "corrupt after frame %u.\n", ti->fname, file->num_packets_read);
    return finish();
  }

  chunk = (unsigned char *)safemalloc(size);
  frame = rmff_read_next_frame(file, chunk);
  if (frame == NULL) {
    if (file->num_packets_read < file->num_packets_in_chunk)
      mxwarn(PFX "%s: File contains fewer frames than expected or is "
             "corrupt after frame %u.\n", ti->fname, file->num_packets_read);
    safefree(chunk);
    return finish();
  }

  timecode = (int64_t)frame->timecode * 1000000ll;
  dmx = find_demuxer(frame->id);
  if (dmx == NULL) {
    rmff_release_frame(frame);
    return EMOREDATA;
  }

  if (dmx->track->type == RMFF_TRACK_TYPE_VIDEO) {
    assemble_packet(dmx, chunk, size, timecode, (frame->flags & 2) == 2);
    safefree(chunk);

  } else if (dmx->is_aac)
    deliver_aac_frames(dmx, chunk, size);

  else
    queue_audio_frames(dmx, chunk, size, timecode, frame->flags);

  rmff_release_frame(frame);

  return EMOREDATA;
}

void
real_reader_c::queue_one_audio_frame(real_demuxer_t *dmx,
                                     unsigned char *chunk,
                                     uint32_t length,
                                     uint64_t timecode,
                                     uint32_t flags) {
  rv_segment_t segment;

  segment.data = chunk;
  segment.size = length;
  segment.offset = flags;
  dmx->segments->push_back(segment);
  dmx->c_timecode = timecode;
  mxverb(2, "enqueueing one for %u/'%s' length %u timecode %llu flags "
         "0x%08x\n", dmx->track->id, ti->fname, length, timecode, flags);
}

void
real_reader_c::queue_audio_frames(real_demuxer_t *dmx,
                                  unsigned char *chunk,
                                  uint32_t length,
                                  uint64_t timecode,
                                  uint32_t flags) {
  // Enqueue the packets if no packets are in the queue or if the current
  // packet's timecode is the same as the timecode of those before.
  if ((dmx->segments->size() == 0) ||
      (dmx->c_timecode == timecode)) {
    queue_one_audio_frame(dmx, chunk, length, timecode, flags);
    return;
  }

  // This timecode is different. So let's push the packets out.
  deliver_audio_frames(dmx, (timecode - dmx->c_timecode) /
                       dmx->segments->size());
  // Enqueue this packet.
  queue_one_audio_frame(dmx, chunk, length, timecode, flags);
}

void
real_reader_c::deliver_audio_frames(real_demuxer_t *dmx, uint64_t duration) {
  uint32_t i;
  rv_segment_t segment;

  if (dmx->segments->size() == 0)
    return;

  for (i = 0; i < dmx->segments->size(); i++) {
    segment = (*dmx->segments)[i];
    mxverb(2, "delivering audio for %u/'%s' length %llu timecode %llu flags "
           "0x%08x duration %llu\n", dmx->track->id, ti->fname, segment.size,
           dmx->c_timecode, (uint32_t)segment.offset, duration);
    dmx->packetizer->process(segment.data, segment.size,
                             dmx->c_timecode, duration,
                             (segment.offset & 2) == 2 ? -1 :
                             dmx->c_reftimecode);
    if ((segment.offset & 2) == 2)
      dmx->c_reftimecode = dmx->c_timecode;
  }
  dmx->c_numpackets += dmx->segments->size();
  dmx->segments->clear();
}

void
real_reader_c::deliver_aac_frames(real_demuxer_t *dmx,
                                  unsigned char *chunk,
                                  uint32_t length) {
  uint32_t num_sub_packets, data_idx, i, sub_length, len_check;

  if (length < 2) {
    mxwarn(PFX "Short AAC audio packet for track ID %u of "
           "'%s' (length: %u < 2)\n", dmx->track->id, ti->fname, length);
    safefree(chunk);
    return;
  }
  num_sub_packets = chunk[1] >> 4;
  mxverb(2, PFX "num_sub_packets = %u\n", num_sub_packets);
  if (length < (2 + num_sub_packets * 2)) {
    mxwarn(PFX "Short AAC audio packet for track ID %u of "
           "'%s' (length: %u < %u)\n", dmx->track->id, ti->fname, length,
           2 + num_sub_packets * 2);
    safefree(chunk);
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
           "'%s' (length: %u != len_check %u)\n", dmx->track->id, ti->fname,
           length, len_check);
    safefree(chunk);
    return;
  }
  data_idx = 2 + num_sub_packets * 2;
  for (i = 0; i < num_sub_packets; i++) {
    sub_length = get_uint16_be(&chunk[2 + i * 2]);
    dmx->packetizer->process(&chunk[data_idx], sub_length);
    data_idx += sub_length;
  }
  safefree(chunk);
}

// }}}

// {{{ FUNCTIONS display_*(), set_headers(), identify()

int
real_reader_c::display_priority() {
  return DISPLAYPRIORITY_MEDIUM;
}

void
real_reader_c::display_progress(bool final) {
  if (final)
    mxinfo("progress: %u/%u packets (100%%)\r", file->num_packets_in_chunk,
           file->num_packets_in_chunk);
  else
    mxinfo("progress: %u/%u packets (%u%%)\r", file->num_packets_read,
           file->num_packets_in_chunk, file->num_packets_read * 100 /
           file->num_packets_in_chunk);
}

void
real_reader_c::set_headers() {
  uint32_t i, k;
  real_demuxer_t *d;

  for (i = 0; i < demuxers.size(); i++)
    demuxers[i]->headers_set = false;

  for (i = 0; i < ti->track_order->size(); i++) {
    d = NULL;
    for (k = 0; k < demuxers.size(); k++)
      if (demuxers[k]->track->id == (*ti->track_order)[i]) {
        d = demuxers[k];
        break;
      }
    if ((d != NULL) && (d->packetizer != NULL) && !d->headers_set) {
      d->packetizer->set_headers();
      d->headers_set = true;
    }
  }
  for (i = 0; i < demuxers.size(); i++)
    if ((demuxers[i]->packetizer != NULL) && !demuxers[i]->headers_set) {
      demuxers[i]->packetizer->set_headers();
      demuxers[i]->headers_set = true;
    }
}

void
real_reader_c::identify() {
  int i;
  real_demuxer_t *demuxer;

  mxinfo("File '%s': container: RealMedia\n", ti->fname);
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

// }}}

// {{{ FUNCTION real_reader_c::deliver_segments()

void
real_reader_c::deliver_segments(real_demuxer_t *dmx,
                                int64_t timecode) {
  uint32_t len, total;
  rv_segment_t *segment;
  int i;
  unsigned char *buffer, *ptr;

  if (dmx->segments->size() == 0)
    return;

  len = 0;
  total = 0;

  for (i = 0; i < dmx->segments->size(); i++) {
    segment = &(*dmx->segments)[i];
    if (len < (segment->offset + segment->size))
      len = segment->offset + segment->size;
    total += segment->size;
  }

  if (len != total) {
    mxwarn("\nreal_reader: packet assembly failed. "
           "Expected packet length was %d but found only %d sub packets "
           "containing %d bytes. Sub packet number: %u. Trying to "
           "continue.\n", len, dmx->segments->size(), total,
           file->num_packets_read);
    len = 0;
    for (i = 0; i < dmx->segments->size(); i++) {
      segment = &(*dmx->segments)[i];
      segment->offset = len;
      len += segment->size;
    }
  }

  len += 1 + 2 * 4 * (dmx->f_merged ? 1: dmx->segments->size());
  buffer = (unsigned char *)safemalloc(len);
  ptr = buffer;

  *ptr = dmx->f_merged ? 0 : dmx->segments->size() - 1;
  ptr++;

  if (dmx->f_merged) {
    put_uint32(ptr, 1);
    ptr += 4;
    put_uint32(ptr, 0);
    ptr += 4; 
  } else {
    for (i = 0; i < dmx->segments->size(); i++) {
      put_uint32(ptr, 1);
      ptr += 4;
      put_uint32(ptr, (*dmx->segments)[i].offset);
      ptr += 4;
    }
  }

  for (i = 0; i < dmx->segments->size(); i++) {
    segment = &(*dmx->segments)[i];
    memcpy(ptr, segment->data, segment->size);
    ptr += segment->size;
  }

  dmx->packetizer->process(buffer, len, timecode, -1,
                           dmx->c_keyframe ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC);

  for (i = 0; i < dmx->segments->size(); i++)
    safefree((*dmx->segments)[i].data);
  dmx->segments->clear();
}

// }}}

// {{{ FUNCTSION real_reader_c::assemble_packet()

void
real_reader_c::assemble_packet(real_demuxer_t *dmx,
                               unsigned char *p,
                               int size,
                               int64_t timecode,
                               bool keyframe) {
  uint32_t vpkg_header, vpkg_length, vpkg_offset, vpkg_subseq, vpkg_seqnum;
  uint32_t len;
  byte_cursor_c bc(p, size);
  rv_segment_t segment;
  int64_t this_timecode;

  if (dmx->segments->size() == 0) {
    dmx->c_keyframe = keyframe;
    dmx->f_merged = false;
  }

  if (timecode != dmx->c_timecode)
    deliver_segments(dmx, dmx->c_timecode);

  try {
    while (bc.get_len() > 2) {
      vpkg_subseq = 0;
      vpkg_seqnum = 0;
      vpkg_length = 0;
      vpkg_offset = 0;
      this_timecode = timecode;

      // bit 7: 1=last block in block chain
      // bit 6: 1=short header (only one block?)
      vpkg_header = bc.get_uint8();

      if ((vpkg_header & 0xc0) == 0x40) {
        // seems to be a very short header
        // 2 bytes, purpose of the second byte yet unknown
        bc.skip(1);
        vpkg_length = bc.get_len();

      } else {
        if ((vpkg_header & 0x40) == 0)
          // sub-seqnum (bits 0-6: number of fragment. bit 7: ???)
          vpkg_subseq = bc.get_uint8() & 0x7f;

        // size of the complete packet
        // bit 14 is always one (same applies to the offset)
        vpkg_length = bc.get_uint16_be();

        if ((vpkg_length & 0x8000) == 0x8000)
          dmx->f_merged = true;

        if ((vpkg_length & 0x4000) == 0) {
          vpkg_length <<= 16;
          vpkg_length |= bc.get_uint16_be();
          vpkg_length &= 0x3fffffff;

        } else
          vpkg_length &= 0x3fff;

        // offset of the following data inside the complete packet
        // Note: if (hdr&0xC0)==0x80 then offset is relative to the
        // _end_ of the packet, so it's equal to fragment size!!!
        vpkg_offset = bc.get_uint16_be();

        if ((vpkg_offset & 0x4000) == 0) {
          vpkg_offset <<= 16;
          vpkg_offset |= bc.get_uint16_be();
          vpkg_offset &= 0x3fffffff;

        } else
          vpkg_offset &= 0x3fff;

        vpkg_seqnum = bc.get_uint8();

        if ((vpkg_header & 0xc0) == 0xc0) {
          this_timecode = (int64_t)vpkg_offset * 1000000ll;
          vpkg_offset = 0;
        } else if ((vpkg_header & 0xc0) == 0x80)
          vpkg_offset = vpkg_length - vpkg_offset;
      }

      len = min(bc.get_len(), (int)(vpkg_length - vpkg_offset));
      segment.offset = vpkg_offset;
      segment.data = (unsigned char *)safemalloc(len);
      segment.size = len;
      bc.get_bytes(segment.data, len);
      dmx->segments->push_back(segment);
      dmx->c_timecode = this_timecode;

      if (!dmx->rv_dimensions)
        set_dimensions(dmx, segment.data, segment.size);

      if (((vpkg_header & 0x80) == 0x80) ||
          ((vpkg_offset + len) >= vpkg_length)) {
        deliver_segments(dmx, this_timecode);
        dmx->c_keyframe = false;
      }
    }

  } catch(...) {
    die(PFX "Caught exception during parsing of a video "
        "packet. Aborting.\n");
  }
}

// }}}

// {{{ FUNCTIONS real_reader_c::get_rv_dimensions(), set_dimensions()

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
  KaxTrackEntry *track_entry;

  if (get_rv_dimensions(buffer, size, width, height)) {
    if ((dmx->width != width) || (dmx->height != height)) {
      if (!ti->aspect_ratio_given && !ti->display_dimensions_given) {
        disp_width = dmx->width;
        disp_height = dmx->height;

        dmx->width = width;
        dmx->height = height;

      } else if (ti->display_dimensions_given) {
        disp_width = ti->display_width;
        disp_height = ti->display_height;

        dmx->width = width;
        dmx->height = height;

      } else {                  // ti->aspect_ratio_given == true
        dmx->width = width;
        dmx->height = height;

        if (ti->aspect_ratio > ((float)width / (float)height)) {
          disp_width = (uint32_t)(height * ti->aspect_ratio);
          disp_height = height;

        } else {
          disp_width = width;
          disp_height = (uint32_t)(width / ti->aspect_ratio);
        }

      }

      track_entry = dmx->packetizer->get_track_entry();
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

// }}}

// {{{ FUNCTION real_reader_c::get_information_from_data()

void
real_reader_c::get_information_from_data() {
  int i;
  real_demuxer_t *dmx;
  bool information_found;
  rmff_frame_t *frame;

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

  file->io->seek(file->handle, file->first_data_header_offset + 10,
                 SEEK_SET);
  file->num_packets_read = 0;
}

// }}}

void
real_reader_c::flush_packetizers() {
  uint32_t i;

  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->packetizer != NULL)
      demuxers[i]->packetizer->flush();
}
