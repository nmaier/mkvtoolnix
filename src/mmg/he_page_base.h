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

#include <wx/panel.h>
#include <wx/treebase.h>

#include <ebml/EbmlElement.h>

using namespace libebml;

class header_editor_frame_c;

class he_page_base_c: public wxPanel {
public:
  std::vector<he_page_base_c *> m_children;
  header_editor_frame_c *m_parent;
  wxTreeItemId m_page_id;
  EbmlElement *m_l1_element;

public:
  he_page_base_c(header_editor_frame_c *parent);
  virtual ~he_page_base_c();

  virtual bool has_been_modified();
  virtual bool has_this_been_modified() = 0;
  virtual void do_modifications();
  virtual void modify_this() = 0;
  virtual wxTreeItemId validate();
  virtual bool validate_this() = 0;
};

#endif // __HE_PAGE_BASE_H
