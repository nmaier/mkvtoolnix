#ifndef MTX_MMGQT_MAIN_WINDOW_H
#define MTX_MMGQT_MAIN_WINDOW_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/mux_config.h"

#include <QDir>
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
  Q_OBJECT;

protected:
  MuxConfig m_config;

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

public slots:
  virtual void onAddFiles();

protected:
  virtual QStringList selectFilesToAdd();
  virtual void addFile(QString const &fileName, bool append);

private:
  Ui::MainWindow *ui;
};

#endif // MTX_MMGQT_MAIN_WINDOW_H
