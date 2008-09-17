/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   MPEG ES demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <memory>

#include "ac3_common.h"
#include "common.h"
#include "error.h"
#include "M2VParser.h"
#include "mp3_common.h"
#include "r_mpeg.h"
#include "smart_pointers.h"
#include "output_control.h"
#include "p_ac3.h"
#include "p_avc.h"
#include "p_dts.h"
#include "p_mp3.h"
#include "p_video.h"

#define PROBESIZE 4
#define READ_SIZE 1024 * 1024

int
mpeg_es_reader_c::probe_file(mm_io_c *io,
                             int64_t size) {
  if (PROBESIZE > size)
    return 0;

  try {
    unsigned char *buf = (unsigned char *)safemalloc(READ_SIZE);
    io->setFilePointer(0, seek_beginning);
    int num_read = io->read(buf, READ_SIZE);

    if (4 > num_read) {
      safefree(buf);
      return 0;
    }
    io->setFilePointer(0, seek_beginning);

    // MPEG TS starts with 0x47.
    if (0x47 == buf[0]) {
      safefree(buf);
      return 0;
    }

    // MPEG PS starts with 0x000001ba.
    uint32_t value = get_uint32_be(buf);
    if (MPEGVIDEO_PACKET_START_CODE == value) {
      safefree(buf);
      return 0;
    }

    // Due to type detection woes mkvmerge requires
    // the stream to start with a MPEG start code.
    if (!mpeg_is_start_code(value))
      return 0;

    bool sequence_start_code_found = false;
    bool picture_start_code_found  = false;
    bool gop_start_code_found      = false;

    // Let's look for a MPEG ES start code inside the first 1 MB.
    int i;
    for (i = 4; i < num_read - 1; i++) {
      if (MPEGVIDEO_SEQUENCE_START_CODE == value)
        sequence_start_code_found = true;
      else if (MPEGVIDEO_PICTURE_START_CODE == value)
        picture_start_code_found  = true;
      else if (MPEGVIDEO_GOP12_START_CODE == value)
        gop_start_code_found      = true;

      if (sequence_start_code_found && picture_start_code_found && gop_start_code_found)
        break;

      value <<= 8;
      value  |= buf[i];
    }

    safefree(buf);

    if (!(sequence_start_code_found && picture_start_code_found && gop_start_code_found))
      return 0;

    // Let's try to read one frame.
    M2VParser parser;
    if (!read_frame(parser, *io, READ_SIZE, true))
      return 0;

  } catch (...) {
    return 0;
  }

  return 1;
}

