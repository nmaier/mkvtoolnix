/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxChapters.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTracks.h>

#include "common/command_line.h"
#include "common/mm_io_x.h"
#include "common/unique_numbers.h"
#include "common/version.h"
#include "propedit/propedit_cli_parser.h"

static void
display_update_element_result(const EbmlCallbacks &callbacks,
                              kax_analyzer_c::update_element_result_e result) {
  std::string message((boost::format(Y("Updating the '%1%' element failed. Reason:")) % callbacks.DebugName).str());
  message += " ";

  switch (result) {
    case kax_analyzer_c::uer_error_segment_size_for_element:
      message += Y("The element was written at the end of the file, but the segment size could not be updated. Therefore the element will not be visible. The process will be aborted. The file has been changed!");
      break;

    case kax_analyzer_c::uer_error_segment_size_for_meta_seek:
      message += Y("The meta seek element was written at the end of the file, but the segment size could not be updated. Therefore the element will not be visible. The process will be aborted. The file has been changed!");
      break;

    case kax_analyzer_c::uer_error_meta_seek:
      message += Y("The Matroska file was modified, but the meta seek entry could not be updated. This means that players might have a hard time finding this element. Please use your favorite player to check this file.");
      break;

    default:
      message += Y("An unknown error occured. The file has been modified.");
  }

  mxerror(message + "\n");
}

static void
write_changes(options_cptr &options,
              kax_analyzer_c *analyzer) {
  std::vector<EbmlId> ids_to_write;
  ids_to_write.push_back(KaxInfo::ClassInfos.GlobalId);
  ids_to_write.push_back(KaxTracks::ClassInfos.GlobalId);
  ids_to_write.push_back(KaxTags::ClassInfos.GlobalId);
  ids_to_write.push_back(KaxChapters::ClassInfos.GlobalId);
  ids_to_write.push_back(KaxAttachments::ClassInfos.GlobalId);

  for (auto &id_to_write : ids_to_write) {
    for (auto &target : options->m_targets) {
      if (!target->get_level1_element())
        continue;

      EbmlMaster &l1_element = *target->get_level1_element();

      if (id_to_write != l1_element.Generic().GlobalId)
        continue;

      mxverb(2, boost::format(Y("Element %1% is written.\n")) % l1_element.Generic().DebugName);

      kax_analyzer_c::update_element_result_e result = l1_element.ListSize() ? analyzer->update_element(&l1_element, true) : analyzer->remove_elements(EbmlId(l1_element));
      if (kax_analyzer_c::uer_success != result)
        display_update_element_result(l1_element.Generic(), result);

      break;
    }
  }
}

static void
run(options_cptr &options) {
  console_kax_analyzer_cptr analyzer;

  try {
    if (!kax_analyzer_c::probe(options->m_file_name))
      mxerror(boost::format("The file '%1%' is not a Matroska file or it could not be found.\n") % options->m_file_name);

    analyzer = console_kax_analyzer_cptr(new console_kax_analyzer_c(options->m_file_name));
  } catch (mtx::mm_io::exception &ex) {
    mxerror(boost::format("The file '%1%' could not be opened for reading and writing: %1.\n") % options->m_file_name % ex);
  }

  mxinfo(boost::format("%1%\n") % Y("The file is being analyzed."));

  analyzer->set_show_progress(options->m_show_progress);

  bool ok = false;
  try {
    ok = analyzer->process(options->m_parse_mode, MODE_WRITE, true);
  } catch (mtx::mm_io::exception &ex) {
    mxerror(boost::format(Y("The file '%1%' could not be opened for reading and writing, or a read/write operation on it failed: %2%.\n")) % options->m_file_name % ex);
  } catch (...) {
  }

  if (!ok)
    mxerror(Y("This file could not be opened or parsed.\n"));

  options->find_elements(analyzer.get());
  options->validate();

  if (debugging_c::requested("dump_options")) {
    mxinfo("\nDumping options after file and element analysis\n\n");
    options->dump_info();
  }

  options->execute();

  mxinfo(Y("The changes are written to the file.\n"));

  write_changes(options, analyzer.get());

  mxinfo(Y("Done.\n"));

  mxexit(0);
}

static
void setup(char **argv) {
  mtx_common_init("mkvpropedit", argv[0]);
  clear_list_of_unique_numbers(UNIQUE_ALL_IDS);
  version_info = get_version_info("mkvpropedit", vif_full);
}


/** \brief Setup and high level program control

   Calls the functions for setup, handling the command line arguments,
   the actual processing and cleaning up.
*/
int
main(int argc,
     char **argv) {
  setup(argv);

  options_cptr options = propedit_cli_parser_c(command_line_utf8(argc, argv)).run();

  if (debugging_c::requested("dump_options")) {
    mxinfo("\nDumping options after parsing the command line\n\n");
    options->dump_info();
  }

  run(options);

  mxexit();
}
