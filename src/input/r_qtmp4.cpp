/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   Quicktime and MP4 reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
   The second half of the parse_headers() function after the
     "// process chunkmap:" comment was taken from mplayer's
     demux_mov.c file which is distributed under the GPL as well. Thanks to
     the original authors.
*/

#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_ZLIB_H)
#include <zlib.h>
#endif

#include "aac_common.h"
#include "avilib.h"
#include "common.h"
#include "hacks.h"
#include "matroska.h"
#include "p_aac.h"
#include "p_mp3.h"
#include "p_passthrough.h"
#include "p_pcm.h"
#include "p_video.h"
#include "r_qtmp4.h"

using namespace std;
using namespace libmatroska;

#define PFX "Quicktime/MP4 reader: "

#if defined(ARCH_BIGENDIAN)
#define BE2STR(a) ((char *)&a)[0], ((char *)&a)[1], ((char *)&a)[2], \
                  ((char *)&a)[3]
#define LE2STR(a) ((char *)&a)[3], ((char *)&a)[2], ((char *)&a)[1], \
                  ((char *)&a)[0]
#else
#define BE2STR(a) ((char *)&a)[3], ((char *)&a)[2], ((char *)&a)[1], \
                  ((char *)&a)[0]
#define LE2STR(a) ((char *)&a)[0], ((char *)&a)[1], ((char *)&a)[2], \
                  ((char *)&a)[3]
#endif

int
qtmp4_reader_c::probe_file(mm_io_c *in,
                           int64_t size) {
  uint32_t atom;
  uint64_t atom_size;

  if (size < 20)
    return 0;
  try {
    in->setFilePointer(0, seek_beginning);

    atom_size = in->read_uint32_be();
    atom = in->read_uint32_be();
    if (atom_size == 1)
      atom_size = in->read_uint64_be();

    mxverb(3, PFX "Atom: '%c%c%c%c'; size: %llu\n", BE2STR(atom), atom_size);

    if ((atom == FOURCC('m', 'o', 'o', 'v')) ||
        (atom == FOURCC('f', 't', 'y', 'p')) ||
        (atom == FOURCC('m', 'd', 'a', 't')))
        return 1;

  } catch (exception &ex) {
    return 0;
  }

  return 0;
}

qtmp4_reader_c::qtmp4_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  try {
    io = new mm_file_io_c(ti->fname);
    io->setFilePointer(0, seek_end);
    file_size = io->getFilePointer();
    io->setFilePointer(0, seek_beginning);
    if (!qtmp4_reader_c::probe_file(io, file_size))
      throw error_c(PFX "Source is not a valid Quicktime/MP4 file.");

    if (verbose)
      mxinfo(FMT_FN "Using the Quicktime/MP4 demultiplexer.\n",
             ti->fname.c_str());

    parse_headers();

  } catch (exception &ex) {
    throw error_c(PFX "Could not read the source file.");
  }
}

qtmp4_reader_c::~qtmp4_reader_c() {
  ti->private_data = NULL;

  delete io;
}

qt_atom_t
qtmp4_reader_c::read_atom() {
  qt_atom_t a;

  a.pos = io->getFilePointer();
  a.size = io->read_uint32_be();
  a.fourcc = io->read_uint32_be();
  a.hsize = 8;
  if (a.size == 1) {
    a.size = io->read_uint64_be();
    a.hsize += 8;
  } else if (a.size == 0)
    a.size = file_size - io->getFilePointer() + 8;
  if (a.size < a.hsize)
    mxerror(PFX "Invalid chunk size %llu at %llu.\n", a.size, a.pos);

  return a;
}

#define skip_atom() io->setFilePointer(atom.pos + atom.size)

