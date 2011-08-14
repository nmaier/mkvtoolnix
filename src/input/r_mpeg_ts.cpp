/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MPEG TS (transport stream) demultiplexer module

   Written by Massimo Callegari <massimocallegari@yahoo.it>
   and Moritz Bunkus <moritz@bunkus.org>.
*/

#include <iostream>

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/math.h"
#include "common/mp3.h"
#include "common/ac3.h"
#include "common/truehd.h"
#include "common/mpeg1_2.h"
#include "common/mpeg4_p2.h"
#include "output/p_mpeg1_2.h"
#include "output/p_avc.h"
#include "output/p_mp3.h"
#include "output/p_ac3.h"
#include "output/p_truehd.h"

#include "input/r_mpeg_ts.h"


#define TS_CONSECUTIVE_PACKETS 16
#define TS_PROBE_SIZE          (2 * TS_CONSECUTIVE_PACKETS * 204)
#define TS_PIDS_DETECT_SIZE    4 * 1024 * 1024

#define GET_PID(p)             (((static_cast<uint16_t>(p->pid_msb) << 8) | p->pid_lsb) & 0x1FFF)
#define CONTINUITY_COUNTER(p)  (static_cast<unsigned char>(p->continuity_counter) & 0x0F)
#define SECTION_LENGTH(p)      (((static_cast<uint16_t>(p->section_length_msb) << 8) | p->section_length_lsb) & 0x0FFF)
#define CRC32(p)               ((static_cast<uint32_t>(p->crc_3msb) << 24) | (static_cast<uint32_t>(p->crc_2msb) << 16) | (static_cast<uint32_t>(p->crc_1msb) <<  8) | (static_cast<uint32_t>(p->crc_lsb)))
#define PROGRAM_NUMBER(p)      ((static_cast<uint16_t>(p->program_number_msb) << 8)| p->program_number_lsb)
#define PCR_PID(p)             (((static_cast<uint16_t>(p->pcr_pid_msb) << 8) | p->pcr_pid_lsb) & 0x1FFF)
#define PROGRAM_INFO_LENGTH(p) (((static_cast<uint16_t>(p->program_info_length_msb) << 8 ) | p->program_info_length_lsb) & 0x0FFF)
#define ES_INFO_LENGTH(p)      (((static_cast<uint16_t>(p->es_info_length_msb) << 8 ) | p->es_info_length_lsb) & 0x0FFF)

int mpeg_ts_reader_c::potential_packet_sizes[] = { 188, 192, 204, 0 };
int mpeg_ts_reader_c::detected_packet_size     = 188;

typedef enum {
  INPUT_PROBE = 0,
  INPUT_READ  = 1,
} mpeg_ts_input_type_t;

typedef enum {
  PAT_TYPE      = 0,
  PMT_TYPE      = 1,
  ES_VIDEO_TYPE = 2,
  ES_AUDIO_TYPE = 3,
  ES_SUBT_TYPE  = 4,
  ES_UNKNOWN    = 5,
} mpeg_ts_pid_type_t;

typedef enum {
  ISO_11172_VIDEO           = 0x01, // ISO/IEC 11172 Video
  ISO_13818_VIDEO           = 0x02, // ISO/IEC 13818-2 Video
  ISO_11172_AUDIO           = 0x03, // ISO/IEC 11172 Audio
  ISO_13818_AUDIO           = 0x04, // ISO/IEC 13818-3 Audio
  ISO_13818_PRIVATE         = 0x05, // ISO/IEC 13818-1 private sections
  ISO_13818_PES_PRIVATE     = 0x06, // ISO/IEC 13818-1 PES packets containing private data
  ISO_13522_MHEG            = 0x07, // ISO/IEC 13512 MHEG
  ISO_13818_DSMCC           = 0x08, // ISO/IEC 13818-1 Annex A  DSM CC
  ISO_13818_TYPE_A          = 0x0a, // ISO/IEC 13818-6 Multiprotocol encapsulation
  ISO_13818_TYPE_B          = 0x0b, // ISO/IEC 13818-6 DSM-CC U-N Messages
  ISO_13818_TYPE_C          = 0x0c, // ISO/IEC 13818-6 Stream Descriptors
  ISO_13818_TYPE_D          = 0x0d, // ISO/IEC 13818-6 Sections (any type, including private data)
  ISO_13818_AUX             = 0x0e, // ISO/IEC 13818-1 auxiliary
  ISO_13818_PART7_AUDIO     = 0x0f, // ISO/IEC 13818-7 Audio with ADTS transport sytax
  ISO_14496_PART2_VIDEO     = 0x10, // ISO/IEC 14496-2 Visual (MPEG-4)
  ISO_14496_PART3_AUDIO     = 0x11, // ISO/IEC 14496-3 Audio with LATM transport syntax
  ISO_14496_PART10_VIDEO    = 0x1b, // ISO/IEC 14496-10 Video (MPEG-4 part 10/AVC, aka H.264)
  STREAM_AUDIO_AC3          = 0x81, // Audio AC3 (A52)
  STREAM_AUDIO_AC3_LOSSLESS = 0x83, // Audio AC3 - Dolby lossless
  STREAM_AUDIO_AC3_PLUS     = 0x84, // Audio AC3 - Dolby Digital Plus
  STREAM_AUDIO_DTS_HD       = 0x85, // Audio DTS HD
  STREAM_VIDEO_VC1          = 0xEA, // Video VC-1
} mpeg_ts_stream_type_t;

bool
mpeg_ts_reader_c::probe_file(mm_io_c *io,
                             uint64_t size) {
  try {
    std::vector<int> positions;
    size = size > TS_PROBE_SIZE ? TS_PROBE_SIZE : size;
    memory_cptr buffer(new memory_c(safemalloc(size), size, true));
    unsigned char *mem = buffer->get_buffer();
    size_t i, k;

    io->setFilePointer(0, seek_beginning);
    size = io->read(mem, size);

    for (i = 0; i < size; ++i)
      if (0x47 == mem[i])
        positions.push_back(i);

    for (i = 0; positions.size() > i; ++i) {
      for (k = 0; 0 != potential_packet_sizes[k]; ++k) {
        unsigned int pos            = positions[i];
        unsigned int packet_size    = potential_packet_sizes[k];
        unsigned int num_startcodes = 1;

        while ((TS_CONSECUTIVE_PACKETS > num_startcodes) && (pos < size) && (0x47 == mem[pos])) {
          pos += packet_size;
          ++num_startcodes;
        }

        if (TS_CONSECUTIVE_PACKETS <= num_startcodes) {
          detected_packet_size = packet_size;
          return true;
        }
      }
    }
  } catch (...) {
  }

  return false;
}