mpeg_es_reader_c::mpeg_es_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {

  try {
    M2VParser parser;

    io   = new mm_file_io_c(ti.fname);
    size = io->get_size();

    // Let's find the first frame. We need its information like
    // resolution, MPEG version etc.
    if (!read_frame(parser, *io, 1024 * 1024)) {
      delete io;
      throw "";
    }

    io->setFilePointer(0);

    MPEG2SequenceHeader seq_hdr = parser.GetSequenceHeader();
    version                     = parser.GetMPEGVersion();
    width                       = seq_hdr.width;
    height                      = seq_hdr.height;
    frame_rate                  = seq_hdr.frameRate;
    aspect_ratio                = seq_hdr.aspectRatio;

    if ((0 >= aspect_ratio) || (1 == aspect_ratio))
      dwidth = width;
    else
      dwidth = (int)(height * aspect_ratio);
    dheight = height;

    MPEGChunk *raw_seq_hdr = parser.GetRealSequenceHeader();
    if (NULL != raw_seq_hdr) {
      ti.private_data = (unsigned char *)safememdup(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
      ti.private_size = raw_seq_hdr->GetSize();
    }

    mxverb(2, "mpeg_es_reader: v %d width %d height %d FPS %e AR %e\n", version, width, height, frame_rate, aspect_ratio);

  } catch (...) {
    throw error_c("mpeg_es_reader: Could not open the file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the MPEG ES demultiplexer.\n", ti.fname.c_str());
}

mpeg_es_reader_c::~mpeg_es_reader_c() {
  delete io;
}

void
mpeg_es_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  mxinfo(FMT_TID "Using the MPEG-1/2 video output module.\n", ti.fname.c_str(), (int64_t)0);
  add_packetizer(new mpeg1_2_video_packetizer_c(this, version, frame_rate, width, height, dwidth, dheight, false, ti));
}

file_status_e
mpeg_es_reader_c::read(generic_packetizer_c *,
                       bool) {
  unsigned char *chunk = (unsigned char *)safemalloc(20000);
  int num_read         = io->read(chunk, 20000);
  if (0 > num_read) {
    safefree(chunk);
    return FILE_STATUS_DONE;
  }

  if (0 < num_read)
    PTZR0->process(new packet_t(new memory_c(chunk, num_read, true)));
  if (20000 > num_read)
    PTZR0->flush();

  bytes_processed = io->getFilePointer();

  return 20000 > num_read ? FILE_STATUS_DONE : FILE_STATUS_MOREDATA;
}

bool
mpeg_es_reader_c::read_frame(M2VParser &parser,
                             mm_io_c &in,
                             int64_t max_size,
                             bool flush) {
  int bytes_probed;

  bytes_probed = 0;
  while (true) {
    int state;

    state = parser.GetState();

    if (MPV_PARSER_STATE_NEED_DATA == state) {
      if ((max_size != -1) && (bytes_probed > max_size))
        return false;

      int bytes_to_read     = (parser.GetFreeBufferSpace() < READ_SIZE) ? parser.GetFreeBufferSpace() : READ_SIZE;
      unsigned char *buffer = new unsigned char[bytes_to_read];
      int bytes_read        = in.read(buffer, bytes_to_read);
      if (0 == bytes_read) {
        delete [] buffer;
        break;
      }
      bytes_probed += bytes_read;

      parser.WriteData(buffer, bytes_read);
      parser.SetEOS();
      delete [] buffer;

    } else if (MPV_PARSER_STATE_FRAME == state)
      return true;

    else if ((MPV_PARSER_STATE_EOS == state) || (MPV_PARSER_STATE_ERROR == state))
      return false;
  }

  return false;
}

int
mpeg_es_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
mpeg_es_reader_c::identify() {
  id_result_container("MPEG elementary stream (ES)");
  id_result_track(0, ID_RESULT_TRACK_VIDEO, mxsprintf("MPEG %d", version));
}

// ------------------------------------------------------------------------

#define PS_PROBE_SIZE 1 * 1024 * 1024

static bool
operator <(const mpeg_ps_track_ptr &a,
           const mpeg_ps_track_ptr &b) {
  return a->sort_key < b->sort_key;
}

int
mpeg_ps_reader_c::probe_file(mm_io_c *io,
                             int64_t size) {
  try {
    memory_c af_buf((unsigned char *)safemalloc(PS_PROBE_SIZE), 0, true);
    unsigned char *buf = af_buf.get();
    int num_read;

    io->setFilePointer(0, seek_beginning);
    num_read = io->read(buf, PS_PROBE_SIZE);
    if (4 > num_read)
      return 0;
    io->setFilePointer(0, seek_beginning);

    if (get_uint32_be(buf) != MPEGVIDEO_PACKET_START_CODE)
      return 0;

    return 1;

  } catch (...) {
    return 0;
  }
}

mpeg_ps_reader_c::mpeg_ps_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {

  init_reader();
}

void
mpeg_ps_reader_c::init_reader() {
  int i;

  try {
    io        = new mm_file_io_c(ti.fname);
    size      = io->get_size();
    file_done = false;

  } catch (...) {
    throw error_c("mpeg_ps_reader: Could not open the file.");
  }

  try {
    uint32_t header;
    uint8_t byte;
    bool done;

    bytes_processed = 0;

    for (i = 0; i < 512; i++)
      id2idx[i] = -1;
    memset(es_map, 0, sizeof(uint32_t) * NUM_ES_MAP_ENTRIES);
    memset(blacklisted_ids, 0, sizeof(bool) * 512);

    header  = io->read_uint32_be();
    done    = io->eof();
    version = -1;

    while (!done) {
      uint8_t stream_id;
      uint16_t pes_packet_length;

      switch (header) {
        case MPEGVIDEO_PACKET_START_CODE:
          mxverb(3, "mpeg_ps: packet start at " LLD "\n", io->getFilePointer() - 4);

          if (-1 == version) {
            byte = io->read_uint8();
            if ((byte & 0xc0) != 0)
              version = 2;      // MPEG-2 PS
            else
              version = 1;
            io->skip(-1);
          }

          io->skip(2 * 4);   // pack header
          if (2 == version) {
            io->skip(1);
            byte = io->read_uint8() & 0x07;
            io->skip(byte);  // stuffing bytes
          }
          header = io->read_uint32_be();
          break;

        case MPEGVIDEO_SYSTEM_HEADER_START_CODE:
          mxverb(3, "mpeg_ps: system header start code at " LLD "\n", io->getFilePointer() - 4);

          io->skip(2 * 4);   // system header
          byte = io->read_uint8();
          while ((byte & 0x80) == 0x80) {
            io->skip(2);     // P-STD info
            byte = io->read_uint8();
          }
          io->skip(-1);
          header = io->read_uint32_be();
          break;

        case MPEGVIDEO_MPEG_PROGRAM_END_CODE:
          done = !resync_stream(header);
          break;

        case MPEGVIDEO_PROGRAM_STREAM_MAP_START_CODE:
          parse_program_stream_map();
          break;

        default:
          if (!mpeg_is_start_code(header)) {
            mxverb(3, "mpeg_ps: unknown header 0x%08x at " LLD "\n", header, io->getFilePointer() - 4);
            done = !resync_stream(header);
            break;
          }

          stream_id = header & 0xff;
          io->save_pos();
          found_new_stream(stream_id);
          io->restore_pos();
          pes_packet_length = io->read_uint16_be();

          mxverb(3, "mpeg_ps: id 0x%02x len %u at " LLD "\n", stream_id, pes_packet_length, io->getFilePointer() - 4 - 2);

          io->skip(pes_packet_length);

          header = io->read_uint32_be();

          break;
      }

      done |= io->eof() || (io->getFilePointer() >= PS_PROBE_SIZE);
    } // while (!done)

  } catch (...) {
  }

  sort_tracks();
  calculate_global_timecode_offset();

  io->setFilePointer(0, seek_beginning);

  if (verbose)
    mxinfo(FMT_FN "Using the MPEG PS demultiplexer.\n", ti.fname.c_str());
}

mpeg_ps_reader_c::~mpeg_ps_reader_c() {
  delete io;
}

void
mpeg_ps_reader_c::sort_tracks() {
  int i;

  for (i = 0; tracks.size() > i; ++i)
    tracks[i]->sort_key = (  'v' == tracks[i]->type ? 0x00000
                           : 'a' == tracks[i]->type ? 0x10000
                           : 's' == tracks[i]->type ? 0x20000
                           :                          0x30000)
      + (256 > tracks[i]->id ? tracks[i]->id + 256 : tracks[i]->id);

  sort(tracks.begin(), tracks.end());

  for (i = 0; tracks.size() > i; ++i)
    id2idx[tracks[i]->id] = i;

  mxverb(2, "mpeg_ps: Supported streams, sorted by ID: ");
  for (i = 0; tracks.size() > i; ++i)
    if (256 > tracks[i]->id)
      mxverb(2, "%02x ", tracks[i]->id);
    else
      mxverb(2, "bd(%02x) ", tracks[i]->id - 256);
  mxverb(2, "\n");
}

void
mpeg_ps_reader_c::calculate_global_timecode_offset() {
  // Calculate by how much the timecodes have to be offset
  if (tracks.empty())
    return;

  int i;
  int64_t min_timecode = tracks[0]->timecode_offset;
  for (i = 1; i < tracks.size(); i++)
    if ((-1 == min_timecode) || (tracks[i]->timecode_offset < min_timecode))
      min_timecode = tracks[i]->timecode_offset;

  if (-1 != min_timecode)
    for (i = 0; i < tracks.size(); i++)
      tracks[i]->timecode_offset -= min_timecode;

  mxverb(2, "mpeg_ps: Timecode offset: min was " LLD " ", min_timecode);
  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->id > 0xff)
      mxverb(2, "bd(%02x)=" LLD " ", tracks[i]->id - 256, tracks[i]->timecode_offset);
    else
      mxverb(2, "%02x=" LLD " ", tracks[i]->id, tracks[i]->timecode_offset);
  mxverb(2, "\n");
}

