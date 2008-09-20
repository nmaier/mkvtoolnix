/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   MPEG ES (elementary stream) demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <cstring>
#include <memory>

#include "common.h"
#include "error.h"
#include "M2VParser.h"
#include "r_mpeg.h"
#include "smart_pointers.h"
#include "output_control.h"
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
