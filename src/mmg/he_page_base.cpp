/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: abstract base class for all pages

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include "he_page_base.h"

he_page_base_c::he_page_base_c(wxTreebook *parent)
  : wxPanel(parent)
  , m_tree(parent)
  , m_page_id(m_tree->GetTreeCtrl()->GetCount())
  , m_storage(NULL)
{
}

he_page_base_c::~he_page_base_c() {
  delete m_storage;
}
