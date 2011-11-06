/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>
#include <boost/regex.hpp>
#include <sstream>
#include <vector>

#include "common/mm_multi_file_io.h"
#include "common/output.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"

namespace bfs = boost::filesystem;

mm_multi_file_io_c::file_t::file_t(const bfs::path &file_name,
                                   uint64_t global_start,
                                   mm_file_io_cptr file)
  : m_file_name(file_name)
  , m_size(file->get_size())
  , m_global_start(global_start)
  , m_file(file)
{
}

mm_multi_file_io_c::mm_multi_file_io_c(std::vector<bfs::path> file_names,
                                       const std::string &display_file_name)
  : m_display_file_name(display_file_name)
  , m_total_size(0)
  , m_current_pos(0)
  , m_current_local_pos(0)
  , m_current_file(0)
{
  for (auto &file_name : file_names) {
    mm_file_io_cptr file(new mm_file_io_c(file_name.string()));
    m_files.push_back(mm_multi_file_io_c::file_t(file_name, m_total_size, file));

    m_total_size += file->get_size();
  }
}

mm_multi_file_io_c::~mm_multi_file_io_c() {
  close();
}

uint64
mm_multi_file_io_c::getFilePointer() {
  return m_current_pos;
}

void
mm_multi_file_io_c::setFilePointer(int64 offset,
                                   seek_mode mode) {
  int64_t new_pos
    = seek_beginning == mode ? offset
    : seek_end       == mode ? m_total_size  - offset
    :                          m_current_pos + offset;

  if ((0 > new_pos) || (static_cast<int64_t>(m_total_size) < new_pos))
    throw mtx::mm_io::seek_x();

  m_current_file = 0;
  for (auto &file : m_files) {
    if ((file.m_global_start + file.m_size) < static_cast<uint64_t>(new_pos)) {
      ++m_current_file;
      continue;
    }

    m_current_pos       = new_pos;
    m_current_local_pos = new_pos - file.m_global_start;
    file.m_file->setFilePointer(m_current_local_pos, seek_beginning);
    break;
  }
}

uint32
mm_multi_file_io_c::_read(void *buffer,
                          size_t size) {
  size_t num_read_total     = 0;
  unsigned char *buffer_ptr = static_cast<unsigned char *>(buffer);

  while (!eof() && (num_read_total < size)) {
    mm_multi_file_io_c::file_t &file = m_files[m_current_file];
    size_t num_to_read = static_cast<size_t>(std::min(static_cast<uint64_t>(size) - static_cast<uint64_t>(num_read_total), file.m_size - m_current_local_pos));

    if (0 != num_to_read) {
      size_t num_read      = file.m_file->read(buffer_ptr, num_to_read);
      num_read_total      += num_read;
      buffer_ptr          += num_read;
      m_current_local_pos += num_read;
      m_current_pos       += num_read;

      if (num_read != num_to_read)
        break;
    }

    if ((m_current_local_pos >= file.m_size) && (m_files.size() > (m_current_file + 1))) {
      ++m_current_file;
      m_current_local_pos = 0;
      m_files[m_current_file].m_file->setFilePointer(0, seek_beginning);
    }
  }

  return num_read_total;
}

size_t
mm_multi_file_io_c::_write(const void *buffer,
                           size_t size) {
  throw mtx::mm_io::wrong_read_write_access_x();
}

void
mm_multi_file_io_c::close() {
  for (auto &file : m_files)
    file.m_file->close();

  m_files.clear();
  m_total_size        = 0;
  m_current_pos       = 0;
  m_current_local_pos = 0;
}

bool
mm_multi_file_io_c::eof() {
  return m_files.empty() || ((m_current_file == (m_files.size() - 1)) && (m_current_local_pos >= m_files[m_current_file].m_size));
}

std::vector<bfs::path>
mm_multi_file_io_c::get_file_names() {
  std::vector<bfs::path> file_names;
  for (auto &file : m_files)
    file_names.push_back(file.m_file_name);

  return file_names;
}

void
mm_multi_file_io_c::create_verbose_identification_info(std::vector<std::string> &verbose_info) {
  for (auto &file : m_files)
    if (file.m_file_name != m_files.front().m_file_name)
      verbose_info.push_back((boost::format("other_file:%1%") % escape(file.m_file_name.string())).str());
}

void
mm_multi_file_io_c::display_other_file_info() {
  std::stringstream out;

  for (auto &file : m_files)
    if (file.m_file_name != m_files.front().m_file_name) {
      if (!out.str().empty())
        out << ", ";
      out << file.m_file_name.filename();
    }

  if (!out.str().empty())
    mxinfo(boost::format(Y("'%1%': Processing the following files as well: %2%\n")) % m_display_file_name % out.str());
}

struct path_sorter_t {
  bfs::path m_path;
  int m_number;

  path_sorter_t(const bfs::path &path, int number)
    : m_path(path)
    , m_number(number)
  {
  }

  bool operator <(const path_sorter_t &cmp) const {
    return m_number < cmp.m_number;
  }
};

mm_multi_file_io_cptr
mm_multi_file_io_c::open_multi(const std::string &display_file_name,
                               bool single_only) {
  bfs::path first_file_name(bfs::system_complete(bfs::path(display_file_name)));
  std::string base_name = bfs::basename(first_file_name);
  std::string extension = ba::to_lower_copy(bfs::extension(first_file_name));
  boost::regex file_name_re("(.+[_\\-])(\\d+)$", boost::regex::perl);
  boost::smatch matches;

  if (!boost::regex_match(base_name, matches, file_name_re) || single_only) {
    std::vector<bfs::path> file_names;
    file_names.push_back(first_file_name);
    return mm_multi_file_io_cptr(new mm_multi_file_io_c(file_names, display_file_name));
  }

  int start_number = 1;
  parse_int(matches[2].str(), start_number);

  base_name = ba::to_lower_copy(matches[1].str());

  std::vector<path_sorter_t> paths;
  paths.push_back(path_sorter_t(first_file_name, start_number));

  bfs::directory_iterator end_itr;
  for (bfs::directory_iterator itr(first_file_name.branch_path()); itr != end_itr; ++itr) {
    if (   bfs::is_directory(itr->status())
        || !ba::iequals(bfs::extension(itr->path()), extension))
      continue;

    std::string stem   = bfs::basename(itr->path());
    int current_number = 0;

    if (   !boost::regex_match(stem, matches, file_name_re)
        || !ba::iequals(matches[1].str(), base_name)
        || !parse_int(matches[2].str(), current_number)
        || (current_number <= start_number))
      continue;

    paths.push_back(path_sorter_t(itr->path(), current_number));
  }

  std::sort(paths.begin(), paths.end());

  std::vector<bfs::path> file_names;
  for (auto &path : paths)
    file_names.push_back(path.m_path);

  return mm_multi_file_io_cptr(new mm_multi_file_io_c(file_names, display_file_name));
}
