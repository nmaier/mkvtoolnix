/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  dts_common.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: dts_common.cpp,v 1.1 2003/05/15 08:58:52 mosu Exp $
    \brief helper function for DTS data
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <string.h>
#include <stdio.h>

#include "dts_common.h"

static int DTS_SAMPLEFREQS[16] =
{
      0,    8000,    16000,   32000,   64000,  128000,   11025,    22050,
  44100,   88200,   176400,   12000,   24000,   48000,   96000,   192000
};

static int DTS_BITRATES[30] =
{
    32000,     56000,     64000,     96000,    112000,    128000,    192000,
   224000,    256000,    320000,    384000,    448000,    512000,    576000,
   640000,    768000,    896000,   1024000,   1152000,   1280000,   1344000,
  1408000,   1411200,   1472000,   1536000,   1920000,   2048000,   3072000,
  3840000,   4096000
};


int find_dts_header(unsigned char *buf, int size, dts_header_t *dts_header) {
  // dts_header_t header;
  int i;
  unsigned char * indata_ptr;
  int ftype;
  int surp;
  int unknown_bit;
  int fsize;
  int amode;
  int nblks;
  int sfreq;
  int rate;

  for (i = 0; i < (size - 9); i++) {
    if ((buf[i] != 0x7f) || (buf[i + 1] != 0xfe) || (buf[i + 2] != 0x80) ||
        (buf[i + 3] != 0x01))
      continue;
    
    indata_ptr = buf + i;
   
    ftype = indata_ptr[4] >> 7;

    surp = (indata_ptr[4] >> 2) & 0x1f;
    surp = (surp + 1) % 32;
    //fprintf(stderr,"surp = %d\n", surp);
   
    unknown_bit = (indata_ptr[4] >> 1) & 0x01;

    nblks = (indata_ptr[4] & 0x01) << 6 | (indata_ptr[5] >> 2);
    nblks = nblks + 1;

    fsize = (indata_ptr[5] & 0x03) << 12 | (indata_ptr[6] << 4) |
      (indata_ptr[7] >> 4);
    fsize = fsize + 1;

    amode = (indata_ptr[7] & 0x0f) << 2 | (indata_ptr[8] >> 6);
    sfreq = (indata_ptr[8] >> 2) & 0x0f;
    rate = (indata_ptr[8] & 0x03) << 3 | ((indata_ptr[9] >> 5) & 0x07);

    if (ftype != 1) {
      // "DTS: Termination frames not handled, REPORT BUG\n";
      return -1;
    }

    if (sfreq != 13) {
      // "DTS: Only 48kHz supported, REPORT BUG\n";
      return -1;
    }

    if ((fsize > 8192) || (fsize < 96))  {
      // "DTS: fsize: %d invalid, REPORT BUG\n", fsize;
      return -1;
    }

    if ((nblks != 8) &&
        (nblks != 16) &&
        (nblks != 32) &&
        (nblks != 64) &&
        (nblks != 128) &&
        (ftype == 1)) {
      // "DTS: nblks %d not valid for normal frame, REPORT BUG\n", nblks;
      return -1;
    }
   
    // header ok
    dts_header->sample_rate = DTS_SAMPLEFREQS[sfreq];
    dts_header->bit_rate = DTS_BITRATES[rate];
    dts_header->bytes = fsize;
   
    return i;
  }
  
  return -1;
}
