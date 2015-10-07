/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   CoreAudio reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Initial DTS support by Peter Niemayer <niemayer@isg.de> and
     modified by Moritz Bunkus.
*/

#include "common/common_pch.h"

#include "common/alac.h"
#include "common/endian.h"
#include "common/mm_io_x.h"
#include "common/strings/formatting.h"
#include "input/r_coreaudio.h"
#include "output/p_alac.h"

int
coreaudio_reader_c::probe_file(mm_io_c *in,
                               uint64_t size) {
  try {
    if (8 > size)
      return false;

    in->setFilePointer(0, seek_beginning);

    std::string magic;
    return (4 == in->read(magic, 4)) && (balg::to_lower_copy(magic) == "caff") ? 1 : 0;

  } catch (...) {
  }

  return 0;
}

coreaudio_reader_c::coreaudio_reader_c(const track_info_c &ti,
                                       const mm_io_cptr &in)
  : generic_reader_c{ti, in}
  , m_supported{}
  , m_sample_rate{}
  , m_flags{}
  , m_bytes_per_packet{}
  , m_frames_per_packet{}
  , m_channels{}
  , m_bites_per_sample{}
  , m_debug_headers{"coreaudio_reader|coreaudio_reader_headers"}
  , m_debug_chunks{ "coreaudio_reader|coreaudio_reader_chunks"}
  , m_debug_packets{"coreaudio_reader|coreaudio_reader_packets"}
{
}

coreaudio_reader_c::~coreaudio_reader_c() {
}

void
coreaudio_reader_c::identify() {
  if (m_supported) {
    id_result_container();
    id_result_track(0, ID_RESULT_TRACK_AUDIO, m_codec.get_name(m_codec_name));

  } else
    id_result_container_unsupported(m_in->get_file_name(), (boost::format("CoreAudio (%1%)") % m_codec_name).str());
}

void
coreaudio_reader_c::read_headers() {
  if (!coreaudio_reader_c::probe_file(m_in.get(), m_size))
    throw mtx::input::invalid_format_x();

  // Skip "caff" magic, version and flags
  m_in->setFilePointer(8);

  scan_chunks();

  parse_desc_chunk();
  parse_pakt_chunk();
  parse_kuki_chunk();

  if (m_debug_headers)
    dump_headers();
}

void
coreaudio_reader_c::dump_headers()
  const {
  mxdebug(boost::format("File '%1%' CoreAudio header dump\n"
                        "  'desc' chunk:\n"
                        "    codec:             %2%\n"
                        "    supported:         %3%\n"
                        "    sample_rate:       %4%\n"
                        "    flags:             %5%\n"
                        "    bytes_per_packet:  %6%\n"
                        "    frames_per_packet: %7%\n"
                        "    channels:          %8%\n"
                        "    bits_per_sample:   %9%\n"
                        "  'kuki' chunk:\n"
                        "    magic cookie:      %10%\n"
                        )
          % m_ti.m_fname
          % m_codec_name % m_supported % m_sample_rate % m_flags % m_bytes_per_packet % m_frames_per_packet % m_channels % m_bites_per_sample
          % (m_magic_cookie ? (boost::format("present, size %1%") % m_magic_cookie->get_size()).str() : std::string{"not present"})
          );

  if (m_codec.is(CT_A_ALAC) && m_magic_cookie && (m_magic_cookie->get_size() >= sizeof(alac::codec_config_t))) {
    auto cfg = reinterpret_cast<alac::codec_config_t *>(m_magic_cookie->get_buffer());

    mxdebug(boost::format("ALAC magic cookie dump:\n"
                          "  frame length:         %1%\n"
                          "  compatible version:   %2%\n"
                          "  bit depth:            %3%\n"
                          "  rice history mult:    %4%\n"
                          "  rice initial history: %5%\n"
                          "  rice limit:           %6%\n"
                          "  num channels:         %7%\n"
                          "  max run:              %8%\n"
                          "  max frame bytes:      %9%\n"
                          "  avg bit rate:         %10%\n"
                          "  sample rate:          %11%\n"
                          )
                          % get_uint32_be(&cfg->frame_length)
                          % static_cast<unsigned int>(cfg->compatible_version)
                          % static_cast<unsigned int>(cfg->bit_depth)
                          % static_cast<unsigned int>(cfg->rice_history_mult)
                          % static_cast<unsigned int>(cfg->rice_initial_history)
                          % static_cast<unsigned int>(cfg->rice_limit)
                          % static_cast<unsigned int>(cfg->num_channels)
                          % get_uint16_be(&cfg->max_run)
                          % get_uint32_be(&cfg->max_frame_bytes)
                          % get_uint32_be(&cfg->avg_bit_rate)
                          % get_uint32_be(&cfg->sample_rate)
            );
  }
}

