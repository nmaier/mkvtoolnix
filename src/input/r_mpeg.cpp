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
#include "p_ac3.h"
#include "p_dts.h"
#include "p_mp3.h"
#include "p_video.h"

#define PROBESIZE 4
#define READ_SIZE 1024 * 1024

int
mpeg_es_reader_c::probe_file(mm_io_c *mm_io,
                             int64_t size) {
  unsigned char *buf;
  int num_read, i;
  uint32_t value;
  M2VParser parser;

  if (size < PROBESIZE)
    return 0;
  try {
    buf = (unsigned char *)safemalloc(READ_SIZE);
    mm_io->setFilePointer(0, seek_beginning);
    num_read = mm_io->read(buf, READ_SIZE);
    if (num_read < 4) {
      safefree(buf);
      return 0;
    }
    mm_io->setFilePointer(0, seek_beginning);

    // MPEG TS starts with 0x47.
    if (buf[0] == 0x47) {
      safefree(buf);
      return 0;
    }

    // MPEG PS starts with 0x000001ba.
    value = get_uint32_be(buf);
    if (value == MPEGVIDEO_PACKET_START_CODE) {
      safefree(buf);
      return 0;
    }

    // Let's look for a MPEG ES start code inside the first 1 MB.
    for (i = 4; i <= num_read; i++) {
      if (value == MPEGVIDEO_SEQUENCE_START_CODE)
        break;
      if (i < num_read) {
        value <<= 8;
        value |= buf[i];
      }
    }
    safefree(buf);
    if (value != MPEGVIDEO_SEQUENCE_START_CODE)
      return 0;

    // Let's try to read one frame.
    if (!read_frame(parser, *mm_io, READ_SIZE))
      return 0;

  } catch (exception &ex) {
    return 0;
  }

  return 1;
}

