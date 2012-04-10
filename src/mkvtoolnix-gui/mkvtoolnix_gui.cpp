#include "common/common_pch.h"

#include <QApplication>

#include "mkvtoolnix-gui/mmg_qt.h"
#include "mkvtoolnix-gui/main_window.h"

int
main(int argc,
     char **argv) {
#ifdef SYS_WINDOWS
  QApplication::setStyle(Q("windows"));
#endif

  QApplication app(argc, argv);

  MainWindow mainWindow;
  mainWindow.show();

  return app.exec();
}