void
qtmp4_reader_c::parse_headers() {
  uint32_t tmp, j, s, pts, last, idx;
  qt_atom_t atom;
  bool headers_parsed;
  int i;

  io->setFilePointer(0);

  headers_parsed = false;
  mdat_pos = -1;
  do {
    atom = read_atom();
    mxverb(2, PFX "'%c%c%c%c' atom, size %lld, at %lld\n", BE2STR(atom),
           atom.size, atom.pos);

    if (atom.fourcc == FOURCC('f', 't', 'y', 'p')) {
      tmp = io->read_uint32_be();
      mxverb(2, PFX "  File type major brand: %c%c%c%c\n", BE2STR(tmp));
      tmp = io->read_uint32_be();
      mxverb(2, PFX "  File type minor brand: 0x%08x\n", tmp);
      for (i = 0; i < ((atom.size - 16) / 4); i++) {
        tmp = io->read_uint32_be();
        mxverb(2, PFX "  File type compatible brands #%d: %c%c%c%c\n", i,
               BE2STR(tmp));
      }

    } else if (atom.fourcc == FOURCC('m', 'o', 'o', 'v')) {
      handle_moov_atom(atom.to_parent(), 0);
      headers_parsed = true;

    } else if (atom.fourcc == FOURCC('w', 'i', 'd', 'e')) {
      skip_atom();

    } else if (atom.fourcc == FOURCC('m', 'd', 'a', 't')) {
      mdat_pos = io->getFilePointer();
      mdat_size = atom.size;
      skip_atom();

    } else if ((atom.fourcc == FOURCC('f', 'r', 'e', 'e')) ||
               (atom.fourcc == FOURCC('s', 'k', 'i', 'p')) ||
               (atom.fourcc == FOURCC('j', 'u', 'n', 'k')) ||
               (atom.fourcc == FOURCC('p', 'n', 'o', 't')) ||
               (atom.fourcc == FOURCC('P', 'I', 'C', 'T'))) {
      skip_atom();

    }

  } while (!io->eof() && (!headers_parsed || (mdat_pos == -1)));

  if (!headers_parsed)
    mxerror(PFX "Have not found any header atoms.\n");
  if (mdat_pos == -1)
    mxerror(PFX "Have not found the 'mdat' atom. No movie data found.\n");

  io->setFilePointer(mdat_pos);

  for (idx = 0; idx < demuxers.size(); idx++) {
    qtmp4_demuxer_ptr &dmx = demuxers[idx];

    if (((dmx->type == 'v') &&
         strncasecmp(dmx->fourcc, "SVQ", 3) &&
         strncasecmp(dmx->fourcc, "cvid", 4) &&
         strncasecmp(dmx->fourcc, "rle ", 4) &&
         strncasecmp(dmx->fourcc, "mp4v", 4) &&
         strncasecmp(dmx->fourcc, "avc1", 4))
        ||
        ((dmx->type == 'a') &&
         strncasecmp(dmx->fourcc, "QDM", 3) &&
         strncasecmp(dmx->fourcc, "MP4A", 4) &&
         strncasecmp(dmx->fourcc, "twos", 4) &&
         strncasecmp(dmx->fourcc, "swot", 4))) {
      mxwarn(PFX "Unknown/unsupported FourCC '%.4s' for track %u.\n",
             dmx->fourcc, dmx->id);

      continue;
    }

    if ((dmx->type == 'a') &&
        !strncasecmp(dmx->fourcc, "MP4A", 4)) {
      if ((dmx->esds.object_type_id != MP4OTI_MPEG4Audio) && // AAC..
          (dmx->esds.object_type_id != MP4OTI_MPEG2AudioMain) &&
          (dmx->esds.object_type_id != MP4OTI_MPEG2AudioLowComplexity) &&
          (dmx->esds.object_type_id != MP4OTI_MPEG2AudioScaleableSamplingRate)
          &&
          (dmx->esds.object_type_id != MP4OTI_MPEG2AudioPart3) && // MP3...
          (dmx->esds.object_type_id != MP4OTI_MPEG1Audio)) {
        mxwarn(PFX "The audio track %u is using an unsupported 'object type "
               "id' of %u in the 'esds' atom. Skipping this track.\n",
               dmx->id, dmx->esds.object_type_id);
        continue;
      }

      if (((dmx->esds.object_type_id == MP4OTI_MPEG4Audio) || // AAC..
           (dmx->esds.object_type_id == MP4OTI_MPEG2AudioMain) ||
           (dmx->esds.object_type_id == MP4OTI_MPEG2AudioLowComplexity) ||
           (dmx->esds.object_type_id ==
            MP4OTI_MPEG2AudioScaleableSamplingRate)) &&
          (dmx->esds.decoder_config == NULL)) {
        mxwarn(PFX "The AAC track %u is missing the esds atom/the decoder "
               "config. Skipping this track.\n", dmx->id);
        continue;
      }
    }

    if (dmx->type == 'v') {
      if ((dmx->v_width == 0) || (dmx->v_height == 0) ||
          ((dmx->fourcc[0] == 0) && (dmx->fourcc[1] == 0) &&
           (dmx->fourcc[2] == 0) && (dmx->fourcc[3] == 0))) {
        mxwarn(PFX "Track %u is missing some data. Broken header atoms?\n",
               dmx->id);
        continue;
      }
      if (!strncasecmp(dmx->fourcc, "mp4v", 4)) {
        if (!dmx->esds_parsed) {
          mxwarn(PFX "The video track %u is missing the ESDS atom. "
                 "Skipping this track.\n", dmx->id);
          continue;
        }

        // The MP4 container can also contain MPEG1 and MPEG2 encoded
        // video. The object type ID in the ESDS tells the demuxer what
        // it is. So let's check for those.
        // If the FourCC is unmodified then MPEG4 is assumed.
        if ((dmx->esds.object_type_id == MP4OTI_MPEG2VisualSimple) ||
            (dmx->esds.object_type_id == MP4OTI_MPEG2VisualMain) ||
            (dmx->esds.object_type_id == MP4OTI_MPEG2VisualSNR) ||
            (dmx->esds.object_type_id == MP4OTI_MPEG2VisualSpatial) ||
            (dmx->esds.object_type_id == MP4OTI_MPEG2VisualHigh) ||
            (dmx->esds.object_type_id == MP4OTI_MPEG2Visual422))
          memcpy(dmx->fourcc, "mpg2", 4);
        else if (dmx->esds.object_type_id == MP4OTI_MPEG1Visual)
          memcpy(dmx->fourcc, "mpg1", 4);
        else {
          // This is MPEG4 video, and we need header data for it.
          if (dmx->esds.decoder_config == NULL) {
            mxwarn(PFX "MPEG4 track %u is missing the esds atom/the decoder "
                   "config. Skipping this track.\n", dmx->id);
            continue;
          }
        }

      } else if (!strncasecmp(dmx->fourcc, "avc1", 4)) {
        if ((dmx->priv == NULL) || (dmx->priv_size < 4)) {
          mxwarn(PFX "MPEG4 part 10/AVC track %u is missing its decoder "
                 "config. Skipping this track.\n", dmx->id);
          continue;
        }
      }
    }

    if ((dmx->type == 'a') &&
        ((dmx->a_channels == 0) || (dmx->a_samplerate == 0.0))) {
      mxwarn(PFX "Track %u is missing some data. Broken header atoms?\n",
             dmx->id);
      continue;
    }

    if (dmx->type == '?') {
      mxwarn(PFX "Track %u has an unknown type.\n", dmx->id);
      continue;
    }

    dmx->ok = true;

    last = dmx->chunk_table.size();

    // process chunkmap:
    i = dmx->chunkmap_table.size();
    while (i > 0) {
      i--;
      for (j = dmx->chunkmap_table[i].first_chunk; j < last; j++) {
        dmx->chunk_table[j].desc =
          dmx->chunkmap_table[i].sample_description_id;
        dmx->chunk_table[j].size = dmx->chunkmap_table[i].samples_per_chunk;
      }
      last = dmx->chunkmap_table[i].first_chunk;
    }

    // calc pts of chunks:
    s = 0;
    for (j = 0; j < dmx->chunk_table.size(); j++) {
      dmx->chunk_table[j].samples = s;
      s += dmx->chunk_table[j].size;
    }

    // workaround for fixed-size video frames (dv and uncompressed)
    if ((dmx->sample_table.size() == 0) && (dmx->type != 'a')) {
      for (i = 0; i < s; i++) {
        qt_sample_t sample;

        sample.size = dmx->sample_size;
        dmx->sample_table.push_back(sample);
      }
      dmx->sample_size = 0;
    }

    if (dmx->sample_table.size() == 0) {
      // constant sampesize
      if ((dmx->durmap_table.size() == 1) ||
          ((dmx->durmap_table.size() == 2) &&
           (dmx->durmap_table[1].number == 1)))
        dmx->duration = dmx->durmap_table[0].duration;
      else
        mxerror(PFX "Constant samplesize & variable duration not yet "
                "supported. Contact the author if you have such a sample "
                "file.\n");
      continue;
    }

    // calc pts:
    s = 0;
    pts = 0;
    for (j = 0; j < dmx->durmap_table.size(); j++) {
      for (i = 0; i < dmx->durmap_table[j].number; i++) {
        dmx->sample_table[s].pts = pts;
        s++;
        pts += dmx->durmap_table[j].duration;
      }
    }

    // calc sample offsets
    s = 0;
    for (j = 0; j < dmx->chunk_table.size(); j++) {
      uint64_t pos = dmx->chunk_table[j].pos;

      for (i = 0; i < dmx->chunk_table[j].size; i++) {
        dmx->sample_table[s].pos = pos;
        pos += dmx->sample_table[s].size;
        s++;
      }
    }

    // calc pts/dts offsets
    for (j = 0; j < dmx->raw_frame_offset_table.size(); j++) {
      int k;

      for (k = 0; k < dmx->raw_frame_offset_table[j].count; k++)
        dmx->frame_offset_table.
          push_back(dmx->raw_frame_offset_table[j].offset);
    }
    mxverb(3, PFX "Frame offset table: %u entries\n",
           dmx->frame_offset_table.size());
  }

  mxverb(2, PFX "Number of valid tracks found: %u\n", demuxers.size());
}

void
qtmp4_reader_c::parse_header_priv_atoms(qtmp4_demuxer_ptr &dmx,
                                        unsigned char *mem,
                                        int size,
                                        int level) {
  mm_mem_io_c mio(mem, size);

  try {
    while (!mio.eof()) {
      uint32_t add_atom, add_atom_size;

      add_atom_size = mio.read_uint32_be();
      add_atom = mio.read_uint32_be();
      add_atom_size -= 8;
      mxverb(2, PFX "%*s%s private data size: %u, type: "
             "'%c%c%c%c'\n", (level + 1) * 2, "", dmx->type == 'a' ? "Audio" :
             "Video", add_atom_size + 8, BE2STR(add_atom));
      if (dmx->priv == NULL) {
        uint32_t patom_size, patom;

        dmx->priv_size = add_atom_size;
        dmx->priv = (unsigned char *)safemalloc(dmx->priv_size);
        if (mio.read(dmx->priv, dmx->priv_size) != dmx->priv_size)
          throw exception();

        mm_mem_io_c memio(dmx->priv, dmx->priv_size);

        if (add_atom == FOURCC('e', 's', 'd', 's')) {
          if (!dmx->esds_parsed)
            dmx->esds_parsed = parse_esds_atom(memio, dmx, level + 1);
        } else {
          while (!memio.eof()) {
            patom_size = memio.read_uint32_be();
            if (patom_size <= 8)
              break;
            patom = memio.read_uint32_be();
            mxverb(2, PFX "%*sAtom size: %u, atom: '%c%c%c%c'\n",
                   (level + 2) * 2, "", patom_size, BE2STR(patom));
            if ((patom == FOURCC('e', 's', 'd', 's')) && !dmx->esds_parsed) {
              memio.save_pos();
              dmx->esds_parsed = parse_esds_atom(memio, dmx, level + 2);
              memio.restore_pos();
            }
            memio.skip(patom_size - 8);
          }
        }
      } else
        mio.skip(add_atom_size);
    }
  } catch(...) {
  }
}

