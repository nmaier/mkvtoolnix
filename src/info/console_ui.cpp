/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   A console UI for mkvinfo

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include "common.h"
#include "mkvinfo.h"

void
console_show_element(int level,
                     const std::string &text,
                     int64_t position) {
  char *level_buffer;

  level_buffer = new char[level + 1];
  memset(&level_buffer[1], ' ', level);
  level_buffer[0] = '|';
  level_buffer[level] = 0;
  mxinfo(boost::format("%1%+ %2%") % level_buffer % text);
  if ((1 < verbose) && (0 <= position))
    mxinfo(boost::format(Y(" at %1%")) % position);
  mxinfo("\n");
  delete []level_buffer;
}

void
console_show_error(const std::string &error) {
  mxinfo(boost::format("(%1%) %2%\n") % NAME % error);
}

#if !defined(HAVE_QT) && !defined(HAVE_WXWIDGETS)
void
ui_show_element(int level,
                const std::string &text,
                int64_t position) {
  console_show_element(level, text, position);
}

void
ui_show_error(const std::string &error) {
  console_show_error(error);
}

void
ui_show_progress(int percentage,
                 const std::string &text) {
}

int
ui_run(int argc,
       char **argv) {
  return 0;
}

bool
ui_graphical_available() {
  return false;
}

#endif  // !HAVE_QT && !HAVE_WXWIDGETS
