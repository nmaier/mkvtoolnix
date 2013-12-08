#include "common/common_pch.h"

#include <unordered_map>

#include "common/debugging.h"
#include "common/mm_mpls_multi_file_io.h"
#include "common/strings/formatting.h"

debugging_option_c mm_mpls_multi_file_io_c::ms_debug{"mpls|mpls_multi_io"};

mm_mpls_multi_file_io_c::mm_mpls_multi_file_io_c(std::vector<bfs::path> const &file_names,
                                                 std::string const &display_file_name,
                                                 mtx::mpls::parser_cptr const &mpls_parser)
  : mm_multi_file_io_c{file_names, display_file_name}
  , m_mpls_parser{mpls_parser}
{
}

mm_mpls_multi_file_io_c::~mm_mpls_multi_file_io_c() {
}

std::vector<timecode_c> const &
mm_mpls_multi_file_io_c::get_chapters()
  const {
  return m_mpls_parser->get_chapters();
}

mm_io_cptr
mm_mpls_multi_file_io_c::open_multi(std::string const &display_file_name) {
  try {
    mm_file_io_c in{display_file_name};
    return open_multi(&in);
  } catch (mtx::mm_io::exception &) {
    return mm_io_cptr{};
  }
}

mm_io_cptr
mm_mpls_multi_file_io_c::open_multi(mm_io_c *in) {
  auto mpls_parser = std::make_shared<mtx::mpls::parser_c>();

  if (!mpls_parser->parse(in) || mpls_parser->get_playlist().items.empty()) {
    mxdebug_if(ms_debug, boost::format("Not handling because %1%\n") % (mpls_parser->is_ok() ? "playlist is empty" : "parser not OK"));
    return mm_io_cptr{};
  }

  auto mpls_dir   = bfs::system_complete(bfs::path(in->get_file_name())).remove_filename();
  auto stream_dir = mpls_dir.parent_path() / "STREAM";

  if (!bfs::exists(stream_dir))
    stream_dir = mpls_dir.parent_path() / "stream";

  auto have_stream_dir = bfs::exists(stream_dir);

  mxdebug_if(ms_debug, boost::format("MPLS dir: %1% have stream dir: %2% stream dir: %3%\n") % mpls_dir.string() % have_stream_dir % stream_dir.string());

  auto find_file = [mpls_dir,stream_dir,have_stream_dir](mtx::mpls::play_item_t const &item) -> bfs::path {
    if (have_stream_dir) {
      auto file = stream_dir / (boost::format("%1%.%2%") % item.clip_id % item.codec_id).str();
      if (bfs::exists(file))
        return file;

      file = stream_dir / (boost::format("%1%.%2%") % item.clip_id % balg::to_lower_copy(item.codec_id)).str();
      if (bfs::exists(file))
        return file;
    }

    auto file = mpls_dir / (boost::format("%1%.%2%") % item.clip_id % item.codec_id).str();
    if (bfs::exists(file))
      return file;

    file = mpls_dir / (boost::format("%1%.%2%") % item.clip_id % balg::to_lower_copy(item.codec_id)).str();
    return bfs::exists(file) ? file : bfs::path{};
  };

  std::vector<bfs::path> file_names;
  std::unordered_map<std::string, bool> file_names_seen;

  for (auto const &item : mpls_parser->get_playlist().items) {
    auto file = find_file(item);

    mxdebug_if(ms_debug, boost::format("Item clip ID: %1% codec ID: %2%: have file? %3% file: %4%\n") % item.clip_id % item.codec_id % !file.empty() % file.string());
    if (file.empty() || file_names_seen[file.string()])
      continue;

    file_names.push_back(file);
    file_names_seen[file.string()] = true;
  }

  mxdebug_if(ms_debug, boost::format("Number of files left: %1%\n") % file_names.size());

  if (file_names.empty())
    return mm_io_cptr{};

  return mm_io_cptr{new mm_mpls_multi_file_io_c{file_names, in->get_file_name(), mpls_parser}};
}

void
mm_mpls_multi_file_io_c::create_verbose_identification_info(std::vector<std::string> &verbose_info) {
  boost::system::error_code ec;
  auto total_size = boost::accumulate(m_files, 0ull, [&ec](unsigned long long accu, file_t const &file) { return accu + file.m_size; });

  verbose_info.push_back("playlist:1");
  verbose_info.push_back((boost::format("playlist_duration:%1%") % m_mpls_parser->get_playlist().duration.to_ns()).str());
  verbose_info.push_back((boost::format("playlist_size:%1%")     % total_size)                                    .str());
  verbose_info.push_back((boost::format("playlist_chapters:%1%") % m_mpls_parser->get_chapters().size())          .str());
  for (auto &file : m_files)
    verbose_info.push_back((boost::format("playlist_file:%1%") % escape(file.m_file_name.string())).str());
}
