/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   OS dependant file system & system helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>
*/

#include "common/common_pch.h"

#include "common/error.h"
#include "common/fs_sys_helpers.h"
#include "common/strings/editing.h"
#include "common/strings/utf8.h"

#if defined(SYS_APPLE)
# include <mach-o/dyld.h>
#endif

#if defined(SYS_WINDOWS)

# include <io.h>
# include <windows.h>
# include <winreg.h>
# include <direct.h>
# include <shlobj.h>
# include <sys/timeb.h>

#else

# include <stdlib.h>
# include <sys/time.h>

#endif

static bfs::path s_current_executable_path;

#if defined(SYS_WINDOWS)

HANDLE
CreateFileUtf8(LPCSTR lpFileName,
               DWORD dwDesiredAccess,
               DWORD dwShareMode,
               LPSECURITY_ATTRIBUTES lpSecurityAttributes,
               DWORD dwCreationDisposition,
               DWORD dwFlagsAndAttributes,
               HANDLE hTemplateFile) {
  // convert the name to wide chars
  wchar_t *wbuffer = win32_utf8_to_utf16(lpFileName);
  HANDLE ret       = CreateFileW(wbuffer, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
  delete []wbuffer;

  return ret;
}

int64_t
get_current_time_millis() {
  struct _timeb tb;
  _ftime(&tb);

  return (int64_t)tb.time * 1000 + tb.millitm;
}

bool
get_registry_key_value(const std::string &key,
                       const std::string &value_name,
                       std::string &value) {
  std::vector<std::string> key_parts = split(key, "\\", 2);
  HKEY hkey;
  HKEY hkey_base = key_parts[0] == "HKEY_CURRENT_USER" ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  DWORD error    = RegOpenKeyExA(hkey_base, key_parts[1].c_str(), 0, KEY_READ, &hkey);

  if (ERROR_SUCCESS != error)
    return false;

  bool ok        = false;
  DWORD data_len = 0;
  DWORD dwDisp;
  if (ERROR_SUCCESS == RegQueryValueExA(hkey, value_name.c_str(), nullptr, &dwDisp, nullptr, &data_len)) {
    char *data = new char[data_len + 1];
    memset(data, 0, data_len + 1);

    if (ERROR_SUCCESS == RegQueryValueExA(hkey, value_name.c_str(), nullptr, &dwDisp, (BYTE *)data, &data_len)) {
      value = data;
      ok    = true;
    }

    delete []data;
  }

  RegCloseKey(hkey);

  return ok;
}

void
set_environment_variable(const std::string &key,
                         const std::string &value) {
  SetEnvironmentVariableA(key.c_str(), value.c_str());
  std::string env_buf = (boost::format("%1%=%2%") % key % value).str();
  _putenv(env_buf.c_str());
}

std::string
get_environment_variable(const std::string &key) {
  char buffer[100];
  memset(buffer, 0, 100);

  if (0 == GetEnvironmentVariableA(key.c_str(), buffer, 99))
    return "";

  return buffer;
}

unsigned int
get_windows_version() {
  OSVERSIONINFO os_version_info;

  memset(&os_version_info, 0, sizeof(OSVERSIONINFO));
  os_version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

  if (!GetVersionEx(&os_version_info))
    return WINDOWS_VERSION_UNKNOWN;

  return (os_version_info.dwMajorVersion << 16) | os_version_info.dwMinorVersion;
}

bfs::path
mtx::get_application_data_folder() {
  wchar_t szPath[MAX_PATH];

  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, szPath)))
    return bfs::path{to_utf8(std::wstring(szPath))} / "mkvtoolnix";

  return bfs::path{};
}

#else // SYS_WINDOWS

int64_t
get_current_time_millis() {
  struct timeval tv;
  if (0 != gettimeofday(&tv, nullptr))
    return -1;

  return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

bfs::path
mtx::get_application_data_folder() {
  const char *home = getenv("HOME");
  if (!home)
    return bfs::path{};

  // If $HOME/.mkvtoolnix exists already then keep using it to avoid
  // losing existing user configuration.
  auto old_default_folder = bfs::path{home} / ".mkvtoolnix";
  if (boost::filesystem::exists(old_default_folder))
    return old_default_folder;

  // If XDG_CONFIG_HOME is set then use that folder.
  auto xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home)
    return bfs::path{xdg_config_home} / "mkvtoolnix";

  // If all fails then use the XDG fallback folder for config files.
  return bfs::path{home} / ".config" / "mkvtoolnix";
}

#endif // SYS_WINDOWS

namespace mtx {

int
system(std::string const &command) {
#ifdef SYS_WINDOWS
  std::wstring wcommand = to_wide(command);
  auto mem              = memory_c::clone(wcommand.c_str(), (wcommand.length() + 1) * sizeof(wchar_t));

  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(STARTUPINFOW));
  memset(&pi, 0, sizeof(PROCESS_INFORMATION));

  auto result = ::CreateProcessW(NULL,                                           // application name (use only cmd line)
                                 reinterpret_cast<wchar_t *>(mem->get_buffer()), // full command line
                                 NULL,                                           // security attributes: defaults for both
                                 NULL,                                           //   the process and its main thread
                                 FALSE,                                          // inherit handles if we use pipes
                                 CREATE_NO_WINDOW,                               // process creation flags
                                 NULL,                                           // environment (use the same)
                                 NULL,                                           // current directory (use the same)
                                 &si,                                            // startup info (unused here)
                                 &pi                                             // process info
                                 );

  // Wait until child process exits.
  WaitForSingleObject(pi.hProcess, INFINITE);

  // Close process and thread handles.
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return !result ? -1 : 0;

#else  // SYS_WINDOWS
  return ::system(command.c_str());
#endif  // SYS_WINDOWS
}

#if defined(SYS_WINDOWS)
static bfs::path
get_current_exe_path(std::string const &) {
  std::wstring file_name;
  file_name.resize(4000);

  while (true) {
    memset(&file_name[0], 0, file_name.size() * sizeof(std::wstring::value_type));
    auto size = GetModuleFileNameW(nullptr, &file_name[0], file_name.size() - 1);
    if (size) {
      file_name.resize(size);
      break;
    }

    file_name.resize(file_name.size() + 4000);
  }

  return bfs::absolute(bfs::path{to_utf8(file_name)}).parent_path();
}

#elif defined(SYS_APPLE)

static bfs::path
get_current_exe_path(std::string const &) {
  std::string file_name;
  file_name.resize(4000);

  while (true) {
    memset(&file_name[0], 0, file_name.size());
    uint32_t size = file_name.size();
    auto result   = _NSGetExecutablePath(&file_name[0], &size);
    file_name.resize(size);

    if (0 == result)
      break;
  }

  return bfs::absolute(bfs::path{file_name}).parent_path();
}

#else // Neither Windows nor Mac OS

static bfs::path
get_current_exe_path(std::string const &argv0) {
  // Linux
  auto exe = bfs::path{"/proc/self/exe"};
  if (bfs::exists(exe))
    return bfs::absolute(bfs::read_symlink(exe)).parent_path();

  if (argv0.empty())
    return bfs::current_path();

  exe = bfs::absolute(argv0);
  if (bfs::exists(exe))
    return exe.parent_path();

  return bfs::current_path();
}

#endif

bfs::path const &
get_installation_path() {
  return s_current_executable_path;;
}

void
determine_path_to_current_executable(std::string const &argv0) {
  s_current_executable_path = get_current_exe_path(argv0);
}

}
