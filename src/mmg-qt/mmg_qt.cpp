#include "common/common_pch.h"

#include <QApplication>

#include "mmg-qt/mmg_qt.h"
#include "mmg-qt/main_window.h"

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
