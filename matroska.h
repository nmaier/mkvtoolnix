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
    \version \$Id: matroska.h,v 1.9 2003/04/30 11:31:47 mosu Exp $
    \brief Definitions for the various Codec IDs
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __MATROSKA_H
#define __MATROSKA_H

// see http://cvs.corecodec.org/cgi-bin/cvsweb.cgi/~checkout~/matroska/doc/website/specs/codex.html?rev=HEAD&content-type=text/html

#define MKV_A_MP3        "A_MPEG/L3"
#define MKV_A_AC3        "A_AC3"
#define MKV_A_PCM        "A_PCM/INT/LIT"
#define MKV_A_VORBIS     "A_VORBIS"
#define MKV_A_ACM        "A_MS/ACM"

#define MKV_V_MSCOMP     "V_MS/VFW/FOURCC"

#define MKV_S_TEXTASCII  "S_TEXT/ASCII"
#define MKV_S_TEXTUTF8   "S_TEXT/UTF8"

#endif // __MATROSKA_H