mpeg_ts_reader_c::mpeg_ts_reader_c(track_info_c &_ti)
  throw (error_c)
  : generic_reader_c(_ti)
  , file_done(false)
{
  uint16_t i;

  try {
    io   = new mm_file_io_c(m_ti.m_fname);
    size = io->get_size();
  } catch (...) {
    throw error_c(Y("mpeg_ts_reader_c: Could not open the file."));
  }

  try {
    bool done = io->eof();
    unsigned char buf[205]; // maximum TS packet size + 1

    input_status    = INPUT_PROBE;
    bytes_processed = 0;
    PAT_found       = false;
    PMT_found       = false;
    PMT_pid         = -1;

    mxverb(3, boost::format("mpeg_ts: Starting to build PID list. (packet size: %1%)\n") % detected_packet_size);

    mpeg_ts_track_ptr PAT(new mpeg_ts_track_t);
    PAT->pid        = 0;
    PAT->type       = PAT_TYPE;
    PAT->processed  = false;
    PAT->data_ready = false;
    tracks.push_back(PAT);

    buf[0] = io->read_uint8();

    while (!done) {

      if (buf[0] == 0x47) {
        io->read(buf + 1, detected_packet_size);
        if (buf[detected_packet_size] != 0x47) {
          io->skip(0 - detected_packet_size);
          buf[0] = io->read_uint8();
          continue;
        }
        parse_packet(-1, buf);
        if (PAT_found == true && PMT_found == true && es_to_process == 0)
          done = true;
        io->skip(-1);

      } else
        buf[0] = io->read_uint8(); // advance byte per byte to find a new sync

      done |= io->eof() || (io->getFilePointer() >= TS_PIDS_DETECT_SIZE);
    }
  } catch (...) {
  }
  mxverb(3, boost::format("mpeg_ts: Detection done on %1% bytes\n") % io->getFilePointer());
  io->setFilePointer(0, seek_beginning); // rewind file for later remux

  for (i = 0; i < tracks.size(); i++) {
    tracks[i]->payload.remove(tracks[i]->payload.get_size());
    tracks[i]->processed       = false;
    tracks[i]->data_ready      = false;
    tracks[i]->payload_size    = 0;
    // tracks[i]->timecode_offset = -1;
  }
  global_timecode_offset = -1;
}

mpeg_ts_reader_c::~mpeg_ts_reader_c() {
}

void
mpeg_ts_reader_c::identify() {
  unsigned int i;
  /*
  std::vector<std::string> verbose_info;
  io->create_verbose_identification_info(verbose_info);
  */

  id_result_container("MPEG2 Transport Stream (TS)");

  for (i = 0; i < tracks.size(); i++) {
    mpeg_ts_track_ptr &track = tracks[i];
/*
    verbose_info.clear();

    verbose_info.push_back((boost::format("stream_id:%|1$02x| sub_stream_id:%|2$02x|") % track->id.id % track->id.sub_id).str());
*/
    //if ((0 != track->v_dwidth) && (0 != track->v_dheight))
    //  verbose_info.push_back((boost::format("display_dimensions:%1%x%2%") % track->v_dwidth % track->v_dheight).str());

    id_result_track(i, ES_AUDIO_TYPE == track->type ? ID_RESULT_TRACK_AUDIO : ID_RESULT_TRACK_VIDEO,
                      FOURCC('M', 'P', 'G', '1') == track->fourcc ? "MPEG-1"
                    : FOURCC('M', 'P', 'G', '2') == track->fourcc ? "MPEG-2"
                    : FOURCC('A', 'V', 'C', '1') == track->fourcc ? "AVC/h.264"
                    : FOURCC('W', 'V', 'C', '1') == track->fourcc ? "VC1"
                    : FOURCC('M', 'P', '1', ' ') == track->fourcc ? "MPEG-1 layer 1"
                    : FOURCC('M', 'P', '2', ' ') == track->fourcc ? "MPEG-1 layer 2"
                    : FOURCC('M', 'P', '3', ' ') == track->fourcc ? "MPEG-1 layer 3"
                    : FOURCC('A', 'C', '3', ' ') == track->fourcc ? "AC3"
                    : FOURCC('D', 'T', 'S', ' ') == track->fourcc ? "DTS"
                    : FOURCC('T', 'R', 'H', 'D') == track->fourcc ? "TrueHD"
                    : FOURCC('P', 'C', 'M', ' ') == track->fourcc ? "PCM"
                    : FOURCC('L', 'P', 'C', 'M') == track->fourcc ? "LPCM"
                    : "Unknown" );
    /*
                    :                                               Y("unknown"),
                    verbose_info);*/
  }
}

static const uint32_t crc_table[256] = {
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
  0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
  0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
  0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
  0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
  0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
  0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
  0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
  0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
  0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
  0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
  0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
  0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
  0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
  0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
  0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
  0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
  0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
  0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
  0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
  0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
  0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

unsigned long
mpeg_ts_reader_c::calculate_crc32(unsigned char *data,
                                  int len) {
  register int i;
  unsigned long crc = 0xffffffff;

  for (i = 0; i < len; i++)
    crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];

  return crc;
}

