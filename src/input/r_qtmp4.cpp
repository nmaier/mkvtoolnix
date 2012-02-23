/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Quicktime and MP4 reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
   The second half of the parse_headers() function after the
     "// process chunkmap:" comment was taken from mplayer's
     demux_mov.c file which is distributed under the GPL as well. Thanks to
     the original authors.
*/

#include "common/common_pch.h"

#include <algorithm>
#include <boost/math/common_factor.hpp>
#include <boost/range/algorithm.hpp>
#include <cstring>
#include <zlib.h>

#include <avilib.h>

#include "common/aac.h"
#include "common/chapters/chapters.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/iso639.h"
#include "common/matroska.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "input/r_qtmp4.h"
#include "merge/output_control.h"
#include "output/p_aac.h"
#include "output/p_ac3.h"
#include "output/p_mp3.h"
#include "output/p_mpeg1_2.h"
#include "output/p_mpeg4_p2.h"
#include "output/p_mpeg4_p10.h"
#include "output/p_passthrough.h"
#include "output/p_pcm.h"
#include "output/p_video.h"

using namespace libmatroska;

#define MAX_INTERLEAVING_BADNESS 0.4

#if defined(ARCH_BIGENDIAN)
#define BE2STR(a) ((char *)&a)[0] % ((char *)&a)[1] % ((char *)&a)[2] % ((char *)&a)[3]
#define LE2STR(a) ((char *)&a)[3] % ((char *)&a)[2] % ((char *)&a)[1] % ((char *)&a)[0]
#else
#define BE2STR(a) ((char *)&a)[3] % ((char *)&a)[2] % ((char *)&a)[1] % ((char *)&a)[0]
#define LE2STR(a) ((char *)&a)[0] % ((char *)&a)[1] % ((char *)&a)[2] % ((char *)&a)[3]
#endif

#define IS_AAC_OBJECT_TYPE_ID(object_type_id)                    \
  (   (MP4OTI_MPEG4Audio                      == object_type_id) \
   || (MP4OTI_MPEG2AudioMain                  == object_type_id) \
   || (MP4OTI_MPEG2AudioLowComplexity         == object_type_id) \
   || (MP4OTI_MPEG2AudioScaleableSamplingRate == object_type_id))

static std::string
space(int num) {
  return std::string(num, ' ');
}

int
qtmp4_reader_c::probe_file(mm_io_c *in,
                           uint64_t) {
  try {
    in->setFilePointer(0, seek_beginning);

    while (1) {
      uint64_t atom_pos  = in->getFilePointer();
      uint64_t atom_size = in->read_uint32_be();
      uint32_t atom      = in->read_uint32_be();

      if (1 == atom_size)
        atom_size = in->read_uint64_be();

      mxverb(3, boost::format("Quicktime/MP4 reader: Atom: '%1%%2%%3%%4%'; size: %5%\n") % BE2STR(atom) % atom_size);

      if (   (FOURCC('m', 'o', 'o', 'v') == atom)
          || (FOURCC('f', 't', 'y', 'p') == atom)
          || (FOURCC('m', 'd', 'a', 't') == atom)
          || (FOURCC('p', 'n', 'o', 't') == atom))
        return 1;

      if (   (FOURCC('w', 'i', 'd', 'e') != atom)
          && (FOURCC('s', 'k', 'i', 'p') != atom))
        return 0;

      in->setFilePointer(atom_pos + atom_size);
    }

  } catch (...) {
  }

  return 0;
}

