/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "propedit/setup.h"

static void
run(options_c &options) {
  kax_analyzzer_cptr analyzer;

  try {
    if (!kax_analyzer_c::probe(options.m_file_name))
      mxerror(boost::format("The file '%1%' is not a Matroska file or it could not be found.") % options.m_file_name);

    analyzer = kax_analyzer_c::open(options.m_file_name);
  } catch (...) {
    mxerror(boost::format("The file '%1%' could not be opened for read/write access.") % options.m_file_name);
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

  options_c options = parse_args(command_line_utf8(argc, argv));
  options.validate();

  run(options);

  mxexit();
}