void
qtmp4_reader_c::handle_cmov_atom(qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom;

    atom = read_atom();
    mxverb(2, PFX "%*s'%c%c%c%c' atom, size %lld, at %lld\n", 2 * level,
           "", BE2STR(atom.fourcc), atom.size, atom.pos);

    if (atom.fourcc == FOURCC('d', 'c', 'o', 'm'))
      handle_dcom_atom(atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('c', 'm', 'v', 'd'))
      handle_cmvd_atom(atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_cmvd_atom(qt_atom_t atom,
                                 int level) {
#if defined(HAVE_ZLIB_H)
  uint32_t moov_size, cmov_size;
  unsigned char *moov_buf, *cmov_buf;
  int zret;
  z_stream zs;
  mm_io_c *old_io;

  moov_size = io->read_uint32_be();
  mxverb(2, PFX "%*sUncompressed size: %u\n", (level + 1) * 2, "",
         moov_size);

  if (compression_algorithm != FOURCC('z', 'l', 'i', 'b'))
    mxerror(PFX "This file uses compressed headers with an "
            "unknown or unsupported compression algorithm '%c%c%c%c'. "
            "Aborting.\n", BE2STR(compression_algorithm));

  old_io = io;
  cmov_size = atom.size - atom.hsize;
  cmov_buf = (unsigned char *)safemalloc(cmov_size);
  moov_buf = (unsigned char *)safemalloc(moov_size + 16);

  if (io->read(cmov_buf, cmov_size) != cmov_size)
    throw exception();

  zs.zalloc = (alloc_func)NULL;
  zs.zfree = (free_func)NULL;
  zs.opaque = (voidpf)NULL;
  zs.next_in = cmov_buf;
  zs.avail_in = cmov_size;
  zs.next_out = moov_buf;
  zs.avail_out = moov_size;

  zret = inflateInit(&zs);
  if (zret != Z_OK)
    mxerror(PFX "This file uses compressed headers, but the zlib "
            "library could not be initialized. Error code from zlib: "
            "%d. Aborting.\n", zret);

  zret = inflate(&zs, Z_NO_FLUSH);
  if ((zret != Z_OK) && (zret != Z_STREAM_END))
    mxerror(PFX "This file uses compressed headers, but they could not "
            "be uncompressed. Error code from zlib: %d. Aborting.\n",
            zret);

  if (moov_size != zs.total_out)
    mxwarn(PFX "This file uses compressed headers, but the expected "
           "uncompressed size (%u) was not what is available after "
           "uncompressing (%lu).\n", moov_size, zs.total_out);
  zret = inflateEnd(&zs);

  io = new mm_mem_io_c(moov_buf, zs.total_out);
  while (!io->eof()) {
    qt_atom_t next_atom;

    next_atom = read_atom();
    mxverb(2, PFX "%*s'%c%c%c%c' atom at %lld\n", (level + 1) * 2, "",
           BE2STR(next_atom.fourcc), next_atom.pos);

    if (next_atom.fourcc == FOURCC('m', 'o', 'o', 'v'))
      handle_moov_atom(next_atom.to_parent(), level + 1);

    io->setFilePointer(next_atom.pos + next_atom.size);
  }
  delete io;
  io = old_io;
  safefree(moov_buf);
  safefree(cmov_buf);
#else // HAVE_ZLIB_H
  mxerror("mkvmerge was not compiled with zlib. Compressed headers "
          "in QuickTime/MP4 files are therefore not supported.\n");
#endif // HAVE_ZLIB_H
}

void
qtmp4_reader_c::handle_ctts_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  uint32_t count, i;

  io->skip(1 + 3);        // version & flags
  count = io->read_uint32_be();
  mxverb(2, PFX "%*sFrame offset table: %u raw entries\n", level * 2, "",
         count);
  for (i = 0; i < count; i++) {
    qt_frame_offset_t frame_offset;

    frame_offset.count = io->read_uint32_be();
    frame_offset.offset = io->read_uint32_be();
    new_dmx->raw_frame_offset_table.push_back(frame_offset);
    mxverb(3, PFX "%*s%u: count %u offset %u\n", (level + 1) * 2, "",
           i, frame_offset.count, frame_offset.offset);
  }
}

void
qtmp4_reader_c::handle_dcom_atom(qt_atom_t atom,
                                 int level) {
  uint32_t algo;

  algo = io->read_uint32_be();
  mxverb(2, PFX "%*sCompression algorithm: %c%c%c%c\n", level * 2, "",
         BE2STR(algo));
  compression_algorithm = algo;
}

void
qtmp4_reader_c::handle_hdlr_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  hdlr_atom_t hdlr;

  if (atom.size < sizeof(hdlr_atom_t))
    mxerror(PFX "'hdlr' atom is too small. Expected size: >= %d. Actual "
            "size: %lld.\n", sizeof(hdlr_atom_t), atom.size);
  if (io->read(&hdlr, sizeof(hdlr_atom_t)) != sizeof(hdlr_atom_t))
    throw exception();
  mxverb(2, PFX "%*s Component type: %.4s subtype: %.4s\n",
         level * 2, "", (char *)&hdlr.type, (char *)&hdlr.subtype);
  switch (get_uint32_be(&hdlr.subtype)) {
    case FOURCC('s', 'o', 'u', 'n'):
      new_dmx->type = 'a';
      break;
    case FOURCC('v', 'i', 'd', 'e'):
      new_dmx->type = 'v';
      break;
  }
}

void
qtmp4_reader_c::handle_mdhd_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  mdhd_atom_t mdhd;

  if (atom.size < sizeof(mdhd_atom_t))
    mxerror(PFX "'mdhd' atom is too small. Expected size: >= %d. Actual "
            "size: %lld.\n", sizeof(mdhd_atom_t), atom.size);
  if (io->read(&mdhd, sizeof(mdhd_atom_t)) != sizeof(mdhd_atom_t))
    throw exception();
  mxverb(2, PFX "%*s Time scale: %u, duration: %u\n", level * 2, "",
         get_uint32_be(&mdhd.time_scale),
         get_uint32_be(&mdhd.duration));
  new_dmx->timescale = get_uint32_be(&mdhd.time_scale);
  new_dmx->global_duration = get_uint32_be(&mdhd.duration);
}

