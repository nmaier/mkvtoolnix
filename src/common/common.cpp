/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions, common variables

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <stdlib.h>
#ifdef SYS_WINDOWS
# include <windows.h>
#endif
#if defined(HAVE_SYS_SYSCALL_H)
# include <sys/syscall.h>
#endif

#include <matroska/KaxVersion.h>
#include <matroska/FileKax.h>

#include "common/fs_sys_helpers.h"
#include "common/hacks.h"
#include "common/random.h"
#include "common/stereo_mode.h"
#include "common/strings/editing.h"
#include "common/translation.h"

#if !defined(LIBMATROSKA_VERSION) || (LIBMATROSKA_VERSION <= 0x000801)
#define matroska_init()
#define matroska_done()
#endif

#if defined(SYS_WINDOWS)
#include "common/fs_sys_helpers.h"

// See http://msdn.microsoft.com/en-us/library/windows/desktop/ms686219(v=vs.85).aspx
# if !defined(PROCESS_MODE_BACKGROUND_BEGIN)
#  define PROCESS_MODE_BACKGROUND_BEGIN 0x00100000
# endif

// See http://msdn.microsoft.com/en-us/library/windows/desktop/ms686277(v=vs.85).aspx
# if !defined(THREAD_MODE_BACKGROUND_BEGIN)
#  define THREAD_MODE_BACKGROUND_BEGIN 0x00010000
# endif

typedef UINT (WINAPI *p_get_error_mode)(void);
static void
fix_windows_errormode() {
  UINT mode      = SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX;
  HMODULE h_kern = ::LoadLibrary("kernel32");

  if (h_kern) {
    // Vista+ only, but one can do without
    p_get_error_mode get_error_mode = reinterpret_cast<p_get_error_mode>(::GetProcAddress(h_kern, "GetErrorMode"));
    if (get_error_mode)
      mode |= get_error_mode();

    ::FreeLibrary(h_kern);
  }

  ::SetErrorMode(mode);
}
#endif

// Global and static variables

unsigned int verbose = 1;

extern bool g_warning_issued;
static std::string s_program_name;

// Functions

void
mxexit(int code) {
  matroska_done();
  if (code != -1)
    exit(code);

  if (g_warning_issued)
    exit(1);

  exit(0);
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

  // If the lowest priority should be used and we're on Vista or later
  // then use background priority. This also selects a lower I/O
  // priority.
  if ((-2 == priority) && (get_windows_version() >= WINDOWS_VERSION_VISTA)) {
    SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
    return;
  }

  SetPriorityClass(GetCurrentProcess(), s_priority_classes[priority + 2].priority_class);
  SetThreadPriority(GetCurrentThread(), s_priority_classes[priority + 2].thread_priority);

#else
  static const int s_nice_levels[5] = { 19, 2, 0, -2, -5 };

  // Avoid a compiler warning due to glibc having flagged 'nice' with
  // 'warn if return value is ignored'.
  if (!nice(s_nice_levels[priority + 2])) {
  }

# if defined(HAVE_SYSCALL) && defined(SYS_ioprio_set)
  if (0 < s_nice_levels[priority + 2])
    syscall(SYS_ioprio_set,
            1,        // IOPRIO_WHO_PROCESS
            0,        // current process/thread
            3 << 13); // I/O class 'idle'
# endif
#endif
}

static void
mtx_common_cleanup() {
  random_c::cleanup();
  mm_file_io_c::cleanup();
}

void
mtx_common_init(std::string const &program_name) {
  init_common_output(true);

  s_program_name = program_name;

#if defined(SYS_WINDOWS)
  fix_windows_errormode();
#endif

  matroska_init();

  atexit(mtx_common_cleanup);

  srand(time(nullptr));

  init_debugging();
  init_hacks();

  init_locales();

  mm_file_io_c::setup();
  g_cc_local_utf8 = charset_converter_c::init("");
  init_common_output(false);

  stereo_mode_c::init();
}

std::string const &
get_program_name() {
  return s_program_name;
}
