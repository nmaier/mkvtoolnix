#ifndef MTX_MMGQT_MAIN_WINDOW_H
#define MTX_MMGQT_MAIN_WINDOW_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/mux_config.h"
#include "mkvtoolnix-gui/source_file_model.h"
#include "mkvtoolnix-gui/track_model.h"

#include <QDir>
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
  Q_OBJECT;

protected:
  // UI stuff:
  Ui::MainWindow *ui;
  SourceFileModel *m_filesModel;
  TrackModel *m_tracksModel;

  // non-UI stuff:
  MuxConfig m_config;

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

public slots:
  virtual void onAddFiles();
  virtual void resizeFilesColumnsToContents() const;
  virtual void resizeTracksColumnsToContents() const;

protected:
  virtual QStringList selectFilesToAdd();
  virtual void addFile(QString const &fileName, bool append);
};

#endif // MTX_MMGQT_MAIN_WINDOW_H
