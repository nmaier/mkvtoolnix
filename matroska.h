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
    \version \$Id: matroska.h,v 1.1 2003/04/18 12:00:46 mosu Exp $
    \brief Definitions for the various Codec IDs
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __MATROSKA_H
#define __MATROSKA_H

#define MKV_A_MP3        "A_MPEGLAYER3"
#define MKV_A_AC3        "A_DOL_AC3"
#define MKV_A_PCM16      "A_PCM16IN"
#define MKV_A_VORBIS     "A_VORBIS"

#define MKV_V_MSCOMP     "V_MS/VFW/FOURCC"

#define MKV_S_TEXTSIMPLE "S_TEXT/SIMPLE"

#endif // __MATROSKA_H