bool
mpeg_ps_reader_c::read_timestamp(int c,
                                 int64_t &timestamp) {
  int d, e;

  d = io->read_uint16_be();
  e = io->read_uint16_be();

  if (((c & 1) != 1) || ((d & 1) != 1) || ((e & 1) != 1))
    return false;

  timestamp = (int64_t)((((c >> 1) & 7) << 30) | ((d >> 1) << 15) | (e >> 1)) * 100000ll / 9;

  return true;
}

void
mpeg_ps_reader_c::parse_program_stream_map() {
  int len     = 0;
  int64_t pos = io->getFilePointer();

  try {
    len = io->read_uint16_be();

    if (!len || (1018 < len))
      throw false;

    if (0x00 == (io->read_uint8() & 0x80))
      throw false;

    io->skip(1);

    int prog_len = io->read_uint16_be();
    io->skip(prog_len);

    int es_map_len = io->read_uint16_be();
    es_map_len     = MXMIN(es_map_len, len - prog_len - 8);

    while (0 < es_map_len) {
      int type = io->read_uint8();
      int id   = io->read_uint8();

      if ((0xb0 <= id) && (0xef >= id)) {
        int id_offset = id - 0xb0;

        switch (type) {
          case 0x01:
            es_map[id_offset] = FOURCC('M', 'P', 'G', '1');
            break;
          case 0x02:
            es_map[id_offset] = FOURCC('M', 'P', 'G', '2');
            break;
          case 0x03:
          case 0x04:
            es_map[id_offset] = FOURCC('M', 'P', '2', ' ');
            break;
          case 0x0f:
          case 0x11:
            es_map[id_offset] = FOURCC('A', 'A', 'C', ' ');
            break;
          case 0x10:
            es_map[id_offset] = FOURCC('M', 'P', 'G', '4');
            break;
          case 0x1b:
            es_map[id_offset] = FOURCC('A', 'V', 'C', '1');
            break;
          case 0x81:
            es_map[id_offset] = FOURCC('A', 'C', '3', ' ');
            break;
        }
      }

      int plen = io->read_uint16_be();
      plen     = MXMIN(plen, es_map_len);
      io->skip(plen);
      es_map_len -= 4 + plen;
    }

  } catch (...) {
  }

  io->setFilePointer(pos + len);
}