qtmp4_reader_c::qtmp4_reader_c(const track_info_c &ti,
                               const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_mdat_pos(-1)
  , m_mdat_size(0)
  , m_time_scale(1)
  , m_compression_algorithm(0)
  , m_main_dmx(-1)
  , m_audio_encoder_delay_samples(0)
  , m_debug_chapters(    debugging_requested("qtmp4") || debugging_requested("qtmp4_full") || debugging_requested("qtmp4_chapters"))
  , m_debug_headers(     debugging_requested("qtmp4") || debugging_requested("qtmp4_full") || debugging_requested("qtmp4_headers"))
  , m_debug_tables(                                      debugging_requested("qtmp4_full") || debugging_requested("qtmp4_tables"))
  , m_debug_interleaving(debugging_requested("qtmp4") || debugging_requested("qtmp4_full") || debugging_requested("qtmp4_interleaving"))
{
}

void
qtmp4_reader_c::read_headers() {
  try {
    if (!qtmp4_reader_c::probe_file(m_in.get_object(), m_size))
      throw mtx::input::invalid_format_x();

    show_demuxer_info();

    parse_headers();

  } catch (mtx::mm_io::exception &) {
    throw mtx::input::open_x();
  }
}

qtmp4_reader_c::~qtmp4_reader_c() {
  m_ti.m_private_data = NULL;
}

qt_atom_t
qtmp4_reader_c::read_atom(mm_io_c *read_from,
                          bool exit_on_error) {
  qt_atom_t a;

  if (NULL == read_from)
    read_from = m_in.get_object();

  a.pos    = read_from->getFilePointer();
  a.size   = read_from->read_uint32_be();
  a.fourcc = read_from->read_uint32_be();
  a.hsize  = 8;

  if (1 == a.size) {
    a.size   = read_from->read_uint64_be();
    a.hsize += 8;

  } else if (0 == a.size)
    a.size   = m_size - read_from->getFilePointer() + 8;

  if (a.size < a.hsize) {
    if (exit_on_error)
      mxerror(boost::format(Y("Quicktime/MP4 reader: Invalid chunk size %1% at %2%.\n")) % a.size % a.pos);
    else
      throw false;
  }

  return a;
}

#define skip_atom() m_in->setFilePointer(atom.pos + atom.size)

void
qtmp4_reader_c::parse_headers() {
  unsigned int idx;
  uint32_t tmp;

  m_in->setFilePointer(0);

  bool headers_parsed = false;
  do {
    qt_atom_t atom = read_atom();
    mxdebug_if(m_debug_headers, boost::format("'%1%%2%%3%%4%' atom, size %5%, at %6%\n") % BE2STR(atom) % atom.size % atom.pos);

    if (FOURCC('f', 't', 'y', 'p') == atom.fourcc) {
      tmp = m_in->read_uint32_be();
      mxdebug_if(m_debug_headers, boost::format("  File type major brand: %1%%2%%3%%4%\n") % BE2STR(tmp));
      tmp = m_in->read_uint32_be();
      mxdebug_if(m_debug_headers, boost::format("  File type minor brand: 0x%|1$08x|\n") % tmp);

      for (idx = 0; idx < ((atom.size - 16) / 4); ++idx) {
        tmp = m_in->read_uint32_be();
        mxdebug_if(m_debug_headers, boost::format("  File type compatible brands #%1%: %2%%3%%4%%5%\n") % idx % BE2STR(tmp));
      }

    } else if (FOURCC('m', 'o', 'o', 'v') == atom.fourcc) {
      handle_moov_atom(atom.to_parent(), 0);
      headers_parsed = true;

    } else if (FOURCC('w', 'i', 'd', 'e') == atom.fourcc) {
      skip_atom();

    } else if (FOURCC('m', 'd', 'a', 't') == atom.fourcc) {
      m_mdat_pos  = m_in->getFilePointer();
      m_mdat_size = atom.size;
      skip_atom();

    } else
      skip_atom();

  } while (!m_in->eof() && (!headers_parsed || (-1 == m_mdat_pos)));

  if (!headers_parsed)
    mxerror(Y("Quicktime/MP4 reader: Have not found any header atoms.\n"));
  if (-1 == m_mdat_pos)
    mxerror(Y("Quicktime/MP4 reader: Have not found the 'mdat' atom. No movie data found.\n"));

  m_in->setFilePointer(m_mdat_pos);

  for (auto &dmx : m_demuxers) {
    if ((   ('v' == dmx->type)
         && strncasecmp(dmx->fourcc, "SVQ", 3)
         && strncasecmp(dmx->fourcc, "cvid", 4)
         && strncasecmp(dmx->fourcc, "rle ", 4)
         && strncasecmp(dmx->fourcc, "mp4v", 4)
         && strncasecmp(dmx->fourcc, "xvid", 4)
         && strncasecmp(dmx->fourcc, "avc1", 4))
        ||
        (   ('a' == dmx->type)
         && strncasecmp(dmx->fourcc, "QDM", 3)
         && strncasecmp(dmx->fourcc, "MP4A", 4)
         && strncasecmp(dmx->fourcc, ".mp3", 4)
         && strncasecmp(dmx->fourcc, "twos", 4)
         && strncasecmp(dmx->fourcc, "sowt", 4)
         && strncasecmp(dmx->fourcc, "ac-3", 4)
         && strncasecmp(dmx->fourcc, "sac3", 4))) {
      mxwarn(boost::format(Y("Quicktime/MP4 reader: Unknown/unsupported FourCC '%|1$.4s|' for track %2%.\n")) % dmx->fourcc % dmx->id);
      continue;
    }

    if (('a' == dmx->type) && !strncasecmp(dmx->fourcc, "MP4A", 4)) {
      if (   !IS_AAC_OBJECT_TYPE_ID(dmx->esds.object_type_id)
          && (MP4OTI_MPEG2AudioPart3 != dmx->esds.object_type_id) // MP3...
          && (MP4OTI_MPEG1Audio      != dmx->esds.object_type_id)) {
        mxwarn(boost::format(Y("Quicktime/MP4 reader: The audio track %1% is using an unsupported 'object type id' of %2% in the 'esds' atom. Skipping this track.\n"))
               % dmx->id % (int)dmx->esds.object_type_id);
        continue;
      }

      if (IS_AAC_OBJECT_TYPE_ID(dmx->esds.object_type_id) && ((NULL == dmx->esds.decoder_config) || !dmx->a_aac_config_parsed)) {
        mxwarn(boost::format(Y("Quicktime/MP4 reader: The AAC track %1% is missing the esds atom/the decoder config. Skipping this track.\n")) % dmx->id);
        continue;
      }
    }

    if ('v' == dmx->type) {
      if ((0 == dmx->v_width) || (0 == dmx->v_height) || (0 == get_uint32_le(dmx->fourcc))) {
        mxwarn(boost::format(Y("Quicktime/MP4 reader: Track %1% is missing some data. Broken header atoms?\n")) % dmx->id);
        continue;
      }
      if (!strncasecmp(dmx->fourcc, "mp4v", 4)) {
        if (!dmx->esds_parsed) {
          mxwarn(boost::format(Y("Quicktime/MP4 reader: The video track %1% is missing the ESDS atom. Skipping this track.\n")) % dmx->id);
          continue;
        }

        // The MP4 container can also contain MPEG1 and MPEG2 encoded
        // video. The object type ID in the ESDS tells the demuxer what
        // it is. So let's check for those.
        // If the FourCC is unmodified then MPEG4 is assumed.
        if (   (MP4OTI_MPEG2VisualSimple  == dmx->esds.object_type_id)
            || (MP4OTI_MPEG2VisualMain    == dmx->esds.object_type_id)
            || (MP4OTI_MPEG2VisualSNR     == dmx->esds.object_type_id)
            || (MP4OTI_MPEG2VisualSpatial == dmx->esds.object_type_id)
            || (MP4OTI_MPEG2VisualHigh    == dmx->esds.object_type_id)
            || (MP4OTI_MPEG2Visual422     == dmx->esds.object_type_id))
          memcpy(dmx->fourcc, "mpg2", 4);
        else if (MP4OTI_MPEG1Visual       == dmx->esds.object_type_id)
          memcpy(dmx->fourcc, "mpg1", 4);
        else {
          // This is MPEG4 video, and we need header data for it.
          if (NULL == dmx->esds.decoder_config) {
            mxwarn(boost::format(Y("Quicktime/MP4 reader: MPEG4 track %1% is missing the esds atom/the decoder config. Skipping this track.\n")) % dmx->id);
            continue;
          }
        }

      } else if (dmx->v_is_avc) {
        if ((NULL == dmx->priv) || (4 > dmx->priv_size)) {
          mxwarn(boost::format(Y("Quicktime/MP4 reader: MPEG4 part 10/AVC track %1% is missing its decoder config. Skipping this track.\n")) % dmx->id);
          continue;
        }
      }
    }

    if (('a' == dmx->type) && ((0 == dmx->a_channels) || (0.0 == dmx->a_samplerate))) {
      mxwarn(boost::format(Y("Quicktime/MP4 reader: Track %1% is missing some data. Broken header atoms?\n")) % dmx->id);
      continue;
    }

    if ('?' == dmx->type) {
      mxwarn(boost::format(Y("Quicktime/MP4 reader: Track %1% has an unknown type.\n")) % dmx->id);
      continue;
    }

    dmx->update_tables(m_time_scale);

    dmx->ok = true;
  }

  detect_interleaving();

  read_chapter_track();

  if (!g_identifying)
    calculate_timecodes();

  mxdebug_if(m_debug_headers, boost::format("Number of valid tracks found: %1%\n") % m_demuxers.size());
}

void
qtmp4_reader_c::calculate_timecodes() {
  int64_t min_timecode = 0;

  for (auto &dmx : m_demuxers) {
    dmx->calculate_fps();
    dmx->calculate_timecodes();
    if (dmx->min_timecode < min_timecode)
      min_timecode = dmx->min_timecode;
  }

  if (0 > min_timecode) {
    min_timecode *= -1;
    for (auto &dmx : m_demuxers)
      dmx->adjust_timecodes(min_timecode);

    mxwarn_fn(m_ti.m_fname,
              boost::format(Y("This file contains at least one frame with a negative timecode. "
                              "All timecodes will be adjusted by %1% so that none is negative anymore.\n"))
              % format_timecode(min_timecode, 3));

  } else
    min_timecode = 0;

  for (auto &dmx : m_demuxers)
    dmx->build_index();
}

void
qtmp4_reader_c::handle_audio_encoder_delay(qtmp4_demuxer_cptr &dmx) {
  if ((0 == m_audio_encoder_delay_samples) || (0 == dmx->a_samplerate) || (-1 == dmx->ptzr))
    return;

  PTZR(dmx->ptzr)->m_ti.m_tcsync.displacement -= (m_audio_encoder_delay_samples * 1000000000ll) / dmx->a_samplerate;
  m_audio_encoder_delay_samples                = 0;
}

void
qtmp4_reader_c::parse_video_header_priv_atoms(qtmp4_demuxer_cptr &dmx,
                                              unsigned char *mem,
                                              size_t size,
                                              int level) {
  if (!dmx->v_is_avc && strncasecmp(dmx->fourcc, "mp4v", 4) && strncasecmp(dmx->fourcc, "xvid", 4) && (0 != size)) {
    dmx->priv_size = size;
    dmx->priv      = (unsigned char *)safememdup(mem, size);

    return;
  }

  mm_mem_io_c mio(mem, size);

  try {
    while (!mio.eof() && (mio.getFilePointer() < size)) {
      qt_atom_t atom;

      try {
        atom = read_atom(&mio);
      } catch (...) {
        return;
      }

      mxdebug_if(m_debug_headers, boost::format("%1%Video private data size: %2%, type: '%3%%4%%5%%6%'\n") % space((level + 1) * 2 + 1) % atom.size % BE2STR(atom.fourcc));

      if ((FOURCC('e', 's', 'd', 's') == atom.fourcc) ||
          (FOURCC('a', 'v', 'c', 'C') == atom.fourcc)) {
        if (NULL == dmx->priv) {
          dmx->priv_size = atom.size - atom.hsize;
          dmx->priv      = (unsigned char *)safemalloc(dmx->priv_size);

          if (mio.read(dmx->priv, dmx->priv_size) != dmx->priv_size) {
            safefree(dmx->priv);
            dmx->priv      = NULL;
            dmx->priv_size = 0;
            return;
          }
        }

        if ((FOURCC('e', 's', 'd', 's') == atom.fourcc) && !dmx->esds_parsed) {
          mm_mem_io_c memio(dmx->priv, dmx->priv_size);
          dmx->esds_parsed = parse_esds_atom(memio, dmx, level + 1);
        }
      }

      mio.setFilePointer(atom.pos + atom.size);
    }
  } catch(...) {
  }
}

void
qtmp4_reader_c::parse_audio_header_priv_atoms(qtmp4_demuxer_cptr &dmx,
                                              unsigned char *mem,
                                              size_t size,
                                              int level) {
  mm_mem_io_c mio(mem, size);

  try {
    while (!mio.eof() && (mio.getFilePointer() < (size - 8))) {
      qt_atom_t atom;

      try {
        atom = read_atom(&mio, false);
      } catch (...) {
        return;
      }

      if (FOURCC('e', 's', 'd', 's') != atom.fourcc) {
        mio.setFilePointer(atom.pos + 4);
        continue;
      }

      mxdebug_if(m_debug_headers, boost::format("%1%Audio private data size: %2%, type: '%3%%4%%5%%6%'\n") % space((level + 1) * 2 + 1) % atom.size % BE2STR(atom.fourcc));

      if (!dmx->esds_parsed) {
        mm_mem_io_c memio(mem + atom.pos + atom.hsize, atom.size - atom.hsize);
        dmx->esds_parsed = parse_esds_atom(memio, dmx, level + 1);

        if (dmx->esds_parsed && !strncasecmp(dmx->fourcc, "MP4A", 4) && IS_AAC_OBJECT_TYPE_ID(dmx->esds.object_type_id)) {
          int profile, sample_rate, channels, output_sample_rate;
          bool aac_is_sbr;

          if (dmx->esds.decoder_config_len < 2)
            mxwarn(boost::format(Y("Track %1%: AAC found, but decoder config data has length %2%.\n")) % dmx->id % (int)dmx->esds.decoder_config_len);

          else if (!parse_aac_data(dmx->esds.decoder_config, dmx->esds.decoder_config_len, profile, channels, sample_rate, output_sample_rate, aac_is_sbr))
            mxwarn(boost::format(Y("Track %1%: The AAC information could not be parsed.\n")) % dmx->id);

          else {
            mxdebug_if(m_debug_headers, boost::format(" AAC: profile: %1%, sample_rate: %2%, channels: %3%, output_sample_rate: %4%, sbr: %5%\n") % profile % sample_rate % channels % output_sample_rate % aac_is_sbr);
            if (aac_is_sbr)
              profile = AAC_PROFILE_SBR;

            dmx->a_channels               = channels;
            dmx->a_samplerate             = sample_rate;
            dmx->a_aac_profile            = profile;
            dmx->a_aac_output_sample_rate = output_sample_rate;
            dmx->a_aac_is_sbr             = aac_is_sbr;
            dmx->a_aac_config_parsed      = true;
          }
        }
      }

      mio.setFilePointer(atom.pos + atom.size);
    }
  } catch(...) {
  }
}

#define print_basic_atom_info() \
  mxdebug_if(m_debug_headers, boost::format("%1%'%2%%3%%4%%5%' atom, size %6%, at %7%\n") % space(2 * level + 1) % BE2STR(atom.fourcc) % atom.size % atom.pos);

#define print_atom_too_small_error(name, type)                                                                          \
  mxerror(boost::format(Y("Quicktime/MP4 reader: '%1%' atom is too small. Expected size: >= %2%. Actual size: %3%.\n")) \
          % name % sizeof(type) % atom.size);

void
qtmp4_reader_c::handle_cmov_atom(qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom;

    atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('d', 'c', 'o', 'm') == atom.fourcc)
      handle_dcom_atom(atom.to_parent(), level + 1);

    else if (FOURCC('c', 'm', 'v', 'd') == atom.fourcc)
      handle_cmvd_atom(atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_cmvd_atom(qt_atom_t atom,
                                 int level) {
  uint32_t moov_size = m_in->read_uint32_be();
  mxdebug_if(m_debug_headers, boost::format("%1%Uncompressed size: %2%\n") % space((level + 1) * 2 + 1) % moov_size);

  if (m_compression_algorithm != FOURCC('z', 'l', 'i', 'b'))
    mxerror(boost::format(Y("Quicktime/MP4 reader: This file uses compressed headers with an unknown "
                            "or unsupported compression algorithm '%1%%2%%3%%4%'. Aborting.\n")) % BE2STR(m_compression_algorithm));

  mm_io_cptr old_in       = m_in;
  uint32_t cmov_size      = atom.size - atom.hsize;
  memory_cptr af_cmov_buf = memory_c::alloc(cmov_size);
  unsigned char *cmov_buf = af_cmov_buf->get_buffer();
  memory_cptr af_moov_buf = memory_c::alloc(moov_size + 16);
  unsigned char *moov_buf = af_moov_buf->get_buffer();

  if (m_in->read(cmov_buf, cmov_size) != cmov_size)
    throw mtx::input::header_parsing_x();

  z_stream zs;
  zs.zalloc    = (alloc_func)NULL;
  zs.zfree     = (free_func)NULL;
  zs.opaque    = (voidpf)NULL;
  zs.next_in   = cmov_buf;
  zs.avail_in  = cmov_size;
  zs.next_out  = moov_buf;
  zs.avail_out = moov_size;

  int zret = inflateInit(&zs);
  if (Z_OK != zret)
    mxerror(boost::format(Y("Quicktime/MP4 reader: This file uses compressed headers, but the zlib library could not be initialized. "
                            "Error code from zlib: %1%. Aborting.\n")) % zret);

  zret = inflate(&zs, Z_NO_FLUSH);
  if ((Z_OK != zret) && (Z_STREAM_END != zret))
    mxerror(boost::format(Y("Quicktime/MP4 reader: This file uses compressed headers, but they could not be uncompressed. "
                            "Error code from zlib: %1%. Aborting.\n")) % zret);

  if (moov_size != zs.total_out)
    mxwarn(boost::format(Y("Quicktime/MP4 reader: This file uses compressed headers, but the expected uncompressed size (%1%) "
                           "was not what is available after uncompressing (%2%).\n")) % moov_size % zs.total_out);

  zret = inflateEnd(&zs);

  m_in = mm_io_cptr(new mm_mem_io_c(moov_buf, zs.total_out));
  while (!m_in->eof()) {
    qt_atom_t next_atom = read_atom();
    mxdebug_if(m_debug_headers, boost::format("%1%'%2%%3%%4%%5%' atom at %6%\n") % space((level + 1) * 2 + 1) % BE2STR(next_atom.fourcc) % next_atom.pos);

    if (FOURCC('m', 'o', 'o', 'v') == next_atom.fourcc)
      handle_moov_atom(next_atom.to_parent(), level + 1);

    m_in->setFilePointer(next_atom.pos + next_atom.size);
  }
  m_in = old_in;
}

void
qtmp4_reader_c::handle_ctts_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags
  uint32_t count = m_in->read_uint32_be();
  mxdebug_if(m_debug_headers, boost::format("%1%Frame offset table: %2% raw entries\n") % space(level * 2 + 1) % count);

  size_t i;
  for (i = 0; i < count; ++i) {
    qt_frame_offset_t frame_offset;

    frame_offset.count  = m_in->read_uint32_be();
    frame_offset.offset = m_in->read_uint32_be();
    new_dmx->raw_frame_offset_table.push_back(frame_offset);

    mxdebug_if(m_debug_tables, boost::format("%1%%2%: count %3% offset %4%\n") % space((level + 1) * 2 + 1) % i % frame_offset.count % frame_offset.offset);
  }
}

void
qtmp4_reader_c::handle_dcom_atom(qt_atom_t,
                                 int level) {
  m_compression_algorithm = m_in->read_uint32_be();
  mxdebug_if(m_debug_headers, boost::format("%1%Compression algorithm: %2%%3%%4%%5%\n") % space(level * 2 + 1) % BE2STR(m_compression_algorithm));
}

void
qtmp4_reader_c::handle_hdlr_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  hdlr_atom_t hdlr;

  if (sizeof(hdlr_atom_t) > atom.size)
    print_atom_too_small_error("hdlr", hdlr_atom_t);

  if (m_in->read(&hdlr, sizeof(hdlr_atom_t)) != sizeof(hdlr_atom_t))
    throw mtx::input::header_parsing_x();

  mxdebug_if(m_debug_headers, boost::format("%1%Component type: %|2$.4s| subtype: %|3$.4s|\n") % space(level * 2 + 1) % (char *)&hdlr.type % (char *)&hdlr.subtype);

  switch (get_uint32_be(&hdlr.subtype)) {
    case FOURCC('s', 'o', 'u', 'n'):
      new_dmx->type = 'a';
      break;

    case FOURCC('v', 'i', 'd', 'e'):
      new_dmx->type = 'v';
      break;

    case FOURCC('t', 'e', 'x', 't'):
      m_chapter_dmx = new_dmx;
      break;
  }
}

