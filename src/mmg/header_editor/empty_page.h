/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: empty page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_HE_EMPTY_PAGE_H
#define MTX_HE_EMPTY_PAGE_H

#include "common/common_pch.h"

#include <wx/string.h>

#include "common/translation.h"
#include "mmg/header_editor/page_base.h"

class he_empty_page_c: public he_page_base_c {
public:
  translatable_string_c m_content;

  wxStaticText *m_st_title, *m_st_content;

public:
  he_empty_page_c(header_editor_frame_c *parent, const translatable_string_c &title, const translatable_string_c &content);
  virtual ~he_empty_page_c();

  virtual bool has_this_been_modified();
  virtual bool validate_this();
  virtual void modify_this();
  virtual void translate_ui();
};

#endif // MTX_HE_EMPTY_PAGE_H