bool
mpeg_ps_reader_c::parse_packet(int id,
                               int64_t &timestamp,
                               int &length,
                               int &full_length,
                               int &aid) {
  length      = io->read_uint16_be();
  full_length = length;

  if (   (0xbc > id) || (0xf0 <= id)
      || (id == 0xbe)           // padding stream
      || (id == 0xbf)) {        // private 2 stream
    io->skip(length);
    return false;
  }

  if (0 == length)
    return false;

  aid       = -1;
  timestamp = -1;

  uint8_t c = 0;
  // Skip stuFFing bytes
  while (0 < length) {
    c = io->read_uint8();
    length--;
    if (c != 0xff)
      break;
  }

  // Skip STD buffer size
  if ((c & 0xc0) == 0x40) {
    if (2 > length)
      return false;
    length -= 2;
    io->skip(1);
    c = io->read_uint8();
  }

  // Presentation time stamp
  if ((c & 0xf0) == 0x20) {
    if (!read_timestamp(c, timestamp))
      return false;
    length -= 4;

  } else if ((c & 0xf0) == 0x30) {
    if (!read_timestamp(c, timestamp))
      return false;
    io->skip(5);
    length -= 4 + 5;

  } else if ((c & 0xc0) == 0x80) {
    int pts_flags;
    int hdrlen;

    if ((c & 0x30) != 0x00)
      mxerror(FMT_FN "Reading encrypted VOBs is not supported.\n", ti.fname.c_str());

    pts_flags  = io->read_uint8() >> 6;
    hdrlen     = io->read_uint8();
    length    -= 2;

    if (hdrlen > length)
      return false;

    if (2 == (pts_flags & 0x02)) {
      if (hdrlen < 5)
        return false;
      c = io->read_uint8();
      if (!read_timestamp(c, timestamp))
        return false;
      length -= 5;
      hdrlen -= 5;
    }

    // DTS
    if (1 == (pts_flags & 0x01)) {
      if (hdrlen < 5)
        return false;
      io->skip(5);
      length -= 5;
      hdrlen -= 5;
    }

    if (0 < hdrlen) {
      length -= hdrlen;
      io->skip(hdrlen);
    }

    if (0xbd == id) {           // DVD audio substream
      if (4 > length)
        return false;
      aid = io->read_uint8();
      length--;

      if ((aid & 0xe0) == 0x20) {
        // Subtitles, not supported yet.
        return false;

      } else if (   ((0x80 <= aid) && (0x8f >= aid))
                 || ((0x98 <= aid) && (0xaf >= aid))
                 || ((0xc0 <= aid) && (0xcf >= aid))) {
        io->skip(3);         // number of frames, startpos
        length -= 3;

        if ((0xa0 == (aid & 0xe0)) && (3 <= length)) { // LPCM
          io->skip(3);
          length -= 3;
        }
      }
    }

  } else if (0x0f != c)
    return false;

  if (0 >= length)
    return false;

  return true;
}

