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
 * FLAC helper functions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include "config.h"

#if defined(HAVE_FLAC_FORMAT_H)

#include <stdarg.h>

#include <FLAC/stream_decoder.h>

#include "common.h"
#include "flac_common.h"

static bool
flac_skip_utf8(bit_cursor_c &bits,
               int size) {
  unsigned long value;
  int num;

  if (!bits.get_bits(8, value))
    return false;

	if (!(value & 0x80))          /* 0xxxxxxx */
		num = 0;
	else if ((value & 0xC0) && !(value & 0x20)) /* 110xxxxx */
		num = 1;
	else if ((value & 0xE0) && !(value & 0x10)) /* 1110xxxx */
		num = 2;
	else if ((value & 0xF0) && !(value & 0x08)) /* 11110xxx */
		num = 3;
	else if ((value & 0xF8) && !(value & 0x04)) /* 111110xx */
		num = 4;
	else if ((value & 0xFC) && !(value & 0x02)) /* 1111110x */
    num = 5;
  else if ((size == 64) && (value & 0xFE) && !(value & 0x01)) /* 11111110 */
    num = 6;
  else
    return false;

  return bits.skip_bits(num * 8);
}

// See http://flac.sourceforge.net/format.html#frame_header
int
flac_get_num_samples(unsigned char *mem,
                     int size,
                     FLAC__StreamMetadata_StreamInfo &stream_info) {
  bit_cursor_c bits(mem, size);
  unsigned long value;
  int free_sample_size;
  int samples;

  // Sync word: 11 1111 1111 1110
  if (!bits.peek_bits(14, value))
    return -1;

  if (value != 0x3ffe)
    return -1;

  bits.skip_bits(14);

  // Reserved
  if (!bits.skip_bits(2))
    return -1;

  // Block size
  if (!bits.get_bits(4, value))
    return -1;

  free_sample_size = 0;
  samples = 0;
  if (value == 0)
    samples = stream_info.min_blocksize;
  else if (value == 1)
    samples = 192;
  else if ((value >= 2) && (value <= 5))
    samples = 576 << (value - 2);
  else if (value >= 8)
    samples = 256 << (value - 8);
  else
    free_sample_size = value;

  // Sample rate
  if (!bits.get_bits(4, value))
    return -1;

  // Channel assignment
  if (!bits.get_bits(4, value))
    return -1;

  // Sample size (3 bits) and zero bit padding (1 bit)
  if (!bits.get_bits(4, value))
    return -1;

  if (stream_info.min_blocksize != stream_info.max_blocksize) {
    if (!flac_skip_utf8(bits, 64))
      return -1;
  } else if (!flac_skip_utf8(bits, 32))
      return -1;

  if ((free_sample_size == 6) || (free_sample_size == 7)) {
    if (!bits.get_bits(8, value))
      return -1;

    samples = value;
    if (free_sample_size == 7) {
      if (!bits.get_bits(8, value))
        return -1;
      samples <<= 8;
      samples |= value;
    }
    samples++;
  }

  // CRC
  if (!bits.skip_bits(8))
    return -1;

  return samples;
}

#define FPFX "flac_decode_headers: "

typedef struct {
  unsigned char *mem;
  int size;
  int nread;

  FLAC__StreamMetadata_StreamInfo stream_info;
  bool stream_info_found;
} flac_header_extractor_t;

static FLAC__StreamDecoderReadStatus
flac_read_cb(const FLAC__StreamDecoder *,
             FLAC__byte buffer[],
             unsigned *bytes,
             void *client_data) {
  flac_header_extractor_t *fhe;
  int num_bytes;

  fhe = (flac_header_extractor_t *)client_data;
  if (fhe->nread == fhe->size)
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  num_bytes = *bytes > (fhe->size - fhe->nread) ? (fhe->size - fhe->nread) :
    *bytes;
  memcpy(buffer, &fhe->mem[fhe->nread], num_bytes);
  fhe->nread += num_bytes;
  *bytes = num_bytes;

  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static void
flac_metadata_cb(const FLAC__StreamDecoder *,
                 const FLAC__StreamMetadata *metadata,
                 void *client_data) {
  flac_header_extractor_t *fhe;

  fhe = (flac_header_extractor_t *)client_data;
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    memcpy(&fhe->stream_info, &metadata->data.stream_info,
           sizeof(FLAC__StreamMetadata_StreamInfo));
    fhe->stream_info_found = true;
  }
}

static FLAC__StreamDecoderWriteStatus
flac_write_cb(const FLAC__StreamDecoder *,
              const FLAC__Frame *frame,
              const FLAC__int32 * const [],
              void *) {
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void
flac_error_cb(const FLAC__StreamDecoder *,
              FLAC__StreamDecoderErrorStatus,
              void *) {
}

int
flac_decode_headers(unsigned char *mem,
                    int size,
                    int num_elements,
                    ...) {
  flac_header_extractor_t fhe;
  FLAC__StreamDecoder *decoder;
  int result, i;
  va_list ap;

  if ((mem == NULL) || (size <= 0))
    return -1;

  memset(&fhe, 0, sizeof(flac_header_extractor_t));
  fhe.mem = mem;
  fhe.size = size;

  decoder = FLAC__stream_decoder_new();
  if (decoder == NULL)
    mxerror(FPFX "FLAC__stream_decoder_new() failed.\n");
  FLAC__stream_decoder_set_client_data(decoder, &fhe);
  if (!FLAC__stream_decoder_set_read_callback(decoder, flac_read_cb))
    mxerror(FPFX "Could not set the read callback.\n");
  if (!FLAC__stream_decoder_set_write_callback(decoder, flac_write_cb))
    mxerror(FPFX "Could not set the write callback.\n");
  if (!FLAC__stream_decoder_set_metadata_callback(decoder, flac_metadata_cb))
    mxerror(FPFX "Could not set the metadata callback.\n");
  if (!FLAC__stream_decoder_set_error_callback(decoder, flac_error_cb))
    mxerror(FPFX "Could not set the error callback.\n");
  if (!FLAC__stream_decoder_set_metadata_respond_all(decoder))
    mxerror(FPFX "Could not set metadata_respond_all.\n");
  if (FLAC__stream_decoder_init(decoder) !=
      FLAC__STREAM_DECODER_SEARCH_FOR_METADATA)
    mxerror(FPFX "Could not initialize the FLAC decoder.\n");
  FLAC__stream_decoder_process_until_end_of_stream(decoder);

  result = 0;
  if (fhe.stream_info_found)
    result |= FLAC_HEADER_STREAM_INFO;

  va_start(ap, num_elements);
  for (i = 0; i < num_elements; i++) {
    int type;

    type = va_arg(ap, int);
    switch (type) {
      case FLAC_HEADER_STREAM_INFO: {
        FLAC__StreamMetadata_StreamInfo *stream_info;

        stream_info = va_arg(ap, FLAC__StreamMetadata_StreamInfo *);
        if (result & FLAC_HEADER_STREAM_INFO)
          memcpy(stream_info, &fhe.stream_info,
                 sizeof(FLAC__StreamMetadata_StreamInfo));
        break;
      }

      default:
        type = va_arg(ap, int);
    }
  }

  return result;
}

#endif
