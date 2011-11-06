/*
   base64.cpp
   Base64 encoding and decoding.

   Part of this file (the two encoding functions) were adopted from
   http://base64.sourceforge.net/ and are licensed under the MIT license
   (following):

   LICENCE:     Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

                Permission is hereby granted, free of charge, to any person
                obtaining a copy of this software and associated
                documentation files (the "Software"), to deal in the
                Software without restriction, including without limitation
                the rights to use, copy, modify, merge, publish, distribute,
                sublicense, and/or sell copies of the Software, and to
                permit persons to whom the Software is furnished to do so,
                subject to the following conditions:

                The above copyright notice and this permission notice shall
                be included in all copies or substantial portions of the
                Software.

                THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
                KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
                WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
                PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
                OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
                OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
                OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
                SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   The decoding function was adapted from
   http://sourceforge.net/projects/base64decoder/ and is licensed under
   the GPL v2 or later. See the file COPYING for details.

   base64 encoding and decoding functions

   See the above URLs for the original authors.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>

#include "common/common_pch.h"

#include "common/base64.h"
#include "common/error.h"

static const char base64_encoding[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
encode_block(const unsigned char in[3],
             int len,
             std::string &out) {
  out += base64_encoding[in[0] >> 2];
  out += base64_encoding[((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)];
  out += (unsigned char)(len > 1 ? base64_encoding[((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)] : '=');
  out += (unsigned char)(len > 2 ? base64_encoding[in[2] & 0x3f]                                  : '=');
}

std::string
base64_encode(const unsigned char *src,
              int src_len,
              bool line_breaks,
              int max_line_len) {
  int pos        = 0;
  int blocks_out = 0;

  if (4 > max_line_len)
    max_line_len = 4;

  int i, len_mod = max_line_len / 4;
  std::string out;

  while (pos < src_len) {
    unsigned char in[3];
    int len = 0;
    for (i = 0; 3 > i; ++i) {
      if (pos < src_len) {
        ++len;
        in[i] = (unsigned char)src[pos];
      } else
        in[i] = 0;
      ++pos;
    }

    encode_block(in, len, out);

    ++blocks_out;
    if (line_breaks && ((blocks_out % len_mod) == 0))
      out += "\n";
  }

  return out;
}

int
base64_decode(const std::string &src,
              unsigned char *dst) {
  unsigned int pos     = 0;
  unsigned int dst_pos = 0;
  unsigned int pad     = 0;
  while (pos < src.size()) {
    unsigned char in[3];
    unsigned int in_pos = 0;

    while ((src.size() > pos) && (4 > in_pos)) {
      unsigned char c = (unsigned char)src[pos];
      ++pos;

      if ((('A' <= c) && ('Z' >= c)) || (('a' <= c) && ('z' >= 'c')) || (('0' <= c) && ('9' >= c)) || ('+' == c) || ('/' == c)) {
        in[in_pos] = c;
        ++in_pos;

      } else if (c == '=')
        pad++;

      else if (!isblanktab(c) && !iscr(c))
        return -1;
    }

    unsigned int values_idx;
    unsigned char values[4];
    for (values_idx = 0; values_idx < in_pos; values_idx++) {
      values[values_idx] =
          (('A' <= in[values_idx]) && ('Z' >= in[values_idx])) ? in[values_idx] - 'A'
        : (('a' <= in[values_idx]) && ('z' >= in[values_idx])) ? in[values_idx] - 'a' + 26
        : (('0' <= in[values_idx]) && ('9' >= in[values_idx])) ? in[values_idx] - '0' + 52
        :  ('+' == in[values_idx])                             ?  62
        :  ('/' == in[values_idx])                             ?  63
        :                                                        255;

      if (255 == values[values_idx])
        throw mtx::base64::invalid_data_x();
    }

    unsigned char mid[6];
    mid[0] = values[0] << 2;
    mid[1] = values[1] >> 4;
    mid[2] = values[1] << 4;
    mid[3] = values[2] >> 2;
    mid[4] = values[2] << 6;
    mid[5] = values[3];

    dst[dst_pos] = mid[0] | mid[1];
    dst_pos++;
    if (1 >= pad) {
      dst[dst_pos] = mid[2] | mid[3];
      dst_pos++;
      if (0 == pad) {
        dst[dst_pos] = mid[4] | mid[5];
        dst_pos++;
      }
    }

    if (0 != pad)
      return dst_pos;
  }

  return dst_pos;
}