void
mpeg_ps_reader_c::new_stream_v_mpeg_1_2(int id,
                                        unsigned char *buf,
                                        int length,
                                        mpeg_ps_track_ptr &track) {
  auto_ptr<M2VParser> m2v_parser(new M2VParser);

  m2v_parser->WriteData(buf, length);

  int state = m2v_parser->GetState();
  while ((MPV_PARSER_STATE_FRAME != state) && (PS_PROBE_SIZE >= io->getFilePointer())) {
    if (!find_next_packet_for_id(id, PS_PROBE_SIZE))
      break;

    int64_t timecode;
    int aid, full_length;
    if (!parse_packet(id, timecode, length, full_length, aid))
      break;

    memory_c new_buf((unsigned char *)safemalloc(length), 0, true);
    if (io->read(new_buf.get(), length) != length)
      break;

    m2v_parser->WriteData(new_buf.get(), length);

    state = m2v_parser->GetState();
  }

  if (MPV_PARSER_STATE_FRAME != state) {
    mxverb(3, "MPEG PS: blacklisting id %d for supposed type MPEG1/2\n", id);
    blacklisted_ids[id] = true;
    return;
  }

  MPEG2SequenceHeader seq_hdr = m2v_parser->GetSequenceHeader();
  auto_ptr<MPEGFrame> frame(m2v_parser->ReadFrame());
  if (frame.get() == NULL)
    throw false;

  track->fourcc         = FOURCC('M', 'P', 'G', '0' + m2v_parser->GetMPEGVersion());
  track->v_version      = m2v_parser->GetMPEGVersion();
  track->v_width        = seq_hdr.width;
  track->v_height       = seq_hdr.height;
  track->v_frame_rate   = seq_hdr.frameRate;
  track->v_aspect_ratio = seq_hdr.aspectRatio;

  if ((0 >= track->v_aspect_ratio) || (1 == track->v_aspect_ratio))
    track->v_dwidth = track->v_width;
  else
    track->v_dwidth = (int)(track->v_height * track->v_aspect_ratio);
  track->v_dheight  = track->v_height;

  MPEGChunk *raw_seq_hdr = m2v_parser->GetRealSequenceHeader();
  if (NULL != raw_seq_hdr) {
    track->raw_seq_hdr      = (unsigned char *) safememdup(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
    track->raw_seq_hdr_size = raw_seq_hdr->GetSize();
  }

  track->use_buffer(128000);
}

void
mpeg_ps_reader_c::new_stream_v_avc(int id,
                                   unsigned char *buf,
                                   int length,
                                   mpeg_ps_track_ptr &track) {
  mpeg4::p10::avc_es_parser_c parser;

  parser.ignore_nalu_size_length_errors();
  if (map_has_key(ti.nalu_size_lengths, tracks.size()))
    parser.set_nalu_size_length(ti.nalu_size_lengths[0]);
  else if (map_has_key(ti.nalu_size_lengths, -1))
    parser.set_nalu_size_length(ti.nalu_size_lengths[-1]);

  parser.add_bytes(buf, length);

  while (!parser.headers_parsed() && (PS_PROBE_SIZE >= io->getFilePointer())) {
    if (!find_next_packet_for_id(id, PS_PROBE_SIZE))
      break;

    int aid, full_length;
    int64_t timecode;
    if (!parse_packet(id, timecode, length, full_length, aid))
      break;
    memory_c new_buf((unsigned char *)safemalloc(length), 0, true);
    if (io->read(new_buf.get(), length) != length)
      break;

    parser.add_bytes(new_buf.get(), length);
  }

  if (!parser.headers_parsed())
    throw false;

  track->v_avcc = parser.get_avcc();

  try {
    mm_mem_io_c avcc(track->v_avcc->get(), track->v_avcc->get_size());
    mm_mem_io_c new_avcc(NULL, track->v_avcc->get_size(), 1024);
    memory_cptr nalu(new memory_c());
    int num_sps, sps, sps_length;
    sps_info_t sps_info;

    avcc.read(nalu, 5);
    new_avcc.write(nalu);

    num_sps = avcc.read_uint8();
    new_avcc.write_uint8(num_sps);
    num_sps &= 0x1f;

    for (sps = 0; sps < num_sps; sps++) {
      bool abort;

      sps_length = avcc.read_uint16_be();
      if ((sps_length + avcc.getFilePointer()) >= track->v_avcc->get_size())
        sps_length = track->v_avcc->get_size() - avcc.getFilePointer();
      avcc.read(nalu, sps_length);

      abort = false;
      if ((0 < sps_length) && ((nalu->get()[0] & 0x1f) == 7)) {
        nalu_to_rbsp(nalu);
        if (!mpeg4::p10::parse_sps(nalu, sps_info, true))
          throw false;
        rbsp_to_nalu(nalu);
        abort = true;
      }

      new_avcc.write_uint16_be(nalu->get_size());
      new_avcc.write(nalu);

      if (abort) {
        avcc.read(nalu, track->v_avcc->get_size() - avcc.getFilePointer());
        new_avcc.write(nalu);
        break;
      }
    }

    track->fourcc   = FOURCC('A', 'V', 'C', '1');
    track->v_avcc   = memory_cptr(new memory_c(new_avcc.get_and_lock_buffer(), new_avcc.getFilePointer(), true));
    track->v_width  = sps_info.width;
    track->v_height = sps_info.height;

    if (sps_info.ar_found) {
      float aspect_ratio = (float)sps_info.width / (float)sps_info.height * (float)sps_info.par_num / (float)sps_info.par_den;
      track->v_aspect_ratio = aspect_ratio;

      if (aspect_ratio > ((float)track->v_width / (float)track->v_height)) {
        track->v_dwidth  = irnd(track->v_height * aspect_ratio);
        track->v_dheight = track->v_height;

      } else {
        track->v_dwidth  = track->v_width;
        track->v_dheight = irnd(track->v_width / aspect_ratio);
      }
    }

    track->use_buffer(256000);

  } catch (...) {
    throw false;
  }
}

void
mpeg_ps_reader_c::new_stream_a_mpeg(int id,
                                    unsigned char *buf,
                                    int length,
                                    mpeg_ps_track_ptr &track) {
  mp3_header_t header;

  if (-1 == find_mp3_header(buf, length))
    throw false;

  decode_mp3_header(buf, &header);
  track->a_channels    = header.channels;
  track->a_sample_rate = header.sampling_frequency;
  track->fourcc        = FOURCC('M', 'P', '0' + header.layer, ' ');
}

void
mpeg_ps_reader_c::new_stream_a_ac3(int id,
                                   unsigned char *buf,
                                   int length,
                                   mpeg_ps_track_ptr &track) {
  ac3_header_t header;

  if (-1 == find_ac3_header(buf, length, &header, false))
    throw false;

  mxverb(2, "first ac3 header bsid %d channels %d sample_rate %d bytes %d samples %d\n", header.bsid, header.channels, header.sample_rate, header.bytes, header.samples);

  track->a_channels    = header.channels;
  track->a_sample_rate = header.sample_rate;
  track->a_bsid        = header.bsid;
}

void
mpeg_ps_reader_c::new_stream_a_dts(int id,
                                   unsigned char *buf,
                                   int length,
                                   mpeg_ps_track_ptr &track) {
  if (-1 == find_dts_header(buf, length, &track->dts_header, false))
    throw false;
}

void
mpeg_ps_reader_c::found_new_stream(int id) {
  if (((0xc0 > id) || (0xef < id)) && (0xbd != id))
    return;

  try {
    int64_t timecode;
    int length, aid, full_length;

    if (!parse_packet(id, timecode, length, full_length, aid))
      throw false;

    if ((0xbd == id) && (-1 == aid))
      return;

    if (0xbd == id)             // DVD audio substream
      id = 256 + aid;

    if (blacklisted_ids[id])
      return;

    if (-1 != id2idx[id]) {
      mpeg_ps_track_ptr &track = tracks[id2idx[id]];
      if ((-1 != timecode) && ((-1 == track->timecode_offset) || (timecode < track->timecode_offset)))
        track->timecode_offset = timecode;
      return;
    }

    mxverb(2, "MPEG PS: new stream id 0x%04x\n", id);

    mpeg_ps_track_ptr track(new mpeg_ps_track_t);
    track->timecode_offset = timecode;
    track->type            = '?';

    if (0xff < id) {
      track->type = 'a';

      if ((0x20 <= aid) && (0x3f >= aid)) {
        track->type   = 's';
        track->fourcc = FOURCC('V', 'S', 'U', 'B');

      } else if (((0x80 <= aid) && (0x87 >= aid)) || ((0xc0 <= aid) && (0xc7 >= aid)))
        track->fourcc = FOURCC('A', 'C', '3', ' ');

      else if ((0x88 <= aid) && (0x8f >= aid))
        track->fourcc = FOURCC('D', 'T', 'S', ' ');

      else if ((0xa0 <= aid) && (0xa7 >= aid))
        track->fourcc = FOURCC('P', 'C', 'M', ' ');

      else
        track->type = '?';

    } else if (0xe0 > id) {
      track->type   = 'a';
      track->fourcc = FOURCC('M', 'P', '2', ' ');

    } else if ((0xe0 <= id) && (0xef >= id)) {
      track->type   = 'v';
      track->fourcc = FOURCC('M', 'P', 'G', '0' + version);
    }

    if ('?' == track->type)
      return;

    memory_c af_buf((unsigned char *)safemalloc(length), 0, true);
    unsigned char *buf = af_buf.get();
    if (io->read(buf, length) != length)
      throw false;

    if ((FOURCC('M', 'P', 'G', '1') == track->fourcc) || (FOURCC('M', 'P', 'G', '2') == track->fourcc)) {
      if (4 > length)
        throw false;

      uint32_t code = get_uint32_be(buf);

      if (0x00000001 == code)
        new_stream_v_avc(id, buf, length, track);

      else if (   (MPEGVIDEO_SEQUENCE_START_CODE      == code)
               || (MPEGVIDEO_PACKET_START_CODE        == code)
               || (MPEGVIDEO_SYSTEM_HEADER_START_CODE == code))
        new_stream_v_mpeg_1_2(id, buf, length, track);

      else
        throw false;

    } else if (FOURCC('M', 'P', '2', ' ') == track->fourcc)
      new_stream_a_mpeg(id, buf, length, track);

    else if (FOURCC('A', 'C', '3', ' ') == track->fourcc)
      new_stream_a_ac3(id, buf, length, track);

    else if (FOURCC('D', 'T', 'S', ' ') == track->fourcc)
      new_stream_a_dts(id, buf, length, track);

    else
      // Unsupported track type
      throw false;

    track->id  = id;
    id2idx[id] = tracks.size();
    tracks.push_back(track);

  } catch (bool err) {
    blacklisted_ids[id] = true;

  } catch (const char *msg) {
    mxerror(FMT_FN "%s\n", ti.fname.c_str(), msg);

  } catch (...) {
    mxerror(FMT_FN "Error parsing a MPEG PS packet during the header reading phase. This stream seems to be badly damaged.\n", ti.fname.c_str());
  }
}

bool
mpeg_ps_reader_c::find_next_packet(int &id,
                                   int64_t max_file_pos) {
  try {
    uint32_t header;

    header = io->read_uint32_be();
    while (1) {
      uint8_t byte;

      if ((-1 != max_file_pos) && (io->getFilePointer() > max_file_pos))
        return false;

      switch (header) {
        case MPEGVIDEO_PACKET_START_CODE:
          if (-1 == version) {
            byte = io->read_uint8();
            if ((byte & 0xc0) != 0)
              version = 2;      // MPEG-2 PS
            else
              version = 1;
            io->skip(-1);
          }

          io->skip(2 * 4);   // pack header
          if (2 == version) {
            io->skip(1);
            byte = io->read_uint8() & 0x07;
            io->skip(byte);  // stuffing bytes
          }
          header = io->read_uint32_be();
          break;

        case MPEGVIDEO_SYSTEM_HEADER_START_CODE:
          io->skip(2 * 4);   // system header
          byte = io->read_uint8();
          while ((byte & 0x80) == 0x80) {
            io->skip(2);     // P-STD info
            byte = io->read_uint8();
          }
          io->skip(-1);
          header = io->read_uint32_be();
          break;

        case MPEGVIDEO_MPEG_PROGRAM_END_CODE:
          if (!resync_stream(header))
            return false;

        case MPEGVIDEO_PROGRAM_STREAM_MAP_START_CODE:
          parse_program_stream_map();
          break;

        default:
          if (!mpeg_is_start_code(header)) {
            if (!resync_stream(header))
              return false;
            continue;
          }

          id = header & 0xff;
          return true;

          break;
      }
    }
  } catch(...) {
    return false;
  }
}

bool
mpeg_ps_reader_c::find_next_packet_for_id(int id,
                                          int64_t max_file_pos) {
  int new_id;

  try {
    while (find_next_packet(new_id, max_file_pos)) {
      if (id == new_id)
        return true;
      io->skip(io->read_uint16_be());
    }
  } catch(...) {
  }
  return false;
}

bool
mpeg_ps_reader_c::resync_stream(uint32_t &header) {
  mxverb(2, "MPEG PS: synchronisation lost at " LLD "; looking for start code\n", io->getFilePointer());

  try {
    while (1) {
      header <<= 8;
      header  |= io->read_uint8();
      if (mpeg_is_start_code(header))
        break;
    }

    mxverb(2, "resync succeeded at " LLD ", header 0x%08x\n", io->getFilePointer() - 4, header);

    return true;

  } catch (...) {
    mxverb(2, "resync failed: exception caught\n");
    return false;
  }
}

void
mpeg_ps_reader_c::create_packetizer(int64_t id) {
  if ((0 > id) || (tracks.size() <= id))
    return;
  if (0 == tracks[id]->ptzr)
    return;
  if (!demuxing_requested(tracks[id]->type, id))
    return;

  ti.id = id;
  mpeg_ps_track_ptr &track = tracks[id];
  if ('a' == track->type) {
    if (   (FOURCC('M', 'P', '1', ' ') == track->fourcc)
        || (FOURCC('M', 'P', '2', ' ') == track->fourcc)
        || (FOURCC('M', 'P', '3', ' ') == track->fourcc)) {
      if (verbose)
        mxinfo(FMT_TID "Using the MPEG audio output module.\n", ti.fname.c_str(), id);
      track->ptzr = add_packetizer(new mp3_packetizer_c(this, track->a_sample_rate, track->a_channels, true, ti));

    } else if (FOURCC('A', 'C', '3', ' ') == track->fourcc) {
      if (verbose)
        mxinfo(FMT_TID "Using the %sAC3 output module.\n", ti.fname.c_str(), id, 16 == track->a_bsid ? "E" : "");
      track->ptzr = add_packetizer(new ac3_packetizer_c(this, track->a_sample_rate, track->a_channels, track->a_bsid, ti));

    } else if (FOURCC('D', 'T', 'S', ' ') == track->fourcc) {
      if (verbose)
        mxinfo(FMT_TID "Using the DTS output module.\n", ti.fname.c_str(), id);
      track->ptzr = add_packetizer(new dts_packetizer_c(this, track->dts_header, ti, true));

    } else
      mxerror("mpeg_ps_reader: Should not have happened #1. %s", BUGMSG);

  } else {                      // if (track->type == 'a')
    if (   (FOURCC('M', 'P', 'G', '1') == track->fourcc)
        || (FOURCC('M', 'P', 'G', '2') == track->fourcc)) {
      if (verbose)
        mxinfo(FMT_TID "Using the MPEG-1/2 video output module.\n", ti.fname.c_str(), id);

      ti.private_data = track->raw_seq_hdr;
      ti.private_size = track->raw_seq_hdr_size;
      track->ptzr     = add_packetizer(new mpeg1_2_video_packetizer_c(this, track->v_version, track->v_frame_rate, track->v_width, track->v_height,
                                                                      track->v_dwidth, track->v_dheight, false, ti));
      ti.private_data = NULL;
      ti.private_size = 0;

    } else if (track->fourcc == FOURCC('A', 'V', 'C', '1')) {
      if (verbose)
        mxinfo(FMT_TID "Using the MPEG-4 part 10 ES video output module.\n", ti.fname.c_str(), id);
      track->ptzr = add_packetizer(new mpeg4_p10_es_video_packetizer_c(this, track->v_avcc, track->v_width, track->v_height, ti));

    } else
      mxerror("mpeg_ps_reader: Should not have happened #2. %s", BUGMSG);
  }

  if (-1 != track->timecode_offset)
    PTZR(track->ptzr)->ti.tcsync.displacement += track->timecode_offset;
}

void
mpeg_ps_reader_c::create_packetizers() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    create_packetizer(i);
}

