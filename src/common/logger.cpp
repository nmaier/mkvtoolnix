/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <chrono>
#include <ctime>

#include "common/logger.h"
#include "common/fs_sys_helpers.h"
#include "common/mm_io_x.h"
#include "common/strings/formatting.h"

logger_cptr logger_c::s_default_logger;

static auto s_program_start_time = std::chrono::system_clock::now();

logger_c::logger_c(bfs::path const &file_name)
  : m_file_name(file_name)
  , m_log_start(get_current_time_millis())
{
  if (!m_file_name.is_absolute())
    m_file_name = bfs::temp_directory_path() / m_file_name;

  if (bfs::exists(m_file_name)) {
    boost::system::error_code ec;
    bfs::remove(m_file_name, ec);
  }
}

void
logger_c::log(std::string const &message) {
  try {
    mm_text_io_c out(new mm_file_io_c(m_file_name.string(), bfs::exists(m_file_name) ? MODE_WRITE : MODE_CREATE));
    out.setFilePointer(0, seek_end);

    auto now  = std::chrono::system_clock::now();
    auto diff = now - s_program_start_time;
    auto tnow = std::chrono::system_clock::to_time_t(now);

    // 2013-03-02 15:42:32
    char timestamp[30];
    std::strftime(timestamp, 30, "%Y-%m-%d %H:%M:%S", std::localtime(&tnow));

    out.puts((boost::format("%1% +%2%ms %3%\n") % timestamp % std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() % message).str());
  } catch (mtx::mm_io::exception &ex) {
  }
}

logger_c &
logger_c::get_default_logger() {
  if (!s_default_logger)
    s_default_logger = std::make_shared<logger_c>("mkvtoolnix-debug.log");
  return *s_default_logger;
}

void
logger_c::set_default_logger(logger_cptr logger) {
  s_default_logger = logger;
}
