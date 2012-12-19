/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>
#include <cstring>
#ifdef SYS_WINDOWS
# include <windows.h>
#endif

#include "common/command_line.h"
#include "common/hacks.h"
#include "common/mm_io_x.h"
#include "common/mm_write_buffer_io.h"
#include "common/strings/editing.h"
#include "common/translation.h"
#include "common/version.h"

/** \brief Reads command line arguments from a file

   Each line contains exactly one command line argument or a
   comment. Arguments are converted to UTF-8 and appended to the array
   \c args.
*/
static void
read_args_from_file(std::vector<std::string> &args,
                    const std::string &filename) {
  mm_text_io_c *mm_io;
  std::string buffer;
  bool skip_next;

  mm_io = nullptr;
  try {
    mm_io = new mm_text_io_c(new mm_file_io_c(filename));
  } catch (...) {
    mxerror(boost::format(Y("The file '%1%' could not be opened for reading command line arguments.\n")) % filename);
  }

  skip_next = false;
  while (!mm_io->eof() && mm_io->getline2(buffer)) {
    if (skip_next) {
      skip_next = false;
      continue;
    }
    strip(buffer);

    if (buffer == "#EMPTY#") {
      args.push_back("");
      continue;
    }

    if ((buffer[0] == '#') || (buffer[0] == 0))
      continue;

    if (buffer == "--command-line-charset") {
      skip_next = true;
      continue;
    }
    args.push_back(unescape(buffer));
  }

  delete mm_io;
}

static std::vector<std::string>
command_line_args_from_environment() {
  std::vector<std::string> args;

  char const *value = getenv("MKVTOOLNIX_OPTIONS");
  if (value)
    args = split(value, " ");
  else {
    value = getenv("MTX_OPTIONS");
    if (value)
      args = split(value, " ");
  }

  std::transform(args.begin(), args.end(), args.begin(), unescape);

  return args;
}

/** \brief Expand the command line parameters

   Takes each command line paramter, converts it to UTF-8, and reads more
   commands from command files if the argument starts with '@'. Puts all
   arguments into a new array.
   On Windows it uses the \c GetCommandLineW() function. That way it can
   also handle multi-byte input like Japanese file names.

   \param argc The number of arguments. This is the same argument that
     \c main normally receives.
   \param argv The arguments themselves. This is the same argument that
     \c main normally receives.
   \return An array of strings converted to UTF-8 containing all the
     command line arguments and any arguments read from option files.
*/
#if !defined(SYS_WINDOWS)
std::vector<std::string>
command_line_utf8(int argc,
                  char **argv) {
  int i;
  std::vector<std::string> args = command_line_args_from_environment();

  charset_converter_cptr cc_command_line = g_cc_stdio;

  for (i = 1; i < argc; i++)
    if (argv[i][0] == '@')
      read_args_from_file(args, &argv[i][1]);
    else {
      if (!strcmp(argv[i], "--command-line-charset")) {
        if ((i + 1) == argc)
          mxerror(Y("'--command-line-charset' is missing its argument.\n"));
        cc_command_line = charset_converter_c::init(!argv[i + 1] ? "" : argv[i + 1]);
        i++;
      } else
        args.push_back(cc_command_line->utf8(argv[i]));
    }

  return args;
}

#else  // !defined(SYS_WINDOWS)

std::vector<std::string>
command_line_utf8(int,
                  char **) {
  std::vector<std::string> args = command_line_args_from_environment();
  std::string utf8;

  int num_args     = 0;
  LPWSTR *arg_list = CommandLineToArgvW(GetCommandLineW(), &num_args);

  if (!arg_list)
    return args;

  int i;
  for (i = 1; i < num_args; i++) {
    std::string arg = win32_wide_to_multi_utf8(arg_list[i]);

    if (arg[0] == L'@')
      read_args_from_file(args, arg.substr(1));

    else
      args.push_back(arg);
  }

  LocalFree(arg_list);

  return args;
}
#endif // !defined(SYS_WINDOWS)

std::string usage_text, version_info;