void
mpeg_ps_reader_c::add_available_track_ids() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    available_track_ids.push_back(i);
}

file_status_e
mpeg_ps_reader_c::read(generic_packetizer_c *,
                       bool) {
  int64_t timecode, packet_pos;
  int new_id, length, aid, full_length;
  unsigned char *buf;

  if (file_done)
    return FILE_STATUS_DONE;

  try {
    while (find_next_packet(new_id)) {
      if ((0xbd != new_id) && ((-1 == id2idx[new_id]) || (-1 == tracks[id2idx[new_id]]->ptzr))) {
        io->skip(io->read_uint16_be());
        continue;
      }

      packet_pos = io->getFilePointer() - 4;
      if (!parse_packet(new_id, timecode, length, full_length, aid)) {
        mxverb(2, "mpeg_ps: packet_parse failed @ " LLD ", skipping %d\n", packet_pos, full_length);
        io->setFilePointer(packet_pos + 4 + 2 + full_length);
        continue;
      }

      if (0xbd == new_id)
        new_id = 256 + aid;

      if ((-1 == id2idx[new_id]) || (-1 == tracks[id2idx[new_id]]->ptzr)) {
        io->skip(length);
        continue;
      }

      mpeg_ps_track_t *track = tracks[id2idx[new_id]].get();

      mxverb(3, "mpeg_ps: packet for %d length %d at " LLD " timecode " LLD "\n", new_id, length, packet_pos, timecode);

      if (0 < track->buffer_size) {
        if (((track->buffer_usage + length) > track->buffer_size)) {
          PTZR(track->ptzr)-> process(new packet_t(new memory_c(track->buffer, track->buffer_usage, false)));
          track->buffer_usage = 0;
        }

        track->assert_buffer_size(length);

        if (io->read(&track->buffer[track->buffer_usage], length) != length) {
          mxverb(2, "mpeg_ps: file_done: io->read\n");
          return finish();
        }

        track->buffer_usage += length;

      } else {
        buf = (unsigned char *)safemalloc(length);

        if (io->read(buf, length) != length) {
          safefree(buf);
          mxverb(2, "mpeg_ps: file_done: io->read\n");
          return finish();
        }

        PTZR(track->ptzr)->process(new packet_t(new memory_c(buf, length, true)));
      }

      return FILE_STATUS_MOREDATA;
    }
    mxverb(2, "mpeg_ps: file_done: !find_next_packet\n");

  } catch(...) {
    mxverb(2, "mpeg_ps: file_done: exception\n");
  }

  return finish();
}

