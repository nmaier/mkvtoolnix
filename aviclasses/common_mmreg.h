// Some common tools
// Copyright (C) 2003  Julien 'Cyrius' Coloos
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
// or visit http://www.gnu.org/licenses/gpl.html

#ifndef __TOOLS_COMMON_MMREG_H__
#define __TOOLS_COMMON_MMREG_H__

#include "os.h"

#include "ac_common.h"
#include "common_endian.h"

//////////////////////////////////////////////////////////////////////
// Some data 'copied' from mmreg.h (Microsoft files)

typedef struct __attribute__((__packed__)) w32waveformat_tag {
     uint16_le  wFormatTag;      /* format type */
     uint16_le  nChannels;      /* number of channels (i.e. mono, stereo...) */
     uint32_le    nSamplesPerSec;    /* sample rate */
     uint32_le    nAvgBytesPerSec;  /* for buffer estimation */
     uint16_le    nBlockAlign;    /* block size of data */
} w32WAVEFORMAT;

#define WAVE_FORMAT_PCM     1

typedef struct __attribute__((__packed__)) w32pcmwaveformat_tag {
     w32WAVEFORMAT  wf;
     uint16_le  wBitsPerSample;
} w32PCMWAVEFORMAT;


#undef WAVE_FORMAT_MPEG
#undef WAVE_FORMAT_MPEGLAYER3
#ifndef WAVE_FORMAT_MPEG
#  define  WAVE_FORMAT_MPEG        0x0050 // Microsoft Corporation
#endif
#ifndef WAVE_FORMAT_MPEGLAYER3
#  define  WAVE_FORMAT_MPEGLAYER3  0x0055 // ISO/MPEG Layer3 Format Tag
#endif
#define WAVE_FORMAT_AC3        0x2000 // ATSC/A-52 (Dolby AC3)

typedef struct __attribute__((__packed__)) tw32WAVEFORMATEX {
  uint16_le  wFormatTag;      // format type
  uint16_le  nChannels;      // number of channels (i.e. mono, stereo...)
  uint32_le  nSamplesPerSec;    // sample rate
  uint32_le  nAvgBytesPerSec;  // for buffer estimation
  uint16_le  nBlockAlign;    // block size of data
  uint16_le  wBitsPerSample;    // Number of bits per sample of mono data
  uint16_le  cbSize;        // The count in bytes of the size of extra information (after cbSize)
} w32WAVEFORMATEX;

// Microsoft MPEG audio WAV definition
//
// MPEG-1 audio wave format (audio layer only).   (0x0050)
typedef struct __attribute__((__packed__)) w32mpeg1waveformat_tag {
  w32WAVEFORMATEX  wfx;
  uint16_le    fwHeadLayer;
  uint32_le    dwHeadBitrate;
  uint16_le    fwHeadMode;
  uint16_le    fwHeadModeExt;
  uint16_le    wHeadEmphasis;
  uint16_le    fwHeadFlags;
  uint32_le    dwPTSLow;
  uint32_le    dwPTSHigh;
} w32MPEG1WAVEFORMAT;

#define ACM_MPEG_LAYER1             (0x0001)
#define ACM_MPEG_LAYER2             (0x0002)
#define ACM_MPEG_LAYER3             (0x0004)
#define ACM_MPEG_STEREO             (0x0001)
#define ACM_MPEG_JOINTSTEREO        (0x0002)
#define ACM_MPEG_DUALCHANNEL        (0x0004)
#define ACM_MPEG_SINGLECHANNEL      (0x0008)
#define ACM_MPEG_PRIVATEBIT         (0x0001)
#define ACM_MPEG_COPYRIGHT          (0x0002)
#define ACM_MPEG_ORIGINALHOME       (0x0004)
#define ACM_MPEG_PROTECTIONBIT      (0x0008)
#define ACM_MPEG_ID_MPEG1           (0x0010)

// MPEG Layer3 WAVEFORMATEX structure
// for WAVE_FORMAT_MPEGLAYER3 (0x0055)
//
#define MPEGLAYER3_WFX_EXTRA_BYTES   12

// WAVE_FORMAT_MPEGLAYER3 format sructure
//
typedef struct __attribute__((__packed__)) w32mpeglayer3waveformat_tag {
  w32WAVEFORMATEX  wfx;
  uint16_le    wID;
  uint32_le    fdwFlags;
  uint16_le    nBlockSize;
  uint16_le    nFramesPerBlock;
  uint16_le    nCodecDelay;
} w32MPEGLAYER3WAVEFORMAT;

#define MPEGLAYER3_ID_UNKNOWN            0
#define MPEGLAYER3_ID_MPEG               1
#define MPEGLAYER3_ID_CONSTANTFRAMESIZE  2

#define MPEGLAYER3_FLAG_PADDING_ISO      0x00000000
#define MPEGLAYER3_FLAG_PADDING_ON       0x00000001
#define MPEGLAYER3_FLAG_PADDING_OFF      0x00000002

#endif // __TOOLS_COMMON_MMREG_H__
