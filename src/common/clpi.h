/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  definitions and helper functions for BluRay clip info data

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_CLPI_COMMON_H
#define MTX_COMMON_CLPI_COMMON_H

#include "common/common_pch.h"

#include <vector>

#include "common/bit_cursor.h"

#define CLPI_FILE_MAGIC   FOURCC('H', 'D', 'M', 'V')
#define CLPI_FILE_MAGIC2A FOURCC('0', '2', '0', '0')
#define CLPI_FILE_MAGIC2B FOURCC('0', '1', '0', '0')

namespace clpi {
  struct program_stream_t {
    uint16_t pid;
    unsigned char coding_type;
    unsigned char format;
    unsigned char rate;
    unsigned char aspect;
    unsigned char oc_flag;
    unsigned char char_code;
    std::string language;

    program_stream_t();

    void dump();
  };
  typedef std::shared_ptr<program_stream_t> program_stream_cptr;

  struct program_t {
    uint32_t spn_program_sequence_start;
    uint16_t program_map_pid;
    unsigned char num_streams;
    unsigned char num_groups;
    std::vector<program_stream_cptr> program_streams;

    program_t();

    void dump();
  };
  typedef std::shared_ptr<program_t> program_cptr;

  class parser_c {
  protected:
    std::string m_file_name;
    bool m_ok;
    debugging_option_c m_debug;

    size_t m_sequence_info_start, m_program_info_start;

  public:
    std::vector<program_cptr> m_programs;

  public:
    parser_c(const std::string &file_name);
    virtual ~parser_c();

    virtual bool parse();
    virtual bool is_ok() {
      return m_ok;
    }

    virtual void dump();

  protected:
    virtual void parse_header(bit_reader_cptr &bc);
    virtual void parse_program_info(bit_reader_cptr &bc);
    virtual void parse_program_stream(bit_reader_cptr &bc, program_cptr &program);
  };
  typedef std::shared_ptr<parser_c> parser_cptr;

};
#endif // MTX_COMMON_CLPI_COMMON_H