file_status_e
mpeg_ps_reader_c::finish() {
  if (file_done)
    return FILE_STATUS_DONE;

  vector<mpeg_ps_track_ptr>::iterator track;

  mxforeach(track, tracks) {
    if (0 < (*track)->buffer_usage) {
      memory_c *mem = new memory_c((*track)->buffer, (*track)->buffer_usage);
      PTZR((*track)->ptzr)->process(new packet_t(mem));
    }
  }

  file_done = true;
  flush_packetizers();

  return FILE_STATUS_DONE;
}

int
mpeg_ps_reader_c::get_progress() {
  return 100 * io->getFilePointer() / size;
}

void
mpeg_ps_reader_c::identify() {
  vector<string> verbose_info;
  int i;

  id_result_container(mxsprintf("MPEG %d program stream (PS)", version));

  for (i = 0; i < tracks.size(); i++) {
    mpeg_ps_track_ptr &track = tracks[i];

    verbose_info.clear();

    if (FOURCC('A', 'V', 'C', '1') == track->fourcc) {
      if (0 != track->v_aspect_ratio)
        verbose_info.push_back(mxsprintf("display_dimensions:%dx%d", track->v_dwidth, track->v_dheight));
      verbose_info.push_back("packetizer:mpeg4_p10_es_video");
    }

    verbose_info.push_back(mxsprintf("stream_id:%#x", track->id));

    id_result_track(i, 'a' == track->type ? ID_RESULT_TRACK_AUDIO : ID_RESULT_TRACK_VIDEO,
                      FOURCC('M', 'P', 'G', '1') == track->fourcc ? "MPEG-1"
                    : FOURCC('M', 'P', 'G', '2') == track->fourcc ? "MPEG-2"
                    : FOURCC('A', 'V', 'C', '1') == track->fourcc ? "AVC/h.264"
                    : FOURCC('M', 'P', '1', ' ') == track->fourcc ? "MPEG-1 layer 1"
                    : FOURCC('M', 'P', '2', ' ') == track->fourcc ? "MPEG-1 layer 2"
                    : FOURCC('M', 'P', '3', ' ') == track->fourcc ? "MPEG-1 layer 3"
                    : FOURCC('A', 'C', '3', ' ') == track->fourcc ? (16 == track->a_bsid ? "EAC3" : "AC3")
                    : FOURCC('D', 'T', 'S', ' ') == track->fourcc ? "DTS"
                    : FOURCC('P', 'C', 'M', ' ') == track->fourcc ? "PCM"
                    : FOURCC('L', 'P', 'C', 'M') == track->fourcc ? "LPCM"
                    :                                               "unknown",
                    verbose_info);
  }
}

