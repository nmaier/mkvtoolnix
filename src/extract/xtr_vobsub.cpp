/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Parts of this code (the MPEG header generation) was written by
   Mike Matsnev <mike@po.cs.msu.su>.
*/

#include "os.h"

#include "checksums.h"
#include "commonebml.h"
#include "iso639.h"
#include "smart_pointers.h"
#include "tta_common.h"
#include "xtr_vobsub.h"

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif
struct PACKED_STRUCTURE mpeg_es_header_t {
  uint8_t	pfx[3];               // 00 00 01
  uint8_t	stream_id;            // BD
  uint8_t	len[2];
  uint8_t	flags[2];
  uint8_t	hlen;
  uint8_t	pts[5];
  uint8_t	lidx;
};

struct PACKED_STRUCTURE mpeg_ps_header_t {
  uint8_t	pfx[4];               // 00 00 01 BA
  uint8_t	scr[6];
  uint8_t	muxr[3];
  uint8_t	stlen;
};
#if defined(COMP_MSC)
#pragma pack(pop)
#endif

xtr_vobsub_c::xtr_vobsub_c(const string &_codec_id,
                           int64_t _tid,
                           track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  private_size(0), stream_id(0x20) {

  int pos;

  base_name = tspec.out_name;
  pos = base_name.rfind('.');
  if (pos >= 0)
    base_name.erase(pos);
}

void
xtr_vobsub_c::create_file(xtr_base_c *_master,
                          KaxTrackEntry &track) {
  KaxCodecPrivate *priv;
  unsigned char *new_priv;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track %lld with the CodecID '%s' is missing the \"codec private"
            "\" element and cannot be extracted.\n", tid, codec_id.c_str());

  if (!content_decoder.initialize(track))
    mxerror("Tracks with unsupported content encoding schemes (compression "
            "or encryption) cannot be extracted.\n");

  private_size = priv->GetSize();

  new_priv = const_cast<unsigned char *>(&binary(*priv));
  if (!content_decoder.reverse(new_priv, private_size,
                               CONTENT_ENCODING_SCOPE_CODECPRIVATE))
    private_data.set(safememdup(&binary(*priv), private_size));
  else
    private_data.set(new_priv);

  master = _master;
  language = kt_get_language(track);

  if (NULL == master) {
    string file_name = base_name + ".sub";

    try {
      out = new mm_file_io_c(file_name, MODE_CREATE);
    } catch (...) {
      mxerror("Failed to create the VobSub data file '%s': %d (%s)\n",
              file_name.c_str(), errno, strerror(errno));
    }

  } else {
    xtr_vobsub_c *vmaster = dynamic_cast<xtr_vobsub_c *>(master);

    if (NULL == vmaster)
      mxerror("Cannot extract tracks of different kinds to the same file. "
              "This was requested for the tracks %lld and %lld.\n", tid,
              master->tid);

    if ((private_size != vmaster->private_size) ||
        memcmp(&binary(*priv), vmaster->private_data.get(), private_size))
      mxerror("Two VobSub tracks can only be extracted into the same file "
              "if their CodecPrivate data matches. This is not the case "
              "for the tracks %lld and %lld.\n", tid, master->tid);

    vmaster->slaves.push_back(this);
    stream_id = vmaster->stream_id + 1;
    vmaster->stream_id++;
  }
}

