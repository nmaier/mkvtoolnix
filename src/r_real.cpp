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
#include "p_ac3.h"
#include "p_passthrough.h"
#include "p_video.h"

// {{{ structs

#if __GNUC__ == 2
#pragma pack(2)
#else
#pragma pack(push,2)
#endif

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

#if __GNUC__ == 2
#pragma pack()
#else
#pragma pack(pop)
#endif

typedef struct {
  uint32_t chunks;              // number of chunks
  uint32_t timecode;            // timecode from packet header
  uint32_t len;                 // length of actual data
  uint32_t chunktab;            // offset to chunk offset array
} dp_hdr_t;

// }}}

// {{{ FUNCTION real_reader_c::probe_file(mm_io_c *io, int64_t size)

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

// }}}

// {{{ C'TOR

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

  last_timecode = -1;
  act_wchar = 0;
  done = false;

  if (verbose)
    mxprint(stdout, "Using RealMedia demultiplexer for %s.\n", ti->fname);

  parse_headers();
  if (!identifying)
    create_packetizers();
}

// }}}

// {{{ D'TOR

real_reader_c::~real_reader_c() {
  real_demuxer_t *demuxer;
  int i, j;

  delete io;

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
    safefree(demuxer);
  }
  demuxers.clear();
}

// }}}

// {{{ FUNCTION real_reader_c::parse_headers()

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
            mxprint(stdout, "title: '%s'\n", buffer);
          safefree(buffer);
        }

        size = io->read_uint16_be(); // author_len
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            mxprint(stdout, "author: '%s'\n", buffer);
          safefree(buffer);
        }

        size = io->read_uint16_be(); // copyright_len
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            mxprint(stdout, "copyright: '%s'\n", buffer);
          safefree(buffer);
        }

        size = io->read_uint16_be(); // comment_len
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            mxprint(stdout, "comment: '%s'\n", buffer);
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
            mxprint(stdout, "stream_name: '%s'\n", buffer);
          safefree(buffer);
        }

        size = io->read_uint8(); // mime_type_size
        if (size > 0) {
          buffer = (char *)safemalloc(size + 1);
          memset(buffer, 0, size + 1);
          if (io->read(buffer, size) != size)
            throw exception();
          if (verbose > 1)
            mxprint(stdout, "mime_type: '%s'\n", buffer);
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
                mxprint(stdout, "real_reader: Warning: Couldn't find RealAudio"
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
        mxprint(stderr, "real_reader: Unknown header type (0x%08x, "
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

// }}}

// {{{ FUNCTION real_reader_c::create_packetizers()

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
        mxprint(stdout, "+-> Using video output module for stream %u (FourCC: "
                "%s).\n", dmx->id, dmx->fourcc);

      dmx->f_merged = false;
      dmx->segments = new vector<rv_segment_t>;

    } else {
      char buffer[20];
      passthrough_packetizer_c *ptzr;

      if (!strncmp(dmx->fourcc, "dnet", 4)) {
        dmx->packetizer =
          new ac3_bs_packetizer_c(this, dmx->samples_per_second, dmx->channels,
                                  ti);
        if (verbose)
          mxprint(stdout, "+-> Using AC3 output module for stream "
                  "%u (FourCC: %s).\n", dmx->id, dmx->fourcc);

      } else {
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
          mxprint(stdout, "+-> Using generic audio output module for stream "
                  "%u (FourCC: %s).\n", dmx->id, dmx->fourcc);
      }
    }

    dmx->packetizer->duplicate_data_on_add(false);
  }

  ti->private_data = NULL;
}

// }}}

// {{{ FUNCTION real_reader_c::find_demuxer(int id)

real_demuxer_t *real_reader_c::find_demuxer(int id) {
  int i;

  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->id == id)
      return demuxers[i];

  return NULL;
}

