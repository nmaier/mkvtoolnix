/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __EXTRACT_EXTRACT_CLI_PARSER_H
#define __EXTRACT_EXTRACT_CLI_PARSER_H

#include "common/os.h"

#include <string>
#include <vector>

#include "common/cli_parser.h"
#include "extract/options.h"

class extract_cli_parser_c: public cli_parser_c {
protected:
  options_c m_options;
  int m_num_unknown_args;

  std::string m_charset;
  bool m_embed_in_ogg;
  bool m_extract_cuesheet;
  int m_blockadd_level;
  target_mode_e m_target_mode;

public:
  extract_cli_parser_c(const std::vector<std::string> &args);

  options_c run();

protected:
  void init_parser();
};

#endif // __EXTRACT_EXTRACT_CLI_PARSER_H
