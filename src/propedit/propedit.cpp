/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "propedit/propedit_cli_parser.h"
#include "propedit/setup.h"

static void
run(options_cptr options) {
  console_kax_analyzer_cptr analyzer;

  try {
    if (!kax_analyzer_c::probe(options->m_file_name))
      mxerror(boost::format("The file '%1%' is not a Matroska file or it could not be found.\n") % options->m_file_name);

    analyzer = console_kax_analyzer_cptr(new console_kax_analyzer_c(options->m_file_name));
  } catch (...) {
    mxerror(boost::format("The file '%1%' could not be opened for read/write access.\n") % options->m_file_name);
  }

  analyzer->set_show_progress(options->m_show_progress);

  if (!analyzer->process(options->m_parse_mode))
    mxerror(Y("This file could not be opened or parsed."));

  options->find_elements(analyzer.get_object());
  options->validate();

  if (debugging_requested("dump_options")) {
    mxinfo("\nDumping options after file and element analysis\n\n");
    options->dump_info();
  }
}

/** \brief Setup and high level program control

   Calls the functions for setup, handling the command line arguments,
   the actual processing and cleaning up.
*/
int
main(int argc,
     char **argv) {
  setup();

  options_cptr options = propedit_cli_parser_c(command_line_utf8(argc, argv)).run();

  if (debugging_requested("dump_options")) {
    mxinfo("\nDumping options after parsing the command line\n\n");
    options->dump_info();
  }

  run(options);

  mxexit();
}
