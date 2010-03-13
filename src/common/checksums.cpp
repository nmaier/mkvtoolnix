/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   checksum calculations

   Written by Moritz Bunkus <moritz@bunkus.org>.

   The code dealing with muliple CRC variations was taken from the ffmpeg
     project, files "libavutil/crc.h" and "libavutil/crc.c".
*/

#include "common/common.h"

#include "common/bswap.h"
#include "common/checksums.h"
#include "common/endian.h"

#define BASE 65521
#define A0 check += *buffer++; sum2 += check;
#define A1 A0 A0
#define A2 A1 A1
#define A3 A2 A2
#define A4 A3 A3
#define A5 A4 A4
#define A6 A5 A5

uint32_t
calc_adler32(const unsigned char *buffer,
             int size) {
  register uint32_t sum2, check;
  register int k;

  check = 1;
  k = size;

  sum2 = (check >> 16) & 0xffffL;
  check &= 0xffffL;
  while (k >= 64) {
    A6;
    k -= 64;
  }

  if (k)
    do {
      A0;
    } while (--k);

  check %= BASE;
  check |= (sum2 % BASE) << 16;

  return check;
}

/*
   The following code was taken from the ffmpeg project, files
   "libavutil/crc.h" and "libavutil/crc.c".

   Its license here is the GPL.
 */

static struct {
  uint8_t  le;
  uint8_t  bits;
  uint32_t poly;
} s_crc_table_params[CRC_MAX] = {
  { 0,  8,       0x07 },
  { 0, 16,     0x8005 },
  { 0, 16,     0x1021 },
  { 0, 32, 0x04C11DB7 },
  { 1, 32, 0xEDB88320 },
};
static uint32_t s_crc_table[CRC_MAX][257];

int
crc_init(uint32_t *ctx,
         int le,
         int bits,
         uint32_t poly,
         int ctx_size) {
  int i, j;
  uint32_t c;

  if ((bits < 8) || (bits > 32) || (poly >= (1LL<<bits)))
    return -1;

  if ((ctx_size != sizeof(uint32_t) * 257) && (ctx_size != sizeof(uint32_t) * 1024))
    return -1;

  for (i = 0; i < 256; i++) {
    if (le) {
      for (c = i, j = 0; j < 8; j++)
        c = (c >> 1) ^ (poly & (-(c & 1)));
      ctx[i] = c;

    } else {
      for (c = i << 24, j = 0; j < 8; j++)
        c = (c << 1) ^ ((poly << (32 - bits)) & (((int32_t)c) >> 31));
      ctx[i] = bswap_32(c);
    }
  }

  ctx[256] = 1;

  if(ctx_size >= sizeof(uint32_t) * 1024)
    for (i = 0; i < 256; i++)
      for(j=0; j<3; j++)
        ctx[256 * (j + 1) + i] = (ctx[256 * j + i] >> 8) ^ ctx[ctx[256 * j + i] & 0xff];

  return 0;
}

const uint32_t *
crc_get_table(crc_type_e crc_id){
  if (!s_crc_table[crc_id][256])
    if (crc_init(s_crc_table[crc_id], s_crc_table_params[crc_id].le, s_crc_table_params[crc_id].bits, s_crc_table_params[crc_id].poly, sizeof(s_crc_table[crc_id])) < 0)
      return NULL;

  return s_crc_table[crc_id];
}

uint32_t
crc_calc(const uint32_t *ctx,
         uint32_t crc,
         const unsigned char *buffer,
         size_t length) {
  const uint8_t *end = buffer + length;

  if (!ctx[256])
    while (buffer < (end - 3)){
      crc    ^= get_uint32_le(buffer);
      crc     =   ctx[3 * 256 + ( crc        & 0xff)]
                ^ ctx[2 * 256 + ((crc >>  8) & 0xff)]
                ^ ctx[1 * 256 + ((crc >> 16) & 0xff)]
                ^ ctx[0 * 256 + ((crc >> 24)       )];
      buffer += 4;
    }

  while(buffer < end)
    crc = ctx[((uint8_t)crc) ^ *buffer++] ^ (crc >> 8);

  return crc;
}