void
qtmp4_reader_c::handle_mdhd_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  if (1 > atom.size)
    print_atom_too_small_error("mdhd", mdhd_atom_t);

  int version = m_in->read_uint8();

  if (0 == version) {
    mdhd_atom_t mdhd;

    if (sizeof(mdhd_atom_t) > atom.size)
      print_atom_too_small_error("mdhd", mdhd_atom_t);
    if (m_in->read(&mdhd.flags, sizeof(mdhd_atom_t) - 1) != (sizeof(mdhd_atom_t) - 1))
      throw mtx::input::header_parsing_x();

    new_dmx->time_scale      = get_uint32_be(&mdhd.time_scale);
    new_dmx->global_duration = get_uint32_be(&mdhd.duration);
    new_dmx->language        = decode_and_verify_language(get_uint16_be(&mdhd.language));

  } else if (1 == version) {
    mdhd64_atom_t mdhd;

    if (sizeof(mdhd64_atom_t) > atom.size)
      print_atom_too_small_error("mdhd", mdhd64_atom_t);
    if (m_in->read(&mdhd.flags, sizeof(mdhd64_atom_t) - 1) != (sizeof(mdhd64_atom_t) - 1))
      throw mtx::input::header_parsing_x();

    new_dmx->time_scale      = get_uint32_be(&mdhd.time_scale);
    new_dmx->global_duration = get_uint64_be(&mdhd.duration);
    new_dmx->language        = decode_and_verify_language(get_uint16_be(&mdhd.language));

  } else
    mxerror(boost::format(Y("Quicktime/MP4 reader: The 'media header' atom ('mdhd') uses the unsupported version %1%.\n")) % version);

  mxdebug_if(m_debug_headers, boost::format("%1%Time scale: %2%, duration: %3%, language: %4%\n") % space(level * 2 + 1) % new_dmx->time_scale % new_dmx->global_duration % new_dmx->language);

  if (0 == new_dmx->time_scale)
    mxerror(Y("Quicktime/MP4 reader: The 'time scale' parameter was 0. This is not supported.\n"));
}

void
qtmp4_reader_c::handle_mdia_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('m', 'd', 'h', 'd') == atom.fourcc)
      handle_mdhd_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('h', 'd', 'l', 'r') == atom.fourcc)
      handle_hdlr_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('m', 'i', 'n', 'f') == atom.fourcc)
      handle_minf_atom(new_dmx, atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_minf_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t parent,
                                 int level) {
  while (0 < parent.size) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('h', 'd', 'l', 'r') == atom.fourcc)
      handle_hdlr_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('s', 't', 'b', 'l') == atom.fourcc)
      handle_stbl_atom(new_dmx, atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_moov_atom(qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('c', 'm', 'o', 'v') == atom.fourcc) {
      m_compression_algorithm = 0;
      handle_cmov_atom(atom.to_parent(), level + 1);

    } else if (FOURCC('m', 'v', 'h', 'd') == atom.fourcc)
      handle_mvhd_atom(atom.to_parent(), level + 1);

    else if (FOURCC('u', 'd', 't', 'a') == atom.fourcc)
      handle_udta_atom(atom.to_parent(), level + 1);

    else if (FOURCC('t', 'r', 'a', 'k') == atom.fourcc) {
      qtmp4_demuxer_cptr new_dmx(new qtmp4_demuxer_c);
      new_dmx->id = m_demuxers.size();

      handle_trak_atom(new_dmx, atom.to_parent(), level + 1);
      if (('?' != new_dmx->type) && (0 != get_uint32_le(new_dmx->fourcc)))
        m_demuxers.push_back(new_dmx);
    }

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_mvhd_atom(qt_atom_t atom,
                                 int level) {
  mvhd_atom_t mvhd;

  if (sizeof(mvhd_atom_t) > (atom.size - atom.hsize))
    print_atom_too_small_error("mvhd", mvhd_atom_t);
  if (m_in->read(&mvhd, sizeof(mvhd_atom_t)) != sizeof(mvhd_atom_t))
    throw mtx::input::header_parsing_x();

  m_time_scale = get_uint32_be(&mvhd.time_scale);

  mxdebug_if(m_debug_headers, boost::format("%1%Time scale: %2%\n") % space(level * 2 + 1) % m_time_scale);
}

void
qtmp4_reader_c::handle_udta_atom(qt_atom_t parent,
                                 int level) {
  while (8 <= parent.size) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('c', 'h', 'p', 'l') == atom.fourcc)
      handle_chpl_atom(atom.to_parent(), level + 1);

    else if (FOURCC('m', 'e', 't', 'a') == atom.fourcc)
      handle_meta_atom(atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_chpl_atom(qt_atom_t,
                                 int level) {
  if (m_ti.m_no_chapters || (NULL != m_chapters))
    return;

  m_in->skip(1 + 3 + 4);          // Version, flags, zero

  int count = m_in->read_uint8();
  mxdebug_if(m_debug_chapters, boost::format("%1%Chapter list: %2% entries\n") % space(level * 2 + 1) % count);

  if (0 == count)
    return;

  std::vector<qtmp4_chapter_entry_t> entries;

  int i;
  for (i = 0; i < count; ++i) {
    uint64_t timecode = m_in->read_uint64_be() * 100;
    memory_cptr buf   = memory_c::alloc(m_in->read_uint8() + 1);
    memset(buf->get_buffer(), 0, buf->get_size());

    if (m_in->read(buf->get_buffer(), buf->get_size() - 1) != (buf->get_size() - 1))
      break;

    entries.push_back(qtmp4_chapter_entry_t(std::string(reinterpret_cast<char *>(buf->get_buffer())), timecode));
  }

  recode_chapter_entries(entries);
  process_chapter_entries(level, entries);
}

void
qtmp4_reader_c::handle_meta_atom(qt_atom_t parent,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags

  while (8 <= parent.size) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('i', 'l', 's', 't') == atom.fourcc)
      handle_ilst_atom(atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_ilst_atom(qt_atom_t parent,
                                 int level) {
  while (8 <= parent.size) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('-', '-', '-', '-') == atom.fourcc)
      handle_4dashes_atom(atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

std::string
qtmp4_reader_c::read_string_atom(qt_atom_t atom,
                                 size_t num_skipped) {
  if ((num_skipped + atom.hsize) > atom.size)
    return "";

  std::string string;
  size_t length = atom.size - atom.hsize - num_skipped;

  m_in->skip(num_skipped);
  m_in->read(string, length);

  return string;
}

void
qtmp4_reader_c::handle_4dashes_atom(qt_atom_t parent,
                                    int level) {
  std::string name, mean, data;

  while (8 <= parent.size) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('n', 'a', 'm', 'e') == atom.fourcc)
      name = read_string_atom(atom, 4);

    else if (FOURCC('m', 'e', 'a', 'n') == atom.fourcc)
      mean = read_string_atom(atom, 4);

    else if (FOURCC('d', 'a', 't', 'a') == atom.fourcc)
      data = read_string_atom(atom, 8);

    skip_atom();
    parent.size -= atom.size;
  }

  mxdebug_if(m_debug_headers, boost::format("'----' content: name=%1% mean=%2% data=%3%\n") % name % mean % data);

  if (name == "iTunSMPB")
    parse_itunsmpb(data);
}

void
qtmp4_reader_c::parse_itunsmpb(std::string data) {
  data = boost::regex_replace(data, boost::regex("[^\\da-fA-F]+", boost::regex::perl), "");

  if (16 > data.length())
    return;

  try {
    m_audio_encoder_delay_samples = from_hex(data.substr(8, 8));
  } catch (std::bad_cast &) {
  }
}

void
qtmp4_reader_c::read_chapter_track() {
  if (m_ti.m_no_chapters || (NULL != m_chapters) || !m_chapter_dmx.is_set())
    return;

  m_chapter_dmx->update_tables(m_time_scale);

  if (m_chapter_dmx->sample_table.empty())
    return;

  std::vector<qtmp4_chapter_entry_t> entries;
  uint64_t pts_scale_gcd = boost::math::gcd(static_cast<uint64_t>(1000000000ull), static_cast<uint64_t>(m_chapter_dmx->time_scale));
  uint64_t pts_scale_num = 1000000000ull                                    / pts_scale_gcd;
  uint64_t pts_scale_den = static_cast<uint64_t>(m_chapter_dmx->time_scale) / pts_scale_gcd;

  for (auto &sample : m_chapter_dmx->sample_table) {
    if (2 >= sample.size)
      continue;

    m_in->setFilePointer(sample.pos, seek_beginning);
    memory_cptr chunk(memory_c::alloc(sample.size));
    if (m_in->read(chunk->get_buffer(), sample.size) != sample.size)
      continue;

    unsigned int name_len = get_uint16_be(chunk->get_buffer());
    if ((name_len + 2) > sample.size)
      continue;

    entries.push_back(qtmp4_chapter_entry_t(std::string(reinterpret_cast<char *>(chunk->get_buffer()) + 2, name_len),
                                            sample.pts * pts_scale_num / pts_scale_den));
  }

  recode_chapter_entries(entries);
  process_chapter_entries(0, entries);
}

void
qtmp4_reader_c::process_chapter_entries(int level,
                                        std::vector<qtmp4_chapter_entry_t> &entries) {
  if (entries.empty())
    return;

  mxdebug_if(m_debug_chapters, boost::format("%1%%2% chapter(s):\n") % space((level + 1) * 2 + 1) % entries.size());

  std::stable_sort(entries.begin(), entries.end());

  mm_mem_io_c out(NULL, 0, 1000);
  out.set_file_name(m_ti.m_fname);
  out.write_bom("UTF-8");

  unsigned int i = 0;
  for (; entries.size() > i; ++i) {
    qtmp4_chapter_entry_t &chapter = entries[i];

    mxdebug_if(m_debug_chapters, boost::format("%1%%2%: start %4% name %3%\n") % space((level + 1) * 2 + 1) % i % chapter.m_name % format_timecode(chapter.m_timecode));

    out.puts(boost::format("CHAPTER%|1$02d|=%|2$02d|:%|3$02d|:%|4$02d|.%|5$03d|\n"
                           "CHAPTER%|1$02d|NAME=%6%\n")
             % i
             % (int)( chapter.m_timecode / 60 / 60 / 1000000000)
             % (int)((chapter.m_timecode      / 60 / 1000000000) %   60)
             % (int)((chapter.m_timecode           / 1000000000) %   60)
             % (int)((chapter.m_timecode           /    1000000) % 1000)
             % chapter.m_name);
  }

  mm_text_io_c text_out(&out, false);
  try {
    m_chapters = parse_chapters(&text_out, 0, -1, 0, m_ti.m_chapter_language, "", true);
    align_chapter_edition_uids(m_chapters);
  } catch (mtx::chapter_parser_x &ex) {
    mxerror(boost::format(Y("The MP4 file '%1%' contains chapters whose format was not recognized. This is often the case if the chapters are not encoded in UTF-8. Use the '--chapter-charset' option in order to specify the charset to use.\n")) % m_ti.m_fname);
  }
}

void
qtmp4_reader_c::handle_stbl_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t parent,
                                 int level) {
  while (0 < parent.size) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('s', 't', 't', 's') == atom.fourcc)
      handle_stts_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('s', 't', 's', 'd') == atom.fourcc)
      handle_stsd_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('s', 't', 's', 's') == atom.fourcc)
      handle_stss_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('s', 't', 's', 'c') == atom.fourcc)
      handle_stsc_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('s', 't', 's', 'z') == atom.fourcc)
      handle_stsz_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('s', 't', 'c', 'o') == atom.fourcc)
      handle_stco_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('c', 'o', '6', '4') == atom.fourcc)
      handle_co64_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('c', 't', 't', 's') == atom.fourcc)
      handle_ctts_atom(new_dmx, atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_stco_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags
  uint32_t count = m_in->read_uint32_be();

  mxdebug_if(m_debug_headers, boost::format("%1%Chunk offset table: %2% entries\n") % space(level * 2 + 1) % count);

  size_t i;
  for (i = 0; i < count; ++i) {
    qt_chunk_t chunk;

    chunk.pos = m_in->read_uint32_be();
    new_dmx->chunk_table.push_back(chunk);
    mxdebug_if(m_debug_tables, boost::format("%1%  %2%\n") % space(level * 2 + 1) % chunk.pos);
  }
}

