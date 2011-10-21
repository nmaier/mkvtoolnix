/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   WAV reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Initial DTS support by Peter Niemayer <niemayer@isg.de> and
     modified by Moritz Bunkus.
*/

#include "common/common_pch.h"

#include <algorithm>

#if defined(COMP_CYGWIN)
#include <sys/unistd.h>         // Needed for swab()
#elif __GNUC__ == 2
#define __USE_XOPEN
#include <unistd.h>
#elif defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#include "common/ac3.h"
#include "common/dts.h"
#include "common/endian.h"
#include "common/error.h"
#include "input/r_wav.h"
#include "output/p_ac3.h"
#include "output/p_dts.h"
#include "output/p_pcm.h"

#include <avilib.h> // for wave_header

#define AC3ACM_READ_SIZE 100000

wav_demuxer_c::wav_demuxer_c(wav_reader_c *reader,
                             wave_header *wheader)
  : m_reader(reader)
  , m_wheader(wheader)
  , m_ptzr(NULL)
  , m_ti(reader->m_ti)
{
}

class wav_ac3acm_demuxer_c: public wav_demuxer_c {
protected:
  ac3_header_t m_ac3header;
  memory_cptr m_buf[2];
  int m_cur_buf;
  bool m_swap_bytes;

public:
  wav_ac3acm_demuxer_c(wav_reader_c *reader, wave_header *wheader);

  virtual ~wav_ac3acm_demuxer_c();

  virtual bool probe(mm_io_cptr &io);

  virtual int64_t get_preferred_input_size() {
    return AC3ACM_READ_SIZE;
  };

  virtual unsigned char *get_buffer() {
    return m_buf[m_cur_buf]->get_buffer();
  };

  virtual void process(int64_t len);
  virtual generic_packetizer_c *create_packetizer();
  virtual std::string get_codec() {
    return 16 == m_ac3header.bsid ? "EAC3" : "AC3";
  };

protected:
  virtual int decode_buffer(int len);
};

#define AC3WAV_BLOCK_SIZE   6144
#define AC3WAV_SYNC_WORD1 0xf872
#define AC3WAV_SYNC_WORD2 0x4e1f

// Structure of AC3-in-WAV:
//
// AA BB C D EE F..F 0..0
//
// Index | Size       | Meaning
// ------+------------+---------------------------
// A     | 2          | AC3WAV_SYNC_WORD1
// B     | 2          | AC3WAV_SYNC_WORD2
// C     | 1          | BSMOD
// D     | 1          | data type; 0x01 = AC3
// E     | 2          | number of bits in payload
// F     | E/8        | one AC3 packet
// 0     | 6144-E/8-8 | zero padding

class wav_ac3wav_demuxer_c: public wav_ac3acm_demuxer_c {
public:
  wav_ac3wav_demuxer_c(wav_reader_c *n_reader, wave_header *n_wheader);

  virtual ~wav_ac3wav_demuxer_c();

  virtual bool probe(mm_io_cptr &io);

  virtual int64_t get_preferred_input_size() {
    return AC3WAV_BLOCK_SIZE;
  };

  virtual void process(int64_t len);

protected:
  virtual int decode_buffer(int len);
};

#define DTS_READ_SIZE 65536

class wav_dts_demuxer_c: public wav_demuxer_c {
private:
  bool m_swap_bytes, m_pack_14_16;
  dts_header_t m_dtsheader;
  memory_cptr m_buf[2];
  int m_cur_buf;

public:
  wav_dts_demuxer_c(wav_reader_c *reader, wave_header *wheader);

  virtual ~wav_dts_demuxer_c();

  virtual bool probe(mm_io_cptr &io);

  virtual int64_t get_preferred_input_size() {
    return DTS_READ_SIZE;
  };

  virtual unsigned char *get_buffer() {
    return m_buf[m_cur_buf]->get_buffer();
  };

