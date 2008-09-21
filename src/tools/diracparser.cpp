#include "os.h"

#include "bit_cursor.h"
#include "byte_buffer.h"
#include "checksums.h"
#include "common.h"
#include "mm_io.h"
#include "dirac_common.h"

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

  virtual string create_checksum_info(memory_cptr packet);
};

void
dirac_info_c::handle_auxiliary_data_unit(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Auxiliary data at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());
}

void
dirac_info_c::handle_end_of_sequence_unit(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("End of sequence at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());
}

void
dirac_info_c::handle_padding_unit(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Padding at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());
}

void
dirac_info_c::handle_picture_unit(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Picture at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());
}

void
dirac_info_c::handle_sequence_header_unit(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Sequence header at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());

  m_seqhdr_found = dirac::parse_sequence_header(packet->get(), packet->get_size(), m_seqhdr);

  if (g_opt_sequence_headers) {
    if (m_seqhdr_found)
      dump_sequence_header(m_seqhdr);
    else
      mxinfo("  parsing failed\n");
  }
}

void
dirac_info_c::handle_unknown_unit(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Unknown (0x%02x) at " LLD " size %d%s\n", packet->get()[4], m_stream_pos, packet->get_size(), checksum.c_str());
}

string
dirac_info_c::create_checksum_info(memory_cptr packet) {
  if (!g_opt_checksum)
    return "";

  return mxsprintf(" checksum 0x%08x", calc_adler32(packet->get(), packet->get_size()));
}

void
dirac_info_c::dump_sequence_header(dirac::sequence_header_t &seqhdr) {
  mxinfo("  Sequence header dump:\n"
         "    major_version:            %u\n"
         "    minor_version:            %u\n"
         "    profile:                  %u\n"
         "    level:                    %u\n"
         "    base_video_format:        %u\n"
         "    pixel_width:              %u\n"
         "    pixel_height:             %u\n"
         "    chroma_format:            %u\n"
         "    interlaced:               %u\n"
         "    top_field_first:          %u\n"
         "    frame_rate_numerator:     %u\n"
         "    frame_rate_denominator:   %u\n"
         "    aspect_ratio_numerator:   %u\n"
         "    aspect_ratio_denominator: %u\n"
         "    clean_width:              %u\n"
         "    clean_height:             %u\n"
         "    left_offset:              %u\n"
         "    top_offset:               %u\n",
         seqhdr.major_version,
         seqhdr.minor_version,
         seqhdr.profile,
         seqhdr.level,
         seqhdr.base_video_format,
         seqhdr.pixel_width,
         seqhdr.pixel_height,
         seqhdr.chroma_format,
         seqhdr.interlaced,
         seqhdr.top_field_first,
         seqhdr.frame_rate_numerator,
         seqhdr.frame_rate_denominator,
         seqhdr.aspect_ratio_numerator,
         seqhdr.aspect_ratio_denominator,
         seqhdr.clean_width,
         seqhdr.clean_height,
         seqhdr.left_offset,
         seqhdr.top_offset);
}

static void
show_help() {
  mxinfo("diracparser [options] input_file_name\n"
         "\n"
         "Options for output and information control:\n"
         "\n"
         "  -c, --checksum         Calculate and output checksums of each unit\n"
         "  -s, --sequence-headers Show the content of sequence headers\n"
         "\n"
         "General options:\n"
         "\n"
         "  -h, --help             This help text\n"
         "  -V, --version          Print version information\n");
  mxexit(0);
}

static void
show_version() {
  mxinfo("diracparser v" VERSION "\n");
  mxexit(0);
}

static void
setup() {
  init_stdio();

#if defined(HAVE_LIBINTL_H)
  if (setlocale(LC_MESSAGES, "") == NULL)
    mxerror("Could not set the locale properly. Check the LANG, LC_ALL and LC_MESSAGES environment variables.\n");
  bindtextdomain("mkvtoolnix", MTX_LOCALE_DIR);
  textdomain("mkvtoolnix");
#endif

  mm_file_io_c::setup();
  cc_local_utf8 = utf8_init("");
  init_cc_stdio();
}

static string
parse_args(vector<string> &args) {
  string file_name;

  vector<string>::iterator arg = args.begin();
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
      mxerror("More than one input file given\n");

    else
      file_name = *arg;

    ++arg;
  }

  if (file_name.empty())
    mxerror("No file name given\n");

  return file_name;
}

static void
parse_file(const string &file_name) {
  mm_file_io_c in(file_name);

  const int buf_size = 100000;
  int64_t size       = in.get_size();

  if (4 > size)
    mxerror("File too small\n");

  memory_cptr mem    = memory_c::alloc(buf_size);
  unsigned char *ptr = mem->get();

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
  setup();

  vector<string> args = command_line_utf8(argc, argv);
  string file_name    = parse_args(args);

  try {
    parse_file(file_name);
  } catch (...) {
    mxerror("File not found\n");
  }

  return 0;
}
