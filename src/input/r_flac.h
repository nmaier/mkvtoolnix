/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_flac.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definitions for the raw FLAC stream reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_FLAC_H
#define __R_FLAC_H

#include "os.h"

#include "mm_io.h"

#if defined(HAVE_FLAC_FORMAT_H)
#include <vector>

#include <FLAC/stream_decoder.h>

#include "p_flac.h"

#define FLAC_BLOCK_TYPE_HEADERS 0
#define FLAC_BLOCK_TYPE_DATA    1

typedef struct {
  int64_t filepos;
  int type, len, samples;
} flac_block_t;

class flac_reader_c: public generic_reader_c {
private:
  mm_io_c *file;
  unsigned char *read_buffer;
  int pos, size, channels, sample_rate, bits_per_sample;
  bool metadata_parsed, done;
  int64_t samples, packet_start, file_size, old_progress;
  vector<flac_block_t> blocks;
  vector<flac_block_t>::iterator current_block;

public:
  flac_reader_c(track_info_c *nti) throw (error_c);
  virtual ~flac_reader_c();

  virtual int read(generic_packetizer_c *ptzr);
  virtual void identify();

  virtual int display_priority();
  virtual void display_progress(bool final = false);

  static int probe_file(mm_io_c *mm_io, int64_t size);

  virtual FLAC__StreamDecoderReadStatus
  read_cb(FLAC__byte buffer[], unsigned *bytes);

  virtual FLAC__StreamDecoderWriteStatus
  write_cb(const FLAC__Frame *frame, const FLAC__int32 * const data[]);

  virtual void metadata_cb(const FLAC__StreamMetadata *metadata);
  virtual void error_cb(FLAC__StreamDecoderErrorStatus status);

protected:
  virtual bool parse_file();
  virtual bool fill_buffer();
};

#else  // HAVE_FLAC_FORMAT_H

class flac_reader_c {
public:
  static int probe_file(mm_io_c *file, int64_t size);
};

#endif // HAVE_FLAC_FORMAT_H


#endif  // __R_FLAC_H