void
coreaudio_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0) || ! m_supported)
    return;

  if (m_codec.is(CT_A_ALAC))
    add_packetizer(create_alac_packetizer());
  else
    assert(false);

  show_packetizer_info(0, PTZR0);
}

generic_packetizer_c *
coreaudio_reader_c::create_alac_packetizer() {
  return new alac_packetizer_c(this, m_ti, m_magic_cookie, m_sample_rate, m_channels);
}

file_status_e
coreaudio_reader_c::read(generic_packetizer_c *,
                         bool) {
  if (!m_supported || (m_current_packet == m_packets.end()))
    return FILE_STATUS_DONE;

  try {
    m_in->setFilePointer(m_current_packet->m_position, seek_beginning);
    auto mem = memory_c::alloc(m_current_packet->m_size);
    if (m_in->read(mem, m_current_packet->m_size) != m_current_packet->m_size)
      throw false;

    PTZR0->process(new packet_t(mem, m_current_packet->m_timecode * m_frames_to_timecode, m_current_packet->m_duration * m_frames_to_timecode));

    ++m_current_packet;

  } catch (...) {
    m_current_packet = m_packets.end();
  }

  return m_current_packet == m_packets.end() ? FILE_STATUS_DONE : FILE_STATUS_MOREDATA;
}

void
coreaudio_reader_c::scan_chunks() {
  try {
    auto file_size = m_in->get_size();

    while (true) {
      coreaudio_chunk_t chunk;
      chunk.m_position = m_in->getFilePointer();

      if (m_in->read(chunk.m_type, 4) != 4)
        return;

      chunk.m_size          = m_in->read_uint64_be();
      chunk.m_size          = std::min<uint64_t>(chunk.m_size ? chunk.m_size : std::numeric_limits<uint64_t>::max(), file_size);
      chunk.m_data_position = m_in->getFilePointer();

      mxdebug_if(m_debug_chunks,boost::format("scan_chunks() new chunk at %1% data position %4% type '%2%' size %3%\n") % chunk.m_position % get_displayable_string(chunk.m_type) % chunk.m_size % chunk.m_data_position);

      m_chunks.push_back(chunk);
      m_in->setFilePointer(chunk.m_size, seek_current);

    }
  } catch (...) {
  }
}

void
coreaudio_reader_c::debug_error_and_throw(boost::format const &format)
  const {
  mxdebug_if(m_debug_headers, boost::format("%1%\n") % format);
  throw mtx::input::header_parsing_x{};
}

coreaudio_chunk_itr
coreaudio_reader_c::find_chunk(std::string const &type,
                               bool throw_on_error,
                               coreaudio_chunk_itr start) {
  auto chunk = std::find_if(start, m_chunks.end(), [&](coreaudio_chunk_t const &chunk) { return chunk.m_type == type; });

  if (throw_on_error && (chunk == m_chunks.end()))
    debug_error_and_throw(boost::format("Chunk type '%1%' not found") % type);

  return chunk;
}

memory_cptr
coreaudio_reader_c::read_chunk(std::string const &type,
                               bool throw_on_error) {
  try {
    auto chunk = find_chunk(type, throw_on_error);
    if (chunk == m_chunks.end())
      return nullptr;

    if (!chunk->m_size)
      debug_error_and_throw(boost::format("Chunk '%1%' at %2% has zero size") % type % chunk->m_position);

    m_in->setFilePointer(chunk->m_data_position);

    memory_cptr mem{memory_c::alloc(chunk->m_size)};
    if (chunk->m_size != m_in->read(mem, chunk->m_size))
      throw mtx::mm_io::end_of_file_x{};

    return mem;

  } catch (mtx::mm_io::exception &ex) {
    debug_error_and_throw(boost::format("I/O error reading the '%1%' chunk: %2%") % type % ex);

  } catch (mtx::input::header_parsing_x &) {
    throw;

  } catch (...) {
    debug_error_and_throw(boost::format("Generic error reading the '%1%' chunk") % type);
  }

  return nullptr;
}

