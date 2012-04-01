/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Parts of this code (the MPEG header generation) was written by
   Mike Matsnev <mike@po.cs.msu.su>.
*/

#include "common/common_pch.h"

#include "common/checksums.h"
#include "common/ebml.h"
#include "common/iso639.h"
#include "common/mm_write_buffer_io.h"
#include "common/tta.h"
#include "extract/xtr_vobsub.h"

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif
struct PACKED_STRUCTURE mpeg_es_header_t {
  uint8_t pfx[3];               // 00 00 01
  uint8_t stream_id;            // BD
  uint8_t len[2];
  uint8_t flags[2];
  uint8_t hlen;
  uint8_t pts[5];
  uint8_t lidx;
};

struct PACKED_STRUCTURE mpeg_ps_header_t {
  uint8_t pfx[4];               // 00 00 01 BA
  uint8_t scr[6];
  uint8_t muxr[3];
  uint8_t stlen;
};
#if defined(COMP_MSC)
#pragma pack(pop)
#endif

xtr_vobsub_c::xtr_vobsub_c(const std::string &codec_id,
                           int64_t tid,
                           track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_base_name(tspec.out_name)
  , m_stream_id(0x20)
{
  int pos = m_base_name.rfind('.');
  if (0 <= pos)
    m_base_name.erase(pos);
}

void
xtr_vobsub_c::create_file(xtr_base_c *master,
                          KaxTrackEntry &track) {
  KaxCodecPrivate *priv = FindChild<KaxCodecPrivate>(&track);
  if (nullptr == priv)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

  init_content_decoder(track);

  m_private_data = decode_codec_private(priv);
  m_private_data->grab();

  m_master   = master;
  m_language = kt_get_language(track);

  if (nullptr == master) {
    std::string sub_file_name = m_base_name + ".sub";

    try {
      m_out = mm_write_buffer_io_c::open(sub_file_name, 128 * 1024);
    } catch (...) {
      mxerror(boost::format(Y("Failed to create the VobSub data file '%1%': %2% (%3%)\n")) % sub_file_name % errno % strerror(errno));
    }

  } else {
    xtr_vobsub_c *vmaster = dynamic_cast<xtr_vobsub_c *>(m_master);

    if (nullptr == vmaster)
      mxerror(boost::format(Y("Cannot extract tracks of different kinds to the same file. This was requested for the tracks %1% and %2%.\n"))
              % m_tid % m_master->m_tid);

    if ((m_private_data->get_size() != vmaster->m_private_data->get_size()) || memcmp(priv->GetBuffer(), vmaster->m_private_data->get_buffer(), m_private_data->get_size()))
      mxerror(boost::format(Y("Two VobSub tracks can only be extracted into the same file if their CodecPrivate data matches. "
                              "This is not the case for the tracks %1% and %2%.\n")) % m_tid % m_master->m_tid);

    vmaster->m_slaves.push_back(this);
    m_stream_id = vmaster->m_stream_id + 1;
    vmaster->m_stream_id++;
  }
}

void
xtr_vobsub_c::handle_frame(memory_cptr &frame,
                           KaxBlockAdditions *,
                           int64_t timecode,
                           int64_t,
                           int64_t,
                           int64_t,
                           bool,
                           bool,
                           bool) {
  static unsigned char padding_data[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  xtr_vobsub_c *vmaster = (nullptr == m_master) ? this : static_cast<xtr_vobsub_c *>(m_master);

  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  unsigned char *data = frame->get_buffer();
  size_t size         = frame->get_size();

  m_positions.push_back(vmaster->m_out->getFilePointer());
  m_timecodes.push_back(timecode);

  uint32_t padding = (2048 - (size + sizeof(mpeg_ps_header_t) + sizeof(mpeg_es_header_t))) & 2047;
  uint32_t first   = size + sizeof(mpeg_ps_header_t) + sizeof(mpeg_es_header_t) > 2048 ? 2048 - sizeof(mpeg_ps_header_t) - sizeof(mpeg_es_header_t) : size;

  uint64_t c       = timecode * 9 / 100000;

  mpeg_ps_header_t ps;
  memset(&ps, 0, sizeof(mpeg_ps_header_t));
  ps.pfx[2]  = 0x01;
  ps.pfx[3]  = 0xba;
  ps.scr[0]  = 0x40 | ((uint8_t)(c >> 27) & 0x38) | 0x04 | ((uint8_t)(c >> 28) & 0x03);
  ps.scr[1]  = (uint8_t)(c >> 20);
  ps.scr[2]  = ((uint8_t)(c >> 12) & 0xf8) | 0x04 | ((uint8_t)(c >> 13) & 0x03);
  ps.scr[3]  = (uint8_t)(c >> 5);
  ps.scr[4]  = ((uint8_t)(c << 3) & 0xf8) | 0x04;
  ps.scr[5]  = 1;
  ps.muxr[0] = 1;
  ps.muxr[1] = 0x89;
  ps.muxr[2] = 0xc3; // just some value
  ps.stlen   = 0xf8;

  mpeg_es_header_t es;
  memset(&es, 0, sizeof(mpeg_es_header_t));
  es.pfx[2]    = 1;
  es.stream_id = 0xbd;
  es.len[0]    = (uint8_t)((first + 9) >> 8);
  es.len[1]    = (uint8_t)(first + 9);
  es.flags[0]  = 0x81;
  es.flags[1]  = 0x80;
  es.hlen      = 5;
  es.pts[0]    = 0x20 | ((uint8_t)(c >> 29) & 0x0e) | 0x01;
  es.pts[1]    = (uint8_t)(c >> 22);
  es.pts[2]    = ((uint8_t)(c >> 14) & 0xfe) | 0x01;
  es.pts[3]    = (uint8_t)(c >> 7);
  es.pts[4]    = (uint8_t)(c << 1) | 0x01;
  es.lidx      = (nullptr == m_master) ? 0x20 : m_stream_id;
  if ((6 > padding) && (first == size)) {
    es.hlen += (uint8_t)padding;
    es.len[0]  = (uint8_t)((first + 9 + padding) >> 8);
    es.len[1]  = (uint8_t)(first + 9 + padding);
  }

  vmaster->m_out->write(&ps, sizeof(mpeg_ps_header_t));
  vmaster->m_out->write(&es, sizeof(mpeg_es_header_t) - 1);
  if ((0 < padding) && (6 > padding) && (first == size))
    vmaster->m_out->write(padding_data, padding);
  vmaster->m_out->write(&es.lidx, 1);
  vmaster->m_out->write(data, first);
  while (first < size) {
    size    -= first;
    data    += first;

    padding  = (2048 - (size + 10 + sizeof(mpeg_ps_header_t))) & 2047;
    first    = size + 10 + sizeof(mpeg_ps_header_t) > 2048 ? 2048 - 10 - sizeof(mpeg_ps_header_t) : size;

    es.len[0]   = (uint8_t)((first + 4) >> 8);
    es.len[1]   = (uint8_t)(first + 4);
    es.flags[1] = 0;
    es.hlen     = 0;
    if ((6 > padding) && (first == size)) {
      es.hlen += (uint8_t)padding;
      es.len[0]  = (uint8_t)((first + 4 + padding) >> 8);
      es.len[1]  = (uint8_t)(first + 4 + padding);
    }

    vmaster->m_out->write(&ps, sizeof(mpeg_ps_header_t));
    vmaster->m_out->write(&es, 9);
    if ((0 < padding) && (6 > padding) && (first == size))
      vmaster->m_out->write(padding_data, padding);
    vmaster->m_out->write(&es.lidx, 1);
    vmaster->m_out->write(data, first);
  }
  if (6 <= padding) {
    padding      -= 6;
    es.stream_id  = 0xbe;
    es.len[0]     = (uint8_t)(padding >> 8);
    es.len[1]     = (uint8_t)padding;

    vmaster->m_out->write(&es, 6); // XXX

    while (0 < padding) {
      uint32_t todo = 8 < padding ? 8 : padding;
      vmaster->m_out->write(padding_data, todo);
      padding -= todo;
    }
  }
}

void
xtr_vobsub_c::finish_file() {
  if (nullptr != m_master)
    return;

  try {
    static const char *header_line = "# VobSub index file, v7 (do not modify this line!)\n";

    m_base_name += ".idx";

    m_out.reset();

    mm_write_buffer_io_c idx(new mm_file_io_c(m_base_name, MODE_CREATE), 128 * 1024);
    mxinfo(boost::format(Y("Writing the VobSub index file '%1%'.\n")) % m_base_name);

    if ((25 > m_private_data->get_size()) || strncasecmp((char *)m_private_data->get_buffer(), header_line, 25))
      idx.puts(header_line);
    idx.write(m_private_data->get_buffer(), m_private_data->get_size());

    write_idx(idx, 0);
    size_t slave;
    for (slave = 0; slave < m_slaves.size(); slave++)
      m_slaves[slave]->write_idx(idx, slave + 1);

  } catch (...) {
    mxerror(boost::format(Y("Failed to create the file '%1%': %2% (%3%)\n")) % m_base_name % errno % strerror(errno));
  }
}

void
xtr_vobsub_c::write_idx(mm_io_c &idx,
                        int index) {
  auto iso639_1 = map_iso639_2_to_iso639_1(m_language);
  idx.puts(boost::format("\nid: %1%, index: %2%\n") % (iso639_1.empty() ? "en" : iso639_1) % index);

  size_t i;
  for (i = 0; i < m_positions.size(); i++) {
    int64_t timecode = m_timecodes[i] / 1000000;

    idx.puts(boost::format("timestamp: %|1$02d|:%|2$02d|:%|3$02d|:%|4$03d|, filepos: %|5$1x|%|6$08x|\n")
             % ((timecode / 60 / 60 / 1000) % 60)
             % ((timecode / 60 / 1000) % 60)
             % ((timecode / 1000) % 60)
             % (timecode % 1000)
             % (m_positions[i] >> 32)
             % (m_positions[i] & 0xffffffff));
  }
}