void
qtmp4_reader_c::handle_co64_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags
  uint32_t count = m_in->read_uint32_be();

  mxdebug_if(m_debug_headers, boost::format("%1%64bit chunk offset table: %2% entries\n") % space(level * 2 + 1) % count);

  size_t i;
  for (i = 0; i < count; ++i) {
    qt_chunk_t chunk;

    chunk.pos = m_in->read_uint64_be();
    new_dmx->chunk_table.push_back(chunk);
    mxdebug_if(m_debug_tables, boost::format("%1%  %2%\n") % space(level * 2 + 1) % chunk.pos);
  }
}

void
qtmp4_reader_c::handle_stsc_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags
  uint32_t count = m_in->read_uint32_be();
  size_t i;
  for (i = 0; i < count; ++i) {
    qt_chunkmap_t chunkmap;

    chunkmap.first_chunk           = m_in->read_uint32_be() - 1;
    chunkmap.samples_per_chunk     = m_in->read_uint32_be();
    chunkmap.sample_description_id = m_in->read_uint32_be();
    new_dmx->chunkmap_table.push_back(chunkmap);
  }

  mxdebug_if(m_debug_headers, boost::format("%1%Sample to chunk/chunkmap table: %2% entries\n") % space(level * 2 + 1) % count);
}

void
qtmp4_reader_c::handle_stsd_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  uint32_t stsd_size = 0;
  m_in->skip(1 + 3);        // version & flags
  uint32_t count = m_in->read_uint32_be();

  size_t i;
  for (i = 0; i < count; ++i) {
    int64_t pos   = m_in->getFilePointer();
    uint32_t size = m_in->read_uint32_be();

    if (4 > size)
      mxerror(boost::format(Y("Quicktime/MP4 reader: The 'size' field is too small in the stream description atom for track ID %1%.\n")) % new_dmx->id);

    memory_cptr af_priv = memory_c::alloc(size);
    unsigned char *priv = af_priv->get_buffer();

    put_uint32_be(priv, size);
    if (m_in->read(priv + sizeof(uint32_t), size - sizeof(uint32_t)) != (size - sizeof(uint32_t)))
      mxerror(boost::format(Y("Quicktime/MP4 reader: Could not read the stream description atom for track ID %1%.\n")) % new_dmx->id);

    if ('a' == new_dmx->type) {
      new_dmx->a_stsd = memory_c::clone(priv, size);

      if (sizeof(sound_v0_stsd_atom_t) > size)
        mxerror(boost::format(Y("Quicktime/MP4 reader: Could not read the sound description atom for track ID %1%.\n")) % new_dmx->id);

      sound_v1_stsd_atom_t sv1_stsd;
      sound_v2_stsd_atom_t sv2_stsd;
      memcpy(&sv1_stsd, priv, sizeof(sound_v0_stsd_atom_t));
      memcpy(&sv2_stsd, priv, sizeof(sound_v0_stsd_atom_t));

      if (0 != new_dmx->fourcc[0])
        mxwarn(boost::format(Y("Quicktime/MP4 reader: Track ID %1% has more than one FourCC. Only using the first one (%|2$.4s|) and not this one (%|3$.4s|).\n"))
               % new_dmx->id % new_dmx->fourcc % (const unsigned char *)sv1_stsd.v0.base.fourcc);
      else
        memcpy(new_dmx->fourcc, sv1_stsd.v0.base.fourcc, 4);

      uint16_t version = get_uint16_be(&sv1_stsd.v0.version);

      mxdebug_if(m_debug_headers, boost::format("%1%FourCC: %|2$.4s|, channels: %3%, sample size: %4%, compression id: %5%, sample rate: 0x%|6$08x|, version: %7%")
                 % space(level * 2 + 1)
                 % (const unsigned char *)sv1_stsd.v0.base.fourcc
                 % get_uint16_be(&sv1_stsd.v0.channels)
                 % get_uint16_be(&sv1_stsd.v0.sample_size)
                 % get_uint16_be(&sv1_stsd.v0.compression_id)
                 % get_uint32_be(&sv1_stsd.v0.sample_rate)
                 % get_uint16_be(&sv1_stsd.v0.version));

      if (1 == version) {
        if (sizeof(sound_v1_stsd_atom_t) > size)
          mxerror(boost::format(Y("Quicktime/MP4 reader: Could not read the extended sound description atom for track ID %1%.\n")) % new_dmx->id);

        memcpy(&sv1_stsd, priv, sizeof(sound_v1_stsd_atom_t));

        if (m_debug_headers)
          mxinfo(boost::format(" samples per packet: %1% bytes per packet: %2% bytes per frame: %3% bytes_per_sample: %4%")
                 % get_uint32_be(&sv1_stsd.v1.samples_per_packet)
                 % get_uint32_be(&sv1_stsd.v1.bytes_per_packet)
                 % get_uint32_be(&sv1_stsd.v1.bytes_per_frame)
                 % get_uint32_be(&sv1_stsd.v1.bytes_per_sample));

      } else if (2 == version) {
        if (sizeof(sound_v2_stsd_atom_t) > size)
          mxerror(boost::format(Y("Quicktime/MP4 reader: Could not read the extended sound description atom for track ID %1%.\n")) % new_dmx->id);

        memcpy(&sv2_stsd, priv, sizeof(sound_v2_stsd_atom_t));

        if (m_debug_headers)
          mxinfo(boost::format(" struct size: %1% sample rate: %|2$016x| channels: %3% const1: 0x%|4$08x| bits per channel: %5% flags: %6% bytes per frame: %7% samples per frame: %8%")
                 % get_uint32_be(&sv2_stsd.v2.v2_struct_size)
                 % get_uint64_be(&sv2_stsd.v2.sample_rate)
                 % get_uint32_be(&sv2_stsd.v2.channels)
                 % get_uint32_be(&sv2_stsd.v2.const1)
                 % get_uint32_be(&sv2_stsd.v2.bits_per_channel)
                 % get_uint32_be(&sv2_stsd.v2.flags)
                 % get_uint32_be(&sv2_stsd.v2.bytes_per_frame)
                 % get_uint32_be(&sv2_stsd.v2.samples_per_frame));
      }

      if (m_debug_headers)
        mxinfo("\n");

      new_dmx->a_channels   = get_uint16_be(&sv1_stsd.v0.channels);
      new_dmx->a_bitdepth   = get_uint16_be(&sv1_stsd.v0.sample_size);
      uint32_t tmp          = get_uint32_be(&sv1_stsd.v0.sample_rate);
      new_dmx->a_samplerate = (float)((tmp & 0xffff0000) >> 16) + (float)(tmp & 0x0000ffff) / 65536.0;

      if (get_uint16_be(&sv1_stsd.v0.version) == 0)
        stsd_size = sizeof(sound_v0_stsd_atom_t);

      else if (get_uint16_be(&sv1_stsd.v0.version) == 1)
        stsd_size = sizeof(sound_v1_stsd_atom_t);

      else if (get_uint16_be(&sv1_stsd.v0.version) == 2)
        stsd_size = sizeof(sound_v2_stsd_atom_t);

    } else if ('v' == new_dmx->type) {
      new_dmx->v_stsd = memory_c::clone(priv, size);

      if (sizeof(video_stsd_atom_t) > size)
        mxerror(boost::format(Y("Quicktime/MP4 reader: Could not read the video description atom for track ID %1%.\n")) % new_dmx->id);

      video_stsd_atom_t v_stsd;
      memcpy(&v_stsd, priv, sizeof(video_stsd_atom_t));

      if (0 != new_dmx->fourcc[0])
        mxwarn(boost::format(Y("Quicktime/MP4 reader: Track ID %1% has more than one FourCC. Only using the first one (%|2$.4s|) and not this one (%|3$.4s|).\n"))
               % new_dmx->id % new_dmx->fourcc % (const unsigned char *)v_stsd.base.fourcc);

      else {
        memcpy(new_dmx->fourcc, v_stsd.base.fourcc, 4);
        new_dmx->v_is_avc = !strncasecmp(new_dmx->fourcc, "avc1", 4);
      }

      mxdebug_if(m_debug_headers, boost::format("%1%FourCC: %|2$.4s|, width: %3%, height: %4%, depth: %5%\n")
                 % space(level * 2 + 1)
                 % (const unsigned char *)v_stsd.base.fourcc
                 % get_uint16_be(&v_stsd.width)
                 % get_uint16_be(&v_stsd.height)
                 % get_uint16_be(&v_stsd.depth));

      new_dmx->v_width    = get_uint16_be(&v_stsd.width);
      new_dmx->v_height   = get_uint16_be(&v_stsd.height);
      new_dmx->v_bitdepth = get_uint16_be(&v_stsd.depth);
      stsd_size           = sizeof(video_stsd_atom_t);
    }

    if ((0 < stsd_size) && (stsd_size < size)) {
      if ('v' == new_dmx->type)
        parse_video_header_priv_atoms(new_dmx, priv + stsd_size, size - stsd_size, level);
      else if ('a' == new_dmx->type)
        parse_audio_header_priv_atoms(new_dmx, priv + stsd_size, size - stsd_size, level);
    }

    m_in->setFilePointer(pos + size);
  }
}

