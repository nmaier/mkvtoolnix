#include <QtCore>
#include <QtGui>

#include "qtcommon.h"

#include "mmg_qt.h"

#include "main_window.h"

mkvmerge_settings_t::mkvmerge_settings_t()
  : executable(Q("mkvmerge"))
  , priority(priority_normal)
{
}

int
main(int argc,
     char **argv) {
#ifdef SYS_WINDOWS
  QApplication::setStyle(Q("windows"));
#endif

  QApplication app(argc, argv);

  main_window_c main_window;
  main_window.show();

  return app.exec();
}
