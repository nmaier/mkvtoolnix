/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: segment info page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "common/ebml.h"
#include "common/segmentinfo.h"
#include "common/wx.h"
#include "mmg/header_editor/frame.h"
#include "mmg/header_editor/top_level_page.h"

he_top_level_page_c::he_top_level_page_c(header_editor_frame_c *parent,
                                         const translatable_string_c &title,
                                         ebml_element_cptr l1_element)
  : he_empty_page_c(parent, title, "")
{
  m_l1_element = l1_element;
}

he_top_level_page_c::~he_top_level_page_c() {
}

void
he_top_level_page_c::init() {
  m_parent->append_page(this);
}

void
he_top_level_page_c::do_modifications() {
  he_page_base_c::do_modifications();

  if (is_id(m_l1_element, KaxInfo))
    fix_mandatory_segmentinfo_elements(m_l1_element.get_object());

  m_l1_element->UpdateSize(true);
}