  virtual void process(int64_t len);
  virtual generic_packetizer_c *create_packetizer();
  virtual std::string get_codec() {
    return "DTS";
  };

private:
  virtual int decode_buffer(int len);
};

class wav_pcm_demuxer_c: public wav_demuxer_c {
private:
  int m_bps;
  memory_cptr m_buffer;
  bool ieee_float;

public:
  wav_pcm_demuxer_c(wav_reader_c *reader, wave_header *wheader, bool _float);

  virtual ~wav_pcm_demuxer_c();

  virtual int64_t get_preferred_input_size() {
    return m_bps;
  };

  virtual unsigned char *get_buffer() {
    return m_buffer->get_buffer();
  };

  virtual void process(int64_t len);
  virtual generic_packetizer_c *create_packetizer();
  virtual std::string get_codec() {
    return "PCM";
  };

  virtual bool probe(mm_io_cptr &) {
    return true;
  };
};

// ----------------------------------------------------------

wav_ac3acm_demuxer_c::wav_ac3acm_demuxer_c(wav_reader_c *reader,
                                           wave_header  *wheader):
  wav_demuxer_c(reader, wheader),
  m_cur_buf(0),
  m_swap_bytes(false) {

  m_buf[0] = memory_c::alloc(AC3ACM_READ_SIZE);
  m_buf[1] = memory_c::alloc(AC3ACM_READ_SIZE);
}

wav_ac3acm_demuxer_c::~wav_ac3acm_demuxer_c() {
}

bool
wav_ac3acm_demuxer_c::probe(mm_io_cptr &io) {
  io->save_pos();
  int len = io->read(m_buf[m_cur_buf]->get_buffer(), AC3ACM_READ_SIZE);
  io->restore_pos();

  int pos = find_consecutive_ac3_headers(m_buf[m_cur_buf]->get_buffer(), len, 4);

  if (-1 == pos) {
    m_swap_bytes = true;
    decode_buffer(len);
    pos = find_consecutive_ac3_headers(m_buf[m_cur_buf]->get_buffer(), len, 4);
  }

  if (-1 == pos)
    return false;

  find_ac3_header(m_buf[m_cur_buf]->get_buffer() + pos, len - pos, &m_ac3header, true);

  return true;
}

int
wav_ac3acm_demuxer_c::decode_buffer(int len) {
  if ((2 < len) && m_swap_bytes) {
    swab((char *)m_buf[m_cur_buf]->get_buffer(), (char *)m_buf[m_cur_buf ^ 1]->get_buffer(), len);
    m_cur_buf ^= 1;
  }

  return 0;
}

generic_packetizer_c *
wav_ac3acm_demuxer_c::create_packetizer() {
  m_ptzr = new ac3_packetizer_c(m_reader, m_ti, m_ac3header.sample_rate, m_ac3header.channels, m_ac3header.bsid);

  show_packetizer_info(0, m_ptzr);

  return m_ptzr;
}

void
wav_ac3acm_demuxer_c::process(int64_t size) {
  if (0 >= size)
    return;

  decode_buffer(size);
  m_ptzr->process(new packet_t(new memory_c(m_buf[m_cur_buf]->get_buffer(), size, false)));
}

// ----------------------------------------------------------

wav_ac3wav_demuxer_c::wav_ac3wav_demuxer_c(wav_reader_c *reader,
                                           wave_header  *wheader):
  wav_ac3acm_demuxer_c(reader, wheader) {
}

wav_ac3wav_demuxer_c::~wav_ac3wav_demuxer_c() {
}

bool
wav_ac3wav_demuxer_c::probe(mm_io_cptr &io) {
  io->save_pos();
  int len = io->read(m_buf[m_cur_buf]->get_buffer(), AC3WAV_BLOCK_SIZE);
  io->restore_pos();

  if (decode_buffer(len) > 0)
    return true;

  m_swap_bytes = true;
  return decode_buffer(len) > 0;
}

