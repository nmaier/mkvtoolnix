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

#ifndef __TOOLS_COMMON_GDIVFW_H__
#define __TOOLS_COMMON_GDIVFW_H__

#include "os.h"

#include <ctype.h>

#include "ac_common.h"
#include "common_endian.h"

//////////////////////////////////////////////////////////////////////
// Some data 'copied' from gdi.h, vfw.h and winerror.h (Microsoft files)

/*
 * Video header (BITMAPINFOHEADER)
 */

typedef struct tagw32BITMAPINFOHEADER {
  uint32_le  biSize;
  sint32_le  biWidth;
  sint32_le  biHeight;
  uint16_le  biPlanes;
  uint16_le  biBitCount;
  uint32_le  biCompression;
  uint32_le  biSizeImage;
  sint32_le  biXPelsPerMeter;
  sint32_le  biYPelsPerMeter;
  uint32_le  biClrUsed;
  uint32_le  biClrImportant;
} w32BITMAPINFOHEADER;

/*
 * AVI headers
 */

typedef struct tagw32RECT {
  sint32_le  left;
  sint32_le  top;
  sint32_le  right;
  sint32_le  bottom;
} w32RECT;

typedef struct _w32AVISTREAMINFOW {
  uint32_le  fccType;
  uint32_le  fccHandler;
  uint32_le  dwFlags;  /* Contains AVITF_* flags */
  uint32_le  dwCaps;
  uint16_le  wPriority;
  uint16_le  wLanguage;
  uint32_le  dwScale;
  uint32_le  dwRate;    /* dwRate / dwScale == samples/second */
  uint32_le  dwStart;
  uint32_le  dwLength;  /* In units above... */
  uint32_le  dwInitialFrames;
  uint32_le  dwSuggestedBufferSize;
  uint32_le  dwQuality;
  uint32_le  dwSampleSize;
  w32RECT    rcFrame;
  uint32_le  dwEditCount;
  uint32_le  dwFormatChangeCount;
  char    szName[64];
} w32AVISTREAMINFOW;

typedef struct _w32AVISTREAMINFOA {
  uint32_le  fccType;
  uint32_le  fccHandler;
  uint32_le  dwFlags;  /* Contains AVITF_* flags */
  uint32_le  dwCaps;
  uint16_le  wPriority;
  uint16_le  wLanguage;
  uint32_le  dwScale;
  uint32_le  dwRate;    /* dwRate / dwScale == samples/second */
  uint32_le  dwStart;
  uint32_le  dwLength;  /* In units above... */
  uint32_le  dwInitialFrames;
  uint32_le  dwSuggestedBufferSize;
  uint32_le  dwQuality;
  uint32_le  dwSampleSize;
  w32RECT    rcFrame;
  uint32_le  dwEditCount;
  uint32_le  dwFormatChangeCount;
  char    szName[64];
} w32AVISTREAMINFOA;

#ifdef UNICODE
#define w32AVISTREAMINFO  w32AVISTREAMINFOW
#else
#define w32AVISTREAMINFO  w32AVISTREAMINFOA
#endif

typedef struct {
  uint32_le  ckid;
  uint32_le  dwFlags;
  uint32_le  dwChunkOffset;  // Position of chunk
  uint32_le  dwChunkLength;  // Length of chunk
} w32AVIINDEXENTRY;

/* Flags for AVI file index */
//#define AVIIF_LIST  0x00000001L
//#define AVIIF_TWOCC  0x00000002L
#define AVIIF_KEYFRAME  0x00000010L


typedef struct {
  uint32_le  dwMicroSecPerFrame;    // frame display rate (or 0L)
  uint32_le  dwMaxBytesPerSec;    // max. transfer rate
  uint32_le  dwPaddingGranularity;  // pad to multiples of this
                  // size; normally 2K.
  uint32_le  dwFlags;        // the ever-present flags
  uint32_le  dwTotalFrames;      // # frames in file
  uint32_le  dwInitialFrames;
  uint32_le  dwStreams;
  uint32_le  dwSuggestedBufferSize;

  uint32_le  dwWidth;
  uint32_le  dwHeight;

  uint32_le  dwReserved[4];
} w32MainAVIHeader;

#define AVISTREAMREAD_CONVENIENT  (-1L)


/*
 * Error codes
 */

#ifndef ICERR_OK
#define ICERR_OK        0L

#define ICERR_UNSUPPORTED    -1L
#define ICERR_BADFORMAT      -2L
#define ICERR_MEMORY      -3L
#define ICERR_INTERNAL      -4L
#define ICERR_BADFLAGS      -5L
#define ICERR_BADPARAM      -6L
#define ICERR_BADSIZE      -7L
#define ICERR_BADHANDLE      -8L
#define ICERR_CANTUPDATE    -9L
#define ICERR_ABORT        -10L
#define ICERR_ERROR        -100L
#define ICERR_BADBITDEPTH    -200L
#define ICERR_BADIMAGESIZE    -201L

