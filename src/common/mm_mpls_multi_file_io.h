/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_MM_MPLS_MULTI_FILE_IO_H
#define MTX_COMMON_MM_MPLS_MULTI_FILE_IO_H

#include "common/common_pch.h"

#include "common/mm_multi_file_io.h"
#include "common/mpls.h"

class mm_mpls_multi_file_io_c: public mm_multi_file_io_c {
protected:
  mtx::mpls::parser_cptr m_mpls_parser;

protected:
  static debugging_option_c ms_debug;

public:
  mm_mpls_multi_file_io_c(const std::vector<bfs::path> &file_names, std::string const &display_file_name, mtx::mpls::parser_cptr const &mpls_parser);
  virtual ~mm_mpls_multi_file_io_c();

  std::vector<timecode_c> const &get_chapters() const;
  virtual void create_verbose_identification_info(std::vector<std::string> &verbose_info);

  static mm_io_cptr open_multi(std::string const &display_file_name);
  static mm_io_cptr open_multi(mm_io_c *in);
};
typedef std::shared_ptr<mm_mpls_multi_file_io_c> mm_mpls_multi_file_io_cptr;

#endif  // MTX_COMMON_MM_MPLS_MULTI_FILE_IO_H
