/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts chapters from Matroska files into other files
   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cassert>
#include <iostream>

#include <avilib.h>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>

#include <matroska/FileKax.h>
#include <matroska/KaxChapters.h>

#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/mm_io.h"
#include "common/mm_io_x.h"
#include "common/kax_analyzer.h"
#include "common/xml/ebml_chapters_converter.h"
#include "extract/mkvextract.h"

using namespace libmatroska;

void
extract_chapters(const std::string &file_name,
                 bool chapter_format_simple,
                 kax_analyzer_c::parse_mode_e parse_mode) {
  kax_analyzer_cptr analyzer;

  // open input file
  try {
    analyzer = kax_analyzer_cptr(new kax_analyzer_c(file_name));
    if (!analyzer->process(parse_mode, MODE_READ, true))
      throw false;
  } catch (mtx::mm_io::exception &ex) {
    show_error(boost::format(Y("The file '%1%' could not be opened for reading: %2%.\n")) % file_name % ex);
    return;
  }

  ebml_master_cptr master = analyzer->read_all(EBML_INFO(KaxChapters));
  if (!master)
    return;

  KaxChapters *chapters = dynamic_cast<KaxChapters *>(master.get());
  assert(chapters);

  if (!chapter_format_simple)
    mtx::xml::ebml_chapters_converter_c::write_xml(*chapters, *g_mm_stdio);

  else {
    int dummy = 1;
    write_chapters_simple(dummy, chapters, g_mm_stdio.get());
  }
}
