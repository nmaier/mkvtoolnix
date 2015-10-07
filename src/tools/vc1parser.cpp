/*
   vc1parser - A tool for testing the VC1 bitstream parser

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
#include "common/translation.h"
#include "common/vc1.h"

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
  virtual void handle_end_of_sequence_packet(memory_cptr packet);
  virtual void handle_entrypoint_packet(memory_cptr packet);
  virtual void handle_field_packet(memory_cptr packet);
  virtual void handle_frame_packet(memory_cptr packet);
  virtual void handle_sequence_header_packet(memory_cptr packet);
  virtual void handle_slice_packet(memory_cptr packet);
  virtual void handle_unknown_packet(uint32_t marker, memory_cptr packet);

  virtual void dump_sequence_header(vc1::sequence_header_t &seqhdr);
  virtual void dump_entrypoint(vc1::entrypoint_t &entrypoint);
  virtual void dump_frame_header(vc1::frame_header_t &frame_header);

  virtual std::string create_checksum_info(memory_cptr packet);
};

void
vc1_info_c::handle_end_of_sequence_packet(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("End of sequence at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);
}

void
vc1_info_c::handle_entrypoint_packet(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Entrypoint at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);

  if (!g_opt_entrypoints)
    return;

  if (!m_seqhdr_found) {
    mxinfo(Y("  No sequence header found yet; parsing not possible\n"));
    return;
  }

  vc1::entrypoint_t entrypoint;
  if (vc1::parse_entrypoint(packet->get_buffer(), packet->get_size(), entrypoint, m_seqhdr))
    dump_entrypoint(entrypoint);
}

void
vc1_info_c::handle_field_packet(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Field at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);
}

void
vc1_info_c::handle_frame_packet(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Frame at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);

  if (!g_opt_frames)
    return;

  if (!m_seqhdr_found) {
    mxinfo(Y("  No sequence header found yet; parsing not possible\n"));
    return;
  }

  vc1::frame_header_t frame_header;
  if (vc1::parse_frame_header(packet->get_buffer(), packet->get_size(), frame_header, m_seqhdr))
    dump_frame_header(frame_header);
}

void
vc1_info_c::handle_sequence_header_packet(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Sequence header at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);

  m_seqhdr_found = vc1::parse_sequence_header(packet->get_buffer(), packet->get_size(), m_seqhdr);

  if (g_opt_sequence_headers) {
    if (m_seqhdr_found)
      dump_sequence_header(m_seqhdr);
    else
      mxinfo(Y("  parsing failed\n"));
  }
}

void
vc1_info_c::handle_slice_packet(memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Slice at %1% size %2%%3%\n")) % m_stream_pos % packet->get_size() % checksum);
}

void
vc1_info_c::handle_unknown_packet(uint32_t marker,
                                  memory_cptr packet) {
  std::string checksum = create_checksum_info(packet);
  mxinfo(boost::format(Y("Unknown (0x%|1$08x|) at %2% size %3%%4%\n")) % marker % m_stream_pos % packet->get_size() % checksum);
}

std::string
vc1_info_c::create_checksum_info(memory_cptr packet) {
  if (!g_opt_checksum)
    return "";

  return (boost::format(Y(" checksum 0x%|1$08x|")) % calc_adler32(packet->get_buffer(), packet->get_size())).str();
}

void
vc1_info_c::dump_sequence_header(vc1::sequence_header_t &seqhdr) {
  static const char *profile_names[4] = { "Simple", "Main", "Complex", "Advanced" };

  mxinfo(boost::format(Y("  Sequence header dump:\n"
                         "    profile:               %1% (%2%)\n"
                         "    level:                 %3%\n"
                         "    chroma_format:         %4%\n"
                         "    frame_rtq_postproc:    %5%\n"
                         "    bit_rtq_postproc:      %6%\n"
                         "    postproc_flag:         %7%\n"
                         "    pixel_width:           %8%\n"
                         "    pixel_height:          %9%\n"
                         "    pulldown_flag:         %10%\n"
                         "    interlace_flag:        %11%\n"
                         "    tf_counter_flag:       %12%\n"
                         "    f_inter_p_flag:        %13%\n"
                         "    psf_mode_flag:         %14%\n"
                         "    display_info_flag:     %15%\n"
                         "    display_width:         %16%\n"
                         "    display_height:        %17%\n"
                         "    aspect_ratio_flag:     %18%\n"
                         "    aspect_ratio_width:    %19%\n"
                         "    aspect_ratio_height:   %20%\n"
                         "    framerate_flag:        %21%\n"
                         "    framerate_num:         %22%\n"
                         "    framerate_den:         %23%\n"
                         "    hrd_param_flag:        %24%\n"
                         "    hrd_num_leaky_buckets: %25%\n"))
         % seqhdr.profile % profile_names[seqhdr.profile]
         % seqhdr.level
         % seqhdr.chroma_format
         % seqhdr.frame_rtq_postproc
         % seqhdr.bit_rtq_postproc
         % seqhdr.postproc_flag
         % seqhdr.pixel_width
         % seqhdr.pixel_height
         % seqhdr.pulldown_flag
         % seqhdr.interlace_flag
         % seqhdr.tf_counter_flag
         % seqhdr.f_inter_p_flag
         % seqhdr.psf_mode_flag
         % seqhdr.display_info_flag
         % seqhdr.display_width
         % seqhdr.display_height
         % seqhdr.aspect_ratio_flag
         % seqhdr.aspect_ratio_width
         % seqhdr.aspect_ratio_height
         % seqhdr.framerate_flag
         % seqhdr.framerate_num
         % seqhdr.framerate_den
         % seqhdr.hrd_param_flag
         % seqhdr.hrd_num_leaky_buckets);
}

void
vc1_info_c::dump_entrypoint(vc1::entrypoint_t &entrypoint) {
  mxinfo(boost::format(Y("  Entrypoint dump:\n"
                         "    broken_link_flag:      %1%\n"
                         "    closed_entry_flag:     %2%\n"
                         "    pan_scan_flag:         %3%\n"
                         "    refdist_flag:          %4%\n"
                         "    loop_filter_flag:      %5%\n"
                         "    fast_uvmc_flag:        %6%\n"
                         "    extended_mv_flag:      %7%\n"
                         "    dquant:                %8%\n"
                         "    vs_transform_flag:     %9%\n"
                         "    overlap_flag:          %10%\n"
                         "    quantizer_mode:        %11%\n"
                         "    coded_dimensions_flag: %12%\n"
                         "    coded_width:           %13%\n"
                         "    coded_height:          %14%\n"
                         "    extended_dmv_flag:     %15%\n"
                         "    luma_scaling_flag:     %16%\n"
                         "    luma_scaling:          %17%\n"
                         "    chroma_scaling_flag:   %18%\n"
                         "    chroma_scaling:        %19%\n"))
         % entrypoint.broken_link_flag
         % entrypoint.closed_entry_flag
         % entrypoint.pan_scan_flag
         % entrypoint.refdist_flag
         % entrypoint.loop_filter_flag
         % entrypoint.fast_uvmc_flag
         % entrypoint.extended_mv_flag
         % entrypoint.dquant
         % entrypoint.vs_transform_flag
         % entrypoint.overlap_flag
         % entrypoint.quantizer_mode
         % entrypoint.coded_dimensions_flag
         % entrypoint.coded_width
         % entrypoint.coded_height
         % entrypoint.extended_dmv_flag
         % entrypoint.luma_scaling_flag
         % entrypoint.luma_scaling
         % entrypoint.chroma_scaling_flag
         % entrypoint.chroma_scaling);
}

void
vc1_info_c::dump_frame_header(vc1::frame_header_t &frame_header) {
  mxinfo(boost::format(Y("  Frame header dump:\n"
                         "    fcm:                     %1% (%2%)\n"
                         "    frame_type:              %3%\n"
                         "    tf_counter:              %4%\n"
                         "    repeat_frame:            %5%\n"
                         "    top_field_first_flag:    %6%\n"
                         "    repeat_first_field_flag: %7%\n"))
         % frame_header.fcm
         % (  frame_header.fcm        == 0x00                      ? Y("progressive")
            : frame_header.fcm        == 0x10                      ? Y("frame-interlace")
            : frame_header.fcm        == 0x11                      ? Y("field-interlace")
            :                                                        Y("unknown"))
         % (  frame_header.frame_type == vc1::FRAME_TYPE_I         ? Y("I")
            : frame_header.frame_type == vc1::FRAME_TYPE_P         ? Y("P")
            : frame_header.frame_type == vc1::FRAME_TYPE_B         ? Y("B")
            : frame_header.frame_type == vc1::FRAME_TYPE_BI        ? Y("BI")
            : frame_header.frame_type == vc1::FRAME_TYPE_P_SKIPPED ? Y("P (skipped)")
            :                                                        Y("unknown"))
         % frame_header.tf_counter
         % frame_header.repeat_frame
         % frame_header.top_field_first_flag
         % frame_header.repeat_first_field_flag);
}

static void
show_help() {
  mxinfo(Y("vc1parser [options] input_file_name\n"
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
           "  -V, --version          Print version information\n"));
  mxexit(0);
}

static void
show_version() {
  mxinfo("vc1parser v" VERSION "\n");
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

    else if ((*arg == "-e") || (*arg == "--entrypoints"))
      g_opt_entrypoints = true;

    else if ((*arg == "-f") || (*arg == "--frames"))
      g_opt_frames = true;

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
  mtx_common_init("vc1parser", argv[0]);

  std::vector<std::string> args = command_line_utf8(argc, argv);
  std::string file_name    = parse_args(args);

  try {
    parse_file(file_name);
  } catch (...) {
    mxerror(Y("File not found\n"));
  }

  return 0;
}
