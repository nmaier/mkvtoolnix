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

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "r_real.h"
#include "p_passthrough.h"
#include "p_video.h"

#pragma pack(push,2)

typedef struct {
  uint32_t size;
  uint32_t fourcc1;
  uint32_t fourcc2;
  uint16_t width;
  uint16_t height;
  uint16_t bpp;
  uint32_t unknown1;
  uint32_t fps;
  uint32_t type1;
  uint32_t type2;
} real_video_props_t;

typedef struct {
  uint32_t fourcc1;             // '.', 'r', 'a', 0xfd
  uint16_t version1;            // 4 or 5
  uint16_t unknown1;            // 00 000
  uint32_t fourcc2;             // .ra4 or .ra5
  uint32_t unknown2;            // ???
  uint16_t version2;            // 4 or 5
  uint32_t header_size;         // == 0x4e
  uint16_t flavor;              // codec flavor id
  uint32_t coded_frame_size;    // coded frame size
  uint32_t unknown3;            // big number
  uint32_t unknown4;            // bigger number
  uint32_t unknown5;            // yet another number
  uint16_t sub_packet_h;
  uint16_t frame_size;
  uint16_t sub_packet_size;
  uint16_t unknown6;            // 00 00
  uint16_t sample_rate;
  uint16_t unknown8;            // 0
  uint16_t sample_size;
  uint16_t channels;
} real_audio_v4_props_t;

typedef struct {
  uint32_t fourcc1;             // '.', 'r', 'a', 0xfd
  uint16_t version1;            // 4 or 5
  uint16_t unknown1;            // 00 000
  uint32_t fourcc2;             // .ra4 or .ra5
  uint32_t unknown2;            // ???
  uint16_t version2;            // 4 or 5
  uint32_t header_size;         // == 0x4e
  uint16_t flavor;              // codec flavor id
  uint32_t coded_frame_size;    // coded frame size
  uint32_t unknown3;            // big number
  uint32_t unknown4;            // bigger number
  uint32_t unknown5;            // yet another number
  uint16_t sub_packet_h;
  uint16_t frame_size;
  uint16_t sub_packet_size;
  uint16_t unknown6;            // 00 00
  uint8_t unknown7[6];          // 0, srate, 0
  uint16_t sample_rate;
  uint16_t unknown8;            // 0
  uint16_t sample_size;
  uint16_t channels;
  uint32_t genr;                // "genr"
  uint32_t fourcc3;             // fourcc
} real_audio_v5_props_t;

#pragma pack(pop)