int real_reader_c::finish() {
  int i;
  real_demuxer_t *dmx;

  for (i = 0; i < demuxers.size(); i++) {
    dmx = demuxers[i];
    if ((dmx->type == 'a') && (dmx->c_data != NULL)) {
      int64_t dur = dmx->c_timecode / dmx->c_numpackets;
      dmx->packetizer->process(dmx->c_data, dmx->c_len, dmx->c_timecode + dur,
                               dur, dmx->c_keyframe ? -1 : dmx->c_reftimecode);
    }
  }

  done = true;

  return 0;
}

// }}}

// {{{ FUNCTION real_reader_c::read()

int real_reader_c::read() {
  uint32_t object_version, length, id, timecode, flags, object_id;
  unsigned char *chunk;
  real_demuxer_t *dmx;
  int64_t fpos;

  if (done)
    return 0;

  try {
    fpos = io->getFilePointer();
    object_version = io->read_uint16_be();
    length = io->read_uint16_be();

    object_id = object_version << 16 + length;

    if (object_id == FOURCC('I', 'N', 'D', 'X'))
      return finish();

    if (object_id == FOURCC('D', 'A', 'T', 'A')) {
      io->skip(2);              // object_version
      io->skip(4);              // num_packets
      io->skip(4);              // next_data_header

      return EMOREDATA;
    }

    id = io->read_uint16_be();
    timecode = io->read_uint32_be();
    io->skip(1);                // reserved
    flags = io->read_uint8();

    if (length < 12) {
      mxprint(stdout, "real_reader: %s: Data packet length is too small: %u. "
              "Other values: object_version: 0x%04x, id: 0x%04x, "
              "timecode: %u, flags: 0x%02x. File position: %lld. Aborting.\n",
              ti->fname, length, object_version, id, timecode, flags, fpos);
      return finish();
    }

    dmx = find_demuxer(id);

    if (dmx == NULL) {
      io->skip(length - 12);
      return EMOREDATA;
    }

    length -= 12;

    chunk = (unsigned char *)safemalloc(length);
    if (io->read(chunk, length) != length) {
      safefree(chunk);
      return finish();
    }

    if (dmx->type == 'v') {
      assemble_packet(dmx, chunk, length, timecode, (flags & 2) == 2);
      safefree(chunk);
    } else {
      if ((timecode - last_timecode) <= (5 * 60 * 1000)) {
        if (dmx->c_data != NULL)
          dmx->packetizer->process(dmx->c_data, dmx->c_len, dmx->c_timecode,
                                   timecode - dmx->c_timecode,
                                   dmx->c_keyframe ? -1 : dmx->c_reftimecode);
        dmx->c_data = chunk;
        dmx->c_len = length;
        dmx->c_timecode = timecode;
        if ((flags & 2) == 2) {
          dmx->c_keyframe = true;
          dmx->c_reftimecode = timecode;
        } else
          dmx->c_keyframe = false;
        dmx->c_numpackets++;
      } else {
        timecode = last_timecode;
        safefree(chunk);
      }
    }

    last_timecode = timecode;

  } catch (exception &ex) {
    return finish();
  }

  return EMOREDATA;
}

// }}}

// {{{ get_packet(), display_*(), set_headers(), identify()

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
  mxprint(stdout, "working... %c (%d%%)\r", wchar[act_wchar],
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

  mxprint(stdout, "File '%s': container: RealMedia\n", ti->fname);
  for (i = 0; i < demuxers.size(); i++) {
    demuxer = demuxers[i];
    mxprint(stdout, "Track ID %d: %s (%s)\n", demuxer->id,
            demuxer->type == 'a' ? "audio" : "video",
            demuxer->fourcc);
  }
}

// }}}