int
wav_ac3wav_demuxer_c::decode_buffer(int len) {
  if (len < 8)
    return -1;

  if (m_swap_bytes) {
    memcpy(      m_buf[m_cur_buf ^ 1]->get_buffer(),         m_buf[m_cur_buf]->get_buffer(),         8);
    swab((char *)m_buf[m_cur_buf]->get_buffer() + 8, (char *)m_buf[m_cur_buf ^ 1]->get_buffer() + 8, len - 8);
    m_cur_buf ^= 1;
  }

  unsigned char *base = m_buf[m_cur_buf]->get_buffer();

  if ((get_uint16_le(&base[0]) != AC3WAV_SYNC_WORD1) || (get_uint16_le(&base[2]) != AC3WAV_SYNC_WORD2) || (0x01 != base[4]))
    return -1;

  int payload_len = get_uint16_le(&base[6]) / 8;

  if ((payload_len + 8) > len)
    return -1;

  int pos = find_ac3_header(&base[8], payload_len, &m_ac3header, true);

  return 0 == pos ? payload_len : -1;
}

void
wav_ac3wav_demuxer_c::process(int64_t size) {
  if (0 >= size)
    return;

  long dec_len = decode_buffer(size);
  if (0 < dec_len)
    m_ptzr->process(new packet_t(new memory_c(m_buf[m_cur_buf]->get_buffer() + 8, dec_len, false)));
}

// ----------------------------------------------------------

wav_dts_demuxer_c::wav_dts_demuxer_c(wav_reader_c *reader,
                                     wave_header  *wheader):
  wav_demuxer_c(reader, wheader),
  m_swap_bytes(false),
  m_pack_14_16(false),
  m_cur_buf(0) {

  m_buf[0] = memory_c::alloc(DTS_READ_SIZE);
  m_buf[1] = memory_c::alloc(DTS_READ_SIZE);
}

wav_dts_demuxer_c::~wav_dts_demuxer_c() {
}

bool
wav_dts_demuxer_c::probe(mm_io_cptr &io) {
  io->save_pos();
  int len = io->read(m_buf[m_cur_buf]->get_buffer(), DTS_READ_SIZE);
  io->restore_pos();

  if (detect_dts(m_buf[m_cur_buf]->get_buffer(), len, m_pack_14_16, m_swap_bytes)) {
    len     = decode_buffer(len);
    int pos = find_consecutive_dts_headers(m_buf[m_cur_buf]->get_buffer(), len, 5);
    if (0 <= pos) {
      if (0 > find_dts_header(m_buf[m_cur_buf]->get_buffer() + pos, len - pos, &m_dtsheader))
        return false;

      mxverb(3, boost::format("DTSinWAV: 14->16 %1% swap %2%\n") % m_pack_14_16 % m_swap_bytes);
      return true;
    }
  }

  return false;
}

int
wav_dts_demuxer_c::decode_buffer(int len) {
  if (m_swap_bytes) {
    swab((char *)m_buf[m_cur_buf]->get_buffer(), (char *)m_buf[m_cur_buf ^ 1]->get_buffer(), len);
    m_cur_buf ^= 1;
  }

  if (m_pack_14_16) {
    dts_14_to_dts_16((unsigned short *)m_buf[m_cur_buf]->get_buffer(), len / 2, (unsigned short *)m_buf[m_cur_buf ^ 1]->get_buffer());
    m_cur_buf ^= 1;
    len        = len * 7 / 8;
  }

  return len;
}

generic_packetizer_c *
wav_dts_demuxer_c::create_packetizer() {
  m_ptzr = new dts_packetizer_c(m_reader, m_ti, m_dtsheader);

  // .wav with DTS are always filled up with other stuff to match the bitrate.
  ((dts_packetizer_c *)m_ptzr)->set_skipping_is_normal(true);

  show_packetizer_info(0, m_ptzr);

  if (1 < verbose)
    print_dts_header(&m_dtsheader);

  return m_ptzr;
}

