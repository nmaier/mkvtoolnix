/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  matroska.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief Definitions for the various Codec IDs
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MATROSKA_H
#define __MATROSKA_H

// see http://cvs.corecodec.org/cgi-bin/cvsweb.cgi/~checkout~/matroska/doc/website/specs/codex.html?rev=HEAD&content-type=text/html

#define MKV_A_AAC_2MAIN  "A_AAC/MPEG2/MAIN"
#define MKV_A_AAC_2LC    "A_AAC/MPEG2/LC"
#define MKV_A_AAC_2SSR   "A_AAC/MPEG2/SSR"
#define MKV_A_AAC_4MAIN  "A_AAC/MPEG4/MAIN"
#define MKV_A_AAC_4LC    "A_AAC/MPEG4/LC"
#define MKV_A_AAC_4SSR   "A_AAC/MPEG4/SSR"
#define MKV_A_AAC_4LTP   "A_AAC/MPEG4/LTP"
#define MKV_A_AAC_4SBR   "A_AAC/MPEG4/SBR"
#define MKV_A_AC3        "A_AC3"
#define MKV_A_DTS        "A_DTS"
#define MKV_A_MP3        "A_MPEG/L3"
#define MKV_A_PCM        "A_PCM/INT/LIT"
#define MKV_A_VORBIS     "A_VORBIS"
#define MKV_A_ACM        "A_MS/ACM"

#define MKV_V_MSCOMP     "V_MS/VFW/FOURCC"
#define MKV_V_REALV1     "V_REAL/RV10"
#define MKV_V_REALV2     "V_REAL/RV20"
#define MKV_V_REALV3     "V_REAL/RV30"
#define MKV_V_REALV4     "V_REAL/RV40"

#define MKV_S_TEXTUTF8   "S_TEXT/UTF8"
#define MKV_S_TEXTSSA    "S_TEXT/SSA"
#define MKV_S_TEXTASS    "S_TEXT/ASS"
#define MKV_S_TEXTUSF    "S_TEXT/USF"

#endif // __MATROSKA_H