void
qtmp4_reader_c::handle_mdia_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom;

    atom = read_atom();
    mxverb(2, PFX "%*s'%c%c%c%c' atom, size %lld, at %lld\n", 2 * level,
           "", BE2STR(atom.fourcc), atom.size, atom.pos);

    if (atom.fourcc == FOURCC('m', 'd', 'h', 'd'))
      handle_mdhd_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('h', 'd', 'l', 'r'))
      handle_hdlr_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('m', 'i', 'n', 'f'))
      handle_minf_atom(new_dmx, atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_minf_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom;

    atom = read_atom();
    mxverb(2, PFX "%*s'%c%c%c%c' atom, size %lld, at %lld\n", 2 * level,
           "", BE2STR(atom.fourcc), atom.size, atom.pos);

    if (atom.fourcc == FOURCC('h', 'd', 'l', 'r'))
      handle_hdlr_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('s', 't', 'b', 'l'))
      handle_stbl_atom(new_dmx, atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_moov_atom(qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom;

    atom = read_atom();
    mxverb(2, PFX "%*s'%c%c%c%c' atom, size %lld, at %lld\n", 2 * level,
           "", BE2STR(atom.fourcc), atom.size, atom.pos);

    if (atom.fourcc == FOURCC('c', 'm', 'o', 'v')) {
      compression_algorithm = 0;
      handle_cmov_atom(atom.to_parent(), level + 1);

    } else if (atom.fourcc == FOURCC('m', 'v', 'h', 'd'))
      handle_mvhd_atom(atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('t', 'r', 'a', 'k')) {
      qtmp4_demuxer_ptr new_dmx(new qtmp4_demuxer_t);

      handle_trak_atom(new_dmx, atom.to_parent(), level + 1);
      if ((new_dmx->type != '?') &&
          ((new_dmx->fourcc[0] != 0) || (new_dmx->fourcc[1] != 0) ||
           (new_dmx->fourcc[2] != 0) || (new_dmx->fourcc[3] != 0)))
        demuxers.push_back(new_dmx);
    }

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_mvhd_atom(qt_atom_t atom,
                                 int level) {
  mvhd_atom_t mvhd;

  if ((atom.size - atom.hsize) < sizeof(mvhd_atom_t))
    mxerror(PFX "'mvhd' atom is too small. Expected size: >= %d. Actual "
            "size: %lld.\n", sizeof(mvhd_atom_t), atom.size - atom.hsize);
  if (io->read(&mvhd, sizeof(mvhd_atom_t)) != sizeof(mvhd_atom_t))
    throw exception();
  mxverb(2, PFX "%*s Time scale: %u\n", level * 2, "",
         get_uint32_be(&mvhd.time_scale));
}

void
qtmp4_reader_c::handle_stbl_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom;

    atom = read_atom();
    mxverb(2, PFX "%*s'%c%c%c%c' atom, size %lld, at %lld\n", 2 * level,
           "", BE2STR(atom.fourcc), atom.size, atom.pos);

    if (atom.fourcc == FOURCC('s', 't', 't', 's'))
      handle_stts_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('s', 't', 's', 'd'))
      handle_stsd_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('s', 't', 's', 's'))
      handle_stss_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('s', 't', 's', 'c'))
      handle_stsc_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('s', 't', 's', 'z'))
      handle_stsz_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('s', 't', 'c', 'o'))
      handle_stco_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('c', 't', 't', 's'))
      handle_ctts_atom(new_dmx, atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_stco_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  uint32_t count, i;

  io->skip(1 + 3);        // version & flags
  count = io->read_uint32_be();

  for (i = 0; i < count; i++) {
    qt_chunk_t chunk;

    chunk.pos = io->read_uint32_be();
    new_dmx->chunk_table.push_back(chunk);
  }

  mxverb(2, PFX "%*sChunk offset table: %u entries\n", level * 2, "", count);
}

void
qtmp4_reader_c::handle_stsc_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  uint32_t count, i;

  io->skip(1 + 3);        // version & flags
  count = io->read_uint32_be();
  for (i = 0; i < count; i++) {
    qt_chunkmap_t chunkmap;

    chunkmap.first_chunk = io->read_uint32_be() - 1;
    chunkmap.samples_per_chunk = io->read_uint32_be();
    chunkmap.sample_description_id = io->read_uint32_be();
    new_dmx->chunkmap_table.push_back(chunkmap);
  }

  mxverb(2, PFX "%*sSample to chunk/chunkmap table: %u entries\n",
         level * 2, "", count);
}

void
qtmp4_reader_c::handle_stsd_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  uint32_t count, i, size, tmp, stsd_size;
  int64_t pos;
  sound_v1_stsd_atom_t sv1_stsd;
  video_stsd_atom_t v_stsd;
  unsigned char *priv;

  stsd_size = 0;
  io->skip(1 + 3);        // version & flags
  count = io->read_uint32_be();
  for (i = 0; i < count; i++) {
    pos = io->getFilePointer();
    size = io->read_uint32_be();
    priv = (unsigned char *)safemalloc(size);
    if (io->read(priv, size) != size)
      mxerror(PFX "Could not read the stream description atom for "
              "track ID %u.\n", new_dmx->id);

    if (new_dmx->type == 'a') {
      if (size < sizeof(sound_v0_stsd_atom_t))
        mxerror(PFX "Could not read the sound description atom for "
                "track ID %u.\n", new_dmx->id);

      memcpy(&sv1_stsd, priv, sizeof(sound_v0_stsd_atom_t));
      if (new_dmx->fourcc[0] != 0)
        mxwarn(PFX "Track ID %u has more than one FourCC. Only using "
               "the first one (%.4s) and not this one (%.4s).\n",
               new_dmx->id, new_dmx->fourcc, sv1_stsd.v0.base.fourcc);
      else
        memcpy(new_dmx->fourcc, sv1_stsd.v0.base.fourcc, 4);

      mxverb(2, PFX "%*sFourCC: %.4s, channels: %d, sample size: %d, "
             "compression id: %d, sample rate: 0x%08x, version: %u",
             level * 2, "", sv1_stsd.v0.base.fourcc,
             get_uint16_be(&sv1_stsd.v0.channels),
             get_uint16_be(&sv1_stsd.v0.sample_size),
             get_uint16_be(&sv1_stsd.v0.compression_id),
             get_uint32_be(&sv1_stsd.v0.sample_rate),
             get_uint16_be(&sv1_stsd.v0.version));
      if (get_uint16_be(&sv1_stsd.v0.version) == 1) {
        if ((size < sizeof(sound_v1_stsd_atom_t)) ||
            (io->read(&sv1_stsd.v1, sizeof(sv1_stsd.v1)) !=
             sizeof(sv1_stsd.v1)))
          mxerror(PFX "Could not read the extended sound description "
                  "atom for track ID %u.\n", new_dmx->id);
        mxverb(2, ", samples per packet: %u, bytes per packet: %u, "
               "bytes per frame: %u, bytes_per_sample: %u",
               get_uint32_be(&sv1_stsd.v1.samples_per_packet),
               get_uint32_be(&sv1_stsd.v1.bytes_per_packet),
               get_uint32_be(&sv1_stsd.v1.bytes_per_frame),
               get_uint32_be(&sv1_stsd.v1.bytes_per_sample));
      }
      mxverb(2, "\n");

      new_dmx->a_channels = get_uint16_be(&sv1_stsd.v0.channels);
      new_dmx->a_bitdepth = get_uint16_be(&sv1_stsd.v0.sample_size);
      tmp = get_uint32_be(&sv1_stsd.v0.sample_rate);
      new_dmx->a_samplerate = (float)((tmp & 0xffff0000) >> 16) +
        (float)(tmp & 0x0000ffff) / 65536.0;

      if (get_uint16_be(&sv1_stsd.v0.version) == 1)
        stsd_size = sizeof(sound_v1_stsd_atom_t);
      else
        stsd_size = sizeof(sound_v0_stsd_atom_t);

      memcpy(&new_dmx->a_stsd, &sv1_stsd, sizeof(sound_v1_stsd_atom_t));

    } else if (new_dmx->type == 'v') {
      if (size < sizeof(video_stsd_atom_t))
        mxerror(PFX "Could not read the video description atom for "
                "track ID %u.\n", new_dmx->id);

      memcpy(&v_stsd, priv, sizeof(video_stsd_atom_t));
      new_dmx->v_stsd = (unsigned char *)safememdup(priv, size);
      new_dmx->v_stsd_size = size;
      if (new_dmx->fourcc[0] != 0)
        mxwarn(PFX "Track ID %u has more than one FourCC. Only using "
               "the first one (%.4s) and not this one (%.4s).\n",
               new_dmx->id, new_dmx->fourcc, v_stsd.base.fourcc);
      else
        memcpy(new_dmx->fourcc, v_stsd.base.fourcc, 4);

      mxverb(2, PFX "%*sFourCC: %.4s, width: %u, height: %u, depth: %u"
             "\n", level * 2, "", v_stsd.base.fourcc,
             get_uint16_be(&v_stsd.width), get_uint16_be(&v_stsd.height),
             get_uint16_be(&v_stsd.depth));

      new_dmx->v_width = get_uint16_be(&v_stsd.width);
      new_dmx->v_height = get_uint16_be(&v_stsd.height);
      new_dmx->v_bitdepth = get_uint16_be(&v_stsd.depth);
      stsd_size = sizeof(video_stsd_atom_t);
    }

    if ((stsd_size > 0) && (stsd_size < size))
      parse_header_priv_atoms(new_dmx, priv + stsd_size, size -
                              stsd_size, level);
    safefree(priv);

    io->setFilePointer(pos + size);
  }
}

