#include "common/common_pch.h"

#include "mkvtoolnix-gui/app.h"
#include "mkvtoolnix-gui/util/settings.h"

App::App(int &argc,
         char **argv)
  : QApplication{argc, argv}
{
  QCoreApplication::setOrganizationName("bunkus.org");
  QCoreApplication::setOrganizationDomain("bunkus.org");
  QCoreApplication::setApplicationName("mkvtoolnix-gui");

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
