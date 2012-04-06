/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   mkvmerge debugging routines

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/fs_sys_helpers.h"
#include "common/output.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "merge/debugging.h"
#include "merge/output_control.h"

static int s_debug_memory_usage_details = -1;

static void
debug_memory_usage_details_hook() {
  if (-1 == s_debug_memory_usage_details)
    return;

  static int64_t s_previous_call = 0;

  int64_t now = get_current_time_millis();
  if ((now - s_previous_call) < s_debug_memory_usage_details)
    return;

  s_previous_call = now;

  std::string timecode = (boost::format("memory_usage_details :: %1%.%|2$04d| :: ") % (now / 1000) % (now % 1000)).str();

  for (auto &file : g_files) {
    mxinfo(boost::format("%1%reader :: %2% :: %3%\n") % timecode % file.name % file.reader->get_queued_bytes());

    for (auto packetizer : file.reader->m_reader_packetizers)
      mxinfo(boost::format("%1%packetizer :: %2% :: %3% :: %4%\n") % timecode % packetizer->m_ti.m_id % typeid(*packetizer).name() % packetizer->get_queued_bytes());
  }
}

int
parse_debug_interval_arg(const std::string &option,
                         int default_value,
                         int invalid_value) {
  int value = invalid_value;

  std::string arg;
  if (!debugging_requested(option, &arg))
    return value;

  if (arg.empty() || !parse_number(arg, value) || (0 >= value))
    value = default_value;

  if (250 > value)
    value *= 1000;

  return value;
}

void
debug_run_main_loop_hooks() {
  static bool s_main_loop_hooks_initialized = false;
  if (!s_main_loop_hooks_initialized) {
    s_main_loop_hooks_initialized = true;

    s_debug_memory_usage_details = parse_debug_interval_arg("memory_usage_details");
  }

  debug_memory_usage_details_hook();
}

