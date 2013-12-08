/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  BluRay clip info data handling

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/clpi.h"

clpi::program_t::program_t()
  : spn_program_sequence_start(0)
  , program_map_pid(0)
  , num_streams(0)
  , num_groups(0)
{
}

void
clpi::program_t::dump() {
  mxinfo(boost::format("Program dump:\n"
                       "  spn_program_sequence_start: %1%\n"
                       "  program_map_pid:            %2%\n"
                       "  num_streams:                %3%\n"
                       "  num_groups:                 %4%\n")
         % spn_program_sequence_start %program_map_pid % static_cast<unsigned int>(num_streams) % static_cast<unsigned int>(num_groups));

  for (auto &program_stream : program_streams)
    program_stream->dump();
}

clpi::program_stream_t::program_stream_t()
  : pid(0)
  , coding_type(0)
  , format(0)
  , rate(0)
  , aspect(0)
  , oc_flag(0)
  , char_code(0)
{
}

void
clpi::program_stream_t::dump() {
  mxinfo(boost::format("Program stream dump:\n"
                       "  pid:         %1%\n"
                       "  coding_type: %2%\n"
                       "  format:      %3%\n"
                       "  rate:        %4%\n"
                       "  aspect:      %5%\n"
                       "  oc_flag:     %6%\n"
                       "  char_code:   %7%\n"
                       "  language:    %8%\n")
         % pid
         % static_cast<unsigned int>(coding_type)
         % static_cast<unsigned int>(format)
         % static_cast<unsigned int>(rate)
         % static_cast<unsigned int>(aspect)
         % static_cast<unsigned int>(oc_flag)
         % static_cast<unsigned int>(char_code)
         % language);
}

clpi::parser_c::parser_c(const std::string &file_name)
  : m_file_name(file_name)
  , m_ok(false)
  , m_debug{"clpi|clpi_parser"}
{
}

clpi::parser_c::~parser_c() {
}

void
clpi::parser_c::dump() {
  mxinfo(boost::format("Parser dump:\n"
                       "  sequence_info_start: %1%\n"
                       "  program_info_start:  %2%\n")
         % m_sequence_info_start % m_program_info_start);

  for (auto &program : m_programs)
    program->dump();
}

bool
clpi::parser_c::parse() {
  try {
    mm_file_io_c m_file(m_file_name, MODE_READ);

    int64_t file_size   = m_file.get_size();
    memory_cptr content = memory_c::alloc(file_size);

    if (file_size != m_file.read(content, file_size))
      throw false;
    m_file.close();

    bit_reader_cptr bc(new bit_reader_c(content->get_buffer(), file_size));

    parse_header(bc);
    parse_program_info(bc);

    if (m_debug)
      dump();

    m_ok = true;

  } catch (...) {
    mxdebug_if(m_debug, "Parsing NOT OK\n");
  }

  return m_ok;
}

void
clpi::parser_c::parse_header(bit_reader_cptr &bc) {
  bc->set_bit_position(0);

  uint32_t magic = bc->get_bits(32);
  mxdebug_if(m_debug, boost::format("File magic 1: 0x%|1$08x|\n") % magic);
  if (CLPI_FILE_MAGIC != magic)
    throw false;

  magic = bc->get_bits(32);
  mxdebug_if(m_debug, boost::format("File magic 2: 0x%|1$08x|\n") % magic);
  if ((CLPI_FILE_MAGIC2A != magic) && (CLPI_FILE_MAGIC2B != magic))
    throw false;

  m_sequence_info_start = bc->get_bits(32);
  m_program_info_start  = bc->get_bits(32);
}

void
clpi::parser_c::parse_program_info(bit_reader_cptr &bc) {
  bc->set_bit_position(m_program_info_start * 8);

  bc->skip_bits(40);            // 32 bits length, 8 bits reserved
  size_t num_program_streams = bc->get_bits(8), program_idx, stream_idx;

  mxdebug_if(m_debug, boost::format("num_program_streams: %1%\n") % num_program_streams);

  for (program_idx = 0; program_idx < num_program_streams; ++program_idx) {
    program_cptr program(new program_t);
    m_programs.push_back(program);

    program->spn_program_sequence_start = bc->get_bits(32);
    program->program_map_pid            = bc->get_bits(16);
    program->num_streams                = bc->get_bits(8);
    program->num_groups                 = bc->get_bits(8);

    for (stream_idx = 0; stream_idx < program->num_streams; ++stream_idx)
      parse_program_stream(bc, program);
  }
}

void
clpi::parser_c::parse_program_stream(bit_reader_cptr &bc,
                                     clpi::program_cptr &program) {
  program_stream_cptr stream(new program_stream_t);
  program->program_streams.push_back(stream);

  stream->pid = bc->get_bits(16);

  if ((bc->get_bit_position() % 8) != 0) {
    mxdebug_if(m_debug, "Bit position not divisible by 8\n");
    throw false;
  }

  size_t length_in_bytes  = bc->get_bits(8);
  size_t position_in_bits = bc->get_bit_position();

  stream->coding_type     = bc->get_bits(8);

  char language[4];
  memset(language, 0, 4);

  switch (stream->coding_type) {
    case 0x01:
    case 0x02:
    case 0xea:
    case 0x1b:
      stream->format = bc->get_bits(4);
      stream->rate   = bc->get_bits(4);
      stream->aspect = bc->get_bits(4);
      bc->skip_bits(2);
      stream->oc_flag = bc->get_bits(1);
      bc->skip_bits(1);
      break;

    case 0x03:
    case 0x04:
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
    case 0xa1:
    case 0xa2:
      stream->format = bc->get_bits(4);
      stream->rate   = bc->get_bits(4);
      bc->get_bytes(reinterpret_cast<unsigned char *>(language), 3);
      break;

    case 0x90:
    case 0x91:
    case 0xa0:
      bc->get_bytes(reinterpret_cast<unsigned char *>(language), 3);
      break;

    case 0x92:
      stream->char_code = bc->get_bits(8);
      bc->get_bytes(reinterpret_cast<unsigned char *>(language), 3);
      break;

    default:
      if (m_debug)
        mxinfo(boost::format("clpi::parser_c::parse_program_stream: Unknown coding type %1%\n") % static_cast<unsigned int>(stream->coding_type));
      break;
  }

  stream->language = language;

  bc->set_bit_position(position_in_bits + length_in_bytes * 8);
}
