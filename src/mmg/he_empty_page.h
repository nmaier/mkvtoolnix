/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: empty page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __HE_EMPTY_PAGE_H
#define __HE_EMPTY_PAGE_H

#include "os.h"

#include "he_page_base.h"

class he_empty_page_c: public he_page_base_c {
public:
  wxString m_title, m_content;

public:
  he_empty_page_c(header_editor_frame_c *parent, const wxString &title, const wxString &content);
  virtual ~he_empty_page_c();

  virtual bool has_this_been_modified();
  virtual bool validate_this();
  virtual void modify_this();
};

#endif // __HE_EMPTY_PAGE_H
