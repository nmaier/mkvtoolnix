/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: segment info page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __HE_TOP_LEVEL_PAGE_H
#define __HE_TOP_LEVEL_PAGE_H

#include "common/os.h"

#include "mmg/header_editor/empty_page.h"

class he_top_level_page_c: public he_empty_page_c {
public:
  he_top_level_page_c(header_editor_frame_c *parent, const translatable_string_c &title, ebml_element_cptr l1_element);
  virtual ~he_top_level_page_c();

  virtual void do_modifications();
  virtual void init();
};

#endif // __HE_TOP_LEVEL_PAGE_H
