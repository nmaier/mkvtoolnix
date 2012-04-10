#include "common/common_pch.h"

#include <QApplication>

#include "mkvtoolnix-gui/app.h"
#include "mkvtoolnix-gui/mkvtoolnix_gui.h"
#include "mkvtoolnix-gui/main_window/main_window.h"

int
main(int argc,
     char **argv) {
#ifdef SYS_WINDOWS
  QApplication::setStyle(Q("windows"));
#endif

  App app(argc, argv);

  MainWindow mainWindow;
  mainWindow.show();

  return app.exec();
}