int real_reader_c::probe_file(mm_io_c *io, int64_t size) {
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

real_reader_c::real_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {

  try {
    io = new mm_io_c(ti->fname, MODE_READ);
    io->setFilePointer(0, seek_end);
    file_size = io->getFilePointer();
    io->setFilePointer(0, seek_beginning);
    if (!real_reader_c::probe_file(io, file_size))
      throw error_c("real_reader: Source is not a valid RealMedia file.");

  } catch (exception &ex) {
    throw error_c("real_reader: Could not read the source file.");
  }

  last_timestamp = -1;

  if (verbose)
    fprintf(stdout, "Using RealMedia demultiplexer for %s.\n", ti->fname);

  parse_headers();
  if (!identifying)
    create_packetizers();
}

real_reader_c::~real_reader_c() {
  real_demuxer_t *demuxer;
  int i;

  delete io;

  for (i = 0; i < demuxers.size(); i++) {
    demuxer = demuxers[i];
    if (demuxer->packetizer != NULL)
      delete demuxer->packetizer;
    safefree(demuxer->private_data);
    safefree(demuxer);
  }
  demuxers.clear();
}

void real_reader_c::parse_headers() {
  uint32_t object_id, i, id, start_time, preroll, size;
  char *buffer;
  real_demuxer_t *dmx;
  real_video_props_t *rvp;
  real_audio_v4_props_t *ra4p;
  real_audio_v5_props_t *ra5p;

  try {
    io->skip(4);                // object_id = ".RIF"
    io->skip(4);                // header size
    io->skip(2);                // object_version

    io->skip(4);                // file_version
    io->skip(4);                // num_headers

    while (1) {
      object_id = io->read_uint32_be();
      io->skip(4);              // size
      io->skip(2);              // object_version

      if (object_id == FOURCC('P', 'R', 'O', 'P')) {
        io->skip(4);            // max_bit_rate
        io->skip(4);            // avg_bit_rate
        io->skip(4);            // max_packet_size
        io->skip(4);            // avg_packet_size
        io->skip(4);            // num_packets
        io->skip(4);            // duration
        io->skip(4);            // preroll
        io->skip(4);            // index_offset
        io->skip(4);            // data_offset
        io->skip(2);            // num_streams
        io->skip(2);            // flags

      } else if (object_id == FOURCC('C', 'O', 'N', 'T')) {
        size = io->read_uint16_be(); // title_len
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            printf("title: '%s'\n", buffer);
          safefree(buffer);
        }

        size = io->read_uint16_be(); // author_len
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            printf("author: '%s'\n", buffer);
          safefree(buffer);
        }

        size = io->read_uint16_be(); // copyright_len
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            printf("copyright: '%s'\n", buffer);
          safefree(buffer);
        }

        size = io->read_uint16_be(); // comment_len
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            printf("comment: '%s'\n", buffer);
          safefree(buffer);
        }

      } else if (object_id == FOURCC('M', 'D', 'P', 'R')) {
        id = io->read_uint16_be();
        io->skip(4);            // max_bit_rate
        io->skip(4);            // avg_bit_rate
        io->skip(4);            // max_packet_size
        io->skip(4);            // avg_packet_size
        start_time = io->read_uint32_be();
        preroll = io->read_uint32_be();
        io->skip(4);            // duration

        size = io->read_uint8(); // stream_name_size
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            printf("stream_name: '%s'\n", buffer);
          safefree(buffer);
        }

        size = io->read_uint8(); // mime_type_size
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            printf("mime_type: '%s'\n", buffer);
          safefree(buffer);
        }

        size = io->read_uint32_be(); // type_specific_size
        if (size > 0) {
          buffer = (char *)safemalloc(size);
          if (io->read(buffer, size) != size)
            throw exception();

          rvp = (real_video_props_t *)buffer;
          ra4p = (real_audio_v4_props_t *)buffer;
          ra5p = (real_audio_v5_props_t *)buffer;

          if ((size >= sizeof(real_video_props_t)) &&
              (get_fourcc(&rvp->fourcc1) == FOURCC('V', 'I', 'D', 'O')) &&
              demuxing_requested('v', id)) {

            dmx = (real_demuxer_t *)safemalloc(sizeof(real_demuxer_t));
            memset(dmx, 0, sizeof(real_demuxer_t));
            dmx->id = id;
            dmx->start_time = start_time;
            dmx->preroll = preroll;
            memcpy(dmx->fourcc, &rvp->fourcc2, 4);
            dmx->fourcc[4] = 0;
            dmx->width = get_uint16_be(&rvp->width);
            dmx->height = get_uint16_be(&rvp->height);
            dmx->type = 'v';
            i = get_uint32_be(&rvp->fps);
            dmx->fps = (float)((i & 0xffff0000) >> 16) +
              ((float)(i & 0x0000ffff)) / 65536.0;
            dmx->private_data = (unsigned char *)safememdup(buffer, size);
            dmx->private_size = size;

            demuxers.push_back(dmx);

          } else if ((size >= sizeof(real_audio_v4_props_t)) &&
                     (get_fourcc(&ra4p->fourcc1) ==
                      FOURCC('.', 'r', 'a', 0xfd)) &&
                     demuxing_requested('a', id)) {
            bool ok;

            ok = true;

            dmx = (real_demuxer_t *)safemalloc(sizeof(real_demuxer_t));
            memset(dmx, 0, sizeof(real_demuxer_t));
            dmx->id = id;
            dmx->start_time = start_time;
            dmx->preroll = preroll;
            dmx->type = 'a';
            if (get_uint16_be(&ra4p->version1) == 4) {
              int slen;
              char *p;

              dmx->channels = get_uint16_be(&ra4p->channels);
              dmx->samples_per_second = get_uint16_be(&ra4p->sample_rate);
              dmx->bits_per_sample = get_uint16_be(&ra4p->sample_size);
              p = (char *)(ra4p + 1);
              slen = (unsigned char)p[0];
              p += (slen + 1);
              slen = (unsigned char)p[0];
              p++;
              if (slen != 4) {
                fprintf(stdout, "real_reader: Warning: Couldn't find RealAudio"
                        " FourCC for id %u (description length: %d) Skipping "
                        "track.\n", id, slen);
                ok = false;
              } else {
                memcpy(dmx->fourcc, p, 4);
                dmx->fourcc[4] = 0;
              }

            } else {

              memcpy(dmx->fourcc, &ra5p->fourcc3, 4);
              dmx->fourcc[4] = 0;
              dmx->channels = get_uint16_be(&ra5p->channels);
              dmx->samples_per_second = get_uint16_be(&ra5p->sample_rate);
              dmx->bits_per_sample = get_uint16_be(&ra5p->sample_size);
            }

            if (ok) {
              dmx->private_data = (unsigned char *)safememdup(buffer, size);
              dmx->private_size = size;

              demuxers.push_back(dmx);
            } else
              free(dmx);
          }

          safefree(buffer);
        }

      } else if (object_id == FOURCC('D', 'A', 'T', 'A')) {
        io->skip(4);            // num_packets
        io->skip(4);            // next_data_header

        break;                  // We're finished!

      } else {
        fprintf(stderr, "real_reader: Unknown header type (0x%08x, "
                "%c%c%c%c).\n", object_id, (char)(object_id >> 24),
                (char)((object_id & 0x00ff0000) >> 16),
                (char)((object_id & 0x0000ff00) >> 8),
                (char)(object_id & 0x000000ff));
        throw exception();
      }
    }

  } catch (exception &ex) {
    throw error_c("real_reader: Could not parse the RealMedia headers.");
  }
}

