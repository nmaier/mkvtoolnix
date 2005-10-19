/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common.h"
#include "commonebml.h"
#include "random.h"

#include "xtr_ogg.h"

#include <vorbis/codec.h>

// ------------------------------------------------------------------------

xtr_flac_c::xtr_flac_c(const string &_codec_id,
                       int64_t _tid,
                       track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec) {
}

void
xtr_flac_c::create_file(xtr_base_c *_master,
                        KaxTrackEntry &track) {
  KaxCodecPrivate *priv;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec "
            "private\" element and cannot be extracted.\n", tid,
            codec_id.c_str());

  xtr_base_c::create_file(_master, track);

  out->write(priv->GetBuffer(), priv->GetSize());
}

// ------------------------------------------------------------------------

xtr_oggbase_c::xtr_oggbase_c(const string &_codec_id,
                             int64_t _tid,
                             track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  packetno(2), previous_end(0) {
}

void
xtr_oggbase_c::create_file(xtr_base_c *_master,
                           KaxTrackEntry &track) {
  sfreq = (int)kt_get_a_sfreq(track);

  xtr_base_c::create_file(_master, track);

  if (no_variable_data)
    ogg_stream_init(&os, 1804289383);
  else
    ogg_stream_init(&os, random_c::generate_31bits());
}

void
xtr_oggbase_c::handle_frame(memory_cptr &frame,
                            KaxBlockAdditions *additions,
                            int64_t timecode,
                            int64_t duration,
                            int64_t bref,
                            int64_t fref,
                            bool keyframe,
                            bool discardable,
                            bool references_valid) {
  if (NULL != buffered_data.get()) {
    ogg_packet op;

    op.b_o_s = 0;
    op.e_o_s = 0;
    op.packetno = packetno;
    op.packet = buffered_data->get();
    op.bytes = buffered_data->get_size();
    op.granulepos = timecode * sfreq / 1000000000;
    ogg_stream_packetin(&os, &op);
    write_pages();

    packetno++;
  }

  buffered_data = memory_cptr(frame->clone());
  previous_end = timecode + duration;
}

void
xtr_oggbase_c::finish_file() {
  ogg_packet op;

  if (NULL == buffered_data.get())
    return;

  // Set the "end of stream" marker on the last packet, handle it
  // and flush all remaining Ogg pages.
  op.b_o_s = 0;
  op.e_o_s = 1;
  op.packetno = packetno;
  op.packet = buffered_data->get();
  op.bytes = buffered_data->get_size();
//   op.granulepos = (previous_end / 1000000) * sfreq / 1000;
  op.granulepos = previous_end * sfreq / 1000000000;
  ogg_stream_packetin(&os, &op);
  flush_pages();
  ogg_stream_clear(&os);
}

void
xtr_oggbase_c::flush_pages() {
  ogg_page page;

  while (ogg_stream_flush(&os, &page)) {
    out->write(page.header, page.header_len);
    out->write(page.body, page.body_len);
  }
}

void
xtr_oggbase_c::write_pages() {
  ogg_page page;

  while (ogg_stream_pageout(&os, &page)) {
    out->write(page.header, page.header_len);
    out->write(page.body, page.body_len);
  }
}

// ------------------------------------------------------------------------

xtr_oggflac_c::xtr_oggflac_c(const string &_codec_id,
                             int64_t _tid,
                             track_spec_t &tspec):
  xtr_oggbase_c(_codec_id, _tid, tspec) {
}

void
xtr_oggflac_c::create_file(xtr_base_c *_master,
                           KaxTrackEntry &track) {
  const binary *ptr;
  ogg_packet op;

  KaxCodecPrivate *priv;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec "
            "private\" element and cannot be extracted.\n", tid,
            codec_id.c_str());

  sfreq = (int)kt_get_a_sfreq(track);

  xtr_base_c::create_file(_master, track);

  if (no_variable_data)
    ogg_stream_init(&os, 1804289383);
  else
    ogg_stream_init(&os, random_c::generate_31bits());

  // Handle the three header packets: Headers, comments, codec
  // setup data.
  op.b_o_s = 1;
  op.e_o_s = 0;
  op.packetno = 0;
  op.packet = (unsigned char *)"fLaC";
  op.bytes = 4;
  op.granulepos = 0;
  ogg_stream_packetin(&os, &op);
  flush_pages();
  op.b_o_s = 0;
  op.packetno = 1;
  ptr = priv->GetBuffer();
  if ((priv->GetSize() >= 4) && (ptr[0] == 'f') &&
      (ptr[1] == 'L') && (ptr[2] == 'a') && (ptr[3] == 'C')) {
    ptr += 4;
    op.bytes = priv->GetSize() - 4;
  } else
    op.bytes = priv->GetSize();
  op.packet = (unsigned char *)safememdup(ptr, op.bytes);
  ogg_stream_packetin(&os, &op);
  safefree(op.packet);
  flush_pages();
  packetno = 2;
}

// ------------------------------------------------------------------------

xtr_oggvorbis_c::xtr_oggvorbis_c(const string &_codec_id,
                                 int64_t _tid,
                                 track_spec_t &tspec):
  xtr_oggbase_c(_codec_id, _tid, tspec) {
}

void
xtr_oggvorbis_c::create_file(xtr_base_c *_master,
                             KaxTrackEntry &track) {
  const unsigned char *c, *headers[3];
  ogg_packet op;
  int offset, header_sizes[3];
  KaxCodecPrivate *priv;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec "
            "private\" element and cannot be extracted.\n", tid,
            codec_id.c_str());

  c = (const unsigned char *)priv->GetBuffer();
  if ((priv->GetSize() == 0) || (c[0] != 2))
    mxerror("Track " LLD " with the CodecID '%s' does not contain valid "
            "headers.\n", tid, codec_id.c_str());

  offset = 1;
  for (packetno = 0; packetno < 2; packetno++) {
    int length = 0;

    while ((offset < priv->GetSize()) && ((unsigned char)255 == c[offset])) {
      length += 255;
      offset++;
    }
    if ((priv->GetSize() - 1) <= offset)
      mxerror("Track " LLD " with the CodecID '%s' does not contain valid "
              "headers.\n", tid, codec_id.c_str());
    length += c[offset];
    offset++;
    header_sizes[packetno] = length;
  }

  headers[0] = &c[offset];
  headers[1] = &c[offset + header_sizes[0]];
  headers[2] = &c[offset + header_sizes[0] + header_sizes[1]];
  header_sizes[2] = priv->GetSize() - offset - header_sizes[0] -
    header_sizes[1];

  xtr_oggbase_c::create_file(_master, track);

  for (packetno = 0; packetno < 3; packetno++) {
    // Handle the three header packets: Headers, comments, codec
    // setup data.
    op.b_o_s = (0 == packetno ? 1 : 0);
    op.e_o_s = 0;
    op.packetno = packetno;
    op.packet = (unsigned char *)safememdup(headers[packetno],
                                            header_sizes[packetno]);
    op.bytes = header_sizes[packetno];
    op.granulepos = 0;
    ogg_stream_packetin(&os, &op);
    if (1 != packetno)
      flush_pages();
    safefree(op.packet);
  }
}

