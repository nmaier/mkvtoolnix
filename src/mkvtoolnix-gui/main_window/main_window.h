#ifndef MTX_MMGQT_MAIN_WINDOW_H
#define MTX_MMGQT_MAIN_WINDOW_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/mux_config.h"
#include "mkvtoolnix-gui/source_file_model.h"
#include "mkvtoolnix-gui/track_model.h"

#include <QComboBox>
#include <QList>
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

  QList<QWidget *> m_audioControls, m_videoControls, m_subtitleControls, m_chapterControls, m_typeIndependantControls, m_allInputControls;
  QList<QComboBox *> m_comboBoxControls;
  bool m_currentlySettingInputControlValues;

  // non-UI stuff:
  MuxConfig m_config;

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  virtual void setStatusBarMessage(QString const &message);

public slots:
  virtual void onSaveConfig();
  virtual void onSaveConfigAs();
  virtual void onOpenConfig();
  virtual void onNew();
  virtual void onAddFiles();
  virtual void onAddToJobQueue();
  virtual void onStartMuxing();

  virtual void onTrackSelectionChanged();

  virtual void onTrackNameChanged(QString newValue);
  virtual void onMuxThisChanged(int newValue);
  virtual void onTrackLanguageChanged(int newValue);
  virtual void onDefaultTrackFlagChanged(int newValue);
  virtual void onForcedTrackFlagChanged(int newValue);
  virtual void onCompressionChanged(int newValue);
  virtual void onTrackTagsChanged(QString newValue);
  virtual void onDelayChanged(QString newValue);
  virtual void onStretchByChanged(QString newValue);
  virtual void onDefaultDurationChanged(QString newValue);
  virtual void onTimecodesChanged(QString newValue);

  virtual void resizeFilesColumnsToContents() const;
  virtual void resizeTracksColumnsToContents() const;

protected:
  virtual QStringList selectFilesToAdd();
  virtual void addFile(QString const &fileName, bool append);
  virtual void setupComboBoxContent();
  virtual void setupControlLists();
  virtual void enableInputControls(QList<QWidget *> const &controls, bool enable);
  virtual void setInputControlValues(Track *track);
  virtual void withSelectedTracks(std::function<void(Track *)> code, bool notIfAppending = false, QWidget *widget = nullptr);
  virtual void addOrRemoveEmptyComboBoxItem(bool add);
};

#endif // MTX_MMGQT_MAIN_WINDOW_H
