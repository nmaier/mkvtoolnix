/*
  mkvextract -- extract tracks from Matroska files into other files

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  extracts tracks from Matroska files into other files

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxBlock.h>

#include "common/alac.h"
#include "common/caf.h"
#include "common/ebml.h"
#include "common/endian.h"
#include "common/mm_write_buffer_io.h"

#include "extract/xtr_alac.h"
#include "extract/xtr_base.h"

xtr_alac_c::xtr_alac_c(std::string const &codec_id,
                       int64_t tid,
                       track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_priv{}
  , m_free_chunk_size{}
  , m_free_chunk_offset{}
  , m_data_chunk_offset{}
  , m_frames_written{}
  , m_packets_written{}
  , m_prev_written{}
{
}

void
xtr_alac_c::create_file(xtr_base_c *master, KaxTrackEntry &track) {
  init_content_decoder(track);

  auto channels = kt_get_a_channels(track);
  auto priv     = FindChild<KaxCodecPrivate>(&track);
  if (!priv)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

  m_priv = decode_codec_private(priv);
  if (m_priv->get_size() != sizeof(alac::codec_config_t))
    mxerror(boost::format(Y("ALAC private data size mismatch\n")));

  xtr_base_c::create_file(master, track);

  m_out->write(std::string{"caff"});                             // mFileType
  m_out->write_uint16_be(1);                                     // mFileVersion
  m_out->write_uint16_be(0);                                     // mFileFlags

  m_out->write(std::string{"desc"});                             // Audio Description chunk
  m_out->write_uint64_be(32ULL);                                 // mChunkSize
  m_out->write_double(static_cast<int>(kt_get_a_sfreq(track)));  // mSampleRate
  m_out->write(std::string{"alac"});                             // mFormatID
  m_out->write_uint32_be(0);                                     // mFormatFlags
  m_out->write_uint32_be(0);                                     // mBytesPerPacket
  m_out->write_uint32_be(caf::defs::default_frames_per_packet);  // mFramesPerPacket
  m_out->write_uint32_be(channels);                              // mChannelsPerFrame
  m_out->write_uint32_be(0);                                     // mBitsPerChannel

  auto kuki_size = 12 + 36 + 8 + (2 < channels ? 24 : 0);        // add the size of ALACChannelLayoutInfo for more than 2 channels
  m_out->write(std::string{"kuki"});
  m_out->write_uint64_be(kuki_size);
  m_out->write_uint8('\0');
  m_out->write_uint8('\0');
  m_out->write_uint8('\0');
  m_out->write_uint8('\14');
  m_out->write(std::string{"frma"});
  m_out->write(std::string{"alac"});

  m_out->write_uint32_be(12 + sizeof(alac::codec_config_t));     // ALAC Specific Info size = 36 (12 + sizeof(ALAXSpecificConfig))
  m_out->write(std::string{"alac"});                             // ALAC Specific Info ID
  m_out->write_uint32_be(0L);                                    // Version Flags

  m_out->write(m_priv);                                          // audio specific config

  auto alo = caf::channel_layout_t();
  if (2 < channels) {
    switch (channels) {
      case 3:
        alo.channel_layout_tag = caf::channel_layout_t::mpeg_3_0_b;
        alo.channel_bitmap     = caf::channel_layout_t::left | caf::channel_layout_t::right | caf::channel_layout_t::center;
        break;
      case 4:
        alo.channel_layout_tag = caf::channel_layout_t::mpeg_4_0_b;
        alo.channel_bitmap     = caf::channel_layout_t::left | caf::channel_layout_t::right | caf::channel_layout_t::center | caf::channel_layout_t::center_surround;
        break;
      case 5:
        alo.channel_layout_tag = caf::channel_layout_t::mpeg_5_0_d;
        alo.channel_bitmap     = caf::channel_layout_t::left | caf::channel_layout_t::right | caf::channel_layout_t::center | caf::channel_layout_t::left_surround | caf::channel_layout_t::right_surround;
        break;
      case 6:
        alo.channel_layout_tag = caf::channel_layout_t::mpeg_5_1_d;
        alo.channel_bitmap     = caf::channel_layout_t::left | caf::channel_layout_t::right | caf::channel_layout_t::center | caf::channel_layout_t::left_surround | caf::channel_layout_t::right_surround | caf::channel_layout_t::lfe_screen;
        break;
      case 7:
        alo.channel_layout_tag = caf::channel_layout_t::aac_6_1;
        alo.channel_bitmap     = caf::channel_layout_t::left | caf::channel_layout_t::right | caf::channel_layout_t::center | caf::channel_layout_t::left_surround
                               | caf::channel_layout_t::right_surround | caf::channel_layout_t::center_surround | caf::channel_layout_t::lfe_screen;
        break;
      case 8:
        alo.channel_layout_tag = caf::channel_layout_t::mpeg_7_1_b;
        alo.channel_bitmap     = caf::channel_layout_t::left | caf::channel_layout_t::right | caf::channel_layout_t::center | caf::channel_layout_t::left_center
                               | caf::channel_layout_t::right_center | caf::channel_layout_t::left_surround | caf::channel_layout_t::right_surround | caf::channel_layout_t::lfe_screen;
        break;
    }

    auto acli = caf::channel_layout_info_t();

    put_uint32_be(&acli.channel_layout_info_size, 24);                         // = sizeof(ALACChannelLayoutInfo)
    put_uint32_be(&acli.channel_layout_info_id,   FOURCC('c', 'h', 'a', 'n')); // = 'chan'
    put_uint32_be(&acli.channel_layout_tag,       alo.channel_layout_tag);
    m_out->write(&acli, sizeof(acli));
  }

  // Terminator atom
  m_out->write_uint32_be(8);        // Channel Layout Info Size
  m_out->write_uint32_be(0);        // Channel Layout Info ID

  if (2 < channels) {
    m_out->write(std::string{"chan"}); // 'chan' chunk immediately following the kuki
    m_out->write_uint64_be(12ULL);     // = sizeof(ALACAudioChannelLayout)

    m_out->write_uint32_be(alo.channel_layout_tag);
    m_out->write_uint32_be(alo.channel_bitmap);
    m_out->write_uint32_be(alo.number_channel_descriptions);
  }

  m_free_chunk_offset = m_out->getFilePointer();    // remember the location of
  m_free_chunk_size   = 16384;

  auto free_chunk = memory_c::alloc(m_free_chunk_size);
  memset(free_chunk->get_buffer(), 0, sizeof(m_free_chunk_size));

  // the 'free' chunk
  m_out->write(std::string{"free"});
  m_out->write_uint64_be(m_free_chunk_size);
  m_out->write(free_chunk);

  m_data_chunk_offset = m_out->getFilePointer();
  m_out->write(std::string{"data"}); // Audio Data Chunk
  m_out->write_uint64_be(-1LL);      // mChunkSize (= -1 if unknown)
  m_out->write_uint32_be(1);         // mEditCount
}

void
xtr_alac_c::finish_file() {
  unsigned int const table_header_size = 24;
  uint64_t const outsize               = 4 + 8 + 8 + 8 + 4 + 4 + m_pkt_sizes.size();
  auto tdiff                           = static_cast<int64_t>(m_data_chunk_offset) - static_cast<int64_t>(m_free_chunk_offset + outsize);
  auto write_pakt                      = [&]() {
    m_out->write(std::string{"pakt"});
    m_out->write_uint64_be(table_header_size + m_pkt_sizes.size());
    m_out->write_uint64_be(m_packets_written);
    m_out->write_uint64_be(m_frames_written);
    m_out->write_uint32_be(0);  // priming frames
    m_out->write_uint32_be(0);  // remainder frames

    // generate the Packet Table Chunk data section
    m_out->write(m_pkt_sizes.data(), m_pkt_sizes.size());
  };

  if (tdiff && (16 > tdiff))
    write_pakt();

  else {
    m_out->setFilePointer(m_free_chunk_offset);
    write_pakt();

    // regenerate a 'free' chunk to fill the interstitium between the 'pakt' and 'data' chunks
    if (tdiff && (16 <= tdiff)) {
      m_out->write(std::string{"free"});
      m_out->write_uint64_be(tdiff - 12);

      auto free_chunk = memory_c::alloc(tdiff - 12);
      memset(free_chunk->get_buffer(), 0, tdiff - 12);
      m_out->write(free_chunk);
    }
  }

  // now that the size of the complete data chunk is known, go back and fill in the
  // mChunkSize field for the 'data' chunk
  m_out->setFilePointer(m_data_chunk_offset + 4);
  m_out->write_uint64_be(m_bytes_written + 4);
}

void
xtr_alac_c::handle_frame(xtr_frame_t &f) {
  m_out->write(f.frame);
  auto tval        = f.frame->get_size();
  m_bytes_written += tval;

  auto amt_diff    = m_bytes_written - m_prev_written;
  m_prev_written   = m_bytes_written;

  for (int i = 4; i > 0; i--) {
    unsigned top = amt_diff >> i * 7;

    if (top)
      m_pkt_sizes.push_back(128 | top);
  }

  m_pkt_sizes.push_back(amt_diff & 127);

  m_packets_written++;
  m_frames_written += caf::defs::default_frames_per_packet;
}