void
wav_dts_demuxer_c::process(int64_t size) {
  if (0 >= size)
    return;

  long dec_len = decode_buffer(size);
  m_ptzr->process(new packet_t(new memory_c(m_buf[m_cur_buf]->get_buffer(), dec_len, false)));
}

// ----------------------------------------------------------

wav_pcm_demuxer_c::wav_pcm_demuxer_c(wav_reader_c *reader,
                                     wave_header  *wheader,
                                     bool _float):
  wav_demuxer_c(reader, wheader),
  m_bps(0),
  ieee_float(_float) {

  m_bps    = get_uint16_le(&m_wheader->common.wChannels) * get_uint16_le(&m_wheader->common.wBitsPerSample) * get_uint32_le(&m_wheader->common.dwSamplesPerSec) / 8;
  m_buffer = memory_c::alloc(m_bps);
}

wav_pcm_demuxer_c::~wav_pcm_demuxer_c() {
}

generic_packetizer_c *
wav_pcm_demuxer_c::create_packetizer() {
  m_ptzr = new pcm_packetizer_c(m_reader, m_ti,
                                get_uint32_le(&m_wheader->common.dwSamplesPerSec),
                                get_uint16_le(&m_wheader->common.wChannels),
                                get_uint16_le(&m_wheader->common.wBitsPerSample),
                                false, ieee_float);

  show_packetizer_info(0, m_ptzr);

  return m_ptzr;
}

void
wav_pcm_demuxer_c::process(int64_t len) {
  if (0 >= len)
    return;

  m_ptzr->process(new packet_t(new memory_c(m_buffer->get_buffer(), len, false)));
}

// ----------------------------------------------------------

int
wav_reader_c::probe_file(mm_io_c *io,
                         uint64_t size) {
  wave_header wheader;

  if (sizeof(wave_header) > size)
    return 0;
  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(&wheader.riff, sizeof(wheader.riff)) != sizeof(wheader.riff))
      return 0;
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }

  if (strncmp((char *)wheader.riff.id,      "RIFF", 4) ||
      strncmp((char *)wheader.riff.wave_id, "WAVE", 4))
    return 0;

  return 1;
}

wav_reader_c::wav_reader_c(track_info_c &ti_)
  throw (error_c):
  generic_reader_c(ti_),
  m_bytes_processed(0),
  m_bytes_in_data_chunks(0),
  m_remaining_bytes_in_current_data_chunk(0),
  m_cur_data_chunk_idx(0) {

  int64_t size;

  try {
    m_io = mm_io_cptr(new mm_file_io_c(m_ti.m_fname));
    m_io->setFilePointer(0, seek_end);
    size = m_io->getFilePointer();
    m_io->setFilePointer(0, seek_beginning);
  } catch (...) {
    throw error_c(boost::format(Y("%1%: Could not open the source file.")) % get_format_name());
  }

  if (!wav_reader_c::probe_file(m_io.get_object(), size))
    throw error_c(Y("wav_reader: Source is not a valid WAVE file."));

  parse_file();
  create_demuxer();
}

wav_reader_c::~wav_reader_c() {
}

