/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * class definitions for the raw FLAC stream reader
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __R_FLAC_H
#define __R_FLAC_H

#include "os.h"

#include "mm_io.h"

#if defined(HAVE_FLAC_FORMAT_H)
#include <vector>

#include <FLAC/seekable_stream_decoder.h>

#include "p_flac.h"

#define FLAC_BLOCK_TYPE_HEADERS 0
#define FLAC_BLOCK_TYPE_DATA    1

typedef struct {
  int64_t filepos;
  int type, len;
} flac_block_t;

class flac_reader_c: public generic_reader_c {
private:
  mm_io_c *file;
  int sample_rate;
  bool metadata_parsed;
  int64_t samples, file_size;
  unsigned char *header;
  int header_size;
  vector<flac_block_t> blocks;
  vector<flac_block_t>::iterator current_block;
  FLAC__StreamMetadata_StreamInfo stream_info;

public:
  flac_reader_c(track_info_c *nti) throw (error_c);
  virtual ~flac_reader_c();

  virtual file_status_t read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  virtual int get_progress();

  static int probe_file(mm_io_c *mm_io, int64_t size);

  virtual FLAC__SeekableStreamDecoderReadStatus
  read_cb(FLAC__byte buffer[], unsigned *bytes);

  virtual FLAC__StreamDecoderWriteStatus
  write_cb(const FLAC__Frame *frame, const FLAC__int32 * const data[]);

  virtual void metadata_cb(const FLAC__StreamMetadata *metadata);
  virtual void error_cb(FLAC__StreamDecoderErrorStatus status);
  virtual FLAC__SeekableStreamDecoderSeekStatus
  seek_cb(uint64_t new_pos);
  virtual FLAC__SeekableStreamDecoderTellStatus
  tell_cb(uint64_t &absolute_byte_offset);
  virtual FLAC__SeekableStreamDecoderLengthStatus
  length_cb(uint64_t &stream_length);
  virtual FLAC__bool eof_cb();

protected:
  virtual bool parse_file();
};

#else  // HAVE_FLAC_FORMAT_H

class flac_reader_c {
public:
  static int probe_file(mm_io_c *file, int64_t size);
};

#endif // HAVE_FLAC_FORMAT_H


#endif  // __R_FLAC_H
