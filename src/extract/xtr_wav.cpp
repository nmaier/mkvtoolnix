/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>
   and Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "os.h"

#include <matroska/KaxBlock.h>

#include "common.h"
#include "commonebml.h"
#include "xtr_wav.h"

xtr_wav_c::xtr_wav_c(const string &_codec_id,
                     int64_t _tid,
                     track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec) {

  memset(&wh, 0, sizeof(wave_header));
}

void
xtr_wav_c::create_file(xtr_base_c *_master,
                       KaxTrackEntry &track) {
  int channels, sfreq, bps;

  channels = kt_get_a_channels(track);
  sfreq = (int)kt_get_a_sfreq(track);
  bps = kt_get_a_bps(track);
  if (-1 == bps)
    mxerror("Track %lld with the CodecID '%s' is missing the \"bits per "
            "second (bps)\" element and cannot be extracted.\n",
            tid, codec_id.c_str());

  xtr_base_c::create_file(_master, track);

  memcpy(&wh.riff.id, "RIFF", 4);
  memcpy(&wh.riff.wave_id, "WAVE", 4);
  memcpy(&wh.format.id, "fmt ", 4);
  put_uint32_le(&wh.format.len, 16);
  put_uint16_le(&wh.common.wFormatTag, 1);
  put_uint16_le(&wh.common.wChannels, channels);
  put_uint32_le(&wh.common.dwSamplesPerSec, sfreq);
  put_uint32_le(&wh.common.dwAvgBytesPerSec, channels * sfreq * bps / 8);
  put_uint16_le(&wh.common.wBlockAlign, 4);
  put_uint16_le(&wh.common.wBitsPerSample, bps);
  memcpy(&wh.data.id, "data", 4);

  out->write(&wh, sizeof(wave_header));
}

void
xtr_wav_c::finish_file() {
  out->setFilePointer(0);
  put_uint32_le(&wh.riff.len, bytes_written + 36);
  put_uint32_le(&wh.data.len, bytes_written);
  out->write(&wh, sizeof(wave_header));
}

// ------------------------------------------------------------------------

xtr_wavpack4_c::xtr_wavpack4_c(const string &_codec_id,
                               int64_t _tid,
                               track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  number_of_samples(0), extract_blockadd_level(tspec.extract_blockadd_level),
  corr_out(NULL) {
}

void
xtr_wavpack4_c::create_file(xtr_base_c *_master,
                            KaxTrackEntry &track) {
  KaxCodecPrivate *priv;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if ((NULL == priv) || (2 > priv->GetSize()))
    mxerror("Track %lld with the CodecID '%s' is missing the \"codec private"
            "\" element and cannot be extracted.\n", tid, codec_id.c_str());
  memcpy(version, &binary(*priv), 2);

  channels = kt_get_a_channels(track);

  xtr_base_c::create_file(_master, track);

  if ((0 != kt_get_max_blockadd_id(track)) && (0 != extract_blockadd_level)) {
    string corr_name = file_name;
    size_t pos = corr_name.rfind('.');

    if ((string::npos != pos) && (0 != pos))
      corr_name.erase(pos + 1);
    corr_name += "wvc";
    try {
      corr_out = new mm_file_io_c(corr_name, MODE_CREATE);
    } catch (...) {
      mxerror(" The file '%s' could not be opened for writing (%s).\n",
              corr_name.c_str(), strerror(errno));
    }
  }
}

void
xtr_wavpack4_c::handle_block(KaxBlock &block,
                             KaxBlockAdditions *additions,
                             int64_t timecode,
                             int64_t duration,
                             int64_t bref,
                             int64_t fref) {
  binary wv_header[32], *mybuffer;
  int i, data_size;
  vector<uint32_t> flags;

  for (i = 0; i < block.NumberFrames(); i++) {
    DataBuffer &data = block.GetBuffer(i);

    // build the main header

    wv_header[0] = 'w';
    wv_header[1] = 'v';
    wv_header[2] = 'p';
    wv_header[3] = 'k';
    memcpy(&wv_header[8], version, 2); // version
    wv_header[10] = 0; // track_no
    wv_header[11] = 0; // index_no
    wv_header[12] = 0xFF; // total_samples is unknown
    wv_header[13] = 0xFF;
    wv_header[14] = 0xFF;
    wv_header[15] = 0xFF;
    put_uint32_le(&wv_header[16], number_of_samples); // block_index
    mybuffer = data.Buffer();
    data_size = data.Size();
    number_of_samples += get_uint32_le(mybuffer);

    // rest of the header:
    memcpy(&wv_header[20], mybuffer, 3 * sizeof(uint32_t));
    // support multi-track files
    if (channels > 2) {
      uint32_t block_size = get_uint32_le(&mybuffer[12]);

      flags.clear();
      put_uint32_le(&wv_header[4], block_size + 24);  // ck_size
      out->write(wv_header, 32);
      flags.push_back(*(uint32_t *)&mybuffer[4]);
      mybuffer += 16;
      out->write(mybuffer, block_size);
      mybuffer += block_size;
      data_size -= block_size + 16;
      while (data_size > 0) {
        block_size = get_uint32_le(&mybuffer[8]);
        memcpy(&wv_header[24], mybuffer, 8);
        put_uint32_le(&wv_header[4], block_size + 24);
        out->write(wv_header, 32);
        flags.push_back(*(uint32_t *)mybuffer);
        mybuffer += 12;
        out->write(mybuffer, block_size);
        mybuffer += block_size;
        data_size -= block_size + 12;
      }
    } else {
      put_uint32_le(&wv_header[4], data_size + 12); // ck_size
      out->write(wv_header, 32);
      out->write(&mybuffer[12], data_size - 12); // the rest of the
    }

    // support hybrid mode data
    if ((NULL != corr_out) && (NULL != additions)) {
      KaxBlockMore *block_more = FINDFIRST(additions, KaxBlockMore);

      if (block_more == NULL)
        break;
      KaxBlockAdditional *block_addition =
        FINDFIRST(block_more, KaxBlockAdditional);
      if (block_addition == NULL)
        break;

      data_size = block_addition->GetSize();
      mybuffer = block_addition->GetBuffer();
      if (channels > 2) {
        size_t flags_index = 0;

        while (data_size > 0) {
          uint32_t block_size = get_uint32_le(&mybuffer[4]);

          put_uint32_le(&wv_header[4], block_size + 24); // ck_size
          memcpy(&wv_header[24], &flags[flags_index++], 4); // flags
          memcpy(&wv_header[28], mybuffer, 4); // crc
          corr_out->write(wv_header, 32);
          mybuffer += 8;
          corr_out->write(mybuffer, block_size);
          mybuffer += block_size;
          data_size -= 8 + block_size;
        }
      } else {
        put_uint32_le(&wv_header[4], data_size + 20); // ck_size
        memcpy(&wv_header[28], mybuffer, 4); // crc
        corr_out->write(wv_header, 32);
        corr_out->write(&mybuffer[4], data_size - 4);
      }
    }
  }
}

void
xtr_wavpack4_c::finish_file() {
  delete corr_out;
}
