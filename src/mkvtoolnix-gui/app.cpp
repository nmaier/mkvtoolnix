#include "common/common_pch.h"

#include "common/common.h"
#include "common/qt.h"
#include "mkvtoolnix-gui/app.h"
#include "mkvtoolnix-gui/util/settings.h"

App::App(int &argc,
         char **argv)
  : QApplication{argc, argv}
{
  mtx_common_init("mkvtoolnix-gui");

  QCoreApplication::setOrganizationName("bunkus.org");
  QCoreApplication::setOrganizationDomain("bunkus.org");
  QCoreApplication::setApplicationName("mkvtoolnix-gui");

#ifdef SYS_WINDOWS
  QApplication::setStyle(Q("windows"));
#endif

  Settings::get().load();

  QObject::connect(this, SIGNAL(aboutToQuit()), this, SLOT(saveSettings()));
}

App::~App() {
}

void
App::saveSettings()
  const {
  Settings::get().save();
}

App *
App::instance() {
  return static_cast<App *>(QApplication::instance());
}
