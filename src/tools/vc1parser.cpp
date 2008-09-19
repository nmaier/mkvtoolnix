#include "os.h"

#include "bit_cursor.h"
#include "byte_buffer.h"
#include "checksums.h"
#include "common.h"
#include "mm_io.h"
#include "vc1_common.h"

static bool g_opt_checksum         = false;
static bool g_opt_entrypoints      = false;
static bool g_opt_frames           = false;
static bool g_opt_sequence_headers = false;

class vc1_info_c: public vc1::es_parser_c {
public:
  vc1_info_c()
    : vc1::es_parser_c() {
  }

  virtual ~vc1_info_c() {
  }

protected:
  virtual void handle_entrypoint_packet(memory_cptr packet);
  virtual void handle_field_packet(memory_cptr packet);
  virtual void handle_frame_packet(memory_cptr packet);
  virtual void handle_sequence_header_packet(memory_cptr packet);
  virtual void handle_slice_packet(memory_cptr packet);
  virtual void handle_unknown_packet(uint32_t marker, memory_cptr packet);

  virtual void dump_sequence_header(vc1::sequence_header_t &seqhdr);
  virtual void dump_entrypoint(vc1::entrypoint_t &entrypoint);
  virtual void dump_frame_header(vc1::frame_header_t &frame_header);

  virtual string create_checksum_info(memory_cptr packet);
};

void
vc1_info_c::handle_entrypoint_packet(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Entrypoint at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());

  if (!g_opt_entrypoints)
    return;

  if (!m_seqhdr_found) {
    mxinfo("  No sequence header found yet; parsing not possible\n");
    return;
  }

  vc1::entrypoint_t entrypoint;
  if (vc1::parse_entrypoint(packet->get(), packet->get_size(), entrypoint, m_seqhdr))
    dump_entrypoint(entrypoint);
}

void
vc1_info_c::handle_field_packet(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Field at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());
}

void
vc1_info_c::handle_frame_packet(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Frame at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());

  if (!g_opt_frames)
    return;

  if (!m_seqhdr_found) {
    mxinfo("  No sequence header found yet; parsing not possible\n");
    return;
  }

  vc1::frame_header_t frame_header;
  if (vc1::parse_frame_header(packet->get(), packet->get_size(), frame_header, m_seqhdr))
    dump_frame_header(frame_header);
}

void
vc1_info_c::handle_sequence_header_packet(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Sequence header at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());

  m_seqhdr_found = vc1::parse_sequence_header(packet->get(), packet->get_size(), m_seqhdr);

  if (g_opt_sequence_headers) {
    if (m_seqhdr_found)
      dump_sequence_header(m_seqhdr);
    else
      mxinfo("  parsing failed\n");
  }
}

void
vc1_info_c::handle_slice_packet(memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Slice at " LLD " size %d%s\n", m_stream_pos, packet->get_size(), checksum.c_str());
}

void
vc1_info_c::handle_unknown_packet(uint32_t marker,
                                  memory_cptr packet) {
  string checksum = create_checksum_info(packet);
  mxinfo("Unknown (0x%08x) at " LLD " size %d%s\n", marker, m_stream_pos, packet->get_size(), checksum.c_str());
}

string
vc1_info_c::create_checksum_info(memory_cptr packet) {
  if (!g_opt_checksum)
    return "";

  return mxsprintf(" checksum 0x%08x", calc_adler32(packet->get(), packet->get_size()));
}

void
vc1_info_c::dump_sequence_header(vc1::sequence_header_t &seqhdr) {
  static const char *profile_names[4] = { "Simple", "Main", "Complex", "Advanced" };

  mxinfo("  Sequence header dump:\n"
         "    profile:               %d (%s)\n"
         "    level:                 %d\n"
         "    chroma_format:         %d\n"
         "    frame_rtq_postproc:    %d\n"
         "    bit_rtq_postproc:      %d\n"
         "    postproc_flag:         %d\n"
         "    pixel_width:           %d\n"
         "    pixel_height:          %d\n"
         "    pulldown_flag:         %d\n"
         "    interlace_flag:        %d\n"
         "    tf_counter_flag:       %d\n"
         "    f_inter_p_flag:        %d\n"
         "    psf_mode_flag:         %d\n"
         "    display_info_flag:     %d\n"
         "    display_width:         %d\n"
         "    display_height:        %d\n"
         "    aspect_ratio_flag:     %d\n"
         "    aspect_ratio_width:    %d\n"
         "    aspect_ratio_height:   %d\n"
         "    framerate_flag:        %d\n"
         "    framerate_num:         %d\n"
         "    framerate_den:         %d\n"
         "    hrd_param_flag:        %d\n"
         "    hrd_num_leaky_buckets: %d\n",
         seqhdr.profile, profile_names[seqhdr.profile],
         seqhdr.level,
         seqhdr.chroma_format,
         seqhdr.frame_rtq_postproc,
         seqhdr.bit_rtq_postproc,
         seqhdr.postproc_flag,
         seqhdr.pixel_width,
         seqhdr.pixel_height,
         seqhdr.pulldown_flag,
         seqhdr.interlace_flag,
         seqhdr.tf_counter_flag,
         seqhdr.f_inter_p_flag,
         seqhdr.psf_mode_flag,
         seqhdr.display_info_flag,
         seqhdr.display_width,
         seqhdr.display_height,
         seqhdr.aspect_ratio_flag,
         seqhdr.aspect_ratio_width,
         seqhdr.aspect_ratio_height,
         seqhdr.framerate_flag,
         seqhdr.framerate_num,
         seqhdr.framerate_den,
         seqhdr.hrd_param_flag,
         seqhdr.hrd_num_leaky_buckets);
}

