/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: abstract base class for all pages

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __HE_PAGE_BASE_H
#define __HE_PAGE_BASE_H

#include "os.h"

#include <vector>

#include <wx/treebook.h>

#include <ebml/EbmlElement.h>

using namespace libebml;

class he_page_base_c: public wxPanel {
public:
  std::vector<he_page_base_c *> m_children;
  wxTreebook *m_tree;
  int m_page_id;
  EbmlElement *m_storage;

public:
  he_page_base_c(wxTreebook *parent);
  virtual ~he_page_base_c();

  virtual bool has_been_modified() = 0;
  virtual bool validate() = 0;
};

#endif // __HE_PAGE_BASE_H