void
qtmp4_reader_c::handle_stss_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  uint32_t count, i;

  io->skip(1 + 3);        // version & flags
  count = io->read_uint32_be();

  for (i = 0; i < count; i++)
    new_dmx->keyframe_table.push_back(io->read_uint32_be());

  mxverb(2, PFX "%*sSync/keyframe table: %u entries\n", level * 2, "", count);
}

void
qtmp4_reader_c::handle_stsz_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  uint32_t count, i, sample_size;

  io->skip(1 + 3);        // version & flags
  sample_size = io->read_uint32_be();
  count = io->read_uint32_be();
  if (sample_size == 0) {
    for (i = 0; i < count; i++) {
      qt_sample_t sample;

      sample.size = io->read_uint32_be();
      new_dmx->sample_table.push_back(sample);
    }

    mxverb(2, PFX "%*sSample size table: %u entries\n", level * 2, "", count);

  } else {
    new_dmx->sample_size = sample_size;
    mxverb(2, PFX "%*sSample size table; global sample size: %u\n",
           level * 2, "", sample_size);
  }
}

void
qtmp4_reader_c::handle_sttd_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  uint32_t count, i;

  io->skip(1 + 3);        // version & flags
  count = io->read_uint32_be();
  for (i = 0; i < count; i++) {
    qt_durmap_t durmap;

    durmap.number = io->read_uint32_be();
    durmap.duration = io->read_uint32_be();
    new_dmx->durmap_table.push_back(durmap);
  }

  mxverb(2, PFX "%*sSample duration table: %u entries\n", level * 2, "",
         count);
}

void
qtmp4_reader_c::handle_stts_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  uint32_t count, i;

  io->skip(1 + 3);        // version & flags
  count = io->read_uint32_be();
  for (i = 0; i < count; i++) {
    qt_durmap_t durmap;

    durmap.number = io->read_uint32_be();
    durmap.duration = io->read_uint32_be();
    new_dmx->durmap_table.push_back(durmap);
  }

  mxverb(2, PFX "%*sSample duration table: %u entries\n", level * 2, "",
         count);
}

void
qtmp4_reader_c::handle_tkhd_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t atom,
                                 int level) {
  tkhd_atom_t tkhd;

  if (atom.size < sizeof(tkhd_atom_t))
    mxerror(PFX "'tkhd' atom is too small. Expected size: >= %d. Actual "
            "size: %lld.\n", sizeof(tkhd_atom_t), atom.size);
  if (io->read(&tkhd, sizeof(tkhd_atom_t)) != sizeof(tkhd_atom_t))
    throw exception();
  mxverb(2, PFX "%*s Track ID: %u\n", level * 2, "",
         get_uint32_be(&tkhd.track_id));
  new_dmx->id = get_uint32_be(&tkhd.track_id);
}

void
qtmp4_reader_c::handle_trak_atom(qtmp4_demuxer_ptr &new_dmx,
                                 qt_atom_t parent,
                                 int level) {
  while (parent.size > 0) {
    qt_atom_t atom;

    atom = read_atom();
    mxverb(2, PFX "%*s'%c%c%c%c' atom, size %lld, at %lld\n", 2 * level,
           "", BE2STR(atom.fourcc), atom.size, atom.pos);

    if (atom.fourcc == FOURCC('t', 'k', 'h', 'd'))
      handle_tkhd_atom(new_dmx, atom.to_parent(), level + 1);

    else if (atom.fourcc == FOURCC('m', 'd', 'i', 'a'))
      handle_mdia_atom(new_dmx, atom.to_parent(), level + 1);

    skip_atom();
    parent.size -= atom.size;
  }
}

void
qtmp4_reader_c::handle_video_with_bframes(qtmp4_demuxer_ptr &dmx,
                                          int64_t timecode,
                                          int64_t duration,
                                          bool is_keyframe,
                                          memory_c &mem) {
  int64_t bref, fref, old_timecode;
  bool is_bframe;

  bref = is_keyframe ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC;
  fref = VFT_NOBFRAME;
  is_bframe = false;

  if (dmx->pos == 0)
    dmx->v_dts_offset = (int64_t)dmx->frame_offset_table[0] *
      1000000000 / dmx->timescale;

  old_timecode = timecode;
  timecode += (int64_t)dmx->frame_offset_table[dmx->pos] *
    1000000000 / dmx->timescale - dmx->v_dts_offset;

  if (!is_keyframe) {
    if (dmx->references.size() == 0)
      mxerror(FMT_FN "The video track does not start with a key "
              "frame but contains B frames. This is not supported.\n",
              ti->fname.c_str());

    // The current frame is a B frame if the timecode is going
    // backwards. Otherwise it's a P frame.
    is_bframe = timecode < dmx->max_timecode;

    if (is_bframe) {
      // A B frame references the first frame in the queue backwards and
      // the second one forwards.
      bref = dmx->references[0];
      if (dmx->references.size() < 2)
        mxerror(FMT_FN "The video track does not start with a key "
                "frame and a P frame but contains B frames. This is not "
                "supported.\n", ti->fname.c_str());
      if (dmx->avc_use_bframes)
        fref = dmx->references[1];

    } else {
      // This is a P frame. At the moment it references the second stored frame
      // or the first one if there's only one. Then store it and remove an old
      // reference if we have more than two references stored.
      bref = dmx->references.back();
      dmx->references.push_back(timecode);
      if (dmx->references.size() > 2)
        dmx->references.pop_front();

    }

  } else {
    // This is an I frame. Store it and remove an old reference if we have
    // more than two references stored.
    dmx->references.push_back(timecode);
    if (dmx->references.size() > 2)
      dmx->references.pop_front();

  }

  mxverb(3, "\n" PFX "frame %u: key %d b %d %lld + %lld = %lld for %lld "
         "   ;; bref %lld fref %lld\n", dmx->pos, is_keyframe,
         is_bframe,
         (old_timecode + dmx->v_dts_offset) / 1000000,
         (int64_t)dmx->frame_offset_table[dmx->pos] * 1000 /
         dmx->timescale,
         (old_timecode + (int64_t)dmx->frame_offset_table[dmx->pos] *
          1000000000 / dmx->timescale) / 1000000,
         timecode / 1000000, bref, fref);

  PTZR(dmx->ptzr)->process(mem, timecode, duration, bref, fref);

  if (timecode > dmx->max_timecode)
    dmx->max_timecode = timecode;
}

