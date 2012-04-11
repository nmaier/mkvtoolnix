#ifndef MTX_MKVTOOLNIX_GUI_APP_H
#define MTX_MKVTOOLNIX_GUI_APP_H

#include "common/common_pch.h"

#include <QApplication>

class App : public QApplication {
  Q_OBJECT;

public:
  App(int &argc, char **argv);
  virtual ~App();

public slots:
  void saveSettings() const;

public:
  static App *instance();
};

#endif  // MTX_MKVTOOLNIX_GUI_APP_H
