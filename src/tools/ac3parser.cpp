/*
   ac3parser - A simple tool for parsing (E)AC3 files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/ac3.h"
#include "common/bit_cursor.h"
#include "common/byte_buffer.h"
#include "common/checksums.h"
#include "common/common_pch.h"
#include "common/endian.h"
#include "common/translation.h"

static bool g_opt_checksum      = false;
static bool g_opt_frame_headers = false;

static void
show_help() {
  mxinfo("ac3parser [options] input_file_name\n"
         "\n"
         "Options for output and information control:\n"
         "\n"
         "  -c, --checksum         Calculate and output checksums of each unit\n"
         "  -f, --frame-headers    Show the content of frame headers\n"
         "\n"
         "General options:\n"
         "\n"
         "  -h, --help             This help text\n"
         "  -V, --version          Print version information\n");
  mxexit(0);
}

static void
show_version() {
  mxinfo("ac3parser v" VERSION "\n");
  mxexit(0);
}

static std::string
parse_args(std::vector<std::string> &args) {
  std::string file_name;

  for (auto & arg: args) {
    if ((arg == "-h") || (arg == "--help"))
      show_help();

    else if ((arg == "-V") || (arg == "--version"))
      show_version();

    else if ((arg == "-c") || (arg == "--checksum"))
      g_opt_checksum = true;

    else if ((arg == "-f") || (arg == "--frame-headers"))
      g_opt_frame_headers = true;

    else if (!file_name.empty())
      mxerror(Y("More than one input file given\n"));

    else
      file_name = arg;
  }

  if (file_name.empty())
    mxerror(Y("No file name given\n"));

  return file_name;
}

static void
parse_file(const std::string &file_name) {
  mm_file_io_c in(file_name);

  const size_t buf_size = 100000;
  int64_t size          = in.get_size();

  if (4 > size)
    mxerror(Y("File too small\n"));

  memory_cptr mem    = memory_c::alloc(buf_size);
  unsigned char *ptr = mem->get_buffer();

  ac3::parser_c parser;

  size_t num_read;
  do {
    num_read = in.read(ptr, buf_size);

    parser.add_bytes(ptr, num_read);
    if (num_read < buf_size)
      parser.flush();

    while (parser.frame_available()) {
      ac3::frame_c frame = parser.get_frame();
      std::string output = frame.to_string(g_opt_frame_headers);

      if (g_opt_checksum) {
        uint32_t adler32  = calc_adler32(frame.m_data->get_buffer(), frame.m_data->get_size());
        output           += (boost::format(" checksum 0x%|1$08x|") % adler32).str();
      }

      mxinfo(boost::format("%1%\n") % output);
    }

  } while (num_read == buf_size);
}

int
main(int argc,
     char **argv) {
  mtx_common_init("ac3parser");

  std::vector<std::string> args = command_line_utf8(argc, argv);
  std::string file_name         = parse_args(args);

  try {
    parse_file(file_name);
  } catch (mtx::mm_io::exception &) {
    mxerror(Y("File not found\n"));
  }

  return 0;
}