void
qtmp4_reader_c::handle_stss_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags
  uint32_t count = m_in->read_uint32_be();

  size_t i;
  for (i = 0; i < count; ++i)
    new_dmx->keyframe_table.push_back(m_in->read_uint32_be());

  std::sort(new_dmx->keyframe_table.begin(), new_dmx->keyframe_table.end());

  mxdebug_if(m_debug_headers, boost::format("%1%Sync/keyframe table: %2% entries\n") % space(level * 2 + 1) % count);
  for (i = 0; i < count; ++i)
    mxdebug_if(m_debug_tables, boost::format("%1%keyframe at %2%\n") % space((level + 1) * 2 + 1) % new_dmx->keyframe_table[i]);
}

void
qtmp4_reader_c::handle_stsz_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags
  uint32_t sample_size = m_in->read_uint32_be();
  uint32_t count       = m_in->read_uint32_be();

  if (0 == sample_size) {
    size_t i;
    for (i = 0; i < count; ++i) {
      qt_sample_t sample;

      sample.size = m_in->read_uint32_be();
      new_dmx->sample_table.push_back(sample);
    }

    mxdebug_if(m_debug_headers, boost::format("%1%Sample size table: %2% entries\n") % space(level * 2 + 1) % count);

  } else {
    new_dmx->sample_size = sample_size;
    mxdebug_if(m_debug_headers, boost::format("%1%Sample size table; global sample size: %2%\n") % space(level * 2 + 1) % sample_size);
  }
}

void
qtmp4_reader_c::handle_sttd_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags
  uint32_t count = m_in->read_uint32_be();

  size_t i;
  for (i = 0; i < count; ++i) {
    qt_durmap_t durmap;

    durmap.number   = m_in->read_uint32_be();
    durmap.duration = m_in->read_uint32_be();
    new_dmx->durmap_table.push_back(durmap);
  }

  mxdebug_if(m_debug_headers, boost::format("%1%Sample duration table: %2% entries\n") % space(level * 2 + 1) % count);
}

void
qtmp4_reader_c::handle_stts_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags
  uint32_t count = m_in->read_uint32_be();

  size_t i;
  for (i = 0; i < count; ++i) {
    qt_durmap_t durmap;

    durmap.number   = m_in->read_uint32_be();
    durmap.duration = m_in->read_uint32_be();
    new_dmx->durmap_table.push_back(durmap);
  }

  mxdebug_if(m_debug_headers, boost::format("%1%Sample duration table: %2% entries\n") % space(level * 2 + 1) % count);
}

void
qtmp4_reader_c::handle_edts_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t parent,
                                 int level) {
  while (0 < parent.size) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('e', 'l', 's', 't') == atom.fourcc)
      handle_elst_atom(new_dmx, atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_elst_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t,
                                 int level) {
  m_in->skip(1 + 3);        // version & flags
  uint32_t count = m_in->read_uint32_be();
  new_dmx->editlist_table.resize(count);

  size_t i;
  for (i = 0; i < count; ++i) {
    qt_editlist_t &editlist = new_dmx->editlist_table[i];

    editlist.duration = m_in->read_uint32_be();
    editlist.pos      = m_in->read_uint32_be();
    editlist.speed    = m_in->read_uint32_be();
  }

  mxdebug_if(m_debug_headers, boost::format("%1%Edit list table: %2% entries\n") % space(level * 2 + 1) % count);
  for (i = 0; i < count; ++i)
    mxdebug_if(m_debug_tables, boost::format("%1%%2%: duration %3% pos %4% speed %5%\n")
               % space((level + 1) * 2 + 1)
               % i
               % new_dmx->editlist_table[i].duration
               % new_dmx->editlist_table[i].pos
               % new_dmx->editlist_table[i].speed);
}

void
qtmp4_reader_c::handle_tkhd_atom(qtmp4_demuxer_cptr &/* new_dmx */,
                                 qt_atom_t atom,
                                 int level) {
  tkhd_atom_t tkhd;

  if (sizeof(tkhd_atom_t) > atom.size)
    print_atom_too_small_error("tkhd", tkhd_atom_t);

  if (m_in->read(&tkhd, sizeof(tkhd_atom_t)) != sizeof(tkhd_atom_t))
    throw mtx::input::header_parsing_x();

  mxdebug_if(m_debug_headers, boost::format("%1%Track ID: %2%\n") % space(level * 2 + 1) % get_uint32_be(&tkhd.track_id));
}

