/*
  base64.cpp
  Base64 encoding and decoding.

  Written by Moritz Bunkus <moritz@bunkus.org>

  Part of this file (the two encoding functions) were adopted from
  http://base64.sourceforge.net/ and are licensed under the MIT license
  (following):

LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

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

*/

/*!
    \file
    \version $Id$
    \brief base64 encoding and decoding functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>

#include <exception>
#include <string>

#include "base64.h"

using namespace std;

static const char base64_encoding[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijkl"
"mnopqrstuvwxyz0123456789+/";

static void encode_block(const unsigned char in[3], int len, string &out) {
  out += base64_encoding[in[0] >> 2];
  out += base64_encoding[((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)];
  out += (unsigned char)
    (len > 1 ? base64_encoding[((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)] :
     '=');
  out += (unsigned char)(len > 2 ? base64_encoding[in[2] & 0x3f] : '=');
}

string base64_encode(const unsigned char *src, int src_len, bool line_breaks) {
  unsigned char in[3];
  int pos, i, len, blocks_out;
  string out;

  pos = 0;
  blocks_out = 0;

  while (pos < src_len) {
    len = 0;
    for (i = 0; i < 3; i++) {
      if (pos < src_len) {
        len++;
        in[i] = (unsigned char)src[pos];
      } else
        in[i] = 0;
      pos++;
    }
    encode_block(in, len, out);
    blocks_out++;
    if (line_breaks && ((blocks_out % 18) == 0))
      out += "\n";
  }

  return out;
}


int base64_decode(const string &src, unsigned char *dst) {
  unsigned char in[4], values[4], mid[6], c;
  int i, k, pos, dst_pos, pad;

  pos = 0;
  dst_pos = 0;
  pad = 0;
  while (pos < src.size()) {
    i = 0;
    while ((pos < src.size()) && (i < 4)) {
      c = (unsigned char)src[pos];
      pos++;

      if (((c >= 'A') && (c <= 'Z')) ||
          ((c >= 'a') && (c <= 'z')) ||
          ((c >= '0') && (c <= '9')) ||
          (c == '+') || (c == '/')) {
        in[i] = c;
        i++;
      } else if (c == '=')
        pad++;
      else if (!isspace(c) && (c != '\r') && (c != '\n'))
        return -1;
    }

    for (k = 0; k < i; k++) {
      if ((in[k] >= 'A') && (in[k] <= 'Z'))
        values[k] = in[k] - 'A';
      else if ((in[k] >= 'a') && (in[k] <= 'z'))
        values[k] = in[k] - 'a' + 26;
      else if ((in[k] >= '0') && (in[k] <= '9'))
        values[k] = in[k] - '0' + 52;
      else if (in[k] == '+')
        values[k] = 62;
      else if (in[k] == '/')
        values[k] = 63;
      else
        throw exception();
    }

    mid[0] = values[0] << 2;
    mid[1] = values[1] >> 4;
    mid[2] = values[1] << 4;
    mid[3] = values[2] >> 2;
    mid[4] = values[2] << 6;
    mid[5] = values[3];

    dst[dst_pos] = mid[0] | mid[1];
    dst_pos++;
    if (pad <= 1) {
      dst[dst_pos] = mid[2] | mid[3];
      dst_pos++;
      if (pad == 0) {
        dst[dst_pos] = mid[4] | mid[5];
        dst_pos++;
      }
    }

    if (pad != 0)
      return dst_pos;
  }

  return dst_pos;
}
