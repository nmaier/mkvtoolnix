/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   terminal access functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#if defined(HAVE_TIOCGWINSZ)
# include <termios.h>
# if defined(HAVE_SYS_IOCTL_H) || defined(GWINSZ_IN_SYS_IOCTL)
#  include <sys/ioctl.h>
# endif // HAVE_SYS_IOCTL_H || GWINSZ_IN_SYS_IOCTL
#endif  // HAVE_TIOCGWINSZ

#include "common/terminal.h"

#define DEFAULT_TERMINAL_COLUMNS 80

int
get_terminal_columns() {
#if defined(HAVE_TIOCGWINSZ)
  struct winsize ws;

  if (ioctl(0, TIOCGWINSZ, &ws) != 0)
    return DEFAULT_TERMINAL_COLUMNS;
  return ws.ws_col;

#else  // HAVE_TIOCGWINSZ
  return DEFAULT_TERMINAL_COLUMNS;
#endif  // HAVE_TIOCGWINSZ
}