void
qtmp4_reader_c::handle_trak_atom(qtmp4_demuxer_cptr &new_dmx,
                                 qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom = read_atom();
    print_basic_atom_info();

    if (FOURCC('t', 'k', 'h', 'd') == atom.fourcc)
      handle_tkhd_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('m', 'd', 'i', 'a') == atom.fourcc)
      handle_mdia_atom(new_dmx, atom.to_parent(), level + 1);

    else if (FOURCC('e', 'd', 't', 's') == atom.fourcc)
      handle_edts_atom(new_dmx, atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

file_status_e
qtmp4_reader_c::read(generic_packetizer_c *ptzr,
                     bool) {
  size_t dmx_idx;

  for (dmx_idx = 0; dmx_idx < m_demuxers.size(); ++dmx_idx) {
    qtmp4_demuxer_cptr &dmx = m_demuxers[dmx_idx];

    if ((-1 == dmx->ptzr) || (PTZR(dmx->ptzr) != ptzr))
      continue;

    if (dmx->pos < dmx->m_index.size())
      break;
  }

  if (m_demuxers.size() == dmx_idx)
    return flush_packetizers();

  qtmp4_demuxer_cptr &dmx = m_demuxers[dmx_idx];
  qt_index_t &index       = dmx->m_index[dmx->pos];

  m_in->setFilePointer(index.file_pos);

  int buffer_offset = 0;
  unsigned char *buffer;

  if (   ('v' == dmx->type)
      && (0 == dmx->pos)
      && (!strncasecmp(dmx->fourcc, "mp4v", 4) || !strncasecmp(dmx->fourcc, "xvid", 4))
      && dmx->esds_parsed
      && (NULL != dmx->esds.decoder_config)) {
    buffer        = (unsigned char *)safemalloc(index.size + dmx->esds.decoder_config_len);
    buffer_offset = dmx->esds.decoder_config_len;

    memcpy(buffer, dmx->esds.decoder_config, dmx->esds.decoder_config_len);

  } else {
    buffer = (unsigned char *)safemalloc(index.size);
  }

  if (m_in->read(buffer + buffer_offset, index.size) != index.size) {
    mxwarn(boost::format(Y("Quicktime/MP4 reader: Could not read chunk number %1%/%2% with size %3% from position %4%. Aborting.\n"))
           % dmx->pos % dmx->m_index.size() % index.size % index.file_pos);
    safefree(buffer);
    return flush_packetizers();
  }

  PTZR(dmx->ptzr)->process(new packet_t(new memory_c(buffer, index.size + buffer_offset, true), index.timecode, index.duration,
                                        index.is_keyframe ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC, VFT_NOBFRAME));
  ++dmx->pos;

  if (dmx->pos < dmx->m_index.size())
    return FILE_STATUS_MOREDATA;

  return flush_packetizers();
}

uint32_t
qtmp4_reader_c::read_esds_descr_len(mm_mem_io_c &memio) {
  uint32_t len           = 0;
  unsigned int num_bytes = 0;
  uint8_t byte;

  do {
    byte = memio.read_uint8();
    len  = (len << 7) | (byte & 0x7f);
    ++num_bytes;

  } while (((byte & 0x80) == 0x80) && (4 > num_bytes));

  return len;
}

bool
qtmp4_reader_c::parse_esds_atom(mm_mem_io_c &memio,
                                qtmp4_demuxer_cptr &dmx,
                                int level) {
  int lsp    = (level + 1) * 2;
  esds_t *e  = &dmx->esds;
  e->version = memio.read_uint8();
  e->flags   = memio.read_uint24_be();
  mxdebug_if(m_debug_headers, boost::format("%1%esds: version: %2%, flags: %3%\n") % space(lsp + 1) % (int)e->version % (int)e->flags);

  uint8_t tag = memio.read_uint8();
  if (MP4DT_ES == tag) {
    uint32_t len       = read_esds_descr_len(memio);
    e->esid            = memio.read_uint16_be();
    e->stream_priority = memio.read_uint8();
    mxdebug_if(m_debug_headers, boost::format("%1%esds: id: %2%, stream priority: %3%, len: %4%\n") % space(lsp + 1) % (int)e->esid % (int)e->stream_priority % len);

  } else {
    e->esid = memio.read_uint16_be();
    mxdebug_if(m_debug_headers, boost::format("%1%esds: id: %2%\n") % space(lsp + 1) % (int)e->esid);
  }

  tag = memio.read_uint8();
  if (MP4DT_DEC_CONFIG != tag) {
    mxdebug_if(m_debug_headers, boost::format("%1%tag is not DEC_CONFIG (0x%|2$02x|) but 0x%|3$02x|.\n") % space(lsp + 1) % MP4DT_DEC_CONFIG % tag);
    return false;
  }

  uint32_t len          = read_esds_descr_len(memio);

  e->object_type_id     = memio.read_uint8();
  e->stream_type        = memio.read_uint8();
  e->buffer_size_db     = memio.read_uint24_be();
  e->max_bitrate        = memio.read_uint32_be();
  e->avg_bitrate        = memio.read_uint32_be();
  e->decoder_config_len = 0;

  mxdebug_if(m_debug_headers, boost::format("%1%esds: decoder config descriptor, len: %2%, object_type_id: %3%, "
                                            "stream_type: 0x%|4$2x|, buffer_size_db: %5%, max_bitrate: %|6$.3f|kbit/s, avg_bitrate: %|7$.3f|kbit/s\n")
             % space(lsp + 1)
             % len
             % (int)e->object_type_id
             % (int)e->stream_type
             % (int)e->buffer_size_db
             % (e->max_bitrate / 1000.0)
             % (e->avg_bitrate / 1000.0));

  tag = memio.read_uint8();
  if (MP4DT_DEC_SPECIFIC == tag) {
    len                   = read_esds_descr_len(memio);
    e->decoder_config_len = len;
    e->decoder_config     = (uint8_t *)safemalloc(len);
    if (memio.read(e->decoder_config, len) != len)
      throw mtx::input::header_parsing_x();

    tag = memio.read_uint8();

    if (m_debug_headers) {
      mxinfo(boost::format("%1%esds: decoder specific descriptor, len: %2%\n") % space(lsp + 1) % len);
      mxinfo(boost::format("Quicktime/MP4 reader:%1%esds: dumping decoder specific descriptor\n") % space(lsp + 3));
      mxhexdump(0, e->decoder_config, e->decoder_config_len, "Debug> ");
    }

  } else
    mxdebug_if(m_debug_headers, boost::format("%1%tag is not DEC_SPECIFIC (0x%|2$02x|) but 0x%|3$02x|.\n") % space(lsp + 1) % MP4DT_DEC_SPECIFIC % tag);

  if (MP4DT_SL_CONFIG == tag) {
    len              = read_esds_descr_len(memio);
    e->sl_config_len = len;
    e->sl_config     = (uint8_t *)safemalloc(len);
    if (memio.read(e->sl_config, len) != len)
      throw mtx::input::header_parsing_x();

    mxdebug_if(m_debug_headers, boost::format("%1%esds: sync layer config descriptor, len: %2%\n") % space(lsp + 1) % len);

  } else
    mxdebug_if(m_debug_headers, boost::format("%1%tag is not SL_CONFIG (0x%|2$02x|) but 0x%|3$02x|.\n") % space(lsp + 1) % MP4DT_SL_CONFIG % tag);

  return true;
}

memory_cptr
qtmp4_reader_c::create_bitmap_info_header(qtmp4_demuxer_cptr &dmx,
                                          const char *fourcc,
                                          size_t extra_size,
                                          const void *extra_data) {
  int full_size           = sizeof(alBITMAPINFOHEADER) + extra_size;
  memory_cptr bih_p       = memory_c::alloc(full_size);
  alBITMAPINFOHEADER *bih = (alBITMAPINFOHEADER *)bih_p->get_buffer();

  memset(bih, 0, full_size);
  put_uint32_le(&bih->bi_size,       full_size);
  put_uint32_le(&bih->bi_width,      dmx->v_width);
  put_uint32_le(&bih->bi_height,     dmx->v_height);
  put_uint16_le(&bih->bi_planes,     1);
  put_uint16_le(&bih->bi_bit_count,  dmx->v_bitdepth);
  put_uint32_le(&bih->bi_size_image, dmx->v_width * dmx->v_height * 3);
  memcpy(&bih->bi_compression, fourcc, 4);

  if (0 != extra_size)
    memcpy(bih + 1, extra_data, extra_size);

  return bih_p;
}

bool
qtmp4_reader_c::create_audio_packetizer_ac3(qtmp4_demuxer_cptr &dmx) {
  memory_cptr buf = memory_c::alloc(64);

  if (!dmx->read_first_bytes(buf, 64, m_in) || (-1 == find_ac3_header(buf->get_buffer(), buf->get_size(), &dmx->m_ac3_header, false))) {
    mxwarn_tid(m_ti.m_fname, dmx->id, Y("No AC3 header found in first frame; track will be skipped.\n"));
    dmx->ok = false;

    return false;
  }

  dmx->ptzr = add_packetizer(new ac3_packetizer_c(this, m_ti, dmx->m_ac3_header.sample_rate, dmx->m_ac3_header.channels, dmx->m_ac3_header.bsid));
  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));

  return true;
}

void
qtmp4_reader_c::create_video_packetizer_svq1(qtmp4_demuxer_cptr &dmx) {
  memory_cptr bih(create_bitmap_info_header(dmx, "SVQ1"));

  m_ti.m_private_size = bih->get_size();
  m_ti.m_private_data = (unsigned char *)bih->get_buffer();

  dmx->ptzr         = add_packetizer(new video_packetizer_c(this, m_ti, MKV_V_MSCOMP, 0.0, dmx->v_width, dmx->v_height));
  m_ti.m_private_data = NULL;

  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));
}

void
qtmp4_reader_c::create_video_packetizer_mpeg4_p2(qtmp4_demuxer_cptr &dmx) {
  memory_cptr bih(create_bitmap_info_header(dmx, "DIVX"));

  m_ti.m_private_size = bih->get_size();
  m_ti.m_private_data = (unsigned char *)bih->get_buffer();
  dmx->ptzr         = add_packetizer(new mpeg4_p2_video_packetizer_c(this, m_ti, 0.0, dmx->v_width, dmx->v_height, false));
  m_ti.m_private_data = NULL;

  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));
}

void
qtmp4_reader_c::create_video_packetizer_mpeg1_2(qtmp4_demuxer_cptr &dmx) {
  int version = dmx->fourcc[3] - '0';
  dmx->ptzr   = add_packetizer(new mpeg1_2_video_packetizer_c(this, m_ti, version, -1.0, dmx->v_width, dmx->v_height, 0, 0, false));

  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));
}

void
qtmp4_reader_c::create_video_packetizer_mpeg4_p10(qtmp4_demuxer_cptr &dmx) {
  if (dmx->frame_offset_table.empty())
    mxwarn_tid(m_ti.m_fname, dmx->id,
               Y("The AVC video track is missing the 'CTTS' atom for frame timecode offsets. "
                 "However, AVC/h.264 allows frames to have more than the traditional one (for P frames) or two (for B frames) references to other frames. "
                 "The timecodes for such frames will be out-of-order, and the 'CTTS' atom is needed for getting the timecodes right. "
                 "As it is missing the timecodes for this track might be wrong. "
                 "You should watch the resulting file and make sure that it looks like you expected it to.\n"));

  m_ti.m_private_size = dmx->priv_size;
  m_ti.m_private_data = dmx->priv;
  dmx->ptzr         = add_packetizer(new mpeg4_p10_video_packetizer_c(this, m_ti, dmx->fps, dmx->v_width, dmx->v_height));
  m_ti.m_private_data = NULL;

  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));
}

void
qtmp4_reader_c::create_video_packetizer_standard(qtmp4_demuxer_cptr &dmx) {
  m_ti.m_private_size = dmx->v_stsd->get_size();
  m_ti.m_private_data = dmx->v_stsd->get_buffer();
  dmx->ptzr         = add_packetizer(new video_packetizer_c(this, m_ti, MKV_V_QUICKTIME, 0.0, dmx->v_width, dmx->v_height));
  m_ti.m_private_data = NULL;

  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));
}

void
qtmp4_reader_c::create_audio_packetizer_aac(qtmp4_demuxer_cptr &dmx) {
  m_ti.m_private_data = dmx->esds.decoder_config;
  m_ti.m_private_size = dmx->esds.decoder_config_len;
  dmx->ptzr         = add_packetizer(new aac_packetizer_c(this, m_ti, AAC_ID_MPEG4, dmx->a_aac_profile, (int)dmx->a_samplerate, dmx->a_channels, false, true));
  m_ti.m_private_data = NULL;
  m_ti.m_private_size = 0;

  if (dmx->a_aac_is_sbr)
    PTZR(dmx->ptzr)->set_audio_output_sampling_freq(dmx->a_aac_output_sample_rate);

  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));
}

void
qtmp4_reader_c::create_audio_packetizer_mp3(qtmp4_demuxer_cptr &dmx) {
  dmx->ptzr = add_packetizer(new mp3_packetizer_c(this, m_ti, (int32_t)dmx->a_samplerate, dmx->a_channels, true));
  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));
}

void
qtmp4_reader_c::create_audio_packetizer_pcm(qtmp4_demuxer_cptr &dmx) {
  dmx->ptzr = add_packetizer(new pcm_packetizer_c(this, m_ti, (int32_t)dmx->a_samplerate, dmx->a_channels, dmx->a_bitdepth, (8 < dmx->a_bitdepth) && ('t' == dmx->fourcc[0])));
  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));
}