/** Handle command line arguments common to all programs

   Iterates over the list of command line arguments and handles the ones
   that are common to all programs. These include --output-charset,
   --redirect-output, --help, --version and --verbose along with their
   short counterparts.

   \param args A vector of strings containing the command line arguments.
     The ones that have been handled are removed from the vector.
   \param redirect_output_short The name of the short option that is
     recognized for --redirect-output. If left empty then no short
     version is accepted.
   \returns \c true if the locale has changed and the function should be
     called again and \c false otherwise.
*/
bool
handle_common_cli_args(std::vector<std::string> &args,
                       const std::string &redirect_output_short) {
  size_t i = 0;

  while (args.size() > i) {
    if (args[i] == "--debug") {
      if ((i + 1) == args.size())
        mxerror("Missing argument for '--debug'.\n");

      request_debugging(args[i + 1]);
      args.erase(args.begin() + i, args.begin() + i + 2);

    } else if (args[i] == "--engage") {
      if ((i + 1) == args.size())
        mxerror(Y("'--engage' lacks its argument.\n"));

      engage_hacks(args[i + 1]);
      args.erase(args.begin() + i, args.begin() + i + 2);

    } else
      ++i;
  }

  // First see if there's an output charset given.
  i = 0;
  while (args.size() > i) {
    if (args[i] == "--output-charset") {
      if ((i + 1) == args.size())
        mxerror(Y("Missing argument for '--output-charset'.\n"));
      set_cc_stdio(args[i + 1]);
      args.erase(args.begin() + i, args.begin() + i + 2);
    } else
      ++i;
  }

  // Now let's see if the user wants the output redirected.
  i = 0;
  while (args.size() > i) {
    if ((args[i] == "--redirect-output") || (args[i] == "-r") ||
        ((redirect_output_short != "") &&
         (args[i] == redirect_output_short))) {
      if ((i + 1) == args.size())
        mxerror(boost::format(Y("'%1%' is missing the file name.\n")) % args[i]);
      try {
        if (!stdio_redirected()) {
          mm_io_cptr file = mm_write_buffer_io_c::open(args[i + 1], 128 * 1024);
          file->write_bom(g_stdio_charset);
          redirect_stdio(file);
        }
        args.erase(args.begin() + i, args.begin() + i + 2);
      } catch(mtx::mm_io::open_x &) {
        mxerror(boost::format(Y("Could not open the file '%1%' for directing the output.\n")) % args[i + 1]);
      }
    } else
      ++i;
  }

  // Check for the translations to use (if any).
  i = 0;
  while (args.size() > i) {
    if (args[i] == "--ui-language") {
      if ((i + 1) == args.size())
        mxerror(Y("Missing argument for '--ui-language'.\n"));

      if (args[i + 1] == "list") {
        mxinfo(Y("Available translations:\n"));
        std::vector<translation_c>::iterator translation = translation_c::ms_available_translations.begin();
        while (translation != translation_c::ms_available_translations.end()) {
          mxinfo(boost::format("  %1% (%2%)\n") % translation->get_locale() % translation->m_english_name);
          ++translation;
        }
        mxexit(0);
      }

      if (-1 == translation_c::look_up_translation(args[i + 1]))
        mxerror(boost::format(Y("There is no translation available for '%1%'.\n")) % args[i + 1]);

      init_locales(args[i + 1]);

      args.erase(args.begin() + i, args.begin() + i + 2);

      return true;
    } else
      ++i;
  }

  // Last find the --help, --version, --check-for-updates arguments.
  i = 0;
  while (args.size() > i) {
    if ((args[i] == "-V") || (args[i] == "--version")) {
      mxinfo(boost::format("%1%\n") % version_info);
      mxexit(0);

    } else if ((args[i] == "-v") || (args[i] == "--verbose")) {
      ++verbose;
      args.erase(args.begin() + i, args.begin() + i + 1);

    } else if ((args[i] == "-q") || (args[i] == "--quiet")) {
      verbose         = 0;
      g_suppress_info = true;
      args.erase(args.begin() + i, args.begin() + i + 1);

    } else if ((args[i] == "-h") || (args[i] == "-?") || (args[i] == "--help"))
      usage();

#if defined(HAVE_CURL_EASY_H)
    else if (args[i] == "--check-for-updates") {
      mtx_release_version_t rel = get_latest_release_version();
      if (!rel.latest_source.valid)
        mxerror(boost::format(Y("The update information could not be retrieved from %1%.\n")) % MTX_VERSION_CHECK_URL);

      std::vector<std::string> keys;
      brng::push_back(keys, rel.urls | badap::map_keys | badap::filtered([](std::string const &key) { return key != "general"; }));
      brng::sort(keys);

      std::string urls;
      for (auto &key : keys)
        urls += key + "_download_url=" + rel.urls[key] + "\n";

      mxinfo(boost::format("version_check_url=%1%\nrunning_version=%2%\navailable_version=%3%\ndownload_url=%4%\n%5%")
             % MTX_VERSION_CHECK_URL % rel.current_version.to_string() % rel.latest_source.to_string() % rel.urls["general"] % urls);
      mxexit(rel.current_version < rel.latest_source ? 1 : 0);
    }
#endif  // defined(HAVE_CURL_EASY_H)

    else
      ++i;
  }

  return false;
}

void
usage(int exit_code) {
  mxinfo(boost::format("%1%\n") % usage_text);
  mxexit(exit_code);
}
