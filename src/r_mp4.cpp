/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_mp4.cpp

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "common.h"
#include "mkvmerge.h"
#include "qtmp4_atoms.h"
#include "r_mp4.h"

using namespace std;
using namespace libmatroska;

#define PFX "Quicktime/MP4 reader: "

int qtmp4_reader_c::probe_file(mm_io_c *in, int64_t size) {
  uint32_t atom_size, object;

  if (size < 20)
    return 0;
  try {
    in->setFilePointer(0, seek_beginning);

    atom_size = in->read_uint32_be();
    object = in->read_uint32_be();

    if ((object == FOURCC('m', 'o', 'o', 'v')) ||
        (object == FOURCC('f', 't', 'y', 'p')))
        return 1;

  } catch (exception &ex) {
    return 0;
  }

  return 0;
}

qtmp4_reader_c::qtmp4_reader_c(track_info_t *nti) throw (error_c) :
  generic_reader_c(nti) {
  try {
    io = new mm_io_c(ti->fname, MODE_READ);
    io->setFilePointer(0, seek_end);
    file_size = io->getFilePointer();
    io->setFilePointer(0, seek_beginning);
    if (!qtmp4_reader_c::probe_file(io, file_size))
      throw error_c(PFX "Source is not a valid Quicktime/MP4 file.");

    done = false;

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

  while (demuxers.size() > 0) {
    dmx = demuxers[demuxers.size() - 1];
    free_demuxer(dmx);
    free(dmx);
    demuxers.pop_back();
  }

  delete io;
}

void qtmp4_reader_c::free_demuxer(qtmp4_demuxer_t *dmx) {
  if (dmx->packetizer != NULL)
    delete dmx->packetizer;
  safefree(dmx->sync_table);
  safefree(dmx->duration_table);
  safefree(dmx->sample_to_chunk_table);
  safefree(dmx->sample_size_table);
  safefree(dmx->chunk_offset_table);
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

#define BE2STR(a) ((char *)&a)[3], ((char *)&a)[2], ((char *)&a)[1], \
                  ((char *)&a)[0]
#define skip_atom() io->setFilePointer(atom_pos + atom_size)

void qtmp4_reader_c::parse_headers() {
  uint32_t atom, atom_hsize, tmp;
  uint64_t atom_size, atom_pos;
  bool headers_parsed;
  int i;

  io->setFilePointer(0);

  new_dmx = NULL;

  headers_parsed = false;
  do {
    read_atom(atom, atom_size, atom_pos, atom_hsize);
    mxverb(2, PFX "'%c%c%c%c' atom at %lld\n", BE2STR(atom), atom_pos);

    if (atom == FOURCC('f', 't', 'y', 'p')) {
      tmp = io->read_uint32_be();
      if (tmp == FOURCC('i', 's', 'o', 'm'))
        mxverb(2, PFX "File type major brand: ISO Media File\n");
      else
        mxwarn(PFX "Unknown file type major brand: %c%c%c%c\n", BE2STR(tmp));
      tmp = io->read_uint32_be();
      mxverb(2, PFX "File type minor brand: %c%c%c%c\n", BE2STR(tmp));
      for (i = 0; i < ((atom_size - 16) / 4); i++) {
        tmp = io->read_uint32();
        mxverb(2, PFX "File type compatible brands #%d: %.4s\n", i, &tmp);
      }

    } else if (atom == FOURCC('m', 'o', 'o', 'v')) {
      handle_header_atoms(atom, atom_size - atom_hsize,
                          atom_pos + atom_hsize, 1);

    } else if (atom == FOURCC('w', 'i', 'd', 'e')) {
      skip_atom();

    } else if (atom == FOURCC('m', 'd', 'a', 't')) {
      headers_parsed = true;

    } else if ((atom == FOURCC('f', 'r', 'e', 'e')) ||
               (atom == FOURCC('s', 'k', 'i', 'p')) ||
               (atom == FOURCC('j', 'u', 'n', 'k')) ||
               (atom == FOURCC('p', 'n', 'o', 't')) ||
               (atom == FOURCC('P', 'I', 'C', 'T'))) {
      skip_atom();

    }

  } while (!headers_parsed);

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
        } else {
          demuxers.push_back(new_dmx);
          new_dmx = NULL;
        }

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
        new_dmx->duration = get_uint32_be(&mdhd.duration);

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
        new_dmx->duration_table = (qt_sample_duration_t *)
          safemalloc(count * sizeof(qt_sample_duration_t));
        for (i = 0; i < count; i++) {
          new_dmx->duration_table[i].number = io->read_uint32_be();
          new_dmx->duration_table[i].duration = io->read_uint32_be();
        }
        new_dmx->duration_table_len = count;

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
          }

          io->setFilePointer(pos + size);
        }

      } else if (atom == FOURCC('s', 't', 's', 's')) {
        uint32_t count, i;

        io->skip(1 + 3);        // version & flags
        count = io->read_uint32_be();
        new_dmx->sync_table = (uint32_t *)safemalloc(count * 4);

        for (i = 0; i < count; i++)
          new_dmx->sync_table[i] = io->read_uint32_be();
        new_dmx->sync_table_len = count;

        mxverb(2, PFX "%*sSync table: %u entries\n", (level + 1) * 2, "",
               count);

      } else if (atom == FOURCC('s', 't', 's', 'c')) {
        uint32_t count, i;

        io->skip(1 + 3);        // version & flags
        count = io->read_uint32_be();
        new_dmx->sample_to_chunk_table = (qt_sample_to_chunk_t *)
          safemalloc(count * sizeof(qt_sample_to_chunk_t));
        for (i = 0; i < count; i++) {
          new_dmx->sample_to_chunk_table[i].first_chunk = io->read_uint32_be();
          new_dmx->sample_to_chunk_table[i].samples_per_chunk =
            io->read_uint32_be();
          new_dmx->sample_to_chunk_table[i].description_id =
            io->read_uint32_be();
        }
        new_dmx->sample_to_chunk_table_len = count;

        mxverb(2, PFX "%*sSample to chunk table: %u entries\n",
               (level + 1) * 2, "", count);

      } else if (atom == FOURCC('s', 't', 's', 'z')) {
        uint32_t count, i, sample_size;

        io->skip(1 + 3);        // version & flags
        sample_size = io->read_uint32_be();
        count = io->read_uint32_be();
        if (sample_size == 0) {
          new_dmx->sample_size_table = (uint32_t *)safemalloc(count * 4);
          for (i = 0; i < count; i++)
            new_dmx->sample_size_table[i] = io->read_uint32_be();
          new_dmx->sample_size_table_len = count;

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
        new_dmx->chunk_offset_table = (uint64_t *)safemalloc(count * 8);
        new_dmx->chunk_offset_table_len = count;

        for (i = 0; i < count; i++)
          new_dmx->chunk_offset_table[i] = io->read_uint32_be();

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
      }

    }

    skip_atom();
    parent_size -= atom_size;
  }

  io->setFilePointer(target_pos);
}

int qtmp4_reader_c::read() {
  return 0;
}

packet_t *qtmp4_reader_c::get_packet() {
  return NULL;
}

int qtmp4_reader_c::display_priority() {
  return -1;
}

void qtmp4_reader_c::display_progress() {
}

void qtmp4_reader_c::set_headers() {
}

void qtmp4_reader_c::identify() {
}

void qtmp4_reader_c::create_packetizers() {
}