void
qtmp4_reader_c::create_audio_packetizer_passthrough(qtmp4_demuxer_cptr &dmx) {
  passthrough_packetizer_c *ptzr = new passthrough_packetizer_c(this, m_ti);
  dmx->ptzr                      = add_packetizer(ptzr);

  ptzr->set_track_type(track_audio);
  ptzr->set_codec_id(MKV_A_QUICKTIME);
  ptzr->set_codec_private(dmx->a_stsd->get_buffer(), dmx->a_stsd->get_size());
  ptzr->set_audio_sampling_freq(dmx->a_samplerate);
  ptzr->set_audio_channels(dmx->a_channels);

  show_packetizer_info(dmx->id, PTZR(dmx->ptzr));
}

void
qtmp4_reader_c::create_packetizer(int64_t tid) {
  unsigned int i;
  qtmp4_demuxer_cptr dmx;

  for (i = 0; i < m_demuxers.size(); ++i)
    if (m_demuxers[i]->id == tid) {
      dmx = m_demuxers[i];
      break;
    }

  if (!dmx.is_set() || !dmx->ok || !demuxing_requested(dmx->type, dmx->id) || (-1 != dmx->ptzr))
    return;

  m_ti.m_id          = dmx->id;
  m_ti.m_language    = dmx->language;

  bool packetizer_ok = true;

  if ('v' == dmx->type) {
    if (!strncasecmp(dmx->fourcc, "mp4v", 4) || !strncasecmp(dmx->fourcc, "xvid", 4))
      create_video_packetizer_mpeg4_p2(dmx);

    else if (!strncasecmp(dmx->fourcc, "mpg1", 4) || !strncasecmp(dmx->fourcc, "mpg2", 4))
      create_video_packetizer_mpeg1_2(dmx);

    else if (dmx->v_is_avc)
      create_video_packetizer_mpeg4_p10(dmx);

    else if (!strncasecmp(dmx->fourcc, "svq1", 4))
      create_video_packetizer_svq1(dmx);

    else
      create_video_packetizer_standard(dmx);


  } else {
    if (!strncasecmp(dmx->fourcc, "MP4A", 4) && IS_AAC_OBJECT_TYPE_ID(dmx->esds.object_type_id))
      create_audio_packetizer_aac(dmx);

    else if (!strncasecmp(dmx->fourcc, "MP4A", 4) && ((MP4OTI_MPEG2AudioPart3 == dmx->esds.object_type_id) || (MP4OTI_MPEG1Audio == dmx->esds.object_type_id)))
      create_audio_packetizer_mp3(dmx);

    else if (!strncasecmp(dmx->fourcc, ".mp3", 4))
      create_audio_packetizer_mp3(dmx);

    else if (!strncasecmp(dmx->fourcc, "twos", 4) || !strncasecmp(dmx->fourcc, "sowt", 4))
      create_audio_packetizer_pcm(dmx);

    else if (!strncasecmp(dmx->fourcc, "ac-3", 4) || !strncasecmp(dmx->fourcc, "sac3", 4))
      packetizer_ok = create_audio_packetizer_ac3(dmx);

    else
      create_audio_packetizer_passthrough(dmx);

    handle_audio_encoder_delay(dmx);
  }

  if (packetizer_ok && (-1 == m_main_dmx))
    m_main_dmx = i;
}

void
qtmp4_reader_c::create_packetizers() {
  unsigned int i;

  m_main_dmx = -1;

  for (i = 0; i < m_demuxers.size(); ++i)
    create_packetizer(m_demuxers[i]->id);
}

int
qtmp4_reader_c::get_progress() {
  if (-1 == m_main_dmx)
    return 100;

  qtmp4_demuxer_cptr &dmx = m_demuxers[m_main_dmx];
  unsigned int max_chunks = (0 == dmx->sample_size) ? dmx->sample_table.size() : dmx->chunk_table.size();

  return 100 * dmx->pos / max_chunks;
}

void
qtmp4_reader_c::identify() {
  std::vector<std::string> verbose_info;
  unsigned int i;

  id_result_container();

  for (i = 0; i < m_demuxers.size(); ++i) {
    qtmp4_demuxer_cptr &dmx = m_demuxers[i];

    verbose_info.clear();

    if (dmx->v_is_avc)
      verbose_info.push_back("packetizer:mpeg4_p10_video");

    if (!dmx->language.empty())
      verbose_info.push_back((boost::format("language:%1%") % dmx->language).str());

    id_result_track(dmx->id, dmx->type == 'v' ? ID_RESULT_TRACK_VIDEO : dmx->type == 'a' ? ID_RESULT_TRACK_AUDIO : ID_RESULT_TRACK_UNKNOWN,
                    (boost::format("%|1$.4s|") %  dmx->fourcc).str(), verbose_info);
  }

  if (NULL != m_chapters)
    id_result_chapters(count_chapter_atoms(*m_chapters));
}

void
qtmp4_reader_c::add_available_track_ids() {
  unsigned int i;

  for (i = 0; i < m_demuxers.size(); ++i)
    add_available_track_id(m_demuxers[i]->id);
}

std::string
qtmp4_reader_c::decode_and_verify_language(uint16_t coded_language) {
  std::string language;
  int i;

  for (i = 0; 3 > i; ++i)
    language += (char)(((coded_language >> ((2 - i) * 5)) & 0x1f) + 0x60);

  ba::to_lower(language);

  if (is_valid_iso639_2_code(language.c_str()))
    return language;

  return "";
}

void
qtmp4_reader_c::recode_chapter_entries(std::vector<qtmp4_chapter_entry_t> &entries) {
  if (g_identifying) {
    for (auto &entry : entries)
      entry.m_name = empty_string;
    return;
  }

  std::string charset              = m_ti.m_chapter_charset.empty() ? "UTF-8" : m_ti.m_chapter_charset;
  charset_converter_cptr converter = charset_converter_c::init(m_ti.m_chapter_charset);
  converter->enable_byte_order_marker_detection(true);

  if (m_debug_chapters) {
    mxdebug(boost::format("Number of chapter entries: %1%\n") % entries.size());
    size_t num = 0;
    for (auto &entry : entries) {
      mxdebug(boost::format("  Chapter %1%: name length %2%\n") % num++ % entry.m_name.length());
      mxhexdump(0, entry.m_name.c_str(), entry.m_name.length(), "Debug> ");
    }
  }

  for (auto &entry : entries)
    entry.m_name = converter->utf8(entry.m_name);

  converter->enable_byte_order_marker_detection(false);
}

void
qtmp4_reader_c::detect_interleaving() {
  std::list<qtmp4_demuxer_cptr> demuxers_to_read;
  boost::remove_copy_if(m_demuxers, std::back_inserter(demuxers_to_read), [&](const qtmp4_demuxer_cptr &dmx) {
      return !(dmx->ok && (dmx->is_audio() || dmx->is_video()) && demuxing_requested(dmx->type, dmx->id) && (dmx->sample_table.size() > 1));
    });

  if (demuxers_to_read.size() < 2) {
    mxdebug_if(m_debug_interleaving, boost::format("Interleaving: Not enough tracks to care about interleaving.\n"));
    return;
  }

  auto cmp = [](const qt_sample_t &s1, const qt_sample_t &s2) -> uint64_t { return s1.pos < s2.pos; };

  std::list<float> gradients;
  for (auto &dmx : demuxers_to_read) {
    uint64_t min = boost::min_element(dmx->sample_table, cmp)->pos;
    uint64_t max = boost::max_element(dmx->sample_table, cmp)->pos;
    gradients.push_back(static_cast<float>(max - min) / m_in->get_size());

    mxdebug_if(m_debug_interleaving, boost::format("Interleaving: Track id %1% min %2% max %3% gradient %4%\n") % dmx->id % min % max % gradients.back());
  }

  float badness = *boost::max_element(gradients) - *boost::min_element(gradients);
  mxdebug_if(m_debug_interleaving, boost::format("Interleaving: Badness: %1% (%2%)\n") % badness % (MAX_INTERLEAVING_BADNESS < badness ? "badly interleaved" : "ok"));

  if (MAX_INTERLEAVING_BADNESS < badness)
    m_in->enable_buffering(false);
}

// ----------------------------------------------------------------------

void
qtmp4_demuxer_c::calculate_fps() {
  fps = 0.0;

  if ((1 == durmap_table.size()) && (0 != durmap_table[0].duration) && ((0 != sample_size) || (0 == frame_offset_table.size()))) {
    // Constant FPS. Let's set the default duration.
    fps = (double)time_scale / (double)durmap_table[0].duration;
    mxdebug_if(m_debug_fps, boost::format("calculate_fps: case 1: %1%\n") % fps);

  } else if (!sample_table.empty()) {
    std::map<int64_t, int> duration_map;

    std::accumulate(sample_table.begin() + 1, sample_table.end(), sample_table[0], [&](qt_sample_t &previous_sample, qt_sample_t &current_sample) {
      duration_map[current_sample.pts - previous_sample.pts]++;
      return current_sample;
    });

    auto most_common = std::accumulate(duration_map.begin(), duration_map.end(), std::pair<int64_t, int>(*duration_map.begin()),
                                       [](std::pair<int64_t, int> &a, std::pair<int64_t, int> e) { return e.second > a.second ? e : a; });
    if (most_common.first)
      fps = (double)1000000000.0 / (double)to_nsecs(most_common.first);

    mxdebug_if(m_debug_fps, boost::format("calculate_fps: case 2: most_common %1% = %2%, fps %3%\n") % most_common.first % most_common.second % fps);
  }
}

int64_t
qtmp4_demuxer_c::to_nsecs(int64_t value) {
  int i;

  for (i = 1; i <= 100000000ll; i *= 10) {
    int64_t factor   = 1000000000ll / i;
    int64_t value_up = value * factor;

    if ((value_up / factor) == value)
      return value_up / (time_scale / (1000000000ll / factor));
  }

  return value / (time_scale / 1000000000ll);
}

