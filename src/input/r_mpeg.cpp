/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * MPEG ES demultiplexer module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "error.h"
#include "M2VParser.h"
#include "r_mpeg.h"
#include "p_video.h"

#define PROBESIZE 4
#define READ_SIZE 1024 * 1024

int
mpeg_es_reader_c::probe_file(mm_io_c *mm_io,
                             int64_t size) {
  unsigned char buf[PROBESIZE];
  M2VParser parser;

  if (size < PROBESIZE)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(buf, PROBESIZE) != PROBESIZE)
      mm_io->setFilePointer(0, seek_beginning);
    mm_io->setFilePointer(0, seek_beginning);

    if ((buf[0] != 0x00) || (buf[1] != 0x00) || (buf[2] != 0x01))
      return 0;

    if (!read_frame(parser, *mm_io, 1024 * 1024))
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

    mm_io = new mm_file_io_c(ti->fname);
    size = mm_io->get_size();

    // Let's find the first frame. We need its information like
    // resolution, MPEG version etc.
    if (!read_frame(parser, *mm_io, 1024 * 1024)) {
      delete mm_io;
      throw "";
    }

    version = parser.GetMPEGVersion();
    seq_hdr = parser.GetSequenceHeader();
    width = seq_hdr.width;
    height = seq_hdr.height;
    frame_rate = seq_hdr.frameRate;
    aspect_ratio = seq_hdr.aspectRatio;

    mxverb(2, "mpeg_es_reader: v %d width %d height %d FPS %lf AR %lf\n",
           version, width, height, frame_rate, aspect_ratio);

    duration = 0;

  } catch (exception &ex) {
    throw error_c("mpeg_es_reader: Could not open the file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the MPEG ES demultiplexer.\n", ti->fname);
}

mpeg_es_reader_c::~mpeg_es_reader_c() {
  delete mm_io;
}

void
mpeg_es_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  add_packetizer(new video_packetizer_c(this,
                                        mxsprintf("V_MPEG%d", version).c_str(),
                                        frame_rate, width, height, false, ti));

  mxinfo(FMT_TID "Using the video output module.\n", ti->fname, (int64_t)0);
}

file_status_t
mpeg_es_reader_c::read(generic_packetizer_c *,
                       bool) {
  MPEGFrame *frame;

  if (!read_frame(m2v_parser, *mm_io)) {
    PTZR0->flush();
    return file_status_done;
  }

  frame = m2v_parser.ReadFrame();
  if (!frame) {
    PTZR0->flush();
    return file_status_done;
  }

  mxinfo("frame size %u tc %lld 1st %lld 2nd %lld\n", frame->size,
         frame->timecode, frame->firstRef, frame->secondRef);
  memory_c mem(frame->data, frame->size, true);
  PTZR0->process(mem, frame->timecode, frame->duration,
                 frame->firstRef, frame->secondRef);

  frame->data = NULL;
  delete frame;

  bytes_processed = mm_io->getFilePointer();

  return file_status_moredata;
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

      bytes_to_read = (parser.GetBufferFreeSpace() < READ_SIZE) ?
        parser.GetBufferFreeSpace() : READ_SIZE;
      buffer = new unsigned char[bytes_to_read];
      bytes_read = in.read(buffer, bytes_to_read);
      printf("btr %d br %d\n", bytes_to_read, bytes_read);
      if (bytes_read == 0)
        break;
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
         "Track ID 0: video (MPEG %d)\n", ti->fname, version);
}
