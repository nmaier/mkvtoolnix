/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>
   and Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/common_pch.h"

#include <matroska/KaxBlock.h>

#include "common/ebml.h"
#include "common/endian.h"
#include "common/mm_write_buffer_io.h"
#include "extract/xtr_wav.h"

xtr_wav_c::xtr_wav_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
{
  memset(&m_wh, 0, sizeof(wave_header));
}

void
xtr_wav_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  init_content_decoder(track);

  int channels = kt_get_a_channels(track);
  int sfreq    = (int)kt_get_a_sfreq(track);
  int bps      = kt_get_a_bps(track);

  if (-1 == bps)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"bits per second (bps)\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

  xtr_base_c::create_file(master, track);

  memcpy(&m_wh.riff.id,      "RIFF", 4);
  memcpy(&m_wh.riff.wave_id, "WAVE", 4);
  memcpy(&m_wh.format.id,    "fmt ", 4);
  memcpy(&m_wh.data.id,      "data", 4);

  put_uint32_le(&m_wh.format.len,              16);
  put_uint16_le(&m_wh.common.wFormatTag,        1);
  put_uint16_le(&m_wh.common.wChannels,        channels);
  put_uint32_le(&m_wh.common.dwSamplesPerSec,  sfreq);
  put_uint32_le(&m_wh.common.dwAvgBytesPerSec, channels * sfreq * bps / 8);
  put_uint16_le(&m_wh.common.wBlockAlign,       4);
  put_uint16_le(&m_wh.common.wBitsPerSample,   bps);

  m_out->write(&m_wh, sizeof(wave_header));
}

void
xtr_wav_c::finish_file() {
  m_out->setFilePointer(0);

  put_uint32_le(&m_wh.riff.len, m_bytes_written + 36);
  put_uint32_le(&m_wh.data.len, m_bytes_written);

  m_out->write(&m_wh, sizeof(wave_header));
}

// ------------------------------------------------------------------------

xtr_wavpack4_c::xtr_wavpack4_c(const std::string &codec_id,
                               int64_t tid,
                               track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_number_of_samples(0)
  , m_extract_blockadd_level(tspec.extract_blockadd_level)
  , m_corr_out(nullptr)
{
}

void
xtr_wavpack4_c::create_file(xtr_base_c *master,
                            KaxTrackEntry &track) {
  memory_cptr mpriv;

  init_content_decoder(track);

  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (priv)
    mpriv = decode_codec_private(priv);

  if ((nullptr == priv) || (2 > mpriv->get_size()))
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);
  memcpy(m_version, mpriv->get_buffer(), 2);

  xtr_base_c::create_file(master, track);

  m_channels = kt_get_a_channels(track);

  if ((0 != kt_get_max_blockadd_id(track)) && (0 != m_extract_blockadd_level)) {
    std::string corr_name = m_file_name;
    size_t pos            = corr_name.rfind('.');

    if ((std::string::npos != pos) && (0 != pos))
      corr_name.erase(pos + 1);
    corr_name += "wvc";

    try {
      m_corr_out = mm_write_buffer_io_c::open(corr_name, 5 * 1024 * 1024);
    } catch (...) {
      mxerror(boost::format(Y("The file '%1%' could not be opened for writing (%2%).\n")) % corr_name % strerror(errno));
    }
  }
}

void
xtr_wavpack4_c::handle_frame(memory_cptr &frame,
                             KaxBlockAdditions *additions,
                             int64_t,
                             int64_t,
                             int64_t,
                             int64_t,
                             bool,
                             bool,
                             bool) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  // build the main header

  binary wv_header[32];
  memcpy(wv_header, "wvpk", 4);
  memcpy(&wv_header[8], m_version, 2); // version
  wv_header[10] = 0;                   // track_no
  wv_header[11] = 0;                   // index_no
  wv_header[12] = 0xFF;                // total_samples is unknown
  wv_header[13] = 0xFF;
  wv_header[14] = 0xFF;
  wv_header[15] = 0xFF;
  put_uint32_le(&wv_header[16], m_number_of_samples); // block_index

  const binary *mybuffer  = frame->get_buffer();
  int data_size           = frame->get_size();
  m_number_of_samples    += get_uint32_le(mybuffer);

  // rest of the header:
  memcpy(&wv_header[20], mybuffer, 3 * sizeof(uint32_t));

  std::vector<uint32_t> flags;

  // support multi-track files
  if (2 < m_channels) {
    uint32_t block_size = get_uint32_le(&mybuffer[12]);

    put_uint32_le(&wv_header[4], block_size + 24);  // ck_size
    m_out->write(wv_header, 32);
    flags.push_back(*(uint32_t *)&mybuffer[4]);
    mybuffer += 16;
    m_out->write(mybuffer, block_size);
    mybuffer  += block_size;
    data_size -= block_size + 16;
    while (0 < data_size) {
      block_size = get_uint32_le(&mybuffer[8]);
      memcpy(&wv_header[24], mybuffer, 8);
      put_uint32_le(&wv_header[4], block_size + 24);
      m_out->write(wv_header, 32);

      flags.push_back(*(uint32_t *)mybuffer);
      mybuffer += 12;
      m_out->write(mybuffer, block_size);

      mybuffer  += block_size;
      data_size -= block_size + 12;
    }

  } else {
    put_uint32_le(&wv_header[4], data_size + 12); // ck_size
    m_out->write(wv_header, 32);
    m_out->write(&mybuffer[12], data_size - 12); // the rest of the
  }

  // support hybrid mode data
  if (m_corr_out && (nullptr != additions)) {
    KaxBlockMore *block_more = FINDFIRST(additions, KaxBlockMore);

    if (nullptr == block_more)
      return;

    KaxBlockAdditional *block_addition = FINDFIRST(block_more, KaxBlockAdditional);
    if (nullptr == block_addition)
      return;

    data_size = block_addition->GetSize();
    mybuffer  = block_addition->GetBuffer();

    if (2 < m_channels) {
      size_t flags_index = 0;

      while (0 < data_size) {
        uint32_t block_size = get_uint32_le(&mybuffer[4]);

        put_uint32_le(&wv_header[4], block_size + 24); // ck_size
        memcpy(&wv_header[24], &flags[flags_index++], 4); // flags
        memcpy(&wv_header[28], mybuffer, 4); // crc
        m_corr_out->write(wv_header, 32);
        mybuffer += 8;
        m_corr_out->write(mybuffer, block_size);
        mybuffer += block_size;
        data_size -= 8 + block_size;
      }

    } else {
      put_uint32_le(&wv_header[4], data_size + 20); // ck_size
      memcpy(&wv_header[28], mybuffer, 4); // crc
      m_corr_out->write(wv_header, 32);
      m_corr_out->write(&mybuffer[4], data_size - 4);
    }
  }
}

void
xtr_wavpack4_c::finish_file() {
  m_out->setFilePointer(12);
  m_out->write_uint32_le(m_number_of_samples);

  if (m_corr_out) {
    m_corr_out->setFilePointer(12);
    m_corr_out->write_uint32_le(m_number_of_samples);
  }
}