void
wav_reader_c::parse_file() {
  int chunk_idx;

  if (m_io->read(&m_wheader.riff, sizeof(m_wheader.riff)) != sizeof(m_wheader.riff))
    throw error_c(Y("wav_reader: could not read WAVE header."));

  scan_chunks();

  if ((chunk_idx = find_chunk("fmt ")) == -1)
    throw error_c(Y("wav_reader: No format chunk found."));

  m_io->setFilePointer(m_chunks[chunk_idx].pos, seek_beginning);

  try {
    if (m_io->read(&m_wheader.format, sizeof(m_wheader.format)) != sizeof(m_wheader.format))
      throw false;

    if (static_cast<uint64_t>(m_chunks[chunk_idx].len) >= sizeof(alWAVEFORMATEXTENSIBLE)) {
      alWAVEFORMATEXTENSIBLE format;
      if (m_io->read(&format, sizeof(format)) != sizeof(format))
        throw false;
      memcpy(&m_wheader.common, &format, sizeof(m_wheader.common));

      m_format_tag = get_uint16_le(&m_wheader.common.wFormatTag);
      if (0xfffe == m_format_tag)
        m_format_tag = get_uint32_le(&format.extension.guid.data1);

    } else if (m_io->read(&m_wheader.common, sizeof(m_wheader.common)) != sizeof(m_wheader.common))
      throw false;

    else
      m_format_tag = get_uint16_le(&m_wheader.common.wFormatTag);

  } catch (...) {
    throw error_c(Y("wav_reader: The format chunk could not be read."));
  }

  if ((m_cur_data_chunk_idx = find_chunk("data", 0, false)) == -1)
    throw error_c(Y("wav_reader: No data chunk was found."));

  if (debugging_requested("wav_reader") || debugging_requested("wav_reader_headers"))
    dump_headers();

  m_io->setFilePointer(m_chunks[m_cur_data_chunk_idx].pos + sizeof(struct chunk_struct), seek_beginning);

  m_remaining_bytes_in_current_data_chunk = m_chunks[m_cur_data_chunk_idx].len;
}

void
wav_reader_c::dump_headers() {
  mxinfo(boost::format("File '%1%' wave_header dump\n"
                       "  riff:\n"
                       "    id:      %2%%3%%4%%5%\n"
                       "    len:     %6%\n"
                       "    wave_id: %7%%8%%9%%10%\n"
                       "  common:\n"
                       "    wFormatTag:       %|11$04x|\n"
                       "    wChannels:        %12%\n"
                       "    dwSamplesPerSec:  %13%\n"
                       "    dwAvgBytesPerSec: %14%\n"
                       "    wBlockAlign:      %15%\n"
                       "    wBitsPerSample:   %16%\n"
                       "  actual format_tag:  %17%\n")
         % m_ti.m_fname
         % char(m_wheader.riff.id[0]) % char(m_wheader.riff.id[1]) % char(m_wheader.riff.id[2]) % char(m_wheader.riff.id[3])
         % get_uint32_le(&m_wheader.riff.len)
         % char(m_wheader.riff.wave_id[0]) % char(m_wheader.riff.wave_id[1]) % char(m_wheader.riff.wave_id[2]) % char(m_wheader.riff.wave_id[3])
         % get_uint16_le(&m_wheader.common.wFormatTag)
         % get_uint16_le(&m_wheader.common.wChannels)
         % get_uint32_le(&m_wheader.common.dwSamplesPerSec)
         % get_uint32_le(&m_wheader.common.dwAvgBytesPerSec)
         % get_uint16_le(&m_wheader.common.wBlockAlign)
         % get_uint16_le(&m_wheader.common.wBitsPerSample)
         % m_format_tag);
}

void
wav_reader_c::create_demuxer() {
  m_ti.m_id = 0;                    // ID for this track.

  if (0x2000 == m_format_tag) {
    m_demuxer = wav_demuxer_cptr(new wav_ac3acm_demuxer_c(this, &m_wheader));
    if (!m_demuxer->probe(m_io))
      m_demuxer.clear();
  }

  if (!m_demuxer.is_set()) {
    m_demuxer = wav_demuxer_cptr(new wav_dts_demuxer_c(this, &m_wheader));
    if (!m_demuxer->probe(m_io))
      m_demuxer.clear();
  }

  if (!m_demuxer.is_set()) {
    m_demuxer = wav_demuxer_cptr(new wav_ac3wav_demuxer_c(this, &m_wheader));
    if (!m_demuxer->probe(m_io))
      m_demuxer.clear();
  }

  if (!m_demuxer.is_set() && ((0x0001 == m_format_tag) || (0x0003 == m_format_tag)))
    m_demuxer = wav_demuxer_cptr(new wav_pcm_demuxer_c(this, &m_wheader, 0x00003 == m_format_tag));

  show_demuxer_info();
}

void
wav_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0) || !m_demuxer.is_set())
    return;

  add_packetizer(m_demuxer->create_packetizer());
}