#define ICERR_CUSTOM      -400L    // errors less than ICERR_CUSTOM...
#endif

#if !defined(SYS_WINDOWS)
#ifndef MMNOMMIO
#define MMIOERR_BASE        256
#define MMIOERR_FILENOTFOUND    (MMIOERR_BASE + 1)  /* file not found */
#define MMIOERR_OUTOFMEMORY      (MMIOERR_BASE + 2)  /* out of memory */
#define MMIOERR_CANNOTOPEN      (MMIOERR_BASE + 3)  /* cannot open */
#define MMIOERR_CANNOTCLOSE      (MMIOERR_BASE + 4)  /* cannot close */
#define MMIOERR_CANNOTREAD      (MMIOERR_BASE + 5)  /* cannot read */
#define MMIOERR_CANNOTWRITE      (MMIOERR_BASE + 6)  /* cannot write */
#define MMIOERR_CANNOTSEEK      (MMIOERR_BASE + 7)  /* cannot seek */
#define MMIOERR_CANNOTEXPAND    (MMIOERR_BASE + 8)  /* cannot expand file */
#define MMIOERR_CHUNKNOTFOUND    (MMIOERR_BASE + 9)  /* chunk not found */
#define MMIOERR_UNBUFFERED      (MMIOERR_BASE + 10) /*  */
#define MMIOERR_PATHNOTFOUND    (MMIOERR_BASE + 11) /* path incorrect */
#define MMIOERR_ACCESSDENIED    (MMIOERR_BASE + 12) /* file was protected */
#define MMIOERR_SHARINGVIOLATION  (MMIOERR_BASE + 13) /* file in use */
#define MMIOERR_NETWORKERROR    (MMIOERR_BASE + 14) /* network not responding */
#define MMIOERR_TOOMANYOPENFILES  (MMIOERR_BASE + 15) /* no more file handles  */
#define MMIOERR_INVALIDFILE      (MMIOERR_BASE + 16) /* default error file error */
#endif
#endif

/*
 * long values
 */
//  Values are 32 bit values layed out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//      Sev - is the severity code
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//
#define FACILITY_ITF                     4

//  longs are 32 bit values layed out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +-+-+-+-+-+---------------------+-------------------------------+
//  |S|R|C|N|r|    Facility         |               Code            |
//  +-+-+-+-+-+---------------------+-------------------------------+
//
//      S - Severity - indicates success/fail
//          0 - Success
//          1 - Fail (COERROR)
//
//      R - reserved portion of the facility code, corresponds to NT's
//              second severity bit.
//
//      C - reserved portion of the facility code, corresponds to NT's
//              C field.
//
//      N - reserved portion of the facility code. Used to indicate a
//              mapped NT status value.
//
//      r - reserved portion of the facility code. Reserved for internal
//              use. Used to indicate long values that are not status
//              values, but are instead message ids for display strings.
//
//      Facility - is the facility code
//
//      Code - is the facility's status code

//
// Severity values
//
//#define SEVERITY_SUCCESS    0
#define SEVERITY_ERROR      1

//
// Success codes
//
#if !defined(SYS_WINDOWS)
#define S_OK                                   ((long)0x00000000L)
#define S_FALSE                                ((long)0x00000001L)
#endif

#undef MAKE_SCODE
#define MAKE_SCODE(sev,fac,code)                      \
  ((sint32) ((uint32(sev)<<31) | (uint32(fac)<<16) | (uint32(code))) )

#ifndef AVIERR_OK
#define AVIERR_OK        0L

#define MAKE_AVIERR(error)  MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0x4000 + error)

#define AVIERR_UNSUPPORTED    MAKE_AVIERR(101)
#define AVIERR_BADFORMAT    MAKE_AVIERR(102)
#define AVIERR_MEMORY      MAKE_AVIERR(103)
#define AVIERR_INTERNAL      MAKE_AVIERR(104)
#define AVIERR_BADFLAGS      MAKE_AVIERR(105)
#define AVIERR_BADPARAM      MAKE_AVIERR(106)
#define AVIERR_BADSIZE      MAKE_AVIERR(107)
#define AVIERR_BADHANDLE    MAKE_AVIERR(108)
#define AVIERR_FILEREAD      MAKE_AVIERR(109)
#define AVIERR_FILEWRITE    MAKE_AVIERR(110)
#define AVIERR_FILEOPEN      MAKE_AVIERR(111)
#define AVIERR_COMPRESSOR    MAKE_AVIERR(112)
#define AVIERR_NOCOMPRESSOR    MAKE_AVIERR(113)
#define AVIERR_READONLY      MAKE_AVIERR(114)
#define AVIERR_NODATA      MAKE_AVIERR(115)
#define AVIERR_BUFFERTOOSMALL  MAKE_AVIERR(116)
#define AVIERR_CANTCOMPRESS    MAKE_AVIERR(117)
#define AVIERR_USERABORT    MAKE_AVIERR(198)
#define AVIERR_ERROR      MAKE_AVIERR(199)
#endif


