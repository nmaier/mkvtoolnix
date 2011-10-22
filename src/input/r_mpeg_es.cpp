/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MPEG ES (elementary stream) demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cstring>

#include "common/endian.h"
#include "common/error.h"
#include "common/mpeg4_p2.h"
#include "input/r_mpeg_es.h"
#include "merge/output_control.h"
#include "mpegparser/M2VParser.h"
#include "output/p_mpeg1_2.h"

#define PROBESIZE 4
#define READ_SIZE 1024 * 1024

int
mpeg_es_reader_c::probe_file(mm_io_c *io,
                             uint64_t size) {
  if (PROBESIZE > size)
    return 0;

  try {
    memory_cptr af_buf = memory_c::alloc(READ_SIZE);
    unsigned char *buf = af_buf->get_buffer();
    io->setFilePointer(0, seek_beginning);
    int num_read = io->read(buf, READ_SIZE);

    if (4 > num_read)
      return 0;

    io->setFilePointer(0, seek_beginning);

    // MPEG TS starts with 0x47.
    if (0x47 == buf[0])
      return 0;

    // MPEG PS starts with 0x000001ba.
    uint32_t value = get_uint32_be(buf);
    if (MPEGVIDEO_PACKET_START_CODE == value)
      return 0;

    // Due to type detection woes mkvmerge requires
    // the stream to start with a MPEG start code.
    if (!mpeg_is_start_code(value))
      return 0;

    bool gop_start_code_found      = false;
    bool sequence_start_code_found = false;
    bool ext_start_code_found      = false;
    bool picture_start_code_found  = false;
    bool slice_start_code_found    = false;

    bool ok                        = false;

    // Let's look for a MPEG ES start code inside the first 1 MB.
    int i;
    for (i = 4; i < num_read - 1; i++) {
      if (mpeg_is_start_code(value)) {
        mxverb(3, boost::format("mpeg_es_detection: start code found; fourth byte: 0x%|1$02x|\n") % (value & 0xff));

        if (MPEGVIDEO_SEQUENCE_START_CODE == value)
          sequence_start_code_found = true;

        else if (MPEGVIDEO_PICTURE_START_CODE == value)
          picture_start_code_found  = true;

        else if (MPEGVIDEO_GOP12_START_CODE   == value)
          gop_start_code_found      = true;

        else if (MPEGVIDEO_EXT_START_CODE     == value)
          gop_start_code_found      = true;

        else if ((MPEGVIDEO_FIRST_SLICE_START_CODE >= value) && (MPEGVIDEO_LAST_SLICE_START_CODE <= value))
          slice_start_code_found    = true;

        ok = sequence_start_code_found && picture_start_code_found && (gop_start_code_found || ext_start_code_found || slice_start_code_found);
        if (ok)
          break;
      }

      value <<= 8;
      value  |= buf[i];
    }

    mxverb(3,
           boost::format("mpeg_es_detection: sequence %1% picture %2% gop %3% ext %4% slice %5%\n")
           % sequence_start_code_found % picture_start_code_found % gop_start_code_found % ext_start_code_found % slice_start_code_found);

    if (!ok)
      return 0;

    // Let's try to read one frame.
    M2VParser parser;
    parser.SetProbeMode();
    if (!read_frame(parser, *io, READ_SIZE, true))
      return 0;

  } catch (...) {
    return 0;
  }

  return 1;
}

mpeg_es_reader_c::mpeg_es_reader_c(track_info_c &_ti)
  throw (error_c)
  : generic_reader_c(_ti)
  , bytes_processed(0)
{

  try {
    M2VParser parser;

    io   = new mm_file_io_c(m_ti.m_fname);
    size = io->get_size();

    // Let's find the first frame. We need its information like
    // resolution, MPEG version etc.
    parser.SetProbeMode();
    if (!read_frame(parser, *io, 1024 * 1024)) {
      delete io;
      throw "";
    }

    io->setFilePointer(0);

    MPEG2SequenceHeader seq_hdr = parser.GetSequenceHeader();
    version                     = parser.GetMPEGVersion();
    interlaced                  = !seq_hdr.progressiveSequence;
    width                       = seq_hdr.width;
    height                      = seq_hdr.height;
    frame_rate                  = seq_hdr.progressiveSequence ? seq_hdr.frameOrFieldRate : seq_hdr.frameOrFieldRate * 2.0f;
    aspect_ratio                = seq_hdr.aspectRatio;

    if ((0 >= aspect_ratio) || (1 == aspect_ratio))
      dwidth = width;
    else
      dwidth = (int)(height * aspect_ratio);
    dheight = height;

    MPEGChunk *raw_seq_hdr = parser.GetRealSequenceHeader();
    if (NULL != raw_seq_hdr) {
      m_ti.m_private_data = (unsigned char *)safememdup(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
      m_ti.m_private_size = raw_seq_hdr->GetSize();
    }

    mxverb(2, boost::format("mpeg_es_reader: version %1% width %2% height %3% FPS %4% AR %5%\n") % version % width % height % frame_rate % aspect_ratio);

  } catch (...) {
    throw error_c(boost::format(Y("%1%: Could not open the file.")) % get_format_name());
  }
  show_demuxer_info();
}

mpeg_es_reader_c::~mpeg_es_reader_c() {
  delete io;
}

void
mpeg_es_reader_c::create_packetizer(int64_t) {
  generic_packetizer_c *m2vpacketizer;
  if (!demuxing_requested('v', 0) || (NPTZR() != 0))
    return;

  m2vpacketizer = new mpeg1_2_video_packetizer_c(this, m_ti, version, frame_rate, width, height, dwidth, dheight, false);
  add_packetizer(m2vpacketizer);
  m2vpacketizer->set_video_interlaced_flag(interlaced);
  
  show_packetizer_info(0, m2vpacketizer);
}

file_status_e
mpeg_es_reader_c::read(generic_packetizer_c *,
                       bool) {
  int64_t bytes_to_read = std::min(static_cast<int64_t>(READ_SIZE), size - bytes_processed);
  if (0 >= bytes_to_read)
    return flush_packetizers();

  memory_cptr chunk = memory_c::alloc(bytes_to_read);
  int64_t num_read  = io->read(chunk, bytes_to_read);

  if (0 < num_read) {
    chunk->set_size(num_read);
    PTZR0->process(new packet_t(chunk));
    bytes_processed = io->getFilePointer();
  }

  return bytes_to_read > num_read ? flush_packetizers() : FILE_STATUS_MOREDATA;
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
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_VIDEO, (boost::format("MPEG %1%") % version).str());
}
