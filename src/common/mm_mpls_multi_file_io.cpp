#include "common/common_pch.h"

#include <unordered_map>

#include "common/debugging.h"
#include "common/mm_mpls_multi_file_io.h"

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
  bool debug       = debugging_requested("mpls|mpls_multi_io");
  auto mpls_parser = std::make_shared<mtx::mpls::parser_c>();

  if (!mpls_parser->parse(in) || mpls_parser->get_playlist().items.empty()) {
    mxdebug_if(debug, boost::format("Not handling because %1%\n") % (mpls_parser->is_ok() ? "playlist is empty" : "parser not OK"));
    return mm_io_cptr{};
  }

  auto mpls_dir   = bfs::system_complete(bfs::path(in->get_file_name())).remove_filename();
  auto stream_dir = mpls_dir / ".." / "STREAM";

  if (!bfs::exists(stream_dir))
    stream_dir = mpls_dir / ".." / "stream";

  auto have_stream_dir = bfs::exists(stream_dir);

  mxdebug_if(debug, boost::format("MPLS dir: %1% have stream dir: %2% stream dir: %3%\n") % mpls_dir.string() % have_stream_dir % stream_dir.string());

  auto find_file = [mpls_dir,stream_dir,have_stream_dir](mtx::mpls::play_item_t const &item) -> bfs::path {
    if (have_stream_dir) {
      auto file = stream_dir / (boost::format("%1%.%2%") % item.clip_id % item.codec_id).str();
      if (bfs::exists(file))
        return bfs::canonical(file);

      file = stream_dir / (boost::format("%1%.%2%") % item.clip_id % balg::to_lower_copy(item.codec_id)).str();
      if (bfs::exists(file))
        return bfs::canonical(file);
    }

    auto file = mpls_dir / (boost::format("%1%.%2%") % item.clip_id % item.codec_id).str();
    if (bfs::exists(file))
      return bfs::canonical(file);

    file = mpls_dir / (boost::format("%1%.%2%") % item.clip_id % balg::to_lower_copy(item.codec_id)).str();
    return bfs::exists(file) ? bfs::canonical(file) : bfs::path{};
  };

  std::vector<bfs::path> file_names;
  std::unordered_map<std::string, bool> file_names_seen;

  for (auto const &item : mpls_parser->get_playlist().items) {
    auto file = find_file(item);

    mxdebug_if(debug, boost::format("Item clip ID: %1% codec ID: %2%: have file? %3% file: %4%\n") % item.clip_id % item.codec_id % !file.empty() % file.string());
    if (file.empty() || file_names_seen[file.string()])
      continue;

    file_names.push_back(file);
    file_names_seen[file.string()] = true;
  }

  mxdebug_if(debug, boost::format("Number of files left: %1%\n") % file_names.size());

  if (file_names.empty())
    return mm_io_cptr{};

  return mm_io_cptr{new mm_mpls_multi_file_io_c{file_names, in->get_file_name(), mpls_parser}};
}
