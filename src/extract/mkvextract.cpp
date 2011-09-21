/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   command line parsing, setup

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cassert>
#include <iostream>
#include <stdlib.h>

#include "common/chapters/chapters.h"
#include "common/command_line.h"
#include "common/matroska.h"
#include "common/mm_io.h"
#include "common/strings/parsing.h"
#include "common/tags/writer.h"
#include "common/translation.h"
#include "common/version.h"
#include "common/xml/element_mapping.h"
#include "extract/extract_cli_parser.h"
#include "extract/mkvextract.h"

using namespace libmatroska;

#define NAME "mkvextract"

enum operation_mode_e {
  MODE_TRACKS,
  MODE_TAGS,
  MODE_ATTACHMENTS,
  MODE_CHAPTERS,
  MODE_CUESHEET,
  MODE_TIMECODES_V2,
};

bool g_no_variable_data = false;

void
show_element(EbmlElement *l,
             int level,
             const std::string &info) {
  if (9 < level)
    mxerror(boost::format(Y("mkvextract.cpp/show_element(): level > 9: %1%")) % level);

  if (0 == verbose)
    return;

  char level_buffer[10];
  memset(&level_buffer[1], ' ', 9);
  level_buffer[0]     = '|';
  level_buffer[level] = 0;

  mxinfo(boost::format("(%1%) %2%+ %3%") % NAME % level_buffer % info);
  if (NULL != l)
    mxinfo(boost::format(Y(" at %1%")) % l->GetElementPosition());
  mxinfo("\n");
}

void
show_error(const std::string &error) {
  mxerror(boost::format("(%1%) %2%\n") % NAME % error);
}

static void
setup() {
  mtx_common_init();

  set_process_priority(-1);

  verbose      = 0;
  version_info = get_version_info("mkvextract", vif_full);
}

int
main(int argc,
     char **argv) {
  setup();

  options_c options = extract_cli_parser_c(command_line_utf8(argc, argv)).run();

  if (options_c::em_tracks == options.m_extraction_mode) {
    extract_tracks(options.m_file_name, options.m_tracks);

    if (0 == verbose)
      mxinfo(Y("Progress: 100%\n"));

  } else if (options_c::em_tags == options.m_extraction_mode)
    extract_tags(options.m_file_name, options.m_parse_mode);

  else if (options_c::em_attachments == options.m_extraction_mode)
    extract_attachments(options.m_file_name, options.m_tracks, options.m_parse_mode);

  else if (options_c::em_chapters == options.m_extraction_mode)
    extract_chapters(options.m_file_name, options.m_simple_chapter_format, options.m_parse_mode);

  else if (options_c::em_cuesheet == options.m_extraction_mode)
    extract_cuesheet(options.m_file_name, options.m_parse_mode);

  else if (options_c::em_timecodes_v2 == options.m_extraction_mode)
    extract_timecodes(options.m_file_name, options.m_tracks, 2);

  else
    usage(2);

  return 0;
}