file_status_e
qtmp4_reader_c::read(generic_packetizer_c *ptzr,
                     bool force) {
  uint32_t i, k, frame, frame_size;
  bool chunks_left, is_keyframe;
  int64_t timecode, duration;
  unsigned char *buffer;

  frame = 0;
  chunks_left = false;
  for (i = 0; i < demuxers.size(); i++) {
    qtmp4_demuxer_ptr &dmx = demuxers[i];

    if (PTZR(dmx->ptzr) != ptzr)
      continue;

    if (dmx->sample_size != 0) {
      if (dmx->pos >= dmx->chunk_table.size())
        continue;

      io->setFilePointer(dmx->chunk_table[dmx->pos].pos);
      timecode = 1000000000 *
        ((uint64_t)dmx->chunk_table[dmx->pos].samples *
         (uint64_t)dmx->duration) / (uint64_t)dmx->timescale;

      if (dmx->sample_size != 1) {
        if (!dmx->warning_printed) {
          mxwarn(PFX "Track %u: sample_size (%u) != 1.\n", dmx->id,
                 dmx->sample_size);
          dmx->warning_printed = true;
        }
        frame_size = dmx->chunk_table[dmx->pos].size * dmx->sample_size;
      } else
        frame_size = dmx->chunk_table[dmx->pos].size;

      if (dmx->type == 'a') {
        if (get_uint16_be(&dmx->a_stsd.v0.version) == 1) {
          frame_size *= get_uint32_be(&dmx->a_stsd.v1.bytes_per_frame);
          frame_size /= get_uint32_be(&dmx->a_stsd.v1.samples_per_packet);
        } else
          frame_size = frame_size * dmx->a_channels *
            get_uint16_be(&dmx->a_stsd.v0.sample_size) / 8;

      }

      if (dmx->keyframe_table.size() == 0)
        is_keyframe = true;
      else {
        is_keyframe = false;
        for (k = 0; k < dmx->keyframe_table.size(); k++)
          if (dmx->keyframe_table[k] == (frame + 1)) {
            is_keyframe = true;
            break;
          }
      }

      buffer = (unsigned char *)safemalloc(frame_size);
      if (io->read(buffer, frame_size) != frame_size) {
        mxwarn(PFX "Could not read chunk number %u/%u with size %u from "
               "position %lld. Aborting.\n", frame, dmx->chunk_table.size(),
               frame_size, dmx->chunk_table[dmx->pos].pos);
        safefree(buffer);
        flush_packetizers();

        return FILE_STATUS_DONE;
      }

      if ((dmx->pos + 1) < dmx->chunk_table.size())
        duration = 1000000000 *
          ((uint64_t)dmx->chunk_table[dmx->pos + 1].samples *
           (uint64_t)dmx->duration) /
          (uint64_t)dmx->timescale - timecode;
      else
        duration = dmx->avg_duration;
      dmx->avg_duration = (dmx->avg_duration * dmx->pos + duration) /
        (dmx->pos + 1);

      memory_c mem(buffer, frame_size, true);
      PTZR(dmx->ptzr)->process(mem, timecode, duration, is_keyframe ?
                               VFT_IFRAME : VFT_PFRAMEAUTOMATIC);
      dmx->pos++;

      if (dmx->pos < dmx->chunk_table.size())
        chunks_left = true;

    } else {
      if (dmx->pos >= dmx->sample_table.size())
        continue;

      frame = dmx->pos;

      timecode = (int64_t)dmx->sample_table[frame].pts * 1000000000 /
        dmx->timescale;
      if ((frame + 1) < dmx->sample_table.size())
        duration = (int64_t)dmx->sample_table[frame + 1].pts * 1000000000 /
          dmx->timescale - timecode;
      else
        duration = dmx->avg_duration;
      dmx->avg_duration = (dmx->avg_duration * frame + duration) /
        (frame + 1);

      if (dmx->keyframe_table.size() == 0)
        is_keyframe = true;
      else {
        is_keyframe = false;
        for (k = 0; k < dmx->keyframe_table.size(); k++)
          if (dmx->keyframe_table[k] == (frame + 1)) {
            is_keyframe = true;
            break;
          }
      }

      frame_size = dmx->sample_table[frame].size;
      if ((dmx->type == 'v') && (dmx->pos == 0) &&
          !strncasecmp(dmx->fourcc, "mp4v", 4) &&
          dmx->esds_parsed && (dmx->esds.decoder_config != NULL)) {
        buffer = (unsigned char *)
          safemalloc(frame_size + dmx->esds.decoder_config_len);
        memcpy(buffer, dmx->esds.decoder_config, dmx->esds.decoder_config_len);
        io->setFilePointer(dmx->sample_table[frame].pos);
        if (io->read(buffer + dmx->esds.decoder_config_len, frame_size) !=
            frame_size) {
          mxwarn(PFX "Could not read chunk number %u/%u with size %u from "
                 "position %lld. Aborting.\n", frame, dmx->chunk_table.size(),
                 frame_size, dmx->chunk_table[dmx->pos].pos);
          safefree(buffer);
          flush_packetizers();

          return FILE_STATUS_DONE;
        }
        frame_size += dmx->esds.decoder_config_len;
      } else {
        buffer = (unsigned char *)safemalloc(frame_size);
        io->setFilePointer(dmx->sample_table[frame].pos);
        if (io->read(buffer, frame_size) != frame_size) {
          mxwarn(PFX "Could not read chunk number %u/%u with size %u from "
                 "position %lld. Aborting.\n", frame, dmx->sample_table.size(),
                 frame_size, dmx->sample_table[frame].pos);
          safefree(buffer);
          flush_packetizers();

          return FILE_STATUS_DONE;
        }
      }

      memory_c mem(buffer, frame_size, true);
      if ((dmx->type == 'v') && (dmx->pos < dmx->frame_offset_table.size()) &&
          !strncasecmp(dmx->fourcc, "avc1", 4))
        handle_video_with_bframes(dmx, timecode, duration, is_keyframe, mem);

      else
        PTZR(dmx->ptzr)->process(mem, timecode, duration, is_keyframe ?
                                 VFT_IFRAME : VFT_PFRAMEAUTOMATIC);

      dmx->pos++;
      if (dmx->pos < dmx->sample_table.size())
        chunks_left = true;
    }

    if (chunks_left)
      return FILE_STATUS_MOREDATA;
    else {
      flush_packetizers();
      return FILE_STATUS_DONE;
    }
  }

  flush_packetizers();

  return FILE_STATUS_DONE;
}

uint32_t
qtmp4_reader_c::read_esds_descr_len(mm_mem_io_c &memio) {
  uint32_t len, num_bytes;
  uint8_t byte;

  len = 0;
  num_bytes = 0;
  do {
    byte = memio.read_uint8();
    num_bytes++;
    len = (len << 7) | (byte & 0x7f);
  } while (((byte & 0x80) == 0x80) && (num_bytes < 4));

  return len;
}

