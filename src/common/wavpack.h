/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for the WAVPACK file format

   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Based on a software from David Bryant <dbryant@impulse.net>.
*/

#ifndef __MTX_COMMON_WAVPACK_COMMON_H
#define __MTX_COMMON_WAVPACK_COMMON_H

#include "common/common_pch.h"

#include "common/mm_io.h"

/* All integers are little endian. */

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif
struct PACKED_STRUCTURE wavpack_header_t {
  char ck_id [4];         // "wvpk"
  uint32_t ck_size;       // size of entire frame (minus 8, of course)
  uint16_t version;       // 0x403 for now
  uint8_t track_no;       // track number (0 if not used, like now)
  uint8_t index_no;       // remember these? (0 if not used, like now)
  uint32_t total_samples; // for entire file (-1 if unknown)
  uint32_t block_index;   // index of first sample in block (to file begin)
  uint32_t block_samples; // # samples in this block
  uint32_t flags;         // various flags for id and decoding
  uint32_t crc;           // crc for actual decoded data
};
#if defined(COMP_MSC)
#pragma pack(pop)
#endif

struct wavpack_meta_t {
  int channel_count;
  int bits_per_sample;
  uint32_t sample_rate;
  uint32_t samples_per_block;
  bool has_correction;

  wavpack_meta_t();
};

// or-values for "flags"

#define WV_BYTES_STORED    3    // 1-4 bytes/sample
#define WV_MONO_FLAG       4    // not stereo
#define WV_HYBRID_FLAG     8    // hybrid mode
#define WV_JOINT_STEREO    0x10 // joint stereo
#define WV_CROSS_DECORR    0x20 // no-delay cross decorrelation
#define WV_HYBRID_SHAPE    0x40 // noise shape (hybrid mode only)
#define WV_FLOAT_DATA      0x80 // ieee 32-bit floating point data

#define WV_INT32_DATA      0x100  // special extended int handling
#define WV_HYBRID_BITRATE  0x200  // bitrate noise (hybrid mode only)
#define WV_HYBRID_BALANCE  0x400  // balance noise (hybrid stereo mode only)

#define WV_INITIAL_BLOCK   0x800   // initial block of multichannel segment
#define WV_FINAL_BLOCK     0x1000  // final block of multichannel segment

#define WV_SHIFT_LSB      13
#define WV_SHIFT_MASK     (0x1fL << WV_SHIFT_LSB)

#define WV_MAG_LSB        18
#define WV_MAG_MASK       (0x1fL << WV_MAG_LSB)

#define WV_SRATE_LSB      23
#define WV_SRATE_MASK     (0xfL << WV_SRATE_LSB)

#define WV_IGNORED_FLAGS  0x18000000  // reserved, but ignore if encountered
#define WV_NEW_SHAPING    0x20000000  // use IIR filter for negative shaping
#define WV_UNKNOWN_FLAGS  0xC0000000  // also reserved, but refuse decode if
                                      //  encountered

int32_t wv_parse_frame(mm_io_c &mm_io, wavpack_header_t &header, wavpack_meta_t &meta, bool read_blocked_frames, bool keep_initial_position);

#endif // __MTX_COMMON_WAVPACK_COMMON_H
