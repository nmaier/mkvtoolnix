/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  common.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: common.cpp,v 1.9 2003/04/18 10:08:24 mosu Exp $
    \brief helper functions, common variables
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int verbose = 1;

void _die(const char *s, const char *file, int line) {
  fprintf(stderr, "die @ %s/%d : %s\n", file, line, s);
  exit(1);
}

void _trace(const char *func, const char *file, int line) {
  fprintf(stdout, "trace: %s:%s (%d)\n", file, func, line);
}

/*
  Codec types, see
  http://www.corecodec.com/modules.php?op=modload&name=PNphpBB2&file=viewtopic&t=227

  Video:
  1. 'V_RGB24' : RGB, 24 Bit 
  2. V_YUV2' : YUV2 
  etc. .. too lazy to list all colourspaces ... somebody got a list ? 
  
  11. 'V_MS/VFW/FOURCC' : Unknown codec, probably not in matroska native codec
      list, see codec 'private' data and use suitable decoder based on FourCC
      code ( former : MCF-A stream ) ; stream is transmuxed from AVI or OGM,
      muxed from DirectShow or created with an existing VfW codec from VdubMod
      or the like 
  12. 'V_MS/DSHOW/GUID' : Unknown codec, probably not in matroska native codec
      list, see codec 'private' data and use suitable decoder based on GUID
      ( Formerly : MCF-A stream, also for ACM audio codecs ) ; stream is
      transmuxed from AVI ( breaking specs ) , OGM or muxed via DirectShow 
  13. 'V_MPEG4IS0_SP' : Video, MPEG4ISO simple profile ( DivX4 ) ; stream was
      created via UCI codec or transmuxed from MP4, not simply transmuxed from
      AVI ( but it may be possible to convert a MPEG4 video stream coming from
      an AVI, with special tools )
  14. 'V_MPEG4IS0_SAP' : Video, MPEG4ISO simple advanced profile ( DivX5,
      XviD ) ; stream was created ... ( see above ) 
  15. 'V_MPEG4IS0_AP' : Video, MPEG4ISO advanced profile ; stream was
      created ... ( see above ) 
  16. 'V_MSMPEG4' : Video, Microsoft MPEG4 V1/2/3 and derivates, means DivX3,
      Angelpotion, SMR, etc. ; stream was created using VfW codec or
      transmuxed from AVI 
  17. 'V_MPEG2VIDEO' ; Video, MPEG 2 
  18. 'V_MPEG1VIDEO' ; Video, MPEG 1 
  19. 'V_REALRV9' ; Video, Realmedia, RV 9 
  20. 'V_QUICKTIME/FOURCC' ; Video, Quicktime, QT FourCC ; anybody aware of QT
      codecs that dont have a FourCC ? 
  21. 'V_MSWMV' ; Video, Microsoft Video 
  22. 'V_INDEO5' ; Video, Indeo 5 ; transmuxed from AVI or created using VfW
      codec 
  23. 'V_MJPEGLL' ; Video, MJpeg Lossless 
  24. 'V_MJPEG2000' ; Video, MJpeg 2000 
  25. 'V_HUFFYUV' ; Video, HuffYuv, lossless ; auch als VfW möglich 
  26. 'V_DV-1' ; Video, DV Video , type 1 
  27. 'V_DV-2' ; Video, DV Video , type 2 
  28. 'V_THEORA' ; Video, Ogg Theora 
  29. 'V_TARKIN' ; Video, Ogg Tarkin 
  30. 'V_ON2VP4' ; Video, ON2, VP4 
  31. 'V_ON2VP5' ; Video, ON2, VP5 
  32. 'V_3IVXD4' ; Video, 3ivX ( is D4 decoder downwards compatible ? ) 
  33. 'V_SORENSEN' ; Video, Sorensen codec ; same here, how many versions are
      there ? 
  ...... to be continued 
  
  
  Audio : 
  1. 'A_MS/ACM' ; Probably unknown audio codec, use M$ UUID to identify and
     call ACM codec or Dshow filter for decoding 
  2. 'A_VORBIS' ; Audio, Vorbis, created via UCI or transmuxed from OGG/OGM 
  3. 'A_MPEGLAYER3' ; Audio, MPEG 1, 2, 2.5 Layer 3 ( MP3 ) ; created via UCI
     or transmuxed from MP3, OGM or AVI 
  4. 'A_MPEGLAYER2' ; Audio, MPEG 1, 2 , 2.5 Layer 2 ( MP2 ) ; created via UCI
     ( tooLame ) or transmuxed from MPEG 
  5. 'A_PCM16IN' ; Audio, PCM, Integer 16 Bit 
  6. 'A_PCM24IN' ; Audio, PCM, Integer 24 Bit 
  7. 'A_PCM24FP' ; Audio, PCM, 24 Bit Floating Point 
  8. 'A_PCM32FP' ; Audio, PCM, 32 Bit Floating Point 
  9. 'A_FLAC16' ; Audio, FLAC, 16 Bit ( lossless ) 
  10. 'A_FLAC24' ; Audio, FLAC, 24 Bit ( lossless ) 
  11. 'A_MPEG4_AAC' ; Audio, AAC 
  12. 'A_DOL_AC3' ; Audio, AC3 
  13. 'A_MUSEPACK_MPC' ; Audio, MPC 
  14. 'A_MSWMA' ; Audio, Microsoft Media Audio 
  etc. p.p. 
  
  
  Subtitles : 
  1. 'S_USF' : Universal Subtitles Format 
  2. 'S_SSA' : Fansubber format 
  3. 'S_VOBSUB' : Vobsub format 
  4. 'S_ASS' :

*/

