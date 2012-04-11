#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/main_window/main_window.h"
#include "mkvtoolnix-gui/forms/main_window.h"
#include "mkvtoolnix-gui/util/file_identifier.h"
#include "mkvtoolnix-gui/util/file_type_filter.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileDialog>
#include <QList>
#include <QString>

void
MainWindow::setupControlLists() {
  m_typeIndependantControls << ui->generalBox << ui->muxThisLabel << ui->muxThis << ui->miscellaneousBox << ui->userDefinedTrackOptionsLabel << ui->userDefinedTrackOptions;

  m_audioControls << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag << ui->forcedTrackFlagLabel << ui->forcedTrackFlag
                  << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTacks << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                  << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes << ui->audioPropertiesBox << ui->aacIsSBR << ui->cuesLabel << ui->cues;

  m_videoControls << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag
                  << ui->forcedTrackFlagLabel << ui->forcedTrackFlag << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTacks << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                  << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->defaultDurationLabel << ui->defaultDuration << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes
                  << ui->picturePropertiesBox << ui->setAspectRatio << ui->aspectRatio << ui->setDisplayWidthHeight << ui->displayWidth << ui->displayDimensionsXLabel << ui->displayHeight << ui->stereoscopyLabel
                  << ui->stereoscopy << ui->croppingLabel << ui->cropping << ui->cuesLabel << ui->cues;

  m_subtitleControls << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag
                     << ui->forcedTrackFlagLabel << ui->forcedTrackFlag << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTacks << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                     << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes
                     << ui->subtitleAndChapterPropertiesBox << ui->characterSetLabel << ui->subtitleCharacterSet << ui->cuesLabel << ui->cues;

  m_chapterControls << ui->subtitleAndChapterPropertiesBox << ui->characterSetLabel << ui->subtitleCharacterSet;

  m_allInputControls << ui->generalBox << ui->muxThisLabel << ui->muxThis << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag
                     << ui->forcedTrackFlagLabel << ui->forcedTrackFlag << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTacks << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                     << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->defaultDurationLabel << ui->defaultDuration << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes
                     << ui->picturePropertiesBox << ui->setAspectRatio << ui->aspectRatio << ui->setDisplayWidthHeight << ui->displayWidth << ui->displayDimensionsXLabel << ui->displayHeight << ui->stereoscopyLabel
                     << ui->stereoscopy << ui->croppingLabel << ui->cropping << ui->audioPropertiesBox << ui->aacIsSBR << ui->subtitleAndChapterPropertiesBox << ui->characterSetLabel << ui->subtitleCharacterSet
                     << ui->miscellaneousBox << ui->cuesLabel << ui->cues << ui->userDefinedTrackOptionsLabel << ui->userDefinedTrackOptions;

}

void
MainWindow::onTrackSelectionChanged() {
  enableInputControls(m_allInputControls, false);

  auto selection = ui->tracks->selectionModel()->selection();
  if (selection.isEmpty())
    return;

  if (1 < selection.size()) {
    enableInputControls(m_allInputControls, true);
    return;
  }

  enableInputControls(m_typeIndependantControls, true);

  auto idxs = selection.at(0).indexes();
  if (idxs.isEmpty() || !idxs.at(0).isValid())
    return;

  auto track = static_cast<Track *>(idxs.at(0).internalPointer());
  if (!track)
    return;

  if (track->isAudio())
    enableInputControls(m_audioControls, true);

  else if (track->isVideo())
    enableInputControls(m_videoControls, true);

  else if (track->isSubtitles())
    enableInputControls(m_subtitleControls, true);

  else if (track->isChapters())
    enableInputControls(m_chapterControls, true);
}

void
MainWindow::enableInputControls(QList<QWidget *> const &controls,
                                bool enable) {
  for (auto &control : controls)
    control->setEnabled(enable);
}

void
MainWindow::onAddFiles() {
  auto fileNames = selectFilesToAdd();
  if (fileNames.empty())
    return;

  auto numFilesBefore = m_config.m_files.size();

  for (auto &fileName : fileNames)
    addFile(fileName, false);

  if (numFilesBefore != m_config.m_files.size()) {
    m_filesModel->setSourceFiles(m_config.m_files);
    m_tracksModel->setTracks(m_config.m_tracks);
    resizeFilesColumnsToContents();
    resizeTracksColumnsToContents();
  }
}

QStringList
MainWindow::selectFilesToAdd() {
  QFileDialog dlg{this};
  dlg.setNameFilters(FileTypeFilter::get());
  dlg.setFileMode(QFileDialog::ExistingFiles);
  dlg.setDirectory(Settings::get().m_lastOpenDir);
  dlg.setWindowTitle(QY("Add media files"));

  if (!dlg.exec())
    return QStringList{};

  Settings::get().m_lastOpenDir = dlg.directory();

  return dlg.selectedFiles();
}

void
MainWindow::addFile(QString const &fileName,
                    bool /*append*/) {
  FileIdentifier identifier{ this, fileName };
  if (!identifier.identify())
    return;

  m_config.m_files << identifier.file();
  for (auto &track : identifier.file()->m_tracks)
    m_config.m_tracks << track.get();
}

void
MainWindow::resizeFilesColumnsToContents()
  const {
  auto columnCount = m_filesModel->columnCount(QModelIndex{});
  for (auto column = 0; columnCount > column; ++column)
    ui->files->resizeColumnToContents(column);
}

void
MainWindow::resizeTracksColumnsToContents()
  const {
  auto columnCount = m_tracksModel->columnCount(QModelIndex{});
  for (auto column = 0; columnCount > column; ++column)
    ui->tracks->resizeColumnToContents(column);
}
