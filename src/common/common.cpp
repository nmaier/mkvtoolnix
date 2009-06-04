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
#include "common/strings/editing.h"

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

static std::map<std::string, std::string> s_debugging_options;

bool
debugging_requested(const char *option,
                    std::string *arg) {
  std::map<std::string, std::string>::iterator option_ptr = s_debugging_options.find(std::string(option));
  if (s_debugging_options.end() == option_ptr)
    return false;

  if (NULL != arg)
    *arg = option_ptr->second;

  return true;
}

bool
debugging_requested(const std::string &option,
                    std::string *arg) {
  return debugging_requested(option.c_str(), arg);
}

void
request_debugging(const std::string &options) {
  std::vector<std::string> all_options = split(options);
  std::vector<std::string>::iterator i;

  mxforeach(i, all_options) {
    std::vector<std::string> parts = split(*i, ":", 2);
    if (1 == parts.size())
      s_debugging_options[parts[0]] = "";
    else
      s_debugging_options[parts[0]] = parts[1];
  }
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
