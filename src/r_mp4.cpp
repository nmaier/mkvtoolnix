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

#include "common.h"
#include "mkvmerge.h"
#include "r_mp4.h"

using namespace std;
using namespace libmatroska;

int qtmp4_reader_c::probe_file(mm_io_c *in, int64_t size) {
  uint32_t objsize, object;

  if (size < 20)
    return 0;
  try {
    in->setFilePointer(0, seek_beginning);

    objsize = in->read_uint32_be();
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
      throw error_c("quicktime_mp4_reader: Source is not a valid Quicktime/"
                    "MP4 file.");

    done = false;

    if (verbose)
      mxinfo("Using Quicktime/MP4 demultiplexer for %s.\n", ti->fname);

    parse_headers();
    if (!identifying)
      create_packetizers();

  } catch (exception &ex) {
    throw error_c("quicktime_mp4_reader: Could not read the source file.");
  }
}

qtmp4_reader_c::~qtmp4_reader_c() {
  delete io;
}

#define BE2STR(a) ((char *)&a)[3], ((char *)&a)[2], ((char *)&a)[1], \
                  ((char *)&a)[0]

void qtmp4_reader_c::parse_headers() {
  uint32_t objsize, object, tmp;
  bool headers_parsed;
  int i;

  io->setFilePointer(0);

  headers_parsed = false;
  do {
    objsize = io->read_uint32_be();
    object = io->read_uint32_be();

    if (objsize < 8)
      mxerror("quicktime_mp4_reader: Invalid chunk size %u at %lld.\n",
              objsize, io->getFilePointer() - 8);

    if (object == FOURCC('f', 't', 'y', 'p')) {
      mxverb(2, "quicktime_mp4_reader: ftyp header at %lld\n",
             io->getFilePointer() - 8);

      tmp = io->read_uint32_be();
      if (tmp == FOURCC('i', 's', 'o', 'm'))
        mxverb(2, "quicktime_mp4_reader: File type major brand: ISO Media "
               "File\n");
      else
        mxwarn("quicktime_mp4_reader: Unknown file type major brand: %.4s\n",
               BE2STR(tmp));
      tmp = io->read_uint32_be();
      mxverb(2, "quicktime_mp4_reader: File type minor brand: %.4s\n",
             BE2STR(tmp));
      for (i = 0; i < ((objsize - 16) / 4); i++) {
        tmp = io->read_uint32();
        mxverb(2, "quicktime_mp4_reader: File type compatible brands #%d: "
               "%.4s\n", i, &tmp);
      }

    } else if (object == FOURCC('m', 'o', 'o', 'v')) {
      mxverb(2, "quicktime_mp4_reader: moov header at %lld\n",
             io->getFilePointer() - 8);

      io->skip(4);              // sub object size?
      tmp = io->read_uint32_be();
      if (tmp == FOURCC('r', 'm', 'r', 'a'))
        mxerror("quicktime_mp4_reader: Reference/Playlist Media Files are "
                "not supported.\n");

      io->skip(objsize - 8 - 8);

    } else if (object == FOURCC('w', 'i', 'd', 'e')) {
      mxverb(2, "quicktime_mp4_reader: wide header at %lld\n",
             io->getFilePointer() - 8);
      io->skip(objsize - 8);

    } else if (object == FOURCC('m', 'd', 'a', 't')) {
      mxverb(2, "quicktime_mp4_reader: mdat header at %lld\n",
             io->getFilePointer() - 8);
      io->skip(objsize - 8);

    } else if ((object == FOURCC('f', 'r', 'e', 'e')) ||
               (object == FOURCC('s', 'k', 'i', 'p')) ||
               (object == FOURCC('j', 'u', 'n', 'k'))) {
      mxverb(2, "quicktime_mp4_reader: %c%c%c%c header at %lld\n",
             ((char *)&object)[3], ((char *)&object)[2],
             ((char *)&object)[1], ((char *)&object)[0],
             io->getFilePointer() - 8);
      io->skip(objsize - 8);

    } else if ((object == FOURCC('p', 'n', 'o', 't')) ||
               (object == FOURCC('P', 'I', 'C', 'T'))) {
      mxverb(2, "quicktime_mp4_reader: %c%c%c%c header at %lld\n",
             ((char *)&object)[3], ((char *)&object)[2],
             ((char *)&object)[1], ((char *)&object)[0],
             io->getFilePointer() - 8);
      io->skip(objsize - 8);

    } else {
      mxwarn("quicktime_mp4_reader: Unknown/unsupported chunk type '%c%c%c%c'"
             " with size %lld found at %lld. Skipping.\n",
             ((char *)&object)[3], ((char *)&object)[2],
             ((char *)&object)[1], ((char *)&object)[0], objsize,
             io->getFilePointer() - 8);
      io->skip(objsize - 8);

    }

  } while (!headers_parsed);
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