file_status_e
wav_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (!m_demuxer.is_set())
    return FILE_STATUS_DONE;

  int64_t        requested_bytes = std::min(m_remaining_bytes_in_current_data_chunk, m_demuxer->get_preferred_input_size());
  unsigned char *buffer          = m_demuxer->get_buffer();
  int64_t        num_read;

  num_read = m_io->read(buffer, requested_bytes);

  if (0 >= num_read)
    return flush_packetizers();

  m_demuxer->process(num_read);

  m_bytes_processed                       += num_read;
  m_remaining_bytes_in_current_data_chunk -= num_read;

  if (!m_remaining_bytes_in_current_data_chunk) {
    m_cur_data_chunk_idx = find_chunk("data", m_cur_data_chunk_idx + 1, false);

    if (-1 == m_cur_data_chunk_idx)
      return flush_packetizers();

    m_io->setFilePointer(m_chunks[m_cur_data_chunk_idx].pos + sizeof(struct chunk_struct), seek_beginning);

    m_remaining_bytes_in_current_data_chunk = m_chunks[m_cur_data_chunk_idx].len;
  }

  return FILE_STATUS_MOREDATA;
}

void
wav_reader_c::scan_chunks() {
  wav_chunk_t new_chunk;
  bool debug_chunks = debugging_requested("wav_reader") || debugging_requested("wav_reader_chunks");

  try {
    int64_t file_size = m_io->get_size();

    while (true) {
      new_chunk.pos = m_io->getFilePointer();

      if (m_io->read(new_chunk.id, 4) != 4)
        return;

      new_chunk.len = m_io->read_uint32_le();

      if (debug_chunks)
        mxinfo(boost::format("wav_reader_c::scan_chunks() new chunk at %1% type %2% length %3%\n")
               % new_chunk.pos % get_displayable_string(new_chunk.id, 4) % new_chunk.len);

      if (!strncasecmp(new_chunk.id, "data", 4))
        m_bytes_in_data_chunks += new_chunk.len;

      else if (!m_chunks.empty() && !strncasecmp(m_chunks.back().id, "data", 4) && (file_size > 0x100000000ll)) {
        wav_chunk_t &previous_chunk  = m_chunks.back();
        int64_t this_chunk_len       = file_size - previous_chunk.pos - sizeof(struct chunk_struct);
        m_bytes_in_data_chunks      -= previous_chunk.len;
        m_bytes_in_data_chunks      += this_chunk_len;
        previous_chunk.len           = this_chunk_len;

        if (debug_chunks)
          mxinfo(boost::format("wav_reader_c::scan_chunks() hugh data chunk with wrong length at %1%; re-calculated from file size; new length %2%\n")
                 % previous_chunk.pos % previous_chunk.len);

        break;
      }

      m_chunks.push_back(new_chunk);
      m_io->setFilePointer(new_chunk.len, seek_current);

    }
  } catch (...) {
  }
}

int
wav_reader_c::find_chunk(const char *id,
                         int start_idx,
                         bool allow_empty) {
  size_t idx;

  for (idx = start_idx; idx < m_chunks.size(); ++idx)
    if (!strncasecmp(m_chunks[idx].id, id, 4) && (allow_empty || m_chunks[idx].len))
      return idx;

  return -1;
}

int
wav_reader_c::get_progress() {
  return m_bytes_in_data_chunks ? (100 * m_bytes_processed / m_bytes_in_data_chunks) : 100;
}

void
wav_reader_c::identify() {
  if (m_demuxer.is_set()) {
    id_result_container();
    id_result_track(0, ID_RESULT_TRACK_AUDIO, m_demuxer->get_codec());

  } else {
    uint16_t format_tag = get_uint16_le(&m_wheader.common.wFormatTag);
    id_result_container_unsupported(m_io->get_file_name(), (boost::format("RIFF WAVE (wFormatTag = 0x%|1$04x|)") % format_tag).str());
  }
}