void
vc1_info_c::dump_entrypoint(vc1::entrypoint_t &entrypoint) {
  mxinfo("  Entrypoint dump:\n"
         "    broken_link_flag:      %d\n"
         "    closed_entry_flag:     %d\n"
         "    pan_scan_flag:         %d\n"
         "    refdist_flag:          %d\n"
         "    loop_filter_flag:      %d\n"
         "    fast_uvmc_flag:        %d\n"
         "    extended_mv_flag:      %d\n"
         "    dquant:                %d\n"
         "    vs_transform_flag:     %d\n"
         "    overlap_flag:          %d\n"
         "    quantizer_mode:        %d\n"
         "    coded_dimensions_flag: %d\n"
         "    coded_width:           %d\n"
         "    coded_height:          %d\n"
         "    extended_dmv_flag:     %d\n"
         "    luma_scaling_flag:     %d\n"
         "    luma_scaling:          %d\n"
         "    chroma_scaling_flag:   %d\n"
         "    chroma_scaling:        %d\n",
         entrypoint.broken_link_flag,
         entrypoint.closed_entry_flag,
         entrypoint.pan_scan_flag,
         entrypoint.refdist_flag,
         entrypoint.loop_filter_flag,
         entrypoint.fast_uvmc_flag,
         entrypoint.extended_mv_flag,
         entrypoint.dquant,
         entrypoint.vs_transform_flag,
         entrypoint.overlap_flag,
         entrypoint.quantizer_mode,
         entrypoint.coded_dimensions_flag,
         entrypoint.coded_width,
         entrypoint.coded_height,
         entrypoint.extended_dmv_flag,
         entrypoint.luma_scaling_flag,
         entrypoint.luma_scaling,
         entrypoint.chroma_scaling_flag,
         entrypoint.chroma_scaling);
}

void
vc1_info_c::dump_frame_header(vc1::frame_header_t &frame_header) {
  mxinfo("  Frame header dump:\n"
         "    fcm:                     %d\n"
         "    frame_type:              %s\n"
         "    tf_counter:              %d\n"
         "    repeat_frame:            %d\n"
         "    top_field_first_flag:    %d\n"
         "    repeat_first_field_flag: %d\n",
         frame_header.fcm,
           frame_header.frame_type == vc1::FRAME_TYPE_I         ? "I"
         : frame_header.frame_type == vc1::FRAME_TYPE_P         ? "P"
         : frame_header.frame_type == vc1::FRAME_TYPE_B         ? "B"
         : frame_header.frame_type == vc1::FRAME_TYPE_BI        ? "BI"
         : frame_header.frame_type == vc1::FRAME_TYPE_P_SKIPPED ? "P (skipped)"
         :                                                        "unknown",
         frame_header.tf_counter,
         frame_header.repeat_frame,
         frame_header.top_field_first_flag,
         frame_header.repeat_first_field_flag);
}

static void
show_help() {
  mxinfo("vc1parser [options] input_file_name\n"
         "\n"
         "Options for output and information control:\n"
         "\n"
         "  -c, --checksum         Calculate and output checksums of each unit\n"
         "  -e, --entrypoints      Show the content of entry point headers\n"
         "  -f, --frames           Show basic frame header content\n"
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
  mxinfo("vc1parser v" VERSION "\n");
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

    else if ((*arg == "-e") || (*arg == "--entrypoints"))
      g_opt_entrypoints = true;

    else if ((*arg == "-f") || (*arg == "--frames"))
      g_opt_frames = true;

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

  vc1_info_c parser;

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
