/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for WebMedia in mkvmerge

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_MERGE_WEBMEDIA_H
#define __MTX_MERGE_WEBMEDIA_H

#include "common/os.h"

enum output_compatibility_e {
  OC_MATROSKA,
  OC_WEBMEDIA,
};

bool MTX_DLL_API outputting_webmedia();
void MTX_DLL_API set_output_compatibility(output_compatibility_e compatibility);
output_compatibility_e MTX_DLL_API get_output_compatbility();

#endif // __MTX_MERGE_WEBMEDIA_H