int
mpeg_ts_reader_c::parse_pat(unsigned char *pat) {
  if (pat == NULL) {
    mxverb(3, boost::format("mpeg_ts:parse_pat: Invalid parameters!\n"));
    return -1;
  }

  mpeg_ts_pat_t *pat_header = (mpeg_ts_pat_t *)pat;

  if (pat_header->table_id != 0x00) {
    mxverb(3, boost::format("mpeg_ts:parse_pat: Invalid PAT table_id!\n"));
    return -1;
  }

  if (pat_header->section_syntax_indicator != 1 || pat_header->current_next_indicator == 0) {
    mxverb(3, boost::format("mpeg_ts:parse_pat: Invalid PAT section_syntax_indicator/current_next_indicator!\n"));
    return -1;
  }

  if (pat_header->section_number != 0 || pat_header->last_section_number != 0) {
    mxverb(3, boost::format("mpeg_ts:parse_pat: Unsupported multiple section PAT!\n"));
    return -1;
  }

  unsigned short pat_section_length = SECTION_LENGTH(pat_header);
  uint32_t elapsed_CRC              = calculate_crc32(pat, 3 + pat_section_length - 4/*CRC32*/);
  uint32_t read_CRC                 = CRC32(((mpeg_ts_crc_t *)(pat + 3 + pat_section_length - 4)));

  if (elapsed_CRC != read_CRC) {
    mxverb(3, boost::format("mpeg_ts:parse_pat: Wrong PAT CRC !!! Elapsed = 0x%|1$08x|, read 0x%|2$08x|\n") % elapsed_CRC % read_CRC);
    return -1;
  }

  if (pat_section_length < 13 || pat_section_length > 1021) {
    mxverb(3, boost::format("mpeg_ts:parse_pat: Wrong PAT section_length (= %1%)\n") % pat_section_length);
    return -1;
  }

  unsigned int prog_count = (pat_section_length - 5 - 4/*CRC32*/) / 4;

  unsigned int i                     = 0;
  mpeg_ts_pat_section_t *pat_section = (mpeg_ts_pat_section_t *)(pat + sizeof(mpeg_ts_pat_t));
  for (; i < prog_count; i++, pat_section++) {
    unsigned short local_program_number = PROGRAM_NUMBER(pat_section);
    mxverb(3, boost::format("mpeg_ts:parse_pat: program_number: %1%\n") % local_program_number);

    uint16_t tmp_pid = GET_PID(pat_section);

    if (local_program_number == 0) {
      mxverb(3, boost::format("mpeg_ts:parse_pat: nit_pid: %1%\n") % tmp_pid);
    } else {
      mxverb(3, boost::format("mpeg_ts:parse_pat: pmt_pid: %1%\n") % tmp_pid);

      PAT_found = true;

      bool skip = false;
      for (uint16_t i = 0; i < tracks.size(); i++) {
        if (tracks[i]->pid == tmp_pid) {
          skip = true;
          break;
        }
      }

      if (skip == true)
        continue;

      mpeg_ts_track_ptr PMT(new mpeg_ts_track_t);
      PMT->pid        = tmp_pid;
      PMT->type       = PMT_TYPE;
      PMT->processed  = false;
      PMT->data_ready = false;
      es_to_process   = 0;
      tracks.push_back(PMT);
    }
  }

  return 0;
}

int
mpeg_ts_reader_c::parse_pmt(unsigned char *pmt) {
  if (pmt == NULL) {
    mxverb(3, boost::format("mpeg_ts:parse_pmt: Invalid parameters!\n"));
    return -1;
  }

  mpeg_ts_pmt_t *pmt_header = (mpeg_ts_pmt_t *)pmt;

  if (pmt_header->table_id != 0x02) {
    mxverb(3, boost::format("mpeg_ts:parse_pmt: Invalid PMT table_id!\n"));
    return -1;
  }

  if (pmt_header->section_syntax_indicator != 1 || pmt_header->current_next_indicator == 0) {
    mxverb(3, boost::format("mpeg_ts:parse_pmt: Invalid PMT section_syntax_indicator/current_next_indicator!\n"));
    return -1;
  }

  if (pmt_header->section_number != 0 || pmt_header->last_section_number != 0) {
    mxverb(3, boost::format("mpeg_ts:parse_pmt: Unsupported multiple section PMT!\n"));
    return -1;
  }

  unsigned short pmt_section_length = SECTION_LENGTH(pmt_header);
  uint32_t elapsed_CRC              = calculate_crc32(pmt, 3 + pmt_section_length - 4/*CRC32*/);
  uint32_t read_CRC                 = CRC32(((mpeg_ts_crc_t *)(pmt + 3 + pmt_section_length - 4)));

  if (elapsed_CRC != read_CRC) {
    mxverb(3, boost::format("mpeg_ts:parse_pmt: Wrong PMT CRC !!! Elapsed = 0x%|1$08x|, read 0x%|2$08x|\n") % elapsed_CRC % read_CRC);
    return -1;
  }

  if (pmt_section_length < 13 || pmt_section_length > 1021) {
    mxverb(3, boost::format("mpeg_ts:parse_pmt: Wrong PMT section_length (=%1%)\n") % pmt_section_length);
    return -1;
  }

  mpeg_ts_pmt_descriptor_t *pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)(pmt + sizeof(mpeg_ts_pmt_t));
  unsigned short program_info_length       = PROGRAM_INFO_LENGTH(pmt_header);

  while (pmt_descriptor < (mpeg_ts_pmt_descriptor_t *)(pmt + sizeof(mpeg_ts_pmt_t) + program_info_length)) {
    //mpeg_ts_parse_descriptor(verbose, pmt_descriptor, (void *)stream_info, PROGRAM_DESCRIPTOR);

    pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_descriptor + sizeof(mpeg_ts_pmt_descriptor_t) + pmt_descriptor->length);
  }

  mpeg_ts_pmt_pid_info_t *pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)pmt_descriptor;

  /* Calculate pids_count */
  size_t pids_found = 0;
  while (pmt_pid_info < (mpeg_ts_pmt_pid_info_t *)(pmt + 3 + pmt_section_length - 4/*CRC32*/)) {
    pids_found++;
    pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t) + ES_INFO_LENGTH(pmt_pid_info));
  }

  mxverb(3, boost::format("mpeg_ts:parse_pmt: program number     (%1%)\n") % PROGRAM_NUMBER(pmt_header));
  mxverb(3, boost::format("mpeg_ts:parse_pmt: pcr pid            (%1%)\n") % PCR_PID(pmt_header));

  if (pids_found == 0) {
    mxverb(3, boost::format("mpeg_ts:parse_pmt: There's no information about elementary PIDs\n"));
    return 0;
  }

  pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)pmt_descriptor;

  /* Extract pid_info */
  while (pmt_pid_info < (mpeg_ts_pmt_pid_info_t *)(pmt + 3 + pmt_section_length - 4/*CRC32*/)) {
    mpeg_ts_track_ptr track(new mpeg_ts_track_t);
    unsigned short es_info_length = ES_INFO_LENGTH(pmt_pid_info);
    track->pid                    = GET_PID(pmt_pid_info);
    track->type                   = ES_UNKNOWN;

    switch(pmt_pid_info->stream_type) {
      case ISO_11172_VIDEO:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('M', 'P', 'G', '1');
        break;
      case ISO_13818_VIDEO:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('M', 'P', 'G', '2');
        break;
      case ISO_14496_PART2_VIDEO:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('M', 'P', 'G', '4');
        break;
      case ISO_14496_PART10_VIDEO:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('A', 'V', 'C', '1');
        break;
      case STREAM_VIDEO_VC1:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('W', 'V', 'C', '1');
        break;
      case ISO_11172_AUDIO:
      case ISO_13818_AUDIO:
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('M', 'P', '2', ' ');
        break;
      case ISO_13818_PART7_AUDIO:
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('A', 'A', 'C', ' ');
        break;
      case ISO_14496_PART3_AUDIO:
        // this is AAC+ ... is this ok ?
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('A', 'A', 'C', ' ');
        break;
      case STREAM_AUDIO_AC3:
      case STREAM_AUDIO_AC3_LOSSLESS:
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('A', 'C', '3', ' ');
        break;
      case STREAM_AUDIO_AC3_PLUS:
        // is this ok ?
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('T', 'R', 'H', 'D');
        break;
      case STREAM_AUDIO_DTS_HD:
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('D', 'T', 'S', ' ');
        break;
      case ISO_13818_PES_PRIVATE:
        break;
      default:
        mxverb(3, boost::format("mpeg_ts:parse_pmt: Unknown stream type: %1%\n") % (int)pmt_pid_info->stream_type);
        track->type   = ES_UNKNOWN;
        break;
    }

    pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t));

    while (pmt_descriptor < (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t) + es_info_length)) {
      if (pmt_pid_info->stream_type == ISO_13818_PES_PRIVATE) { // PES containig private data
        switch(pmt_descriptor->tag) {
          case 0x56: // Teletext descriptor
            track->type   = ES_UNKNOWN;
            mxverb(3, boost::format("mpeg_ts:parse_pmt: Teletext found but not handled !!\n"));
            break;
          case 0x59: // Subtitles descriptor
            track->type   = ES_SUBT_TYPE;
            track->fourcc = FOURCC('V', 'S', 'U', 'B');
            break;
          case 0x6A: // AC3 descriptor
            track->type   = ES_AUDIO_TYPE;
            track->fourcc = FOURCC('A', 'C', '3', ' ');
            break;
            /*
              default:
              mxverb(3, boost::format("mpeg_ts:parse_pmt: Unknown descriptor tag: %1%\n") % (int)pmt_descriptor->tag);
              track->type   = ES_UNKNOWN;
              break;
            */
        }
      }

      //mpeg_ts_parse_descriptor(verbose, pmt_descriptor, (void *)&(stream_info->pids_info[i]), PID_DESCRIPTOR);

      pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_descriptor + sizeof(mpeg_ts_pmt_descriptor_t) + pmt_descriptor->length);
    }

    pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t) + es_info_length);
    if (track->type != ES_UNKNOWN) {
      PMT_found         = true;
      track->pid        = track->pid;
      track->processed  = false;
      track->data_ready = false;
      tracks.push_back(track);
      es_to_process++;
      mxverb(3, boost::format("mpeg_ts:parse_pmt: PID %1% has type: 0x%|2$08x|%\n") % track->pid % get_uint32_be(&track->fourcc));
    }
  }

  return 0;
}