void
coreaudio_reader_c::parse_desc_chunk() {
  auto mem = read_chunk("desc");
  mm_mem_io_c chunk{*mem};

  try {
    m_sample_rate = chunk.read_double();
    m_frames_to_timecode.set(1000000000, m_sample_rate);

    if (4 != chunk.read(m_codec_name, 4)) {
      m_codec_name.clear();
      throw mtx::mm_io::end_of_file_x{};
    }

    balg::to_upper(m_codec_name);
    m_codec             = codec_c::look_up(m_codec_name);
    m_flags             = chunk.read_uint32_be();
    m_bytes_per_packet  = chunk.read_uint32_be();
    m_frames_per_packet = chunk.read_uint32_be();
    m_channels          = chunk.read_uint32_be();
    m_bites_per_sample  = chunk.read_uint32_be();

    if (m_codec.is(CT_A_ALAC))
      m_supported = true;

  } catch (mtx::mm_io::exception &ex) {
    debug_error_and_throw(boost::format("I/O exception during 'desc' parsing: %1%") % ex.what());
  } catch (...) {
    debug_error_and_throw(boost::format("Unknown exception during 'desc' parsing"));
  }
}

void
coreaudio_reader_c::parse_pakt_chunk() {
  try {
    auto chunk = read_chunk("pakt");
    mm_mem_io_c mem{*chunk};

    auto num_packets  = mem.read_uint64_be();
    auto num_frames   = mem.read_uint64_be()  // valid frames
                      + mem.read_uint32_be()  // priming frames
                      + mem.read_uint32_be(); // remainder frames
    auto data_chunk   = find_chunk("data");
    auto position     = data_chunk->m_data_position + 4; // skip the "edit count" field
    uint64_t timecode = 0;

    mxdebug_if(m_debug_headers, boost::format("Number of packets: %1%, number of frames: %2%\n") % num_packets % num_frames);

    for (auto packet_num = 0u; packet_num < num_packets; ++packet_num) {
      coreaudio_packet_t packet;

      packet.m_position  = position;
      packet.m_size      = m_bytes_per_packet  ? m_bytes_per_packet  : mem.read_mp4_descriptor_len();
      packet.m_duration  = m_frames_per_packet ? m_frames_per_packet : mem.read_mp4_descriptor_len();
      packet.m_timecode  = timecode;
      position          += packet.m_size;
      timecode          += packet.m_duration;

      m_packets.push_back(packet);

      mxdebug_if(m_debug_packets,
                 boost::format("  %4%) position %1% size %2% raw duration %3% raw timecode %6% duration %7% timecode %8% position in 'pakt' %5%\n")
                 % packet.m_position % packet.m_size % packet.m_duration % packet_num % mem.getFilePointer() % packet.m_timecode
                 % format_timecode(packet.m_duration * m_frames_to_timecode) % format_timecode(packet.m_timecode * m_frames_to_timecode));
    }

    mxdebug_if(m_debug_headers, boost::format("Final value for 'position': %1% data chunk end: %2%\n") % position % (data_chunk->m_position + 12 + data_chunk->m_size));

  } catch (mtx::mm_io::exception &ex) {
    debug_error_and_throw(boost::format("I/O exception during 'pakt' parsing: %1%") % ex.what());
  } catch (...) {
    debug_error_and_throw(boost::format("Unknown exception during 'pakt' parsing"));
  }

  m_current_packet = m_packets.begin();
}

void
coreaudio_reader_c::parse_kuki_chunk() {
  if (!m_supported)
    return;

  auto chunk = read_chunk("kuki", false);
  if (!chunk)
    return;

  if (m_codec.is(CT_A_ALAC))
    handle_alac_magic_cookie(chunk);
}

void
coreaudio_reader_c::handle_alac_magic_cookie(memory_cptr chunk) {
  if (chunk->get_size() < sizeof(alac::codec_config_t))
    debug_error_and_throw(boost::format("Invalid ALAC magic cookie; size: %1% < %2%") % chunk->get_size() % sizeof(alac::codec_config_t));

  // Convert old-style magic cookie
  if (!memcmp(chunk->get_buffer() + 4, "frmaalac", 8)) {
    mxdebug_if(m_debug_headers, boost::format("Converting old-style ALAC magic cookie with size %1% to new style\n") % chunk->get_size());

    auto const old_style_min_size
      = 12                            // format atom
      + 12                            // ALAC specific info (size, ID, version, flags)
      + sizeof(alac::codec_config_t); // the magic cookie itself

    if (chunk->get_size() < old_style_min_size)
      debug_error_and_throw(boost::format("Invalid old-style ALAC magic cookie; size: %1% < %2%") % chunk->get_size() % old_style_min_size);

    m_magic_cookie = memory_c::clone(chunk->get_buffer() + 12 + 12, sizeof(alac::codec_config_t));

  } else
    m_magic_cookie = chunk;


  mxdebug_if(m_debug_headers, boost::format("Magic cookie size: %1%\n") % m_magic_cookie->get_size());
}