bool
qtmp4_reader_c::parse_esds_atom(mm_mem_io_c &memio,
                                qtmp4_demuxer_ptr &dmx,
                                int level) {
  uint32_t len;
  uint8_t tag;
  esds_t *e;
  int lsp;

  lsp = (level + 1) * 2;
  e = &dmx->esds;
  e->version = memio.read_uint8();
  e->flags = memio.read_uint24_be();
  mxverb(2, PFX "%*sesds: version: %u, flags: %u\n", lsp, "", e->version,
         e->flags);
  tag = memio.read_uint8();
  if (tag == MP4DT_ES) {
    len = read_esds_descr_len(memio);
    e->esid = memio.read_uint16_be();
    e->stream_priority = memio.read_uint8();
    mxverb(2, PFX "%*sesds: id: %u, stream priority: %u, len: %u\n", lsp, "",
           e->esid, (uint32_t)e->stream_priority, len);
  } else {
    e->esid = memio.read_uint16_be();
    mxverb(2, PFX "%*sesds: id: %u\n", lsp, "", e->esid);
  }

  tag = memio.read_uint8();
  if (tag != MP4DT_DEC_CONFIG) {
    mxverb(2, PFX "%*stag is not DEC_CONFIG (0x%02x) but 0x%02x.\n", lsp, "",
           MP4DT_DEC_CONFIG, (uint32_t)tag);
    return false;
  }

  len = read_esds_descr_len(memio);

  e->object_type_id = memio.read_uint8();
  e->stream_type = memio.read_uint8();
  e->buffer_size_db = memio.read_uint24_be();
  e->max_bitrate = memio.read_uint32_be();
  e->avg_bitrate = memio.read_uint32_be();
  mxverb(2, PFX "%*sesds: decoder config descriptor, len: %u, object_type_id: "
         "%u, stream_type: 0x%2x, buffer_size_db: %u, max_bitrate: %.3fkbit/s"
         ", avg_bitrate: %.3fkbit/s\n", lsp, "", len, e->object_type_id,
         e->stream_type, e->buffer_size_db, e->max_bitrate / 1000.0,
         e->avg_bitrate / 1000.0);

  e->decoder_config_len = 0;

  tag = memio.read_uint8();
  if (tag == MP4DT_DEC_SPECIFIC) {
    len = read_esds_descr_len(memio);
    e->decoder_config_len = len;
    e->decoder_config = (uint8_t *)safemalloc(len);
    if (memio.read(e->decoder_config, len) != len)
      throw exception();
    mxverb(2, PFX "%*sesds: decoder specific descriptor, len: %u\n", lsp, "",
           len);
    mxverb(3, PFX "%*sesds: dumping decoder specific descriptor\n", lsp + 2,
           "");
    mxhexdump(3, e->decoder_config, e->decoder_config_len);

    tag = memio.read_uint8();
  } else
    mxverb(2, PFX "%*stag is not DEC_SPECIFIC (0x%02x) but 0x%02x.\n", lsp, "",
           MP4DT_DEC_SPECIFIC, (uint32_t)tag);

  if (tag == MP4DT_SL_CONFIG) {
    len = read_esds_descr_len(memio);
    e->sl_config_len = len;
    e->sl_config = (uint8_t *)safemalloc(len);
    if (memio.read(e->sl_config, len) != len)
      throw exception();
    mxverb(2, PFX "%*sesds: sync layer config descriptor, len: %u\n", lsp, "",
           len);
  } else
    mxverb(2, PFX "%*stag is not SL_CONFIG (0x%02x) but 0x%02x.\n", lsp, "",
           MP4DT_SL_CONFIG, (uint32_t)tag);

  return true;
}