int
mpeg_ts_reader_c::new_stream_v_mpeg_1_2(unsigned char *buf,
                                        unsigned int length,
                                        mpeg_ts_track_ptr &track) {
  counted_ptr<M2VParser> m2v_parser(new M2VParser);

  m2v_parser->SetProbeMode();
  m2v_parser->WriteData(buf, length);

  int state = m2v_parser->GetState();
  if (state != MPV_PARSER_STATE_FRAME) {
    mxverb(3, boost::format("new_stream_v_mpeg_1_2: no valid frame in %1% bytes\n") % length);
    return FILE_STATUS_MOREDATA;
  }
  /*
    while ((MPV_PARSER_STATE_FRAME != state) && (PS_PROBE_SIZE >= io->getFilePointer())) {
    if (!find_next_packet_for_id(id, PS_PROBE_SIZE))
    break;

    int64_t timecode;
    unsigned int full_length;
    if (!parse_packet(id, timecode, length, full_length))
    break;

    memory_c new_buf((unsigned char *)safemalloc(length), 0, true);
    if (io->read(new_buf.get_buffer(), length) != length)
    break;

    m2v_parser->WriteData(new_buf.get_buffer(), length);

    state = m2v_parser->GetState();
    }

    if (MPV_PARSER_STATE_FRAME != state) {
    mxverb(3, boost::format("MPEG PS: blacklisting id 0x%|1$02x|(%|2$02x|) for supposed type MPEG1/2\n") % id.id % id.sub_id);
    blacklisted_ids[id.idx()] = true;
    return;
    }
  */
  MPEG2SequenceHeader seq_hdr = m2v_parser->GetSequenceHeader();
  counted_ptr<MPEGFrame> frame(m2v_parser->ReadFrame());
  if (!frame.is_set())
    return FILE_STATUS_MOREDATA;

  track->fourcc         = FOURCC('M', 'P', 'G', '0' + m2v_parser->GetMPEGVersion());
  track->v_interlaced   = !seq_hdr.progressiveSequence;
  track->v_version      = m2v_parser->GetMPEGVersion();
  track->v_width        = seq_hdr.width;
  track->v_height       = seq_hdr.height;
  track->v_frame_rate   = seq_hdr.progressiveSequence ? seq_hdr.frameOrFieldRate : seq_hdr.frameOrFieldRate * 2.0f;
  track->v_aspect_ratio = seq_hdr.aspectRatio;

  if ((0 >= track->v_aspect_ratio) || (1 == track->v_aspect_ratio))
    track->v_dwidth = track->v_width;
  else
    track->v_dwidth = (int)(track->v_height * track->v_aspect_ratio);
  track->v_dheight  = track->v_height;

  MPEGChunk *raw_seq_hdr = m2v_parser->GetRealSequenceHeader();
  if (NULL != raw_seq_hdr) {
    mxverb(3, boost::format("new_stream_v_mpeg_1_2: sequence header size: %1%\n") % raw_seq_hdr->GetSize());
    track->raw_seq_hdr      = (unsigned char *)safememdup(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
    track->raw_seq_hdr_size = raw_seq_hdr->GetSize();
  }

  mxverb(3, boost::format("new_stream_v_mpeg_1_2: width: %1%, height: %2%\n") % track->v_width % track->v_height);
  if (track->v_width == 0 || track->v_height == 0)
    return FILE_STATUS_MOREDATA;
  //track->use_buffer(128000);
  return 0;
}

int
mpeg_ts_reader_c::new_stream_v_avc(unsigned char *buf,
                                   unsigned int length,
                                   mpeg_ts_track_ptr &track) {
  mpeg4::p10::avc_es_parser_c parser;

  parser.ignore_nalu_size_length_errors();

  mxverb(3, boost::format("new_stream_v_avc: packet size: %1%\n") % length);
  //mxverb(3, boost::format("new_stream_v_avc: %1% %2% %3% %4%\n") % buf[0] %  buf[1] %  buf[2] %  buf[3]);
  //mxhexdump(3, buf, 10);
  /*
  if (map_has_key(m_ti.m_nalu_size_lengths, tracks.size()))
    parser.set_nalu_size_length(m_ti.m_nalu_size_lengths[0]);
  else if (map_has_key(m_ti.m_nalu_size_lengths, -1))
    parser.set_nalu_size_length(m_ti.m_nalu_size_lengths[-1]);
*/
  parser.add_bytes(buf, length);
/*
  while (!parser.headers_parsed() && (PS_PROBE_SIZE >= io->getFilePointer())) {
    if (!find_next_packet_for_id(id, PS_PROBE_SIZE))
      break;

    unsigned int full_length;
    int64_t timecode;
    if (!parse_packet(id, timecode, length, full_length))
      break;
    memory_c new_buf((unsigned char *)safemalloc(length), 0, true);
    if (io->read(new_buf.get_buffer(), length) != length)
      break;

    parser.add_bytes(new_buf.get_buffer(), length);
  }
*/
  if (!parser.headers_parsed())
    return FILE_STATUS_MOREDATA;

  track->v_avcc = parser.get_avcc();

  try {
    mm_mem_io_c avcc(track->v_avcc->get_buffer(), track->v_avcc->get_size());
    mm_mem_io_c new_avcc(NULL, track->v_avcc->get_size(), 1024);
    memory_cptr nalu(new memory_c());
    int num_sps, sps, sps_length;
    sps_info_t sps_info;

    avcc.read(nalu, 5);
    new_avcc.write(nalu);

    num_sps = avcc.read_uint8();
    new_avcc.write_uint8(num_sps);
    num_sps &= 0x1f;

    for (sps = 0; sps < num_sps; sps++) {
      bool abort;

      sps_length = avcc.read_uint16_be();
      if ((sps_length + avcc.getFilePointer()) >= track->v_avcc->get_size())
        sps_length = track->v_avcc->get_size() - avcc.getFilePointer();
      avcc.read(nalu, sps_length);

      abort = false;
      if ((0 < sps_length) && ((nalu->get_buffer()[0] & 0x1f) == 7)) {
        nalu_to_rbsp(nalu);
        if (!mpeg4::p10::parse_sps(nalu, sps_info, true))
          throw false;
        rbsp_to_nalu(nalu);
        abort = true;
      }

      new_avcc.write_uint16_be(nalu->get_size());
      new_avcc.write(nalu);

      if (abort) {
        avcc.read(nalu, track->v_avcc->get_size() - avcc.getFilePointer());
        new_avcc.write(nalu);
        break;
      }
    }

    track->fourcc   = FOURCC('A', 'V', 'C', '1');
    track->v_avcc   = memory_cptr(new memory_c(new_avcc.get_and_lock_buffer(), new_avcc.getFilePointer(), true));
    track->v_width  = sps_info.width;
    track->v_height = sps_info.height;

    mxverb(3, boost::format("new_stream_v_avc: timing_info_present %1%, num_units_in_tick %2%, time_scale %3%, fixed_frame_rate %4%\n") % sps_info.timing_info_present % sps_info.num_units_in_tick % sps_info.time_scale % sps_info.fixed_frame_rate);

    track->v_frame_rate = sps_info.time_scale / sps_info.num_units_in_tick;

    if (sps_info.ar_found) {
      float aspect_ratio = (float)sps_info.width / (float)sps_info.height * (float)sps_info.par_num / (float)sps_info.par_den;
      track->v_aspect_ratio = aspect_ratio;

      if (aspect_ratio > ((float)track->v_width / (float)track->v_height)) {
        track->v_dwidth  = irnd(track->v_height * aspect_ratio);
        track->v_dheight = track->v_height;

      } else {
        track->v_dwidth  = track->v_width;
        track->v_dheight = irnd(track->v_width / aspect_ratio);
      }
    }
    mxverb(3, boost::format("new_stream_v_avc: width: %1%, height: %2%\n") % track->v_width % track->v_height);
    //track->use_buffer(256000);

  } catch (...) {
    throw false;
  }

  return 0;
}

int
mpeg_ts_reader_c::new_stream_a_mpeg(unsigned char *buf,
                                    unsigned int length,
                                    mpeg_ts_track_ptr &track) {
  mp3_header_t header;

  if (-1 == find_mp3_header(buf, length))
    return FILE_STATUS_MOREDATA;

  decode_mp3_header(buf, &header);
  track->a_channels    = header.channels;
  track->a_sample_rate = header.sampling_frequency;
  track->fourcc        = FOURCC('M', 'P', '0' + header.layer, ' ');

  mxverb(3, boost::format("new_stream_a_mpeg: Channels: %1%, sample rate: %2%\n") %track->a_channels % track->a_sample_rate);
  return 0;
}

int
mpeg_ts_reader_c::new_stream_a_ac3(unsigned char *buf,
                                   unsigned int length,
                                   mpeg_ts_track_ptr &track) {
  ac3_header_t header;

  if (-1 == find_ac3_header(buf, length, &header, false))
    return FILE_STATUS_MOREDATA;

  mxverb(2,
         boost::format("first ac3 header bsid %1% channels %2% sample_rate %3% bytes %4% samples %5%\n")
         % header.bsid % header.channels % header.sample_rate % header.bytes % header.samples);

  track->a_channels    = header.channels;
  track->a_sample_rate = header.sample_rate;
  track->a_bsid        = header.bsid;

  return 0;
}

int
mpeg_ts_reader_c::new_stream_a_truehd(unsigned char *buf,
                                      unsigned int length,
                                      mpeg_ts_track_ptr &track) {
  truehd_parser_c parser;

  parser.add_data(buf, length);

  while (1) {
    while (parser.frame_available()) {
      truehd_frame_cptr frame = parser.get_next_frame();
      if (truehd_frame_t::sync != frame->m_type)
        continue;

      mxverb(2,
             boost::format("first TrueHD header channels %1% sampling_rate %2% samples_per_frame %3%\n")
             % frame->m_channels % frame->m_sampling_rate % frame->m_samples_per_frame);

      track->a_channels    = frame->m_channels;
      track->a_sample_rate = frame->m_sampling_rate;

      return 0;
    }

     return FILE_STATUS_MOREDATA;
  }

  return FILE_STATUS_MOREDATA;
}

bool
mpeg_ts_reader_c::parse_packet(int id, unsigned char *buf) {
  uint16_t i, ret = -1;
  unsigned char tidx = 0;
  unsigned char *payload;
  unsigned char payload_size;
  unsigned char adf_discontinuity_indicator = 0;
  mpeg_ts_pat_t *table_data;
  mpeg_ts_pes_header_t *pes_data;
  mpeg_ts_packet_header_t *hdr = (mpeg_ts_packet_header_t *)buf;

  int16_t table_pid = (int16_t)GET_PID(hdr);
  //mxverb(3, boost::format("mpeg_ts: PID found: 0x" << std::hex << std::uppercase << table_pid << std::endl;

  if (hdr->transport_error_indicator == 1) //corrupted packet
    return false;

  if (hdr->adaptation_field_control == 0x00 || hdr->adaptation_field_control == 0x02) //no payload
    return false;

  int16_t match_pid = -1;
  if (id != -1 && tracks[id]->processed == false) {
    match_pid = tracks[id]->pid;
    tidx      = id;
  } else {
    for (i = 0; i < tracks.size(); i++) {
      if (tracks[i]->pid == table_pid && tracks[i]->processed == false) {
        match_pid = tracks[i]->pid;
        tidx      = i;
        break;
      }
    }
  }

  if (table_pid == match_pid) {
    //mxverb(3, boost::format("mpeg_ts: match for PID found: " << match_pid << std::endl;
    if (hdr->adaptation_field_control == 0x01) {
      payload = (unsigned char *)hdr + sizeof(mpeg_ts_packet_header_t);

    } else { //hdr->adaptation_field_control == 0x03
      mpeg_ts_adaptation_field_t *adf = (mpeg_ts_adaptation_field_t *)((unsigned char *)hdr + sizeof(mpeg_ts_packet_header_t));

      if (adf->adaptation_field_length > 182) //no payload ?
        return false;

      adf_discontinuity_indicator = adf->discontinuity_indicator;
      payload                     = (unsigned char *)hdr + sizeof(mpeg_ts_packet_header_t) + adf->adaptation_field_length + 1;
    }

    payload_size = 188 - (payload - (unsigned char *)hdr);

    if (hdr->payload_unit_start_indicator) {
      if (tracks[tidx]->type == PAT_TYPE || tracks[tidx]->type == PMT_TYPE) {
        table_data                  = (mpeg_ts_pat_t *)(payload + 1 + *payload);
        payload_size               -=  1 + *payload;
        tracks[tidx]->payload_size  = SECTION_LENGTH(table_data) + 3;
        payload                     = (unsigned char *)table_data;

      } else {
        pes_data                   = (mpeg_ts_pes_header_t *)payload;
        tracks[tidx]->payload_size = ((uint16_t) (pes_data->PES_packet_length_msb) << 8) | (pes_data->PES_packet_length_lsb);
        //if (tracks[tidx]->payload_size > 0)
        //  tracks[tidx]->payload_size = tracks[tidx]->payload_size - 3 - pes_data->PES_header_data_length;
        tracks[tidx]->payload_size = 0;

        mxverb(4, boost::format("   PES info: stream_id = %1%\n") % (int)pes_data->stream_id);
        mxverb(4, boost::format("   PES info: PES_packet_length = %1%\n") % tracks[tidx]->payload_size);
        //mxverb(3, boost::format("scrambling = %d, priority = %d, alignment = %d, copyright = %d, copy = %d\n", pes_hdr->PES_scrambling_control, pes_hdr->PES_priority, pes_hdr->data_alignment_indicator, pes_hdr->copyright, pes_hdr->original_or_copy);
        mxverb(4, boost::format("   PES info: PTS_DTS = %1% ESCR = %2% ES_rate = %3%\n") % (int)pes_data->PTS_DTS % (int)pes_data->ESCR % (int)pes_data->ES_rate);
        mxverb(4, boost::format("   PES info: DSM_trick_mode = %1%, add_copy = %2%, CRC = %3%, ext = %4%\n") % (int)pes_data->DSM_trick_mode % (int)pes_data->additional_copy_info % (int)pes_data->PES_CRC % (int)pes_data->PES_extension);
        mxverb(4, boost::format("   PES info: PES_header_data_length = %1%\n") % (int)pes_data->PES_header_data_length);

        if (pes_data->PTS_DTS > 1) { // 10 and 11 mean PTS is present
          int64_t PTS =        ((uint64_t)(pes_data->PTS_4msb & 0x07))   << 29;
          PTS         = PTS | (((uint64_t)pes_data->PTS_3msb)            << 21);
          PTS         = PTS | (((uint64_t)(pes_data->PTS_2msb & 0x00FE)) << 14);
          PTS         = PTS | (((uint64_t)pes_data->PTS_1msb)            << 6);
          PTS         = PTS | (((uint64_t)(pes_data->PTS_lsb & 0x00FE))  >> 1);

          if (global_timecode_offset == -1)
            global_timecode_offset = (int64_t)PTS;

          if (PTS == tracks[tidx]->timecode) {
            mxverb(3, boost::format("     Adding PES with same PTS as previous !!\n"));
            tracks[tidx]->payload.add(payload, payload_size);
            return true;
          }

          tracks[tidx]->timecode = PTS;

          mxverb(3, boost::format("     PTS found: %1%\n") % tracks[tidx]->timecode);
        }

        payload      = &pes_data->PES_header_data_length + pes_data->PES_header_data_length + 1;
        payload_size = ((unsigned char *) hdr + 188) - (unsigned char *) payload;
        if (tracks[tidx]->payload_size == 0 && tracks[tidx]->payload.get_size() != 0) {

          if (input_status == INPUT_READ) {
            tracks[tidx]->payload_size = tracks[tidx]->payload.get_size();
            mxverb(3, boost::format("mpeg_ts: Table/PES completed (%1%) for PID %2%\n") % tracks[tidx]->payload_size % table_pid);
            track_buffer_ready = tidx;
            io->skip(-188); // skip one TS packet back to retrieve next PES correctly
            return true;
          }
          tracks[tidx]->data_ready = true;
        }
      }
      tracks[tidx]->continuity_counter = CONTINUITY_COUNTER(hdr);

      if (tracks[tidx]->payload_size != 0 && (payload_size + tracks[tidx]->payload.get_size()) > tracks[tidx]->payload_size)
        payload_size = tracks[tidx]->payload_size - tracks[tidx]->payload.get_size();
      //mxverb(3, boost::format("mpeg_ts: Payload size: %1% - PID: %2%\n") % (int)payload_size % table_pid);

    } else {
      /* Check continuity counter */
      if (tracks[tidx]->payload.get_size() != 0) {
        if (!adf_discontinuity_indicator) {
          tracks[tidx]->continuity_counter++;
          tracks[tidx]->continuity_counter %= 16;
          if (CONTINUITY_COUNTER(hdr) != tracks[tidx]->continuity_counter) {
            //delete tracks[tidx].table;
            mxverb(3, boost::format("mpeg_ts: Continuity error on PID: %1%. Continue anyway...\n") % table_pid);
            tracks[tidx]->continuity_counter = CONTINUITY_COUNTER(hdr);
            //tracks[tidx]->payload.remove(tracks[tidx]->payload.get_size());
            //tracks[tidx]->processed = false;
            //tracks[tidx]->data_ready = false;
            //tracks[tidx]->payload_size = 0;
            //return false;
          }
        } else
          tracks[tidx]->continuity_counter = CONTINUITY_COUNTER(hdr);

      } else
        return false;

      if (tracks[tidx]->payload_size != 0 && payload_size > tracks[tidx]->payload_size - tracks[tidx]->payload.get_size())
        payload_size = tracks[tidx]->payload_size - tracks[tidx]->payload.get_size();
    }

    tracks[tidx]->payload.add(payload, payload_size);
    //mxverb(3, boost::format("mpeg_ts: ---------> Written " << (int)payload_size << " bytes for pid " << table_pid << std::endl;

    if (tracks[tidx]->payload.get_size() == 0)
      return false;

    if (tracks[tidx]->payload.get_size() == tracks[tidx]->payload_size)
      tracks[tidx]->data_ready = true;

    /*if (tracks[tidx]->payload.get_size() == tracks[tidx]->payload_size ||
      (input_status == INPUT_PROBE && tracks[tidx]->payload_size == 0 && tracks[tidx]->type == ES_VIDEO_TYPE) ) {*/
    if (tracks[tidx]->data_ready == true) {
      mxverb(3, boost::format("mpeg_ts: Table/PES completed (%1%) for PID %2%\n") % tracks[tidx]->payload.get_size() % table_pid);
      if (input_status == INPUT_PROBE) {

        if (tracks[tidx]->type == PAT_TYPE)
          ret = parse_pat(tracks[tidx]->payload.get_buffer());
        else if (tracks[tidx]->type == PMT_TYPE)
          ret = parse_pmt(tracks[tidx]->payload.get_buffer());
        else if (tracks[tidx]->type == ES_VIDEO_TYPE) {
          if ((FOURCC('M', 'P', 'G', '1') == tracks[tidx]->fourcc) || (FOURCC('M', 'P', 'G', '2') == tracks[tidx]->fourcc))
            ret = new_stream_v_mpeg_1_2(tracks[tidx]->payload.get_buffer(), tracks[tidx]->payload.get_size(), tracks[tidx]);
          else if (FOURCC('A', 'V', 'C', '1') == tracks[tidx]->fourcc)
            ret = new_stream_v_avc(tracks[tidx]->payload.get_buffer(), tracks[tidx]->payload.get_size(), tracks[tidx]);
        }
        else if (tracks[tidx]->type == ES_AUDIO_TYPE) {
          if (FOURCC('M', 'P', '2', ' ') == tracks[tidx]->fourcc)
            ret = new_stream_a_mpeg(tracks[tidx]->payload.get_buffer(), tracks[tidx]->payload.get_size(), tracks[tidx]);
          else if (FOURCC('A', 'C', '3', ' ') == tracks[tidx]->fourcc)
            ret = new_stream_a_ac3(tracks[tidx]->payload.get_buffer(), tracks[tidx]->payload.get_size(), tracks[tidx]);
          else if (FOURCC('T', 'R', 'H', 'D') == tracks[tidx]->fourcc)
            ret = new_stream_a_truehd(tracks[tidx]->payload.get_buffer(), tracks[tidx]->payload.get_size(), tracks[tidx]);
        }
        else if (tracks[tidx]->type == ES_SUBT_TYPE) {
          ret = 0;
        }

        if (ret == 0) {
          if (tracks[tidx]->type == PAT_TYPE || tracks[tidx]->type == PMT_TYPE)
            tracks.erase(tracks.begin() + tidx);
          else {
            tracks[tidx]->processed = true;
            es_to_process--;
            mxverb(3, boost::format("mpeg_ts: ES to process: %1%\n") % es_to_process);
          }
          mxverb(3, boost::format("mpeg_ts: Table PROCESSED for PID %1%\n") % table_pid);
        } else if (ret == FILE_STATUS_MOREDATA) {
          mxverb(3, boost::format("mpeg_ts: Need more data to probe ES\n"));
          tracks[tidx]->processed  = false;
          tracks[tidx]->data_ready = false;
        } else {
          mxverb(3, boost::format("mpeg_ts: Failed to parse packet. Reset and retry\n"));
          tracks[tidx]->payload.remove(tracks[tidx]->payload.get_size());
          tracks[tidx]->processed    = false;
          tracks[tidx]->payload_size = 0;
          return false;
        }
      } else if (input_status == INPUT_READ) {

        // PES completed, set track to quicly send it to the rightpacketizer
        track_buffer_ready = tidx;
      }

    }

    return true;
  }

  return false;
}

void
mpeg_ts_reader_c::create_packetizer(int64_t id) {
  char type = 'v';
  if (tracks[id]->type == ES_AUDIO_TYPE)
    type = 'a';
  else if (tracks[id]->type == ES_SUBT_TYPE)
    type = 's';

  if ((0 > id) || (tracks.size() <= static_cast<size_t>(id)))
    return;
  if (0 == tracks[id]->ptzr)
    return;
  if (!demuxing_requested(type, id))
    return;

  m_ti.m_id = id;
  mpeg_ts_track_ptr &track = tracks[id];
  if (ES_AUDIO_TYPE == track->type) {
    if (   (FOURCC('M', 'P', '1', ' ') == track->fourcc)
        || (FOURCC('M', 'P', '2', ' ') == track->fourcc)
        || (FOURCC('M', 'P', '3', ' ') == track->fourcc)) {
      if (verbose)
        mxinfo_tid(m_ti.m_fname, id, Y("Using the MPEG audio output module.\n"));
      track->ptzr = add_packetizer(new mp3_packetizer_c(this, m_ti, track->a_sample_rate, track->a_channels, true));
    } else if (FOURCC('A', 'C', '3', ' ') == track->fourcc) {
      if (verbose)
        mxinfo_tid(m_ti.m_fname, id, boost::format(Y("Using the %1%AC3 output module.\n")) % (16 == track->a_bsid ? "E" : ""));
      track->ptzr = add_packetizer(new ac3_packetizer_c(this, m_ti, track->a_sample_rate, track->a_channels, track->a_bsid));
    } else if (FOURCC('T', 'R', 'H', 'D') == track->fourcc) {
      if (verbose)
        mxinfo_tid(m_ti.m_fname, id, Y("Using the TrueHD output module.\n"));
      track->ptzr = add_packetizer(new truehd_packetizer_c(this, m_ti, truehd_frame_t::truehd, track->a_sample_rate, track->a_channels));
    }

  } else if (ES_VIDEO_TYPE == track->type) {
    if (   (FOURCC('M', 'P', 'G', '1') == track->fourcc)
        || (FOURCC('M', 'P', 'G', '2') == track->fourcc)) {
      generic_packetizer_c *m2vpacketizer;
      if (verbose)
        mxinfo_tid(m_ti.m_fname, id, Y("Using the MPEG-1/2 video output module.\n"));

      if (track->raw_seq_hdr != NULL) {
        m_ti.m_private_data = track->raw_seq_hdr;
        m_ti.m_private_size = track->raw_seq_hdr_size;
      }
      m2vpacketizer       = new mpeg1_2_video_packetizer_c(this, m_ti, track->v_version, track->v_frame_rate, track->v_width, track->v_height,
                                                           track->v_dwidth, track->v_dheight, false);
      track->ptzr         = add_packetizer(m2vpacketizer);
      m_ti.m_private_data = NULL;
      m_ti.m_private_size = 0;
      m2vpacketizer->set_video_interlaced_flag(track->v_interlaced);

    } else if (track->fourcc == FOURCC('A', 'V', 'C', '1')) {
      mpeg4_p10_es_video_packetizer_c *avcpacketizer;
      if (verbose)
        mxinfo_tid(m_ti.m_fname, id, Y("Using the MPEG-4 part 10 ES video output module.\n"));
      avcpacketizer = new mpeg4_p10_es_video_packetizer_c(this, m_ti, track->v_avcc, track->v_width, track->v_height);
      /*if (track->v_frame_rate != 25)
           avcpacketizer->enable_timecode_generation(true, 1000000000/track->v_frame_rate);*/
      track->ptzr = add_packetizer(avcpacketizer);

    }
  }
}

void
mpeg_ts_reader_c::create_packetizers() {
  size_t i;

  input_status = INPUT_READ;
  mxverb(3, boost::format("mpeg_ts: create packetizers...\n"));
  for (i = 0; i < tracks.size(); i++)
    create_packetizer(i);
}

int
mpeg_ts_reader_c::get_progress() {
  return 100 * io->getFilePointer() / size;
}

file_status_e
mpeg_ts_reader_c::finish() {
  if (file_done)
    return flush_packetizers();

  foreach(mpeg_ts_track_ptr &track, tracks)
    if (0 < track->payload.get_size())
      PTZR(track->ptzr)->process(new packet_t(clone_memory(track->payload.get_buffer(), track->payload.get_size())));

  file_done = true;

  return flush_packetizers();
}

file_status_e
mpeg_ts_reader_c::read(generic_packetizer_c *,
                       bool) {
  bool done = false;
  unsigned char buf[205];

  track_buffer_ready = -1;

  if (file_done)
    return flush_packetizers();

  buf[0] = io->read_uint8();

  while (!done) {
    if (io->eof())
      return finish();

    if (buf[0] == 0x47) {
      if (io->read(buf + 1, detected_packet_size) != (unsigned int)detected_packet_size || io->eof())
        return finish();

      if (buf[detected_packet_size] != 0x47) {
        io->skip(0 - detected_packet_size);
        buf[0] = io->read_uint8();
        continue;
      }
      done = true;
      parse_packet(-1, buf);
      if (track_buffer_ready != -1) { // ES buffer ready

        //if (track_buffer_ready == 0)
        //     m_file->write(tracks[track_buffer_ready]->payload.get_buffer(), tracks[track_buffer_ready]->payload_size);
        if (tracks[track_buffer_ready]->timecode - global_timecode_offset < 0)
          tracks[track_buffer_ready]->timecode = 0;
        else
          tracks[track_buffer_ready]->timecode = (uint64_t)(tracks[track_buffer_ready]->timecode - global_timecode_offset) * 100000ll / 9;

        // WARNING WARNING WARNING - comment this to use source audio PTSs !!!
        if (tracks[track_buffer_ready]->type == ES_AUDIO_TYPE)
          tracks[track_buffer_ready]->timecode = -1;

        mxverb(3, boost::format("mpeg_ts: PTS in nanoseconds: %1%\n") % tracks[track_buffer_ready]->timecode);
        packet_t *new_packet = new packet_t(clone_memory(tracks[track_buffer_ready]->payload.get_buffer(), tracks[track_buffer_ready]->payload_size), tracks[track_buffer_ready]->timecode);

        if (tracks[track_buffer_ready]->ptzr != -1)
          PTZR(tracks[track_buffer_ready]->ptzr)->process(new_packet);
        //mxverb(3, boost::format("mpeg_ts: packet processed... (%1% bytes)\n") % tracks[track_buffer_ready]->payload.get_size());
        tracks[track_buffer_ready]->payload.remove(tracks[track_buffer_ready]->payload.get_size());
        tracks[track_buffer_ready]->processed    = false;
        tracks[track_buffer_ready]->data_ready   = false;
        tracks[track_buffer_ready]->payload_size = 0;
        track_buffer_ready                       = -1;
      }
      io->skip(-1);
    }
    else
      buf[0] = io->read_uint8(); // advance byte per byte to find a new sync
  }

  return FILE_STATUS_MOREDATA;
}