void real_reader_c::create_packetizers() {
  int i;
  real_demuxer_t *dmx;

  for (i = 0; i < demuxers.size(); i++) {
    dmx = demuxers[i];
    ti->private_data = dmx->private_data;
    ti->private_size = dmx->private_size;

    if (dmx->type == 'v') {
      char buffer[20];

      sprintf(buffer, "V_REAL/%s", dmx->fourcc);
      dmx->packetizer = new video_packetizer_c(this, buffer, dmx->fps,
                                               dmx->width, dmx->height,
                                               false, ti);

      if (verbose)
        fprintf(stdout, "+-> Using video output module for stream %u (FourCC: "
                "%s).\n", dmx->id, dmx->fourcc);

    } else {
      char buffer[20];
      passthrough_packetizer_c *ptzr;

      ptzr = new passthrough_packetizer_c(this, ti);
      dmx->packetizer = ptzr;

      sprintf(buffer, "A_REAL/%c%c%c%c", toupper(dmx->fourcc[0]), 
              toupper(dmx->fourcc[1]), toupper(dmx->fourcc[2]),
              toupper(dmx->fourcc[3]));

      ptzr->set_track_type(track_audio);
      ptzr->set_codec_id(buffer);
      ptzr->set_codec_private(dmx->private_data, dmx->private_size);
      ptzr->set_audio_sampling_freq((float)dmx->samples_per_second);
      ptzr->set_audio_channels(dmx->channels);
      ptzr->set_audio_bit_depth(dmx->bits_per_sample);

      if (verbose)
        fprintf(stdout, "+-> Using generic audio output module for stream %u "
                "(FourCC: %s).\n", dmx->id, dmx->fourcc);
    }

    dmx->packetizer->duplicate_data_on_add(false);
  }

  ti->private_data = NULL;
}

real_demuxer_t *real_reader_c::find_demuxer(int id) {
  int i;

  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->id == id)
      return demuxers[i];

  return NULL;
}

int real_reader_c::read() {
  uint32_t object_version, length, id, timestamp, flags, object_id;
  unsigned char *chunk;
  real_demuxer_t *dmx;
  int64_t bref;

  try {
    object_version = io->read_uint16_be();
    length = io->read_uint16_be();

    object_id = object_version << 16 + length;

    if (object_id == FOURCC('I', 'N', 'D', 'X'))
      return 0;
    if (object_id == FOURCC('D', 'A', 'T', 'A')) {
      io->skip(2);              // object_version
      io->skip(4);              // num_packets
      io->skip(4);              // next_data_header

      return EMOREDATA;
    }

    id = io->read_uint16_be();
    timestamp = io->read_uint32_be();
    io->skip(1);                // reserved
    flags = io->read_uint8();

    dmx = find_demuxer(id);

    if (dmx == NULL) {
      io->skip(length - 12);
      return EMOREDATA;
    }

    length -= 12;

    chunk = (unsigned char *)safemalloc(length);
    if (io->read(chunk, length) != length) {
      safefree(chunk);
      return 0;
    }

    if (dmx->type == 'v') {
      if (((flags & 2) == 2) && (last_timestamp != timestamp))
        bref = VFT_IFRAME;
      else if (last_timestamp == timestamp)
        bref = timestamp;
      else
        bref = VFT_PFRAMEAUTOMATIC;

      dmx->packetizer->process(chunk, length, timestamp, -1, bref);

    } else
      dmx->packetizer->process(chunk, length, timestamp);

    last_timestamp = timestamp;

  } catch (exception &ex) {
    return 0;
  }

  return EMOREDATA;
}

packet_t *real_reader_c::get_packet() {
  generic_packetizer_c *winner;
  real_demuxer_t *demuxer;
  int i;

  winner = NULL;

  for (i = 0; i < demuxers.size(); i++) {
    demuxer = demuxers[i];
    if (winner == NULL) {
      if (demuxer->packetizer->packet_available())
        winner = demuxer->packetizer;
    } else if (winner->packet_available() &&
               (winner->get_smallest_timecode() >
                demuxer->packetizer->get_smallest_timecode()))
      winner = demuxer->packetizer;
  }

  if (winner != NULL)
    return winner->get_packet();
  else
    return NULL;
}

int real_reader_c::display_priority() {
  return DISPLAYPRIORITY_MEDIUM;
}

static char wchar[] = "-\\|/-\\|/-";

void real_reader_c::display_progress() {
  fprintf(stdout, "working... %c (%d%%)\r", wchar[act_wchar],
          (int)(io->getFilePointer() * 100 / file_size));
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}

void real_reader_c::set_headers() {
  int i;

  for (i = 0; i < demuxers.size(); i++)
    demuxers[i]->packetizer->set_headers();
}

void real_reader_c::identify() {
  int i;
  real_demuxer_t *demuxer;

  fprintf(stdout, "File '%s': container: RealMedia\n", ti->fname);
  for (i = 0; i < demuxers.size(); i++) {
    demuxer = demuxers[i];
    fprintf(stdout, "Track ID %d: %s (%s)\n", demuxer->id,
            demuxer->type == 'a' ? "audio" : "video",
            demuxer->fourcc);
  }
}