void
xtr_vobsub_c::handle_block(KaxBlock &block,
                           KaxBlockAdditions *additions,
                           int64_t timecode,
                           int64_t duration,
                           int64_t bref,
                           int64_t fref) {
  mpeg_es_header_t es;
  mpeg_ps_header_t ps;
  uint64_t c;
  int i;
  xtr_vobsub_c *vmaster;

  if (NULL == master)
    vmaster = this;
  else
    vmaster = static_cast<xtr_vobsub_c *>(master);

  for (i = 0; i < block.NumberFrames(); i++) {
    autofree_ptr<unsigned char> af_data;
    unsigned char *data;
    uint32_t size;
    DataBuffer &data_buffer = block.GetBuffer(i);

    size = data_buffer.Size();
    data = data_buffer.Buffer();
    if (content_decoder.reverse(data, size, CONTENT_ENCODING_SCOPE_BLOCK))
      af_data.set(data);

    memset(&ps, 0, sizeof(mpeg_ps_header_t));

    ps.pfx[2] = 0x01;
    ps.pfx[3] = 0xba;
    c = timecode * 9 / 100000;

    ps.scr[0] = 0x40 | ((uint8_t)(c >> 27) & 0x38) | 0x04 |
      ((uint8_t)(c >> 28) & 0x03);
    ps.scr[1] = (uint8_t)(c >> 20);
    ps.scr[2] = ((uint8_t)(c >> 12) & 0xf8) | 0x40 |
      ((uint8_t)(c >> 13) & 0x07);
    ps.scr[3] = (uint8_t)(c >> 5);
    ps.scr[4] = ((uint8_t)(c << 3) & 0xf8) | 0x40;
    ps.scr[5] = 1;
    ps.muxr[0] = 1;
    ps.muxr[1] = 0x89;
    ps.muxr[2] = 0xc3; // just some value
    ps.stlen = 0xf8;

    memset(&es, 0, sizeof(mpeg_es_header_t));
    es.pfx[2] = 1;
    es.stream_id = 0xbd;
    es.len[0] = ((size + 9) >> 8) & 0xff;
    es.len[1] = (size + 9) & 0xff;
    es.flags[0] = 0x81;
    es.flags[1] = 0x80;
    es.hlen = 5;
    es.pts[0] = 0x20 | ((uint8_t)(c >> 29) & 0xe0) | 0x01;
    es.pts[1] = (uint8_t)(c >> 22);
    es.pts[2] = ((uint8_t)(c >> 14) & 0xfe) | 0x01;
    es.pts[3] = (uint8_t)(c >> 7);
    es.pts[4] = (uint8_t)(c << 1) | 0x01;
    if (NULL == master)
      es.lidx = 0x20;
    else
      es.lidx = stream_id;

    positions.push_back(vmaster->out->getFilePointer());
    timecodes.push_back(timecode);
    vmaster->out->write(&ps, sizeof(mpeg_ps_header_t));
    vmaster->out->write(&es, sizeof(mpeg_es_header_t));
    vmaster->out->write(data, size);
  }
}

void
xtr_vobsub_c::finish_file() {
  if (NULL != master)
    return;

  try {
    int slave;
    const char *header_line =
      "# VobSub index file, v7 (do not modify this line!)\n";

    base_name += ".idx";

    delete out;
    out = NULL;

    mm_file_io_c idx(base_name, MODE_CREATE);
    mxinfo("Writing the VobSub index file '%s'.\n", base_name.c_str());
    if ((25 > private_size) || strncasecmp((char *)private_data.get(),
                                           header_line, 25))
      idx.printf(header_line);
    idx.write(private_data.get(), private_size);

    write_idx(idx, 0);
    for (slave = 0; slave < slaves.size(); slave++)
      slaves[slave]->write_idx(idx, slave + 1);

  } catch (...) {
    mxerror("Failed to create the file '%s': %d (%s)\n", base_name.c_str(),
            errno, strerror(errno));
  }
}

void
xtr_vobsub_c::write_idx(mm_io_c &idx,
                        int index) {
  const char *iso639_1;
  int i;

  iso639_1 = map_iso639_2_to_iso639_1(language.c_str());
  idx.printf("\nid: %s, index: %d\n", (NULL == iso639_1 ? "en" : iso639_1),
             index);

  for (i = 0; i < positions.size(); i++) {
    int64_t timecode;

    timecode = timecodes[i] / 1000000;
    idx.printf("timestamp: %02d:%02d:%02d:%03d, filepos: %1x%08x\n",
               (int)((timecode / 60 / 60 / 1000) % 60),
               (int)((timecode / 60 / 1000) % 60),
               (int)((timecode / 1000) % 60),
               (int)(timecode % 1000),
               (uint32_t)(positions[i] >> 32),
               (uint32_t)(positions[i] & 0xffffffff));
  }
}
