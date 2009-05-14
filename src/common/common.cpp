/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions, common variables

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <boost/regex.hpp>
#include <stdlib.h>
#include <string>
#ifdef SYS_WINDOWS
# include <windows.h>
#endif

#include "common/common.h"
#include "common/mm_io.h"
#include "common/random.h"

// Global and static variables

int verbose = 1;

extern bool g_warning_issued;

static std::string s_debug_options;

// Functions

void
mxexit(int code) {
  if (code != -1)
    exit(code);

  if (g_warning_issued)
    exit(1);

  exit(0);
}

bool
debugging_requested(const char *option) {
  std::string expression = std::string("\\b") + option + "\\b";
  boost::regex re(expression, boost::regex::perl);

  return boost::regex_search(s_debug_options, re);
}

void
request_debugging(const std::string &options) {
  s_debug_options = options;
}

/** \brief Sets the priority mkvmerge runs with

   Depending on the OS different functions are used. On Unix like systems
   the process is being nice'd if priority is negative ( = less important).
   Only the super user can increase the priority, but you shouldn't do
   such work as root anyway.
   On Windows SetPriorityClass is used.

   \param priority A value between -2 (lowest priority) and 2 (highest
     priority)
 */
void
set_process_priority(int priority) {
#if defined(SYS_WINDOWS)
  static const struct {
    int priority_class, thread_priority;
  } s_priority_classes[5] = {
    { IDLE_PRIORITY_CLASS,         THREAD_PRIORITY_IDLE         },
    { BELOW_NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL },
    { NORMAL_PRIORITY_CLASS,       THREAD_PRIORITY_NORMAL       },
    { ABOVE_NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL },
    { HIGH_PRIORITY_CLASS,         THREAD_PRIORITY_HIGHEST      },
  };

  SetPriorityClass(GetCurrentProcess(), s_priority_classes[priority + 2].priority_class);
  SetThreadPriority(GetCurrentThread(), s_priority_classes[priority + 2].thread_priority);

#else
  static const int s_nice_levels[5] = { 19, 2, 0, -2, -5 };

  // Avoid a compiler warning due to glibc having flagged 'nice' with
  // 'warn if return value is ignored'.
  if (!nice(s_nice_levels[priority + 2]))
    ;
#endif
}

void
mtx_common_cleanup() {
  random_c::cleanup();
  mm_file_io_c::cleanup();
}