void
qtmp4_demuxer_c::calculate_timecodes() {
  unsigned int frame;

  if (0 != sample_size) {
    for (frame = 0; chunk_table.size() > frame; ++frame) {
      timecodes.push_back(to_nsecs((uint64_t)chunk_table[frame].samples * (uint64_t)duration));
      durations.push_back(to_nsecs((uint64_t)chunk_table[frame].size    * (uint64_t)duration));
      frame_indices.push_back(frame);
    }

    if (!timecodes.empty())
      min_timecode = max_timecode = timecodes[0];

    return;
  }

  int64_t v_dts_offset = 0;
  std::vector<int64_t> timecodes_before_offsets;

  if (('v' == type) && v_is_avc && !frame_offset_table.empty())
    v_dts_offset = to_nsecs(frame_offset_table[0]);

  for (frame = 0; sample_table.size() > frame; ++frame) {
    int64_t timecode;

    if (!editlist_table.empty()) {
      unsigned int editlist_pos = 0;
      unsigned int real_frame   = frame;

      while (((editlist_table.size() - 1) > editlist_pos) && (frame >= editlist_table[editlist_pos + 1].start_frame))
        ++editlist_pos;

      if ((editlist_table[editlist_pos].start_frame + editlist_table[editlist_pos].frames) <= frame) {
        // EOF
        // calc pts:
        timecode = to_nsecs(sample_table[real_frame].pts);

      } else {
        // calc real frame index:
        real_frame -= editlist_table[editlist_pos].start_frame;
        real_frame += editlist_table[editlist_pos].start_sample;

        // calc pts:
        timecode = to_nsecs(sample_table[real_frame].pts + editlist_table[editlist_pos].pts_offset);
      }

      frame_indices.push_back(real_frame);

      timecodes_before_offsets.push_back(timecode);

      if (('v' == type) && (frame_offset_table.size() > real_frame) && v_is_avc)
         timecode += to_nsecs(frame_offset_table[real_frame]) - v_dts_offset;

    } else {
      timecode = to_nsecs(sample_table[frame].pts);
      frame_indices.push_back(frame);

      timecodes_before_offsets.push_back(timecode);

      if (('v' == type) && (frame_offset_table.size() > frame) && v_is_avc)
         timecode += to_nsecs(frame_offset_table[frame]) - v_dts_offset;
    }

    timecodes.push_back(timecode);

    if (timecode > max_timecode)
      max_timecode = timecode;
    if (timecode < min_timecode)
      min_timecode = timecode;
  }

  int64_t avg_duration = 0, num_good_frames = 0;

  for (frame = 0; timecodes_before_offsets.size() > (frame + 1); ++frame) {
    int64_t diff = timecodes_before_offsets[frame + 1] - timecodes_before_offsets[frame];

    if (0 >= diff)
      durations.push_back(0);
    else {
      ++num_good_frames;
      avg_duration += diff;
      durations.push_back(diff);
    }
  }

  durations.push_back(0);

  if (num_good_frames) {
    avg_duration /= num_good_frames;
    for (frame = 0; durations.size() > frame; ++frame)
      if (!durations[frame])
        durations[frame] = avg_duration;
  }
}

void
qtmp4_demuxer_c::adjust_timecodes(int64_t delta) {
  size_t i;

  for (i = 0; timecodes.size() > i; ++i)
    timecodes[i] += delta;

  min_timecode += delta;
  max_timecode += delta;
}

void
qtmp4_demuxer_c::update_tables(int64_t global_m_time_scale) {
  uint64_t last = chunk_table.size();

  // process chunkmap:
  size_t j, i = chunkmap_table.size();
  while (i > 0) {
    --i;
    for (j = chunkmap_table[i].first_chunk; j < last; ++j) {
      chunk_table[j].desc = chunkmap_table[i].sample_description_id;
      chunk_table[j].size = chunkmap_table[i].samples_per_chunk;
    }

    last = chunkmap_table[i].first_chunk;

    if (chunk_table.size() <= last)
      break;
  }

  // calc pts of chunks:
  uint64_t s = 0;
  for (j = 0; j < chunk_table.size(); ++j) {
    chunk_table[j].samples  = s;
    s                      += chunk_table[j].size;
  }

  // workaround for fixed-size video frames (dv and uncompressed)
  if (sample_table.empty() && ('a' != type)) {
    for (i = 0; i < s; ++i) {
      qt_sample_t sample;

      sample.size = sample_size;
      sample_table.push_back(sample);
    }

    sample_size = 0;
  }

  if (sample_table.empty()) {
    // constant sampesize
    if ((1 == durmap_table.size()) || ((2 == durmap_table.size()) && (1 == durmap_table[1].number)))
      duration = durmap_table[0].duration;
    else
      mxerror(Y("Quicktime/MP4 reader: Constant samplesize & variable duration not yet supported. Contact the author if you have such a sample file.\n"));

    return;
  }

  // calc pts:
  s            = 0;
  uint64_t pts = 0;

  for (j = 0; j < durmap_table.size(); ++j) {
    for (i = 0; i < durmap_table[j].number; ++i) {
      sample_table[s].pts  = pts;
      pts                 += durmap_table[j].duration;
      ++s;
    }
  }

  // calc sample offsets
  s = 0;
  for (j = 0; j < chunk_table.size(); ++j) {
    uint64_t chunk_pos = chunk_table[j].pos;

    for (i = 0; i < chunk_table[j].size; ++i) {
      sample_table[s].pos  = chunk_pos;
      chunk_pos           += sample_table[s].size;
      ++s;
    }
  }

  // calc pts/dts offsets
  for (j = 0; j < raw_frame_offset_table.size(); ++j) {
    size_t k;

    for (k = 0; k < raw_frame_offset_table[j].count; ++k)
      frame_offset_table.push_back(raw_frame_offset_table[j].offset);
  }

  mxdebug_if(m_debug_tables, boost::format(" Frame offset table: %1% entries\n")    % frame_offset_table.size());
  mxdebug_if(m_debug_tables, boost::format(" Sample table contents: %1% entries\n") % sample_table.size());
  for (i = 0; i < sample_table.size(); ++i) {
    qt_sample_t &sample = sample_table[i];
    mxdebug_if(m_debug_tables, boost::format("   %1%: pts %2% size %3% pos %4%\n") % i % sample.pts % sample.size % sample.pos);
  }

  update_editlist_table(global_m_time_scale);
}

// Also taken from mplayer's demux_mov.c file.
void
qtmp4_demuxer_c::update_editlist_table(int64_t global_time_scale) {
  if (editlist_table.empty())
    return;

  size_t frame = 0, e_pts = 0, i;

  int64_t min_editlist_pts = editlist_table.front().pos;
  for (i = 1; editlist_table.size() > i; ++i)
    min_editlist_pts = std::min(static_cast<int64_t>(editlist_table[i].pos), min_editlist_pts);

  uint64_t pts_offset = 0;
  if (('v' == type) && v_is_avc && !frame_offset_table.empty() && (frame_offset_table[0] <= min_editlist_pts))
    pts_offset = frame_offset_table[0];

  mxdebug_if(m_debug_tables, boost::format("Updating edit list table for track %1%; pts_offset = %2%\n") % id % pts_offset);

  for (i = 0; editlist_table.size() > i; ++i) {
    qt_editlist_t &el = editlist_table[i];
    uint64_t pts      = el.pos;

    if (pts < pts_offset) {
      // skip!
      el.frames  = 0;
      e_pts     += el.duration;
      continue;
    }

    unsigned int sample = 0;

    pts                -= pts_offset;
    el.start_frame      = frame;

    // find start sample
    for (; sample_table.size() > sample; ++sample)
      if (pts <= sample_table[sample].pts)
        break;

    el.start_sample  = sample;
    el.pts_offset    = ((int64_t)e_pts       * (int64_t)time_scale) / (int64_t)global_time_scale - (int64_t)sample_table[sample].pts;
    pts             += ((int64_t)el.duration * (int64_t)time_scale) / (int64_t)global_time_scale;
    e_pts           += el.duration;

    // find end sample
    for (; sample_table.size() > sample; ++sample)
      if (pts <= sample_table[sample].pts)
        break;

    el.frames  = sample - el.start_sample;
    frame     += el.frames;

    mxdebug_if(m_debug_tables, boost::format("  %1%: pts: %2%  1st_sample: %3%  frames: %4% (%|5$5.3f|s)  pts_offset: %6%\n") % i % el.pos % el.start_sample % el.frames % ((float)(el.duration) / (float)time_scale) % el.pts_offset);
  }
}

void
qtmp4_demuxer_c::build_index() {
  if (sample_size != 0)
    build_index_constant_sample_size_mode();
  else
    build_index_chunk_mode();
}

void
qtmp4_demuxer_c::build_index_constant_sample_size_mode() {
  size_t keyframe_table_idx  = 0;
  size_t keyframe_table_size = keyframe_table.size();

  size_t frame_idx;
  for (frame_idx = 0; frame_idx < chunk_table.size(); ++frame_idx) {
    uint64_t frame_size;

    if (1 != sample_size) {
      frame_size = chunk_table[frame_idx].size * sample_size;

    } else {
      frame_size = chunk_table[frame_idx].size;

      if ('a' == type) {
        sound_v1_stsd_atom_t *sound_stsd_atom = (sound_v1_stsd_atom_t *)a_stsd->get_buffer();
        if (get_uint16_be(&sound_stsd_atom->v0.version) == 1) {
          frame_size *= get_uint32_be(&sound_stsd_atom->v1.bytes_per_frame);
          frame_size /= get_uint32_be(&sound_stsd_atom->v1.samples_per_packet);
        } else
          frame_size  = frame_size * a_channels * get_uint16_be(&sound_stsd_atom->v0.sample_size) / 8;
      }
    }

    bool is_keyframe = false;
    if (keyframe_table.empty())
      is_keyframe = true;
    else if ((keyframe_table_idx < keyframe_table_size) && ((frame_idx + 1) == keyframe_table[keyframe_table_idx])) {
      is_keyframe = true;
      ++keyframe_table_idx;
    }

    m_index.push_back(qt_index_t(chunk_table[frame_idx].pos, frame_size, timecodes[frame_idx], durations[frame_idx], is_keyframe));
  }
}

void
qtmp4_demuxer_c::build_index_chunk_mode() {
  size_t keyframe_table_idx  = 0;
  size_t keyframe_table_size = keyframe_table.size();

  size_t frame_idx;
  for (frame_idx = 0; frame_idx < frame_indices.size(); ++frame_idx) {
    int act_frame_idx = frame_indices[frame_idx];

    bool is_keyframe  = false;
    if (keyframe_table.empty())
      is_keyframe = true;
    else if ((keyframe_table_idx < keyframe_table_size) && ((frame_idx + 1) == keyframe_table[keyframe_table_idx])) {
      is_keyframe = true;
      ++keyframe_table_idx;
    }

    m_index.push_back(qt_index_t(sample_table[act_frame_idx].pos, sample_table[act_frame_idx].size, timecodes[frame_idx], durations[frame_idx], is_keyframe));
  }
}

bool
qtmp4_demuxer_c::read_first_bytes(memory_cptr &buf,
                                  int num_bytes,
                                  mm_io_cptr in) {
  size_t buf_pos = 0;
  size_t idx_pos = 0;

  while ((0 < num_bytes) && (idx_pos < m_index.size())) {
    qt_index_t &index          = m_index[idx_pos];
    uint64_t num_bytes_to_read = std::min((int64_t)num_bytes, index.size);

    in->setFilePointer(index.file_pos);
    if (in->read(buf->get_buffer() + buf_pos, num_bytes_to_read) < num_bytes_to_read)
      return false;

    num_bytes -= num_bytes_to_read;
    buf_pos   += num_bytes_to_read;
    ++idx_pos;
  }

  return 0 == num_bytes;
}

bool
qtmp4_demuxer_c::is_audio()
  const {
  return 'a' == type;
}

bool
qtmp4_demuxer_c::is_video()
  const {
  return 'v' == type;
}