/*
 * FourCC codes
 */

#ifdef mmioFOURCC
#  undef mmioFOURCC
#endif
#define mmioFOURCC( ch0, ch1, ch2, ch3 )              \
  ( (uint32)uint8(ch0) | ( (uint32)uint8(ch1) << 8 ) |      \
  ( (uint32)uint8(ch2) << 16 ) | ( (uint32)uint8(ch3) << 24 ) )
/*  Should work for little and big endian architectures
 *  (you must define your FourCC as uint32_le) :
 *  mmioFOURCC(a,b,c,d) will give dcba as value
 *  On little endian architectures, this would be stored as abcd in memory
 *  Affecting the value to an uint32_le will keep this memory ordering
 *  On big endian architectures, this would be stored as dcba in memory
 *  Affecting the value to an uint32_le will swap the byte ordering to abcd in memory
 */
#define bitmapFOURCC  mmioFOURCC

inline bool isValidFOURCC(uint32 fcc) {
  return isprint(uint8(fcc>>24))
    && isprint(uint8(fcc>>16))
    && isprint(uint8(fcc>> 8))
    && isprint(uint8(fcc    ));
}

/* form types, list types, and chunk types */
#define formtypeAVI        mmioFOURCC('A', 'V', 'I', ' ')
#define formtypeAVIEXTENDED    mmioFOURCC('A', 'V', 'I', 'X')
#define listtypeAVIHEADER    mmioFOURCC('h', 'd', 'r', 'l')
#define ckidAVIMAINHDR      mmioFOURCC('a', 'v', 'i', 'h')
#define listtypeSTREAMHEADER  mmioFOURCC('s', 't', 'r', 'l')
#define ckidSTREAMHEADER    mmioFOURCC('s', 't', 'r', 'h')
#define ckidSTREAMFORMAT    mmioFOURCC('s', 't', 'r', 'f')
#define ckidSTREAMHANDLERDATA  mmioFOURCC('s', 't', 'r', 'd')
#define ckidSTREAMNAME      mmioFOURCC('s', 't', 'r', 'n')

#define listtypeAVIMOVIE    mmioFOURCC('m', 'o', 'v', 'i')
#define listtypeAVIRECORD    mmioFOURCC('r', 'e', 'c', ' ')

#define ckidAVINEWINDEX      mmioFOURCC('i', 'd', 'x', '1')
#define ckidOPENDMLINDEX    mmioFOURCC('i', 'n', 'd', 'x')

/*
** Stream types for the <fccType> field of the stream header.
*/
#define streamtypeVIDEO      mmioFOURCC('v', 'i', 'd', 's')
#define streamtypeAUDIO      mmioFOURCC('a', 'u', 'd', 's')
#define streamtypeMIDI      mmioFOURCC('m', 'i', 'd', 's')
#define streamtypeTEXT      mmioFOURCC('t', 'x', 't', 's')
/* Interleaved video/audio stream, used by DV1 format */
#define streamtypeINTERLEAVED  mmioFOURCC('i', 'v', 'a', 's')

/* Basic chunk types */
/*#define cktypeDIBbits      aviTWOCC('d', 'b')
#define cktypeDIBcompressed    aviTWOCC('d', 'c')
#define cktypePALchange      aviTWOCC('p', 'c')
#define cktypeWAVEbytes      aviTWOCC('w', 'b')*/

/* Chunk id to use for extra chunks for padding. */
#define ckidAVIPADDING      mmioFOURCC('J', 'U', 'N', 'K')

/* standard four character codes */
#define FOURCC_RIFF        mmioFOURCC('R', 'I', 'F', 'F')
#define FOURCC_LIST        mmioFOURCC('L', 'I', 'S', 'T')

#ifdef LOWORD
#  undef LOWORD
#  undef HIWORD
#  undef LOBYTE
#  undef HIBYTE
#endif
#define LOWORD(l)      ((uint16)(uint32(l) & 0xFFFF))
#define HIWORD(l)      ((uint16)(uint32(l) >> 16))
#define LOBYTE(w)      ((uint8)(uint32(w) & 0xFF))
#define HIBYTE(w)      ((uint8)(uint32(w) >> 8))

/* Macro to get stream number out of a FOURCC ckid */
#ifdef FromHex
#  undef FromHex
#  undef StreamFromFOURCC
#endif
#define FromHex(n)  (((n) >= 'A') ? ((n) + 10 - 'A') : ((n) - '0'))
#define StreamFromFOURCC(fcc) ((uint16) ((FromHex(LOBYTE(LOWORD(fcc))) << 4) +  \
                                             (FromHex(HIBYTE(LOWORD(fcc))))))


#endif // __TOOLS_COMMON_GDIVFW_H__
