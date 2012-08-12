/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_PROPEDIT_CHAPTER_TARGET_H
#define MTX_PROPEDIT_CHAPTER_TARGET_H

#include "common/common_pch.h"

#include <ebml/EbmlMaster.h>

#include "common/chapters/chapters.h"
#include "propedit/target.h"

using namespace libebml;

class chapter_target_c: public target_c {
protected:
  kax_chapters_cptr m_new_chapters;

public:
  chapter_target_c();
  virtual ~chapter_target_c();

  virtual void validate();

  virtual void parse_chapter_spec(const std::string &spec);
  virtual void dump_info() const;

  virtual bool has_changes() const;

  virtual void set_level1_element(ebml_element_cptr level1_element, ebml_element_cptr track_headers = ebml_element_cptr{});

  virtual void execute();
};

#endif // MTX_PROPEDIT_CHAPTER_TARGET_H
