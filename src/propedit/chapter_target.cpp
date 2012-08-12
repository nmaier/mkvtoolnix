/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxChapters.h>

#include "common/chapters/chapters.h"
#include "propedit/chapter_target.h"

using namespace libmatroska;

chapter_target_c::chapter_target_c()
  : target_c{tt_undefined}
{
}

chapter_target_c::~chapter_target_c() {
}

void
chapter_target_c::validate() {
  if (!m_file_name.empty() && !m_new_chapters)
    m_new_chapters = parse_chapters(m_file_name);
}

void
chapter_target_c::dump_info()
  const
{
  mxinfo(boost::format("  chapter target:\n"
                       "    file_name: %1%\n")
         % m_file_name);
}

bool
chapter_target_c::has_changes()
  const
{
  return true;
}

void
chapter_target_c::set_level1_element(ebml_element_cptr level1_element_cp,
                                     ebml_element_cptr track_headers_cp) {
  m_level1_element_cp = level1_element_cp;
  m_level1_element    = static_cast<EbmlMaster *>(m_level1_element_cp.get());

  m_track_headers_cp  = track_headers_cp;
  m_master            = m_level1_element;
}

void
chapter_target_c::parse_chapter_spec(std::string const &spec) {
  m_file_name = spec;
}

void
chapter_target_c::execute() {
  add_or_replace_all_master_elements(m_new_chapters.get());

  if (!m_level1_element->ListSize())
    return;

  fix_mandatory_chapter_elements(m_level1_element);
  if (!m_level1_element->CheckMandatory())
    mxerror(boost::format(Y("Error parsing the chapters in '%1%': some mandatory elements are missing.\n")) % m_file_name);
}