void
qtmp4_reader_c::create_packetizer(int64_t tid) {
  uint32_t i;
  qtmp4_demuxer_ptr dmx;
  passthrough_packetizer_c *ptzr;

  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->id == tid) {
      dmx = demuxers[i];
      break;
    }
  if (dmx.get() == NULL)
    return;

  if (dmx->ok && demuxing_requested(dmx->type, dmx->id) && (dmx->ptzr == -1)) {
    ti->id = dmx->id;
    if (dmx->type == 'v') {
      if (!strncasecmp(dmx->fourcc, "mp4v", 4)) {
        alBITMAPINFOHEADER *bih;

        bih = (alBITMAPINFOHEADER *)
          safemalloc(sizeof(alBITMAPINFOHEADER));
        memset(bih, 0, sizeof(alBITMAPINFOHEADER));
        put_uint32_le(&bih->bi_size, sizeof(alBITMAPINFOHEADER));
        put_uint32_le(&bih->bi_width, dmx->v_width);
        put_uint32_le(&bih->bi_height, dmx->v_height);
        put_uint16_le(&bih->bi_planes, 1);
        put_uint16_le(&bih->bi_bit_count, dmx->v_bitdepth);
        memcpy(&bih->bi_compression, "DIVX", 4);
        put_uint32_le(&bih->bi_size_image, get_uint32_le(&bih->bi_width) *
                   get_uint32_le(&bih->bi_height) * 3);
        ti->private_size = sizeof(alBITMAPINFOHEADER);
        ti->private_data = (unsigned char *)bih;
        dmx->ptzr =
          add_packetizer(new mpeg4_p2_video_packetizer_c(this, 0.0,
                                                         dmx->v_width,
                                                         dmx->v_height, false,
                                                         ti));
        safefree(bih);
        ti->private_data = NULL;
        mxinfo(FMT_TID "Using the MPEG-4 part 2 video output module.\n",
               ti->fname.c_str(), (int64_t)dmx->id);

      } else if (!strncasecmp(dmx->fourcc, "mpg1", 4) ||
                 !strncasecmp(dmx->fourcc, "mpg2", 4)) {
        int version;

        version = dmx->fourcc[3] - '0';
        dmx->ptzr =
          add_packetizer(new mpeg1_2_video_packetizer_c(this, version,
                                                        -1.0, dmx->v_width,
                                                        dmx->v_height, 0, 0,
                                                        false, ti));
        mxinfo(FMT_TID "Using the MPEG-%d video output module.\n",
               ti->fname.c_str(), (int64_t)dmx->id, version);

      } else if (!strncasecmp(dmx->fourcc, "avc1", 4)) {
        double fps;

        if (dmx->frame_offset_table.size() == 0)
          mxwarn(FMT_TID "The AVC video track is missing the 'CTTS' atom for "
                 "frame timecode offsets. However, AVC/h.264 allows frames "
                 "to have more than the traditional one (for P frames) or "
                 "two (for B frames) references to other frames. The "
                 "timecodes for such frames will be out-of-order, and the "
                 "'CTTS' atom is needed for getting the timecodes right. "
                 "As it is missing the timecodes for this track might be "
                 "wrong. You should watch the resulting file and make sure "
                 "that it looks like you expected it to.\n",
                 ti->fname.c_str(), (int64_t)dmx->id);

        fps = 0.0;
        if ((dmx->durmap_table.size() == 1) &&
            (dmx->durmap_table[0].duration != 0)) {
          if ((dmx->sample_size != 0) ||
              (dmx->frame_offset_table.size() == 0)) {
            // Constant FPS. Let's set the default duration.
            fps = (double)dmx->timescale /
              (double)dmx->durmap_table[0].duration;
            mxverb(3, PFX "fps1: %f\n", fps);

          } else if ((dmx->sample_table.size() > 0) &&
                     (dmx->sample_table.size() ==
                      dmx->frame_offset_table.size())) {
            // Constant FPS but the frame offsets are used. This one is
            // a bit trickier. I have to find the maximum timecode including
            // the frame offsets (for PTS/DTS conversion) and divide that
            // by the number of frames.
            int64_t max_tc;
            int i;

            max_tc = 0;

            for (i = 0; i < dmx->sample_table.size(); i++) {
              int64_t timecode;

              timecode = ((int64_t)dmx->sample_table[i].pts +
                          (int64_t)dmx->frame_offset_table[dmx->pos]) *
                1000000000 / dmx->timescale;
              if (timecode > max_tc)
                max_tc = timecode;
            }
            if (max_tc > 0)
              fps = (double)dmx->sample_table.size() * 1000000000.0 /
                (double)max_tc;

            mxverb(3, PFX "fps2: max_tc %lld s_t.s %u fps %f\n", max_tc,
                   dmx->sample_table.size(), fps);
          }
        }

        ti->private_size = dmx->priv_size;
        ti->private_data = dmx->priv;
        dmx->ptzr =
          add_packetizer(new mpeg4_p10_video_packetizer_c(this, fps,
                                                          dmx->v_width,
                                                          dmx->v_height, ti));
        ti->private_data = NULL;

        if (hack_engaged(ENGAGE_AVC_USE_BFRAMES))
          dmx->avc_use_bframes = true;
        else
          PTZR(dmx->ptzr)->relaxed_timecode_checking = true;
        mxinfo(FMT_TID "Using the MPEG-4 part 10 (AVC) video output "
               "module.\n", ti->fname.c_str(), (int64_t)dmx->id);

      } else {
        ti->private_size = dmx->v_stsd_size;
        ti->private_data = (unsigned char *)dmx->v_stsd;
        dmx->ptzr =
          add_packetizer(new video_packetizer_c(this, MKV_V_QUICKTIME, 0.0,
                                                dmx->v_width, dmx->v_height,
                                                ti));
        ti->private_data = NULL;
        mxinfo(FMT_TID "Using the video output module (FourCC: %.4s).\n",
               ti->fname.c_str(), (int64_t)dmx->id, dmx->fourcc);
      }

    } else {
      if (!strncasecmp(dmx->fourcc, "QDMC", 4) ||
          !strncasecmp(dmx->fourcc, "QDM2", 4)) {
        ptzr = new passthrough_packetizer_c(this, ti);
        dmx->ptzr = add_packetizer(ptzr);

        ptzr->set_track_type(track_audio);
        ptzr->set_codec_id(dmx->fourcc[3] == '2' ? MKV_A_QDMC2 : MKV_A_QDMC);
        ptzr->set_codec_private(dmx->priv, dmx->priv_size);
        ptzr->set_audio_sampling_freq(dmx->a_samplerate);
        ptzr->set_audio_channels(dmx->a_channels);
        ptzr->set_audio_bit_depth(dmx->a_bitdepth);

        mxinfo(FMT_TID "Using the generic audio output module (FourCC: %.4s)."
               "\n", ti->fname.c_str(), (int64_t)dmx->id, dmx->fourcc);

      } else if (!strncasecmp(dmx->fourcc, "MP4A", 4) &&
                 ((dmx->esds.object_type_id == MP4OTI_MPEG4Audio) || // AAC..
                  (dmx->esds.object_type_id == MP4OTI_MPEG2AudioMain) ||
                  (dmx->esds.object_type_id ==
                   MP4OTI_MPEG2AudioLowComplexity) ||
                  (dmx->esds.object_type_id ==
                   MP4OTI_MPEG2AudioScaleableSamplingRate))) {
        int profile, sample_rate, channels, output_sample_rate;
        bool sbraac;

        if (dmx->esds.decoder_config_len >= 2) {
          parse_aac_data(dmx->esds.decoder_config,
                         dmx->esds.decoder_config_len, profile, channels,
                         sample_rate, output_sample_rate, sbraac);
          mxverb(2, PFX "AAC: profile: %d, sample_rate: %d, channels: "
                 "%d, output_sample_rate: %d, sbr: %d\n", profile, sample_rate,
                 channels, output_sample_rate, (int)sbraac);
          if (sbraac)
            profile = AAC_PROFILE_SBR;
          ti->private_data = dmx->esds.decoder_config;
          ti->private_size = dmx->esds.decoder_config_len;
          dmx->ptzr =
            add_packetizer(new aac_packetizer_c(this, AAC_ID_MPEG4, profile,
                                                sample_rate, channels, ti,
                                                false, true));
          if (sbraac)
            PTZR(dmx->ptzr)->
              set_audio_output_sampling_freq(output_sample_rate);
          mxinfo(FMT_TID "Using the AAC output module.\n", ti->fname.c_str(),
                 (int64_t)dmx->id);
          ti->private_data = NULL;
          ti->private_size = 0;

        } else
          mxerror(FMT_TID "AAC found, but decoder config data has length %u."
                  "\n", ti->fname.c_str(), (int64_t)dmx->id,
                  dmx->esds.decoder_config_len);

      } else if (!strncasecmp(dmx->fourcc, "MP4A", 4) &&
                 ((dmx->esds.object_type_id == MP4OTI_MPEG2AudioPart3) ||
                  (dmx->esds.object_type_id == MP4OTI_MPEG1Audio))) {
        dmx->ptzr =
          add_packetizer(new mp3_packetizer_c(this, (int32_t)dmx->a_samplerate,
                                              dmx->a_channels, true, ti));
        mxinfo(FMT_TID "Using the MPEG audio output module.\n",
               ti->fname.c_str(), (int64_t)dmx->id);

      } else if (!strncasecmp(dmx->fourcc, "twos", 4) ||
                 !strncasecmp(dmx->fourcc, "swot", 4)) {
        dmx->ptzr =
          add_packetizer(new pcm_packetizer_c(this, (int32_t)dmx->a_samplerate,
                                              dmx->a_channels, dmx->a_bitdepth,
                                              ti, (dmx->a_bitdepth > 8) &&
                                              (dmx->fourcc[0] == 't')));
        mxinfo(FMT_TID "Using the PCM output module.\n", ti->fname.c_str(),
               (int64_t)dmx->id);

      } else
        die(PFX "Should not have happened #1.");
    }

    if (main_dmx == -1)
      main_dmx = i;
  }
}

void
qtmp4_reader_c::create_packetizers() {
  uint32_t i;

  main_dmx = -1;

  for (i = 0; i < demuxers.size(); i++)
    create_packetizer(demuxers[i]->id);
}

int
qtmp4_reader_c::get_progress() {
  uint32_t max_chunks;

  qtmp4_demuxer_ptr &dmx = demuxers[main_dmx];
  if (dmx->sample_size == 0)
    max_chunks = dmx->sample_table.size();
  else
    max_chunks = dmx->chunk_table.size();
  return 100 * dmx->pos / max_chunks;
}

void
qtmp4_reader_c::identify() {
  uint32_t i;

  mxinfo("File '%s': container: Quicktime/MP4\n", ti->fname.c_str());
  for (i = 0; i < demuxers.size(); i++) {
    qtmp4_demuxer_ptr &dmx = demuxers[i];
    mxinfo("Track ID %u: %s (%.4s)\n", dmx->id,
           dmx->type == 'v' ? "video" :
           dmx->type == 'a' ? "audio" : "unknown",
           dmx->fourcc);
  }
}

void
qtmp4_reader_c::flush_packetizers() {
  uint32_t i;

  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->ptzr != -1)
      PTZR(demuxers[i]->ptzr)->flush();
}

void
qtmp4_reader_c::add_available_track_ids() {
  int i;

  for (i =0 ; i < demuxers.size(); i++)
    available_track_ids.push_back(demuxers[i]->id);
}