char *map_video_codec_fourcc(char *fourcc, int *set_codec_private) {
  *set_codec_private = 0;
  if (fourcc == NULL)
    return NULL;
  if (!strcasecmp(fourcc, "MP42") ||
      !strcasecmp(fourcc, "DIV2") ||
      !strcasecmp(fourcc, "DIVX") ||
      !strcasecmp(fourcc, "XVID") ||
      !strcasecmp(fourcc, "DX50"))
    return "V_MPEG4IS0_SAP";
  else if (!strcasecmp(fourcc, "DIV3") ||
           !strcasecmp(fourcc, "AP41") || // Angel Potion
           !strcasecmp(fourcc, "MPG3") ||
           !strcasecmp(fourcc, "MP43"))
    return "V_MSMPEG4";
  else {
    fprintf(stderr, "Warning: Unknonw FourCC '%s'. Please contact "
            "Moritz Bunkus <moritz@bunkus.org>\n", fourcc);
    *set_codec_private = 1;
    return "V_MS/VFW/FOURCC";
  }
}

char *map_audio_codec_id(int id, int bps, int *set_codec_private) {
  *set_codec_private = 0;
  
  if (id == 0x0001) { // raw PCM
    if (bps == 16)
      return "A_PCM16IN";
    else if (bps == 24)
      return "A_PCM24IN";
    else {
      fprintf(stderr, "Warning: Unknown codec ID '%d' (0x%04x), %d bps. Please "
              "contact Moritz Bunkus <moritz@bunkus.org>\n", id, id, bps);
      *set_codec_private = 1;
      return "A_MS/ACM";
    }
  } else if (id == 0x0055) // MP3
    return "A_MPEGLAYER3";
  else if (id == 0x2000) // AC3
    return "A_DOL_AC3";
  else {
    fprintf(stderr, "Warning: Unknown codec ID '%d' (0x%04x), %d bps. Please "
            "contact Moritz Bunkus <moritz@bunkus.org>\n", id, id, bps);
    *set_codec_private = 1;
    return "A_MS/ACM";
  }
}

u_int16_t get_uint16(const void *buf) {
  u_int16_t      ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[1] & 0xff;
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

u_int32_t get_uint32(const void *buf) {
  u_int32_t      ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[3] & 0xff;
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