// ------------------------------------------------------------------------

#define TS_CONSECUTIVE_PACKETS 16
#define TS_PROBE_SIZE (2 * TS_CONSECUTIVE_PACKETS * 204)

int mpeg_ts_reader_c::potential_packet_sizes[] = { 188, 192, 204, 0 };

bool
mpeg_ts_reader_c::probe_file(mm_io_c *io,
                             int64_t size) {
  try {
    vector<int> positions;
    size = size > TS_PROBE_SIZE ? TS_PROBE_SIZE : size;
    memory_cptr buffer(new memory_c(safemalloc(size), size, true));
    unsigned char *mem = buffer->get();
    int i, k;

    io->setFilePointer(0, seek_beginning);
    size = io->read(mem, size);

    for (i = 0; i < size; ++i)
      if (0x47 == mem[i])
        positions.push_back(i);

    for (i = 0; positions.size() > i; ++i) {
      int pos = positions[i];

      for (k = 0; 0 != potential_packet_sizes[k]; ++k) {
        int packet_size    = potential_packet_sizes[k];
        int num_startcodes = 1;

        while ((TS_CONSECUTIVE_PACKETS > num_startcodes) && (pos < size) && (0x47 == mem[pos])) {
          pos += packet_size;
          ++num_startcodes;
        }

        if (TS_CONSECUTIVE_PACKETS <= num_startcodes) {
          id_result_container_unsupported(io->get_file_name(), "MPEG Transport Stream (TS)");
          // Never reached:
          return true;
        }
      }
    }

  } catch (...) {
  }

  return false;
}
