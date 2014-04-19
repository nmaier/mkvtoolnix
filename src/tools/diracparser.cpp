/*
   diracparser - A tool for testing the Dirac bitstream parser

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/bit_cursor.h"
#include "common/byte_buffer.h"
#include "common/checksums.h"
#include "common/common_pch.h"
#include "common/mm_io.h"
#include "common/dirac.h"
#include "common/translation.h"

static bool g_opt_checksum         = false;
static bool g_opt_sequence_headers = false;

class dirac_info_c: public dirac::es_parser_c {
public:
  dirac_info_c()
    : dirac::es_parser_c() {
  }

  virtual ~dirac_info_c() {
  }

protected:
  virtual void handle_auxiliary_data_unit(memory_cptr packet);
  virtual void handle_end_of_sequence_unit(memory_cptr packet);
  virtual void handle_padding_unit(memory_cptr packet);
  virtual void handle_picture_unit(memory_cptr packet);
  virtual void handle_sequence_header_unit(memory_cptr packet);
  virtual void handle_unknown_unit(memory_cptr packet);

  virtual void dump_sequence_header(dirac::sequence_header_t &seqhdr);

  virtual std::string create_checksum_info(memory_cptr packet);
};

void
dirac_info_c::handle_auxiliary_data_unit(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Auxiliary data at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);
}

void
dirac_info_c::handle_end_of_sequence_unit(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("End of sequence at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);
}

void
dirac_info_c::handle_padding_unit(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Padding at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);
}

void
dirac_info_c::handle_picture_unit(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Picture at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);
}

void
dirac_info_c::handle_sequence_header_unit(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Sequence header at %1% size %2%%3%\n" )) %m_stream_pos % packet->get_size() % checksum);

  m_seqhdr_found = dirac::parse_sequence_header(packet->get_buffer(), packet->get_size(), m_seqhdr);

  if (g_opt_sequence_headers) {
    if (m_seqhdr_found)
      dump_sequence_header(m_seqhdr);
    else
      mxinfo(Y("  parsing failed\n"));
  }
}

void
dirac_info_c::handle_unknown_unit(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Unknown (0x%|1$02x|) at %2% size %3%%4%\n")) % (int)packet->get_buffer()[4] % m_stream_pos % packet->get_size() % checksum);
}

std::string
dirac_info_c::create_checksum_info(memory_cptr packet) {
  if (!g_opt_checksum)
    return "";

  return (boost::format(Y(" checksum 0x%|1$08x|")) % calc_adler32(packet->get_buffer(), packet->get_size())).str();
}

void
dirac_info_c::dump_sequence_header(dirac::sequence_header_t &seqhdr) {
  mxinfo(boost::format(Y("  Sequence header dump:\n"
                         "    major_version:            %1%\n"
                         "    minor_version:            %2%\n"
                         "    profile:                  %3%\n"
                         "    level:                    %4%\n"
                         "    base_video_format:        %5%\n"
                         "    pixel_width:              %6%\n"
                         "    pixel_height:             %7%\n"
                         "    chroma_format:            %8%\n"
                         "    interlaced:               %9%\n"
                         "    top_field_first:          %10%\n"
                         "    frame_rate_numerator:     %11%\n"
                         "    frame_rate_denominator:   %12%\n"
                         "    aspect_ratio_numerator:   %13%\n"
                         "    aspect_ratio_denominator: %14%\n"
                         "    clean_width:              %15%\n"
                         "    clean_height:             %16%\n"
                         "    left_offset:              %17%\n"
                         "    top_offset:               %18%\n"))
         % seqhdr.major_version
         % seqhdr.minor_version
         % seqhdr.profile
         % seqhdr.level
         % seqhdr.base_video_format
         % seqhdr.pixel_width
         % seqhdr.pixel_height
         % seqhdr.chroma_format
         % seqhdr.interlaced
         % seqhdr.top_field_first
         % seqhdr.frame_rate_numerator
         % seqhdr.frame_rate_denominator
         % seqhdr.aspect_ratio_numerator
         % seqhdr.aspect_ratio_denominator
         % seqhdr.clean_width
         % seqhdr.clean_height
         % seqhdr.left_offset
         % seqhdr.top_offset);
}

static void
show_help() {
  mxinfo(Y("diracparser [options] input_file_name\n"
           "\n"
           "Options for output and information control:\n"
           "\n"
           "  -c, --checksum         Calculate and output checksums of each unit\n"
           "  -s, --sequence-headers Show the content of sequence headers\n"
           "\n"
           "General options:\n"
           "\n"
           "  -h, --help             This help text\n"
           "  -V, --version          Print version information\n"));
  mxexit(0);
}

static void
show_version() {
  mxinfo("diracparser v" VERSION "\n");
  mxexit(0);
}

static std::string
parse_args(std::vector<std::string> &args) {
  std::string file_name;

  std::vector<std::string>::iterator arg = args.begin();
  while (arg != args.end()) {
    if ((*arg == "-h") || (*arg == "--help"))
      show_help();

    else if ((*arg == "-V") || (*arg == "--version"))
      show_version();

    else if ((*arg == "-c") || (*arg == "--checksum"))
      g_opt_checksum = true;

    else if ((*arg == "-s") || (*arg == "--sequence-headers"))
      g_opt_sequence_headers = true;

    else if (!file_name.empty())
      mxerror(Y("More than one input file given\n"));

    else
      file_name = *arg;

    ++arg;
  }

  if (file_name.empty())
    mxerror(Y("No file name given\n"));

  return file_name;
}

static void
parse_file(const std::string &file_name) {
  mm_file_io_c in(file_name);

  const int buf_size = 100000;
  int64_t size       = in.get_size();

  if (4 > size)
    mxerror(Y("File too small\n"));

  memory_cptr mem    = memory_c::alloc(buf_size);
  unsigned char *ptr = mem->get_buffer();

  dirac_info_c parser;

  while (1) {
    int num_read = in.read(ptr, buf_size);

    parser.add_bytes(ptr, num_read);
    if (num_read < buf_size) {
      parser.flush();
      break;
    }
  }
}

int
main(int argc,
     char **argv) {
  mtx_common_init("diracparser", argv[0]);

  std::vector<std::string> args = command_line_utf8(argc, argv);
  std::string file_name    = parse_args(args);

  try {
    parse_file(file_name);
  } catch (...) {
    mxerror(Y("File not found\n"));
  }

  return 0;
}
