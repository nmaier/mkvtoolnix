/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for Webm in mkvmerge

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MERGE_WEBM_H
#define MTX_MERGE_WEBM_H

enum output_compatibility_e {
  OC_MATROSKA,
  OC_WEBM,
};

bool outputting_webm();
void set_output_compatibility(output_compatibility_e compatibility);
output_compatibility_e get_output_compatbility();

#endif // MTX_MERGE_WEBM_H
