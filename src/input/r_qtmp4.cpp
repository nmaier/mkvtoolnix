/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_qtmp4.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief Quicktime and MP4 reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

// A lot of code in this file is from the mplayer sources. The original
// authors are to be thanked for their work.

#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_ZLIB_H)
#include <zlib.h>
#endif

#include "common.h"
#include "matroska.h"
#include "mkvmerge.h"
#include "p_aac.h"
#include "p_passthrough.h"
#include "p_pcm.h"
#include "p_video.h"
#include "r_qtmp4.h"

using namespace std;
using namespace libmatroska;

#define PFX "Quicktime/MP4 reader: "

#if WORDS_BIGENDIAN == 1
#define BE2STR(a) ((char *)&a)[0], ((char *)&a)[1], ((char *)&a)[2], \
                  ((char *)&a)[3]
#else
#define BE2STR(a) ((char *)&a)[3], ((char *)&a)[2], ((char *)&a)[1], \
                  ((char *)&a)[0]
#endif

int qtmp4_reader_c::probe_file(mm_io_c *in, int64_t size) {
  uint32_t atom;
  uint64_t atom_size;

  if (size < 20)
    return 0;
  try {
    in->setFilePointer(0, seek_beginning);

    atom_size = in->read_uint32_be();
    if (atom_size == 1)
      atom_size = in->read_uint64_be();
    atom = in->read_uint32_be();

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

qtmp4_reader_c::qtmp4_reader_c(track_info_c *nti) throw (error_c) :
  generic_reader_c(nti) {
  try {
    io = new mm_io_c(ti->fname, MODE_READ);
    io->setFilePointer(0, seek_end);
    file_size = io->getFilePointer();
    io->setFilePointer(0, seek_beginning);
    if (!qtmp4_reader_c::probe_file(io, file_size))
      throw error_c(PFX "Source is not a valid Quicktime/MP4 file.");

    if (verbose)
      mxinfo("Using Quicktime/MP4 demultiplexer for %s.\n", ti->fname);

    parse_headers();
    if (!identifying)
      create_packetizers();

  } catch (exception &ex) {
    throw error_c(PFX "Could not read the source file.");
  }
}

qtmp4_reader_c::~qtmp4_reader_c() {
  qtmp4_demuxer_t *dmx;
  uint32_t max_chunks;

  while (demuxers.size() > 0) {
    dmx = demuxers[demuxers.size() - 1];
    if (dmx->sample_size == 0)
      max_chunks = dmx->sample_table_len;
    else
      max_chunks = dmx->chunk_table_len;
    mxverb(2, PFX "demuxer %d: %u/%u chunks\n", dmx->id, dmx->pos, max_chunks);
    free_demuxer(dmx);
    free(dmx);
    demuxers.pop_back();
  }
  ti->private_data = NULL;

  delete io;
}

void qtmp4_reader_c::free_demuxer(qtmp4_demuxer_t *dmx) {
  if (dmx->packetizer != NULL)
    delete dmx->packetizer;
  safefree(dmx->sample_table);
  safefree(dmx->chunk_table);
  safefree(dmx->chunkmap_table);
  safefree(dmx->durmap_table);
  safefree(dmx->keyframe_table);
  safefree(dmx->editlist_table);
  safefree(dmx->v_desc);
  safefree(dmx->a_priv);
  safefree(dmx->a_esds.decoder_config);
  safefree(dmx->a_esds.sl_config);
}

void qtmp4_reader_c::read_atom(uint32_t &atom, uint64_t &size, uint64_t &pos,
                               uint32_t &hsize) {
  pos = io->getFilePointer();
  size = io->read_uint32_be();
  hsize = 8;
  if (size == 1) {
    size = io->read_uint64_be();
    hsize += 8;
  } else if (size == 0)
    size = file_size - io->getFilePointer() + 4;
  if (size < hsize)
    mxerror(PFX "Invalid chunk size %llu at %lld.\n", size, pos);
  atom = io->read_uint32_be();
}

#define skip_atom() io->setFilePointer(atom_pos + atom_size)

void qtmp4_reader_c::parse_headers() {
  uint32_t atom, atom_hsize, tmp, j, s, pts, last, idx;
  uint64_t atom_size, atom_pos;
  bool headers_parsed;
  int i;
  qtmp4_demuxer_t *dmx;

  io->setFilePointer(0);

  new_dmx = NULL;

  headers_parsed = false;
  mdat_pos = -1;
  do {
    read_atom(atom, atom_size, atom_pos, atom_hsize);
    mxverb(2, PFX "'%c%c%c%c' atom, size %lld, at %lld\n", BE2STR(atom),
           atom_size, atom_pos);

    if (atom == FOURCC('f', 't', 'y', 'p')) {
      tmp = io->read_uint32_be();
      mxverb(2, PFX "  File type major brand: %c%c%c%c\n", BE2STR(tmp));
      tmp = io->read_uint32_be();
      mxverb(2, PFX "  File type minor brand: %c%c%c%c\n", BE2STR(tmp));
      for (i = 0; i < ((atom_size - 16) / 4); i++) {
        tmp = io->read_uint32();
        mxverb(2, PFX "  File type compatible brands #%d: %.4s\n", i, &tmp);
      }

    } else if (atom == FOURCC('m', 'o', 'o', 'v')) {
      handle_header_atoms(atom, atom_size - atom_hsize,
                          atom_pos + atom_hsize, 1);
      headers_parsed = true;

    } else if (atom == FOURCC('w', 'i', 'd', 'e')) {
      skip_atom();

    } else if (atom == FOURCC('m', 'd', 'a', 't')) {
      mdat_pos = io->getFilePointer();
      mdat_size = atom_size;
      skip_atom();

    } else if ((atom == FOURCC('f', 'r', 'e', 'e')) ||
               (atom == FOURCC('s', 'k', 'i', 'p')) ||
               (atom == FOURCC('j', 'u', 'n', 'k')) ||
               (atom == FOURCC('p', 'n', 'o', 't')) ||
               (atom == FOURCC('P', 'I', 'C', 'T'))) {
      skip_atom();

    }

  } while (!io->eof() && (!headers_parsed || (mdat_pos == -1)));

  if (!headers_parsed)
    mxerror(PFX "Have not found any header atoms.\n");
  if (mdat_pos == -1)
    mxerror(PFX "Have not found the 'mdat' atom. No movie data found.\n");

  io->setFilePointer(mdat_pos);

  for (idx = 0; idx < demuxers.size(); idx++) {
    dmx = demuxers[idx];

    if (((dmx->type == 'v') &&
         strncasecmp(dmx->fourcc, "SVQ", 3) &&
         strncasecmp(dmx->fourcc, "cvid", 4) &&
         strncasecmp(dmx->fourcc, "rle ", 4)) ||
        ((dmx->type == 'a') &&
         strncasecmp(dmx->fourcc, "QDM", 3) &&
         strncasecmp(dmx->fourcc, "MP4A", 4) &&
         strncasecmp(dmx->fourcc, "twos", 4) &&
         strncasecmp(dmx->fourcc, "swot", 4))) {
      mxwarn(PFX "Unknown/unsupported FourCC '%.4s' for track %u.\n",
             &dmx->fourcc, dmx->id);

      continue;
    }

    if ((dmx->type == 'a') &&
        !strncasecmp(dmx->fourcc, "MP4A", 4) &&
        (dmx->a_esds.decoder_config == NULL)) {
      mxwarn(PFX "AAC track %u is missing the esds atom/the decoder config.\n",
             dmx->id);

      continue;
    }

    if ((dmx->type == 'v') &&
        ((dmx->v_width == 0) || (dmx->v_height == 0) ||
         ((dmx->fourcc[0] == 0) && (dmx->fourcc[1] == 0) &&
          (dmx->fourcc[2] == 0) && (dmx->fourcc[3] == 0)))) {
      mxwarn(PFX "Track %u is missing some data. Broken header atoms?\n",
             dmx->id);
      continue;
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

    last = dmx->chunk_table_len;

    // process chunkmap:
    i = dmx->chunkmap_table_len;
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
    for (j = 0; j < dmx->chunk_table_len; j++) {
      dmx->chunk_table[j].samples = s;
      s += dmx->chunk_table[j].size;
    }

    // workaround for fixed-size video frames (dv and uncompressed)
    if ((dmx->sample_table_len == 0) && (dmx->type != 'a')) {
      dmx->sample_table_len = s;
      dmx->sample_table = (qt_sample_t *)safemalloc(sizeof(qt_sample_t) * s);
      for (i = 0; i < s; i++)
        dmx->sample_table[i].size = dmx->sample_size;
      dmx->sample_size = 0;
    }

    if (dmx->sample_table_len == 0) {
      // constant sampesize
      if ((dmx->durmap_table_len == 1) ||
          ((dmx->durmap_table_len == 2) && (dmx->durmap_table[1].number == 1)))
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
    for (j = 0; j < dmx->durmap_table_len; j++) {
      for (i = 0; i < dmx->durmap_table[j].number; i++) {
        dmx->sample_table[s].pts = pts;
        s++;
        pts += dmx->durmap_table[j].duration;
      }
    }

    // calc sample offsets
    s = 0;
    for (j = 0; j < dmx->chunk_table_len; j++) {
      uint64_t pos = dmx->chunk_table[j].pos;

      for (i = 0; i < dmx->chunk_table[j].size; i++) {
        dmx->sample_table[s].pos = pos;
        pos += dmx->sample_table[s].size;
        s++;
      }
    }
  }

  mxverb(2, PFX "Number of valid tracks found: %u\n", demuxers.size());
}

void qtmp4_reader_c::handle_header_atoms(uint32_t parent, int64_t parent_size,
                                         uint64_t parent_pos, int level) {
  uint64_t atom_size, atom_pos, target_pos;
  uint32_t atom, atom_hsize;

  target_pos = parent_pos + parent_size;

  while (parent_size > 0) {
    read_atom(atom, atom_size, atom_pos, atom_hsize);

    mxverb(2, PFX "%*s'%c%c%c%c' atom, size %lld, at %lld\n", 2 * level,
           "", BE2STR(atom), atom_size, atom_pos);

    if (parent == FOURCC('m', 'o', 'o', 'v')) {

      if (atom == FOURCC('c', 'm', 'o', 'v')) {
        compression_algorithm = 0;
        handle_header_atoms(atom, atom_size - atom_hsize,
                            atom_pos + atom_hsize, level + 1);

      } else if (atom == FOURCC('m', 'v', 'h', 'd')) {
        mvhd_atom_t mvhd;

        if ((atom_size - atom_hsize) < sizeof(mvhd_atom_t))
          mxerror(PFX "'mvhd' atom is too small. Expected size: >= %d. Actual "
                  "size: %lld.\n", sizeof(mvhd_atom_t) + atom_hsize);
        if (io->read(&mvhd, sizeof(mvhd_atom_t)) != sizeof(mvhd_atom_t))
          throw exception();
        mxverb(2, PFX "%*s Time scale: %u\n", (level + 1) * 2, "",
               get_uint32_be(&mvhd.time_scale));

      } else if (atom == FOURCC('t', 'r', 'a', 'k')) {
        new_dmx = (qtmp4_demuxer_t *)safemalloc(sizeof(qtmp4_demuxer_t));
        memset(new_dmx, 0, sizeof(qtmp4_demuxer_t));
        new_dmx->type = '?';

        handle_header_atoms(atom, atom_size - atom_hsize,
                            atom_pos + atom_hsize, level + 1);
        if ((new_dmx->type == '?') ||
            ((new_dmx->fourcc[0] == 0) && (new_dmx->fourcc[1] == 0) &&
             (new_dmx->fourcc[2] == 0) && (new_dmx->fourcc[3] == 0))) {
          free_demuxer(new_dmx);
          safefree(new_dmx);
        } else
          demuxers.push_back(new_dmx);
        new_dmx = NULL;

      }

    } else if (parent == FOURCC('t', 'r', 'a', 'k')) {
      if (atom == FOURCC('t', 'k', 'h', 'd')) {
        tkhd_atom_t tkhd;

        if ((atom_size - atom_hsize) < sizeof(tkhd_atom_t))
          mxerror(PFX "'tkhd' atom is too small. Expected size: >= %d. Actual "
                  "size: %lld.\n", sizeof(tkhd_atom_t) + atom_hsize);
        if (io->read(&tkhd, sizeof(tkhd_atom_t)) != sizeof(tkhd_atom_t))
          throw exception();
        mxverb(2, PFX "%*s Track ID: %u\n", (level + 1) * 2, "",
               get_uint32_be(&tkhd.track_id));
        new_dmx->id = get_uint32_be(&tkhd.track_id);

      } else if (atom == FOURCC('m', 'd', 'i', 'a'))
        handle_header_atoms(atom, atom_size - atom_hsize,
                            atom_pos + atom_hsize, level + 1);

    } else if (parent == FOURCC('m', 'd', 'i', 'a')) {

      if (atom == FOURCC('m', 'd', 'h', 'd')) {
        mdhd_atom_t mdhd;

        if ((atom_size - atom_hsize) < sizeof(mdhd_atom_t))
          mxerror(PFX "'mdhd' atom is too small. Expected size: >= %d. Actual "
                  "size: %lld.\n", sizeof(mdhd_atom_t) + atom_hsize);
        if (io->read(&mdhd, sizeof(mdhd_atom_t)) != sizeof(mdhd_atom_t))
          throw exception();
        mxverb(2, PFX "%*s Time scale: %u, duration: %u\n", (level + 1) * 2,
               "", get_uint32_be(&mdhd.time_scale),
               get_uint32_be(&mdhd.duration));
        new_dmx->timescale = get_uint32_be(&mdhd.time_scale);
        new_dmx->global_duration = get_uint32_be(&mdhd.duration);

      } else if (atom == FOURCC('h', 'd', 'l', 'r')) {
        hdlr_atom_t hdlr;

        if ((atom_size - atom_hsize) < sizeof(hdlr_atom_t))
          mxerror(PFX "'hdlr' atom is too small. Expected size: >= %d. Actual "
                  "size: %lld.\n", sizeof(hdlr_atom_t) + atom_hsize);
        if (io->read(&hdlr, sizeof(hdlr_atom_t)) != sizeof(hdlr_atom_t))
          throw exception();
        mxverb(2, PFX "%*s Component type: %.4s subtype: %.4s\n",
               (level + 1) * 2, "", &hdlr.type, &hdlr.subtype);
        switch (get_uint32_be(&hdlr.subtype)) {
          case FOURCC('s', 'o', 'u', 'n'):
            new_dmx->type = 'a';
            break;
          case FOURCC('v', 'i', 'd', 'e'):
            new_dmx->type = 'v';
            break;
        }

      } else if (atom == FOURCC('m', 'i', 'n', 'f'))
        handle_header_atoms(atom, atom_size - atom_hsize,
                            atom_pos + atom_hsize, level + 1);

    } else if (parent == FOURCC('m', 'i', 'n', 'f')) {
      if (atom == FOURCC('h', 'd', 'l', 'r')) {
        hdlr_atom_t hdlr;

        if ((atom_size - atom_hsize) < sizeof(hdlr_atom_t))
          mxerror(PFX "'hdlr' atom is too small. Expected size: >= %d. Actual "
                  "size: %lld.\n", sizeof(hdlr_atom_t) + atom_hsize);
        if (io->read(&hdlr, sizeof(hdlr_atom_t)) != sizeof(hdlr_atom_t))
          throw exception();
        mxverb(2, PFX "%*s Component type: %.4s subtype: %.4s\n",
               (level + 1) * 2, "", &hdlr.type, &hdlr.subtype);

      } else if (atom == FOURCC('s', 't', 'b', 'l'))
        handle_header_atoms(atom, atom_size - atom_hsize,
                            atom_pos + atom_hsize, level + 1);

    } else if (parent == FOURCC('s', 't', 'b', 'l')) {

      if (atom == FOURCC('s', 't', 't', 's')) {
        uint32_t count, i;

        io->skip(1 + 3);        // version & flags
        count = io->read_uint32_be();
        new_dmx->durmap_table = (qt_durmap_t *)
          safemalloc(count * sizeof(qt_durmap_t));
        for (i = 0; i < count; i++) {
          new_dmx->durmap_table[i].number = io->read_uint32_be();
          new_dmx->durmap_table[i].duration = io->read_uint32_be();
        }
        new_dmx->durmap_table_len = count;

        mxverb(2, PFX "%*sSample duration table: %u entries\n",
               (level + 1) * 2, "", count);

      } else if (atom == FOURCC('s', 't', 's', 'd')) {
        uint32_t count, i, size, tmp;
        int64_t pos;
        sound_v1_stsd_atom_t sv1_stsd;
        video_stsd_atom_t v_stsd;

        io->skip(1 + 3);        // version & flags
        count = io->read_uint32_be();
        for (i = 0; i < count; i++) {
          pos = io->getFilePointer();
          size = io->read_uint32_be();

          if (new_dmx->type == 'a') {
            if ((size < sizeof(sound_v0_stsd_atom_t)) ||
                (io->read(&sv1_stsd, sizeof(sound_v0_stsd_atom_t)) !=
                 sizeof(sound_v0_stsd_atom_t)))
              mxerror(PFX "Could not read the sound description atom for "
                      "track ID %u.\n", new_dmx->id);

            if (new_dmx->fourcc[0] != 0)
              mxwarn(PFX "Track ID %u has more than one FourCC. Only using "
                     "the first one (%.4s) and not this one (%.4s).\n",
                     new_dmx->id, new_dmx->fourcc, sv1_stsd.v0.base.fourcc);
            else
              memcpy(new_dmx->fourcc, sv1_stsd.v0.base.fourcc, 4);

            mxverb(2, PFX "%*sFourCC: %.4s, channels: %d, sample size: %d, "
                   "compression id: %d, sample rate: 0x%08x, version: %u",
                   (level + 1) * 2, "", sv1_stsd.v0.base.fourcc,
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
              io->setFilePointer(pos + sizeof(sound_v1_stsd_atom_t) + 4);
            else
              io->setFilePointer(pos + sizeof(sound_v0_stsd_atom_t) + 4);

            while (io->getFilePointer() < (pos + size)) {
              uint32_t add_atom, add_atom_size;

              add_atom_size = io->read_uint32_be();
              add_atom = io->read_uint32_be();
              add_atom_size -= 8;
              mxverb(2, PFX "%*sAudio private data size: %u, type: "
                     "'%c%c%c%c'\n", (level + 1) * 2, "", add_atom_size + 8,
                     BE2STR(add_atom));
              if (new_dmx->a_priv == NULL) {
                mm_mem_io_c *memio;
                uint32_t patom_size, patom;

                new_dmx->a_priv_size = add_atom_size;
                new_dmx->a_priv =
                  (unsigned char *)safemalloc(new_dmx->a_priv_size);
                if (io->read(new_dmx->a_priv, new_dmx->a_priv_size) !=
                    new_dmx->a_priv_size)
                  throw exception();

                memio = new mm_mem_io_c(new_dmx->a_priv,
                                        new_dmx->a_priv_size);

                if (add_atom == FOURCC('e', 's', 'd', 's')) {
                  if (!new_dmx->a_esds_parsed)
                    new_dmx->a_esds_parsed = parse_esds_atom(memio, new_dmx,
                                                             level + 1);
                } else {                    
                  while (!memio->eof()) {
                    patom_size = memio->read_uint32_be();
                    if (patom_size <= 8)
                      break;
                    patom = memio->read_uint32_be();
                    mxverb(2, PFX "%*sAtom size: %u, atom: '%c%c%c%c'\n",
                           (level + 2) * 2, "", patom_size, BE2STR(patom));
                    if ((patom == FOURCC('e', 's', 'd', 's')) &&
                        !new_dmx->a_esds_parsed) {
                      memio->save_pos();
                      new_dmx->a_esds_parsed = parse_esds_atom(memio, new_dmx,
                                                               level + 2);
                      memio->restore_pos();
                    }
                    memio->skip(patom_size - 8);
                  }
                }
                delete memio;
              } else
                io->skip(add_atom_size);
            }
            memcpy(&new_dmx->a_stsd, &sv1_stsd, sizeof(sound_v1_stsd_atom_t));

          } else if (new_dmx->type == 'v') {
            if ((size < sizeof(video_stsd_atom_t)) ||
                (io->read(&v_stsd, sizeof(video_stsd_atom_t)) !=
                 sizeof(video_stsd_atom_t)))
              mxerror(PFX "Could not read the video description atom for "
                      "track ID %u.\n", new_dmx->id);

            if (new_dmx->fourcc[0] != 0)
              mxwarn(PFX "Track ID %u has more than one FourCC. Only using "
                     "the first one (%.4s) and not this one (%.4s).\n",
                     new_dmx->id, new_dmx->fourcc, v_stsd.base.fourcc);
            else
              memcpy(new_dmx->fourcc, v_stsd.base.fourcc, 4);

            mxverb(2, PFX "%*sFourCC: %.4s, width: %u, height: %u, depth: %u"
                   "\n", (level + 1) * 2, "", v_stsd.base.fourcc,
                   get_uint16_be(&v_stsd.width), get_uint16_be(&v_stsd.height),
                   get_uint16_be(&v_stsd.depth));

            new_dmx->v_width = get_uint16_be(&v_stsd.width);
            new_dmx->v_height = get_uint16_be(&v_stsd.height);
            new_dmx->v_bitdepth = get_uint16_be(&v_stsd.depth);
            new_dmx->v_desc =
              (qt_image_description_t *)safemalloc(size);
            io->setFilePointer(pos);
            if (io->read(new_dmx->v_desc, size) != size)
              throw exception();
            new_dmx->v_desc_size = size;
          }

          io->setFilePointer(pos + size);
        }

      } else if (atom == FOURCC('s', 't', 's', 's')) {
        uint32_t count, i;

        io->skip(1 + 3);        // version & flags
        count = io->read_uint32_be();
        new_dmx->keyframe_table = (uint32_t *)safemalloc(count * 4);

        for (i = 0; i < count; i++)
          new_dmx->keyframe_table[i] = io->read_uint32_be();
        new_dmx->keyframe_table_len = count;

        mxverb(2, PFX "%*sSync/keyframe table: %u entries\n", (level + 1) * 2,
               "", count);

      } else if (atom == FOURCC('s', 't', 's', 'c')) {
        uint32_t count, i;

        io->skip(1 + 3);        // version & flags
        count = io->read_uint32_be();
        new_dmx->chunkmap_table = (qt_chunkmap_t *)
          safemalloc(count * sizeof(qt_chunkmap_t));
        for (i = 0; i < count; i++) {
          new_dmx->chunkmap_table[i].first_chunk = io->read_uint32_be() - 1;
          new_dmx->chunkmap_table[i].samples_per_chunk =
            io->read_uint32_be();
          new_dmx->chunkmap_table[i].sample_description_id =
            io->read_uint32_be();
        }
        new_dmx->chunkmap_table_len = count;

        mxverb(2, PFX "%*sSample to chunk/chunkmap table: %u entries\n",
               (level + 1) * 2, "", count);

      } else if (atom == FOURCC('s', 't', 's', 'z')) {
        uint32_t count, i, sample_size;

        io->skip(1 + 3);        // version & flags
        sample_size = io->read_uint32_be();
        count = io->read_uint32_be();
        if (sample_size == 0) {
          new_dmx->sample_table =
            (qt_sample_t *)safemalloc(count * sizeof(qt_sample_t));
          for (i = 0; i < count; i++)
            new_dmx->sample_table[i].size = io->read_uint32_be();
          new_dmx->sample_table_len = count;

          mxverb(2, PFX "%*sSample size table: %u entries\n",
                 (level + 1) * 2, "", count);

        } else {
          new_dmx->sample_size = sample_size;
          mxverb(2, PFX "%*sSample size table; global sample size: %u\n",
                 (level + 1) * 2, "", sample_size);
        }

      } else if (atom == FOURCC('s', 't', 'c', 'o')) {
        uint32_t count, i;

        io->skip(1 + 3);        // version & flags
        count = io->read_uint32_be();
        new_dmx->chunk_table =
          (qt_chunk_t *)safemalloc(count * sizeof(qt_chunk_t));
        new_dmx->chunk_table_len = count;

        for (i = 0; i < count; i++)
          new_dmx->chunk_table[i].pos = io->read_uint32_be();

        mxverb(2, PFX "%*sChunk offset table: %u entries\n", (level + 1) * 2,
               "", count);

      }

    } else if (parent == FOURCC('c', 'm', 'o', 'v')) {
      if (atom == FOURCC('d', 'c', 'o', 'm')) {
        uint32_t algo;

        algo = io->read_uint32_be();
        mxverb(2, PFX "%*sCompression algorithm: %c%c%c%c\n", (level + 1) * 2,
               "", BE2STR(algo));
        compression_algorithm = algo;

      } else if (atom == FOURCC('c', 'm', 'v', 'd')) {
#if defined(HAVE_ZLIB_H)
        uint32_t moov_size, cmov_size, next_atom, next_atom_hsize;
        uint64_t next_atom_pos, next_atom_size;
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
        cmov_size = atom_size - atom_hsize;
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
                 "uncompressing (%u).\n", moov_size, zs.total_out);
        zret = inflateEnd(&zs);

        io = new mm_mem_io_c(moov_buf, zs.total_out);
        while (!io->eof()) {
          read_atom(next_atom, next_atom_size, next_atom_pos, next_atom_hsize);
          mxverb(2, PFX "%*s'%c%c%c%c' atom at %lld\n", (level + 1) * 2, "",
                 BE2STR(next_atom), next_atom_pos);

          if (next_atom == FOURCC('m', 'o', 'o', 'v'))
            handle_header_atoms(next_atom, next_atom_size - next_atom_hsize,
                                next_atom_pos + next_atom_hsize, level + 2);

          io->setFilePointer(next_atom_pos + next_atom_size);
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

    }

    skip_atom();
    parent_size -= atom_size;
  }

  io->setFilePointer(target_pos);
}

int qtmp4_reader_c::read(generic_packetizer_c *ptzr) {
  uint32_t i, k, frame, frame_size;
  qtmp4_demuxer_t *dmx;
  bool chunks_left, is_keyframe;
  int64_t timecode, duration;
  unsigned char *buffer;

  frame = 0;
  chunks_left = false;
  for (i = 0; i < demuxers.size(); i++) {
    dmx = demuxers[i];

    if (dmx->packetizer != ptzr)
      continue;

    if (dmx->sample_size != 0) {
      if (dmx->pos >= dmx->chunk_table_len)
        continue;

      io->setFilePointer(dmx->chunk_table[dmx->pos].pos);
      timecode = 1000 * ((uint64_t)dmx->chunk_table[dmx->pos].samples *
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

      if (dmx->keyframe_table_len == 0)
        is_keyframe = true;
      else {
        is_keyframe = false;
        for (k = 0; k < dmx->keyframe_table_len; k++)
          if (dmx->keyframe_table[k] == (frame + 1)) {
            is_keyframe = true;
            break;
          }
      }

      buffer = (unsigned char *)safemalloc(frame_size);
      if (io->read(buffer, frame_size) != frame_size) {
        mxwarn(PFX "Could not read chunk number %u/%u with size %u from "
               "position %lld. Aborting.\n", frame, dmx->chunk_table_len,
               frame_size, dmx->chunk_table[dmx->pos].pos);
        safefree(buffer);

        return 0;
      }

      if ((dmx->pos + 1) < dmx->chunk_table_len)
        duration = 1000 * ((uint64_t)dmx->chunk_table[dmx->pos + 1].samples *
                           (uint64_t)dmx->duration) /
          (uint64_t)dmx->timescale - timecode;
      else
        duration = dmx->avg_duration;
      dmx->avg_duration = (dmx->avg_duration * dmx->pos + duration) /
        (dmx->pos + 1);

      dmx->packetizer->process(buffer, frame_size, timecode, duration,
                               is_keyframe ? VFT_IFRAME :
                               VFT_PFRAMEAUTOMATIC);
      dmx->pos++;

      if (dmx->pos < dmx->chunk_table_len)
        chunks_left = true;

    } else {
      if (dmx->pos >= dmx->sample_table_len)
        continue;

      frame = dmx->pos;

      timecode = (int64_t)dmx->sample_table[frame].pts * 1000 /
        dmx->timescale;
      if ((frame + 1) < dmx->sample_table_len)
        duration = (int64_t)dmx->sample_table[frame + 1].pts * 1000 /
          dmx->timescale - timecode;
      else
        duration = dmx->avg_duration;
      dmx->avg_duration = (dmx->avg_duration * frame + duration) /
        (frame + 1);

      if (dmx->keyframe_table_len == 0)
        is_keyframe = true;
      else {
        is_keyframe = false;
        for (k = 0; k < dmx->keyframe_table_len; k++)
          if (dmx->keyframe_table[k] == (frame + 1)) {
            is_keyframe = true;
            break;
          }
      }

      frame_size = dmx->sample_table[frame].size;
      buffer = (unsigned char *)safemalloc(frame_size);
      io->setFilePointer(dmx->sample_table[frame].pos);
      if (io->read(buffer, frame_size) != frame_size) {
        mxwarn(PFX "Could not read chunk number %u/%u with size %u from "
               "position %lld. Aborting.\n", frame, dmx->sample_table_len,
               frame_size, dmx->sample_table[frame].pos);
        safefree(buffer);

        return 0;
      }

      dmx->packetizer->process(buffer, frame_size, timecode, duration,
                               is_keyframe ? VFT_IFRAME :
                               VFT_PFRAMEAUTOMATIC);
      dmx->pos++;
      if (dmx->pos < dmx->sample_table_len)
        chunks_left = true;
    }

    if (chunks_left)
      return EMOREDATA;
    else
      return 0;
  }

  return 0;
}

uint32_t qtmp4_reader_c::read_esds_descr_len(mm_mem_io_c *memio) {
  uint32_t len, num_bytes;
  uint8_t byte;

  len = 0;
  num_bytes = 0;
  do {
    byte = memio->read_uint8();
    num_bytes++;
    len = (len << 7) | (byte & 0x7f);
  } while (((byte & 0x80) == 0x80) && (num_bytes < 4));

  return len;
}

bool qtmp4_reader_c::parse_esds_atom(mm_mem_io_c *memio,
                                     qtmp4_demuxer_t *dmx, int level) {
  uint32_t len;
  uint8_t tag;
  esds_t *e;
  int lsp;

  lsp = (level + 1) * 2;
  e = &dmx->a_esds;
  e->version = memio->read_uint8();
  e->flags = memio->read_uint24_be();
  mxverb(2, PFX "%*sesds: version: %u, flags: %u\n", lsp, "", e->version,
         e->flags);
  tag = memio->read_uint8();
  if (tag == MP4DT_ES) {
    len = read_esds_descr_len(memio);
    e->esid = memio->read_uint16_be();
    e->stream_priority = memio->read_uint8();
    mxverb(2, PFX "%*sesds: id: %u, stream priority: %u, len: %u\n", lsp, "",
           e->esid, (uint32_t)e->stream_priority, len);
  } else {
    e->esid = memio->read_uint16_be();
    mxverb(2, PFX "%*sesds: id: %u\n", lsp, "", e->esid);
  }

  tag = memio->read_uint8();
  if (tag != MP4DT_DEC_CONFIG) {
    mxverb(2, PFX "%s*tag is not DEC_CONFIG (0x%02x) but 0x%02x.\n", lsp, "",
           MP4DT_DEC_CONFIG, (uint32_t)tag);
    return false;
  }

  len = read_esds_descr_len(memio);

  e->object_type_id = memio->read_uint8();
  e->stream_type = memio->read_uint8();
  e->buffer_size_db = memio->read_uint24_be();
  e->max_bitrate = memio->read_uint32_be();
  e->avg_bitrate = memio->read_uint32_be();
  mxverb(2, PFX "%*sesds: decoder config descriptor, len: %u, object_type_id: "
         "%u, stream_type: 0x%2x, buffer_size_db: %u, max_bitrate: %.3fkbit/s"
         ", avg_bitrate: %.3fkbit/s\n", lsp, "", len, e->object_type_id,
         e->stream_type, e->buffer_size_db, e->max_bitrate / 1000.0,
         e->avg_bitrate / 1000.0);

  e->decoder_config_len = 0;

  tag = memio->read_uint8();
  if (tag != MP4DT_DEC_SPECIFIC) {
    mxverb(2, PFX "%*stag is not DEC_SPECIFIC (0x%02x) but 0x%02x.\n", lsp, "",
           MP4DT_DEC_SPECIFIC, (uint32_t)tag);
    return false;
  }

  len = read_esds_descr_len(memio);
  e->decoder_config_len = len;
  e->decoder_config = (uint8_t *)safemalloc(len);
  if (memio->read(e->decoder_config, len) != len)
    throw exception();
  mxverb(2, PFX "%*sesds: decoder specific descriptor, len: %u\n", lsp, "",
         len);

  tag = memio->read_uint8();
  if (tag != MP4DT_SL_CONFIG) {
    mxverb(2, PFX "%*stag is not SL_CONFIG (0x%02x) but 0x%02x.\n", lsp, "",
           MP4DT_SL_CONFIG, (uint32_t)tag);
    return false;
  }

  len = read_esds_descr_len(memio);
  e->sl_config_len = len;
  e->sl_config = (uint8_t *)safemalloc(len);
  if (memio->read(e->sl_config, len) != len)
    throw exception();
  mxverb(2, PFX "%*sesds: sync layer config descriptor, len: %u\n", lsp, "",
         len);

  return true;
}

void qtmp4_reader_c::create_packetizer(int64_t tid) {
  uint32_t i;
  qtmp4_demuxer_t *dmx;
  passthrough_packetizer_c *ptzr;

  dmx = NULL;
  for (i = 0; i < demuxers.size(); i++)
    if (demuxers[i]->id == tid) {
      dmx = demuxers[i];
      break;
    }
  if (dmx == NULL)
    return;

  if (dmx->ok && demuxing_requested(dmx->type, dmx->id) &&
      (dmx->packetizer == NULL)) {
    ti->id = dmx->id;
    if (dmx->type == 'v') {

      ti->private_size = dmx->v_desc_size;
      ti->private_data = (unsigned char *)dmx->v_desc;
      dmx->packetizer =
        new video_packetizer_c(this, MKV_V_QUICKTIME, 0.0, dmx->v_width,
                               dmx->v_height, false, ti);
      ti->private_data = NULL;

      dmx->packetizer->duplicate_data_on_add(false);

      mxinfo("+-> Using the video packetizer for track %u (FourCC: %.4s).\n",
             dmx->id, dmx->fourcc);

    } else {
      if (!strncasecmp(dmx->fourcc, "QDMC", 4) ||
          !strncasecmp(dmx->fourcc, "QDM2", 4)) {
        ptzr = new passthrough_packetizer_c(this, ti);
        dmx->packetizer = ptzr;

        ptzr->set_track_type(track_audio);
        ptzr->set_codec_id(dmx->fourcc[3] == '2' ? MKV_A_QDMC2 : MKV_A_QDMC);
        ptzr->set_codec_private(dmx->a_priv, dmx->a_priv_size);
        ptzr->set_audio_sampling_freq(dmx->a_samplerate);
        ptzr->set_audio_channels(dmx->a_channels);
        ptzr->set_audio_bit_depth(dmx->a_bitdepth);

        if (verbose)
          mxinfo("+-> Using generic audio output module for stream "
                 "%u (FourCC: %.4s).\n", dmx->id, dmx->fourcc);

      } else if (!strncasecmp(dmx->fourcc, "MP4A", 4)) {
        int profile, srate_idx, channels, osrate_idx;
        bool sbraac;

        osrate_idx = 0;
        if ((dmx->a_esds.decoder_config_len == 2) ||
            (dmx->a_esds.decoder_config_len == 5)) {
          profile = (dmx->a_esds.decoder_config[0] >> 3) - 1;
          srate_idx = ((dmx->a_esds.decoder_config[0] & 0x07) << 1) |
            (dmx->a_esds.decoder_config[1] >> 7);
          channels = (dmx->a_esds.decoder_config[1] & 0x7f) >> 3;
          mxverb(2, PFX "AAC: profile: %d, sample_rate_idx: %d, channels: "
                 "%d", profile, srate_idx, channels);
          if (dmx->a_esds.decoder_config_len == 5) {
            osrate_idx = (dmx->a_esds.decoder_config[4] & 0x7f) >> 3;
            sbraac = true;
            mxverb(2, ", SBR sample_rate_idx: %d", osrate_idx);
            profile = AAC_PROFILE_SBR;
          } else
            sbraac = false;
          mxverb(2, "\n");

          srate_idx = aac_sampling_freq[srate_idx];
          dmx->packetizer = new aac_packetizer_c(this, AAC_ID_MPEG4, profile,
                                                 srate_idx, channels, ti,
                                                 false, true);
          if (sbraac)
            dmx->packetizer->
              set_audio_output_sampling_freq(aac_sampling_freq[osrate_idx]);
          if (verbose)
            mxinfo("+-> Using AAC output module for stream %u.\n", dmx->id);

        } else
          mxerror(PFX "AAC found, but decoder config data has length %u.\n",
                  dmx->a_esds.decoder_config_len);

      } else if (!strncasecmp(dmx->fourcc, "twos", 4) ||
                 !strncasecmp(dmx->fourcc, "swot", 4)) {
        dmx->packetizer =
          new pcm_packetizer_c(this, (uint32_t)dmx->a_samplerate,
                               dmx->a_channels, dmx->a_bitdepth, ti,
                               (dmx->a_bitdepth > 8) &&
                               (dmx->fourcc[0] == 't'));
        if (verbose)
          mxinfo("+-> Using PCM output module for stream %u.\n", dmx->id);

      } else
        die(PFX "Should not have happened #1.");
    }

    if (main_dmx == -1)
      main_dmx = i;
  }
}

void qtmp4_reader_c::create_packetizers() {
  uint32_t i;

  main_dmx = -1;

  for (i = 0; i < ti->track_order->size(); i++)
    create_packetizer((*ti->track_order)[i]);
  for (i = 0; i < demuxers.size(); i++)
    create_packetizer(demuxers[i]->id);
}

void qtmp4_reader_c::set_headers() {
  uint32_t i, k;
  qtmp4_demuxer_t *d;

  for (i = 0; i < demuxers.size(); i++)
    demuxers[i]->headers_set = false;

  for (i = 0; i < ti->track_order->size(); i++) {
    d = NULL;
    for (k = 0; k < demuxers.size(); k++)
      if (demuxers[k]->id == (*ti->track_order)[i]) {
        d = demuxers[k];
        break;
      }
    if ((d != NULL) && (d->packetizer != NULL) && !d->headers_set) {
      d->packetizer->set_headers();
      d->headers_set = true;
    }
  }
  for (i = 0; i < demuxers.size(); i++)
    if ((demuxers[i]->packetizer != NULL) && !demuxers[i]->headers_set) {
      demuxers[i]->packetizer->set_headers();
      demuxers[i]->headers_set = true;
    }
}

int qtmp4_reader_c::display_priority() {
  return DISPLAYPRIORITY_MEDIUM;
}

void qtmp4_reader_c::display_progress(bool final) {
  uint32_t max_chunks;
  qtmp4_demuxer_t *dmx;

  dmx = demuxers[main_dmx];
  if (dmx->sample_size == 0)
    max_chunks = dmx->sample_table_len;
  else
    max_chunks = dmx->chunk_table_len;
  if (final)
    mxinfo("progress: %d/%d chunks (100%%)\r", max_chunks, max_chunks);
  else
    mxinfo("progress: %d/%d chunks (%d%%)\r", dmx->pos, max_chunks,
           dmx->pos * 100 / max_chunks);
}

void qtmp4_reader_c::identify() {
  uint32_t i;
  qtmp4_demuxer_t *dmx;

  mxinfo("File '%s': container: Quicktime/MP4\n", ti->fname);
  for (i = 0; i < demuxers.size(); i++) {
    dmx = demuxers[i];
    mxinfo("Track ID %u: %s (%.4s)\n", dmx->id,
           dmx->type == 'v' ? "video" :
           dmx->type == 'a' ? "audio" : "unknown",
           dmx->fourcc);
  }
}
