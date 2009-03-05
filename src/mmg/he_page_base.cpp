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

bool
he_page_base_c::has_been_modified() {
  if (has_this_been_modified())
    return true;

  std::vector<he_page_base_c *>::iterator it = m_children.begin();

  while (it != m_children.end()) {
    if ((*it)->has_been_modified())
      return true;
    ++it;
  }

  return false;
}

void
he_page_base_c::do_modifications() {
  modify_this();

  std::vector<he_page_base_c *>::iterator it = m_children.begin();

  while (it != m_children.end()) {
    (*it)->do_modifications();
    ++it;
  }
}

int
he_page_base_c::validate() {
  if (!validate_this())
    return m_page_id;

  std::vector<he_page_base_c *>::iterator it = m_children.begin();

  while (it != m_children.end()) {
    int result = (*it)->validate();
    if (-1 != result)
      return result;
    ++it;
  }

  return -1;
}