mpeg_es_reader_c::mpeg_es_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {

  try {
    MPEG2SequenceHeader seq_hdr;
    M2VParser parser;
    MPEGChunk *raw_seq_hdr;

    mm_io = new mm_file_io_c(ti->fname);
    size = mm_io->get_size();

    // Let's find the first frame. We need its information like
    // resolution, MPEG version etc.
    if (!read_frame(parser, *mm_io, 1024 * 1024)) {
      delete mm_io;
      throw "";
    }

    mm_io->setFilePointer(0);
    version = parser.GetMPEGVersion();
    seq_hdr = parser.GetSequenceHeader();
    width = seq_hdr.width;
    height = seq_hdr.height;
    frame_rate = seq_hdr.frameRate;
    aspect_ratio = seq_hdr.aspectRatio;
    if ((aspect_ratio <= 0) || (aspect_ratio == 1))
      dwidth = width;
    else
      dwidth = (int)(height * aspect_ratio);
    dheight = height;
    raw_seq_hdr = parser.GetRealSequenceHeader();
    if (raw_seq_hdr != NULL) {
      ti->private_data = (unsigned char *)
        safememdup(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
      ti->private_size = raw_seq_hdr->GetSize();
    }

    mxverb(2, "mpeg_es_reader: v %d width %d height %d FPS %e AR %e\n",
           version, width, height, frame_rate, aspect_ratio);

  } catch (exception &ex) {
    throw error_c("mpeg_es_reader: Could not open the file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the MPEG ES demultiplexer.\n", ti->fname.c_str());
}

mpeg_es_reader_c::~mpeg_es_reader_c() {
  delete mm_io;
}

void
mpeg_es_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  add_packetizer(new mpeg_12_video_packetizer_c(this, version, frame_rate,
                                                width, height, dwidth, dheight,
                                                ti));

  mxinfo(FMT_TID "Using the MPEG-1/2 video output module.\n",
         ti->fname.c_str(), (int64_t)0);
}

file_status_e
mpeg_es_reader_c::read(generic_packetizer_c *,
                       bool) {
  unsigned char *chunk;
  int num_read;

  chunk = (unsigned char *)safemalloc(20000);
  num_read = mm_io->read(chunk, 20000);
  if (num_read <= 0) {
    safefree(chunk);
    return FILE_STATUS_DONE;
  }

  memory_c mem(chunk, num_read, true);
  PTZR0->process(mem);

  bytes_processed = mm_io->getFilePointer();

  return FILE_STATUS_MOREDATA;
}

bool
mpeg_es_reader_c::read_frame(M2VParser &parser,
                             mm_io_c &in,
                             int64_t max_size) {
  int bytes_probed;

  bytes_probed = 0;
  while (true) {
    int state;

    state = parser.GetState();

    if (state == MPV_PARSER_STATE_NEED_DATA) {
      unsigned char *buffer;
      int bytes_read, bytes_to_read;

      if ((max_size != -1) && (bytes_probed > max_size))
        return false;

      bytes_to_read = (parser.GetFreeBufferSpace() < READ_SIZE) ?
        parser.GetFreeBufferSpace() : READ_SIZE;
      buffer = new unsigned char[bytes_to_read];
      bytes_read = in.read(buffer, bytes_to_read);
      if (bytes_read == 0) {
        delete [] buffer;
        break;
      }
      bytes_probed += bytes_read;

      parser.WriteData(buffer, bytes_read);
      delete [] buffer;

    } else if (state == MPV_PARSER_STATE_FRAME)
      return true;

    else if ((state == MPV_PARSER_STATE_EOS) ||
             (state == MPV_PARSER_STATE_ERROR))
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
  mxinfo("File '%s': container: MPEG elementary stream (ES)\n" 
         "Track ID 0: video (MPEG %d)\n", ti->fname.c_str(), version);
}

// ------------------------------------------------------------------------

#define PS_PROBE_SIZE 1024 * 1024

int
mpeg_ps_reader_c::probe_file(mm_io_c *mm_io,
                             int64_t size) {
  try {
    autofree_ptr<unsigned char> af_buf(safemalloc(PS_PROBE_SIZE));
    unsigned char *buf = af_buf;
    int num_read;

    mm_io->setFilePointer(0, seek_beginning);
    num_read = mm_io->read(buf, PS_PROBE_SIZE);
    if (num_read < 4)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);

    if (get_uint32_be(buf) != MPEGVIDEO_PACKET_START_CODE)
      return 0;

    return 1;

  } catch (exception &ex) {
    return 0;
  }
}

mpeg_ps_reader_c::mpeg_ps_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  try {
    uint32_t header;
    uint8_t byte;
    bool done;
    int i;

    mm_io = new mm_file_io_c(ti->fname);
    size = mm_io->get_size();
    file_done = false;

    bytes_processed = 0;

    for (i = 0; i < 512; i++)
      id2idx[i] = -1;
    header = mm_io->read_uint32_be();
    done = mm_io->eof();
    version = -1;

    while (!done) {
      uint8_t stream_id;
      uint16_t pes_packet_length;

      switch (header) {
        case MPEGVIDEO_PACKET_START_CODE:
          mxverb(3, "mpeg_ps: packet start at %lld\n",
                 mm_io->getFilePointer() - 4);

          if (version == -1) {
            byte = mm_io->read_uint8();
            if ((byte & 0xc0) != 0)
              version = 2;      // MPEG-2 PS
            else
              version = 1;
            mm_io->skip(-1);
          }

          mm_io->skip(2 * 4);   // pack header
          if (version == 2) {
            mm_io->skip(1);
            byte = mm_io->read_uint8() & 0x07;
            mm_io->skip(byte);  // stuffing bytes
          }
          header = mm_io->read_uint32_be();
          break;

        case MPEGVIDEO_SYSTEM_HEADER_START_CODE:
          mxverb(3, "mpeg_ps: system header start code at %lld\n",
                 mm_io->getFilePointer() - 4);

          mm_io->skip(2 * 4);   // system header
          byte = mm_io->read_uint8();
          while ((byte & 0x80) == 0x80) {
            mm_io->skip(2);     // P-STD info
            byte = mm_io->read_uint8();
          }
          mm_io->skip(-1);
          header = mm_io->read_uint32_be();
          break;

        case MPEGVIDEO_MPEG_PROGRAM_END_CODE:
          done = true;
          break;

        default:
          if (!mpeg_is_start_code(header)) {
            mxverb(3, "mpeg_ps: unknown header 0x%08x at %lld\n",
                   header, mm_io->getFilePointer() - 4);
            done = true;
            break;
          }

          stream_id = header & 0xff;
          mm_io->save_pos();
          found_new_stream(stream_id);
          mm_io->restore_pos();
          pes_packet_length = mm_io->read_uint16_be();
          mxverb(3, "mpeg_ps: id 0x%02x len %u at %lld\n", stream_id,
                 pes_packet_length, mm_io->getFilePointer() - 4 - 2);

          mm_io->skip(pes_packet_length);

          header = mm_io->read_uint32_be();

          break;
      }

      done |= mm_io->eof() || (mm_io->getFilePointer() >= PS_PROBE_SIZE);
    } // while (!done)

    mxverb(2, "mpeg_ps: Streams found: ");
    for (i = 0; i < 256; i++)
      if (id2idx[i] != -1)
        mxverb(2, "%02x ", i);
    for (i = 256 ; i < 512; i++)
      if (id2idx[i] != -1)
        mxverb(2, "bd(%02x) ", i - 256);
    mxverb(2, "\n");

    // Calculate by how much the timecodes have to be offset
    if (tracks.size() > 0) {
      int64_t min_timecode;

      min_timecode = tracks[0]->timecode_offset;
      for (i = 1; i < tracks.size(); i++)
        if (tracks[i]->timecode_offset < min_timecode)
          min_timecode = tracks[i]->timecode_offset;
      for (i = 0; i < tracks.size(); i++)
        tracks[i]->timecode_offset -= min_timecode;

      mxverb(2, "mpeg_ps: Timecode offset: min was %lld ", min_timecode);
      for (i = 0; i < tracks.size(); i++)
        if (tracks[i]->id > 0xff)
          mxverb(2, "bd(%02x)=%lld ", tracks[i]->id - 256,
                 tracks[i]->timecode_offset);
        else
          mxverb(2, "%02x=%lld ", tracks[i]->id, tracks[i]->timecode_offset);
      mxverb(2, "\n");
    }

    mm_io->setFilePointer(0, seek_beginning);

  } catch (exception &ex) {
    throw error_c("mpeg_ps_reader: Could not open the file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the MPEG PS demultiplexer.\n", ti->fname.c_str());
}

mpeg_ps_reader_c::~mpeg_ps_reader_c() {
  delete mm_io;
}

bool
mpeg_ps_reader_c::read_timestamp(int c,
                                 int64_t &timestamp) {
  int d, e;

  d = mm_io->read_uint16_be();
  e = mm_io->read_uint16_be();

  if (((c & 1) != 1) || ((d & 1) != 1) || ((e & 1) != 1))
    return false;

  timestamp = (((c >> 1) & 7) << 30) | ((d >> 1) << 15) | (e >> 1);
  timestamp = timestamp * 100000 / 9;

  return true;
}

bool
mpeg_ps_reader_c::parse_packet(int id,
                               int64_t &timestamp,
                               int &length,
                               int &aid) {
  uint8_t c;

  length = mm_io->read_uint16_be();
  if ((id < 0xbc) || (id >= 0xf0) ||
      (id == 0xbe) ||           // padding stream
      (id == 0xbf)) {           // private 2 stream
    mm_io->skip(length);
    return false;
  }

  if (length == 0)
    return false;

  aid = -1;

  c = 0;
  // Skip stuFFing bytes
  while (length > 0) {
    c = mm_io->read_uint8();
    length--;
    if (c != 0xff)
      break;
  }

  // Skip STD buffer size
  if ((c & 0xc0) == 0x40) {
    if (length < 2)
      return false;
    length -= 2;
    mm_io->skip(1);
    c = mm_io->read_uint8();
  }

  // Presentation time stamp
  if ((c & 0xf0) == 0x20) {
    if (!read_timestamp(c, timestamp))
      return false;
    length -= 4;

  } else if ((c & 0xf0) == 0x30) {
    if (!read_timestamp(c, timestamp))
      return false;
    length -= 4;
    mm_io->skip(5);
    length -= 5;

  } else if ((c & 0xc0) == 0x80) {
    int pts_flags;
    int hdrlen;

    if ((c & 0x30) != 0x00)
      mxerror(FMT_FN "Reading encrypted VOBs is not supported.\n",
              ti->fname.c_str());
    pts_flags = mm_io->read_uint8() >> 6;
    hdrlen = mm_io->read_uint8();
    length -= 2;
    if (hdrlen > length)
      return false;

    if ((pts_flags & 2) == 2) {
      if (hdrlen < 5)
        return false;
      c = mm_io->read_uint8();
      if (!read_timestamp(c, timestamp))
        return false;
      length -= 5;
      hdrlen -= 5;

    }
    if (pts_flags == 3) {
      if (hdrlen < 5)
        return false;
      mm_io->skip(5);
      length -= 5;
      hdrlen -= 5;
    }

    if (hdrlen > 0) {
      length -= hdrlen;
      mm_io->skip(hdrlen);
    }

    if (id == 0xbd) {           // DVD audio substream
      if (length < 4)
        return false;
      aid = mm_io->read_uint8();
      length--;

      if ((aid >= 0x80) && (aid <= 0xbf)) {
        mm_io->skip(3);         // number of frames, startpos
        length -= 3;

        if (aid >= 0xa0) {      // LPCM
          if (length < 3)
            return false;
          mm_io->skip(3);
          length -= 3;
        }
      }
    }

  } else if (c != 0x0f)
    return false;

  if (length <= 0)
    return false;

  return true;
}

void
mpeg_ps_reader_c::found_new_stream(int id) {
  if (((id < 0xc0) || (id > 0xef)) && (id != 0xbd))
    return;

  try {
    int64_t timecode;
    int length, aid;
    unsigned char *buf;

    if (!parse_packet(id, timecode, length, aid))
      throw false;

    if ((id == 0xbd) && (aid == -1))
      return;

    if (id == 0xbd)             // DVD audio substream
      id = 256 + aid;

    if (id2idx[id] != -1)
      return;

    mpeg_ps_track_ptr track(new mpeg_ps_track_t);
    track->timecode_offset = timecode;

    if (id > 0xff) {
      if ((aid >= 0x20) && (aid <= 0x3f)) {
        track->type = 's';
        track->fourcc = FOURCC('V', 'S', 'U', 'B');
      } else if ((aid >= 0x80) && (aid <= 0x87)) {
        track->type = 'a';
        track->fourcc = FOURCC('A', 'C', '3', ' ');
      } else if ((aid >= 0x88) && (aid <= 0x90)) {
        track->type = 'a';
        track->fourcc = FOURCC('D', 'T', 'S', ' ');
      } else if ((aid >= 0xa0) && (aid <= 0xa8)) {
        track->type = 'a';
        track->fourcc = FOURCC('P', 'C', 'M', ' ');
      }
    } else if (id < 0xe0) {
      track->type = 'a';
      track->fourcc = FOURCC('M', 'P', '2', ' ');
    } else {
      track->type = 'v';
      track->fourcc = FOURCC('m', 'p', 'g', '0' + version);
    }

    if (track->type == '?')
      return;

    autofree_ptr<unsigned char> af_buf(safemalloc(length));
    buf = af_buf;
    if (mm_io->read(buf, length) != length)
      throw false;

    if (track->type == 'v') {
      if ((track->fourcc == FOURCC('m', 'p', 'g', '1')) ||
          (track->fourcc == FOURCC('m', 'p', 'g', '2'))) {
        int state;
        auto_ptr<M2VParser> m2v_parser(new M2VParser);
        MPEG2SequenceHeader seq_hdr;
        MPEGChunk *raw_seq_hdr;

        m2v_parser->WriteData(buf, length);

        state = m2v_parser->GetState();
        while (state != MPV_PARSER_STATE_FRAME) {
          if (!find_next_packet_for_id(id))
            throw false;

          if (!parse_packet(id, timecode, length, aid))
            throw false;
          autofree_ptr<unsigned char> new_buf(safemalloc(length));
          if (mm_io->read(new_buf, length) != length)
            throw false;

          m2v_parser->WriteData(new_buf, length);

          state = m2v_parser->GetState();
        }

        seq_hdr = m2v_parser->GetSequenceHeader();
        auto_ptr<MPEGFrame> frame(m2v_parser->ReadFrame());
        if (frame.get() == NULL)
          throw false;

        track->v_version = m2v_parser->GetMPEGVersion();
        track->v_width = seq_hdr.width;
        track->v_height = seq_hdr.height;
        track->v_frame_rate = seq_hdr.frameRate;
        track->v_aspect_ratio = seq_hdr.aspectRatio;
        if ((track->v_aspect_ratio <= 0) || (track->v_aspect_ratio == 1))
          track->v_dwidth = track->v_width;
        else
          track->v_dwidth = (int)(track->v_height * track->v_aspect_ratio);
        track->v_dheight = track->v_height;
        raw_seq_hdr = m2v_parser->GetRealSequenceHeader();
        if (raw_seq_hdr != NULL) {
          track->raw_seq_hdr = (unsigned char *)
            safememdup(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
          track->raw_seq_hdr_size = raw_seq_hdr->GetSize();
        }
        track->fourcc = FOURCC('m', 'p', 'g', '0' + track->v_version);

      } else                    // if (track->fourcc == ...)
        // Unsupported video track type
        return;

    } else if (track->type == 'a') { // if (track->type == 'v')
      if (track->fourcc == FOURCC('M', 'P', '2', ' ')) {
        mp3_header_t header;

        if (find_mp3_header(buf, length) != 0)
          throw "Error parsing the first MP2/MP3 audio frame.";
        decode_mp3_header(buf, &header);
        track->a_channels = header.channels;
        track->a_sample_rate = header.sampling_frequency;
        track->fourcc = FOURCC('M', 'P', '0' + header.layer, ' ');

      } else if (track->fourcc == FOURCC('A', 'C', '3', ' ')) {
        ac3_header_t header;

        if (find_ac3_header(buf, length, &header) != 0)
          throw "Error parsing the first AC3 audio frame.";
        track->a_channels = header.channels;
        track->a_sample_rate = header.sample_rate;
        track->a_bsid = header.bsid;

      } else if (track->fourcc == FOURCC('D', 'T', 'S', ' ')) {
        if (find_dts_header(buf, length, &track->dts_header) != 0)
          throw "Error parsing the first DTS audio frame.";

//       } else if (track->fourcc == FOURCC('P', 'C', 'M', ' ')) {

      } else
        // Unsupported audio track type
        return;
    } else
      // Unsupported track type
      return;
 
    track->id = id;
    id2idx[id] = tracks.size();
    tracks.push_back(track);
  } catch(const char *msg) {
    mxerror(FMT_FN "%s\n", ti->fname.c_str(), msg);
  } catch(...) {
    mxerror(FMT_FN "Error parsing a MPEG PS packet during the "
            "header reading phase. This stream seems to be badly damaged.\n",
            ti->fname.c_str());
  }
}

bool
mpeg_ps_reader_c::find_next_packet(int &id) {
  try {
    uint32_t header;

    header = mm_io->read_uint32_be();
    while (1) {
      uint8_t byte;

      switch (header) {
        case MPEGVIDEO_PACKET_START_CODE:
          if (version == -1) {
            byte = mm_io->read_uint8();
            if ((byte & 0xc0) != 0)
              version = 2;      // MPEG-2 PS
            else
              version = 1;
            mm_io->skip(-1);
          }

          mm_io->skip(2 * 4);   // pack header
          if (version == 2) {
            mm_io->skip(1);
            byte = mm_io->read_uint8() & 0x07;
            mm_io->skip(byte);  // stuffing bytes
          }
          header = mm_io->read_uint32_be();
          break;

        case MPEGVIDEO_SYSTEM_HEADER_START_CODE:
          mm_io->skip(2 * 4);   // system header
          byte = mm_io->read_uint8();
          while ((byte & 0x80) == 0x80) {
            mm_io->skip(2);     // P-STD info
            byte = mm_io->read_uint8();
          }
          mm_io->skip(-1);
          header = mm_io->read_uint32_be();
          break;

        case MPEGVIDEO_MPEG_PROGRAM_END_CODE:
          return false;

        default:
          if (!mpeg_is_start_code(header))
            return false;

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
mpeg_ps_reader_c::find_next_packet_for_id(int id) {
  int new_id;

  try {
    while (find_next_packet(new_id)) {
      if (id == new_id)
        return true;
      mm_io->skip(mm_io->read_uint16_be());
    }
  } catch(...) {
  }
  return false;
}

void
mpeg_ps_reader_c::create_packetizer(int64_t id) {
  if ((id < 0) || (id >= tracks.size()))
    return;
  if (tracks[id]->ptzr >= 0)
    return;
  if (!demuxing_requested(tracks[id]->type, id))
    return;

  ti->id = id;
  mpeg_ps_track_ptr &track = tracks[id];
  if (track->type == 'a') {
    if ((track->fourcc == FOURCC('M', 'P', '1', ' ')) ||
        (track->fourcc == FOURCC('M', 'P', '2', ' ')) ||
        (track->fourcc == FOURCC('M', 'P', '3', ' '))) {
      track->ptzr =
        add_packetizer(new mp3_packetizer_c(this, track->a_sample_rate,
                                            track->a_channels, true, ti));
      if (verbose)
        mxinfo(FMT_TID "Using the MPEG audio output module.\n",
               ti->fname.c_str(), id);

    } else if (track->fourcc == FOURCC('A', 'C', '3', ' ')) {
      track->ptzr =
        add_packetizer(new ac3_packetizer_c(this, track->a_sample_rate,
                                            track->a_channels,
                                            track->a_bsid, ti));
      if (verbose)
        mxinfo(FMT_TID "Using the AC3 output module.\n", ti->fname.c_str(),
               id);

    } else if (track->fourcc == FOURCC('D', 'T', 'S', ' ')) {
      track->ptzr =
        add_packetizer(new dts_packetizer_c(this, track->dts_header, ti));
      if (verbose)
        mxinfo(FMT_TID "Using the DTS output module.\n", ti->fname.c_str(),
               id);

    } else
      mxerror("mpeg_ps_reader: Should not have happened #1. %s", BUGMSG);

  } else {                      // if (track->type == 'a')
    if ((track->fourcc == FOURCC('m', 'p', 'g', '1')) ||
        (track->fourcc == FOURCC('m', 'p', 'g', '2'))) {
      mpeg_12_video_packetizer_c *ptzr;

      ti->private_data = track->raw_seq_hdr;
      ti->private_size = track->raw_seq_hdr_size;
      ptzr =
        new mpeg_12_video_packetizer_c(this, track->v_version,
                                       track->v_frame_rate,
                                       track->v_width, track->v_height,
                                       track->v_dwidth, track->v_dheight, ti);
      track->ptzr = add_packetizer(ptzr);
      ti->private_data = NULL;
      ti->private_size = 0;

      if (verbose)
        mxinfo(FMT_TID "Using the MPEG-1/2 video output module.\n",
               ti->fname.c_str(), id);
      
    } else
      mxerror("mpeg_ps_reader: Should not have happened #2. %s", BUGMSG);
  }
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
  int new_id, length, aid;
  unsigned char *buf;

  if (file_done)
    return FILE_STATUS_DONE;

  try {
    while (find_next_packet(new_id)) {
      if ((new_id != 0xbd) &&
          ((id2idx[new_id] == -1) ||
           (tracks[id2idx[new_id]]->ptzr == -1))) {
        mm_io->skip(mm_io->read_uint16_be());
        continue;
      }

      packet_pos = mm_io->getFilePointer() - 4;
      if (!parse_packet(new_id, timecode, length, aid)) {
        file_done = true;
        mxverb(2, "mpeg_ps: file_done: !parse_packet\n");
        return FILE_STATUS_DONE;
      }

      if (new_id == 0xbd)
        new_id = 256 + aid;

      if ((id2idx[new_id] == -1) ||
          (tracks[id2idx[new_id]]->ptzr == -1)) {
        mm_io->skip(length);
        continue;
      }

      mxverb(3, "mpeg_ps: packet for %d length %d at %lld\n", new_id, length,
             packet_pos);

      buf = (unsigned char *)safemalloc(length);
      if (mm_io->read(buf, length) != length) {
        safefree(buf);
        file_done = true;
        mxverb(2, "mpeg_ps: file_done: mm_io->read\n");
        return FILE_STATUS_DONE;
      }

      memory_c mem(buf, length, true);
      PTZR(tracks[id2idx[new_id]]->ptzr)->process(mem);

      return FILE_STATUS_MOREDATA;
    }
  } catch(...) {
  }
  file_done = true;
  mxverb(2, "mpeg_ps: file_done: exception\n");
  return FILE_STATUS_DONE;
}

int
mpeg_ps_reader_c::get_progress() {
  return 100 * mm_io->getFilePointer() / size;
}

void
mpeg_ps_reader_c::identify() {
  int i;

  mxinfo("File '%s': container: MPEG %d program stream (PS)\n",
         ti->fname.c_str(), version);
  for (i = 0; i < tracks.size(); i++) {
    mpeg_ps_track_ptr &track = tracks[i];
    mxinfo("Track ID %d: %s (%s)\n", i,
           track->type == 'a' ? "audio" : "video",
           track->fourcc == FOURCC('m', 'p', 'g', '1') ? "MPEG-1" :
           track->fourcc == FOURCC('m', 'p', 'g', '2') ? "MPEG-2" :
           track->fourcc == FOURCC('M', 'P', '1', ' ') ? "MPEG-1 layer 1" :
           track->fourcc == FOURCC('M', 'P', '2', ' ') ? "MPEG-1 layer 2" :
           track->fourcc == FOURCC('M', 'P', '3', ' ') ? "MPEG-1 layer 3" :
           track->fourcc == FOURCC('A', 'C', '3', ' ') ? "AC3" :
           track->fourcc == FOURCC('D', 'T', 'S', ' ') ? "DTS" :
           track->fourcc == FOURCC('P', 'C', 'M', ' ') ? "PCM" :
           track->fourcc == FOURCC('L', 'P', 'C', 'M') ? "LPCM" :
           "unknown");
  }
}

