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

#include "os.h"

#include "he_empty_page.h"

class he_top_level_page_c: public he_empty_page_c {
public:
  he_top_level_page_c(wxTreebook *parent, const wxString &title, EbmlElement *l1_element);
  virtual ~he_top_level_page_c();

  virtual void do_modifications();
};

#endif // __HE_TOP_LEVEL_PAGE_H