void real_reader_c::deliver_segments(real_demuxer_t *dmx, int64_t timecode) {
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

  if (len != total)
    die("real_reader_c: deliver_segments() failed: len %d, total %d.", len,
        total);

  len += 1 + 2 * 4 * (dmx->f_merged ? 1: dmx->segments->size());
  buffer = (unsigned char *)safemalloc(len);
  ptr = buffer;

  *ptr = dmx->f_merged ? 0 : dmx->segments->size() - 1;
  ptr++;

  if (dmx->f_merged) {
    *((uint32_t *)ptr) = 1;
    ptr += 4;
    *((uint32_t *)ptr) = 0;
    ptr += 4; 
  } else {
    for (i = 0; i < dmx->segments->size(); i++) {
      *((uint32_t *)ptr) = 1;
      ptr += 4;
      *((uint32_t *)ptr) = (*dmx->segments)[i].offset;
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

void real_reader_c::assemble_packet(real_demuxer_t *dmx, unsigned char *p,
                                    int size, int64_t timecode,
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
      vpkg_header = bc.get_byte();

      if (verbose > 1)
        mxprint(stdout, "\nPACKET: size: %d; fib: ", size);

      if ((vpkg_header & 0xc0) == 0x40) {
        // seems to be a very short header
        // 2 bytes, purpose of the second byte yet unknown
        bc.skip(1);
        vpkg_length = bc.get_len();
        if (verbose > 1)
          mxprint(stdout, "short, len: %d\n", vpkg_length);

      } else {
        if ((vpkg_header & 0x40) == 0) {
          // sub-seqnum (bits 0-6: number of fragment. bit 7: ???)
          vpkg_subseq = bc.get_byte() & 0x7f;
          if (verbose > 1)
            mxprint(stdout, "subseq: %d ", vpkg_subseq);
        }

        // size of the complete packet
        // bit 14 is always one (same applies to the offset)
        vpkg_length = bc.get_word();
        if (verbose > 1)
          mxprint(stdout, "l: %02x %02x ", vpkg_length >> 8,
                  vpkg_length & 0xff);

        if ((vpkg_length & 0x8000) == 0x8000)
          dmx->f_merged = true;

        if ((vpkg_length & 0x4000) == 0) {
          vpkg_length <<= 16;
          vpkg_length |= bc.get_word();
          vpkg_length &= 0x3fffffff;
          if (verbose > 1)
            mxprint(stdout, "l+: %02x %02x ",
                    (vpkg_length >> 8) & 0xff, vpkg_length & 0xff);
        } else
          vpkg_length &= 0x3fff;

        // offset of the following data inside the complete packet
        // Note: if (hdr&0xC0)==0x80 then offset is relative to the
        // _end_ of the packet, so it's equal to fragment size!!!
        vpkg_offset = bc.get_word();

        if (verbose > 1)
          mxprint(stdout, "o: %02x %02x ", vpkg_offset >> 8,
                  vpkg_offset & 0xff);

        if ((vpkg_offset & 0x4000) == 0) {
          vpkg_offset <<= 16;
          vpkg_offset |= bc.get_word();
          vpkg_offset &= 0x3fffffff;
          if (verbose > 1)
            mxprint(stdout, "o+: %02x %02x ",
                    (vpkg_offset >> 8 ) &0xff, vpkg_offset & 0xff);
        } else
          vpkg_offset &= 0x3fff;

        vpkg_seqnum = bc.get_byte();
        if (verbose > 1)
          mxprint(stdout, "seq: %02x\n", vpkg_seqnum);

        if ((vpkg_header & 0xc0) == 0xc0) {
          this_timecode = vpkg_offset;
          vpkg_offset = 0;
        } else if ((vpkg_header & 0xc0) == 0x80)
          vpkg_offset = vpkg_length - vpkg_offset;
      }

      len = min(bc.get_len(), vpkg_length - vpkg_offset);
      segment.offset = vpkg_offset;
      segment.data = (unsigned char *)safemalloc(len);
      segment.size = len;
      bc.get_bytes(segment.data, len);
      dmx->segments->push_back(segment);
      dmx->c_timecode = this_timecode;

      if (((vpkg_header & 0x80) == 0x80) ||
          ((vpkg_offset + len) >= vpkg_length)) {
        deliver_segments(dmx, this_timecode);
        dmx->c_keyframe = false;
      }
    }

  } catch(...) {
    die("real_reader: Caught exception during parsing of a video "
        "packet. Aborting.\n");
  }
}
