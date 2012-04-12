#include "common/common_pch.h"

#include "common/extern_data.h"
#include "common/iso639.h"
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

  m_comboBoxControls << ui->muxThis << ui->trackLanguage << ui->defaultTrackFlag << ui->forcedTrackFlag << ui->compression << ui->cues;
}

void
MainWindow::setupComboBoxContent() {
  // Language
  std::vector<std::pair<QString, QString> > languages;
  for (auto &language : iso639_languages)
    languages.push_back(std::make_pair(QString{"%1 (%2)"}.arg(to_qs(language.english_name)).arg(to_qs(language.iso639_2_code)), to_qs(language.iso639_2_code)));

  brng::sort(languages, [](std::pair<QString, QString> const &a, std::pair<QString, QString> const &b) { return a.first < b.first; });

  for (auto &language: languages)
    ui->trackLanguage->addItem(language.first, language.second);

  // Character set
  QStringList characterSets;
  for (auto &sub_charset : sub_charsets)
    characterSets << to_qs(sub_charset);
  characterSets.sort();

  ui->subtitleCharacterSet->addItem(Q(""));
  ui->subtitleCharacterSet->addItems(characterSets);
}

void
MainWindow::onTrackSelectionChanged() {
  enableInputControls(m_allInputControls, false);

  auto selection = ui->tracks->selectionModel()->selection();
  if (selection.isEmpty())
    return;

  if (1 < selection.size()) {
    setInputControlValues(nullptr);
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

  setInputControlValues(track);

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
MainWindow::addOrRemoveEmptyComboBoxItem(bool add) {
  for (auto &comboBox : m_comboBoxControls) {
    if (add && (comboBox->itemText(0) != Q("")))
      comboBox->insertItem(0, Q(""));
    else if (!add && (comboBox->itemText(0) == Q("")))
      comboBox->removeItem(0);
  }
}

void
MainWindow::setInputControlValues(Track *track) {
  m_currentlySettingInputControlValues = true;

  addOrRemoveEmptyComboBoxItem(nullptr == track);

  if (nullptr == track) {
    for (auto &comboBox : m_comboBoxControls)
      comboBox->setCurrentIndex(0);

    ui->trackName->setText(Q(""));

    m_currentlySettingInputControlValues = false;
    return;
  }

  // TODO
  ui->muxThis->setCurrentIndex(track->m_muxThis ? 0 : 1);
  ui->trackName->setText(track->m_name);

  for (int idx = 0; ui->trackLanguage->count() > idx; ++idx)
    if (ui->trackLanguage->itemData(idx).toString() == track->m_language) {
      ui->trackLanguage->setCurrentIndex(idx);
      break;
    }

  // QRegExp re{"\\((.+)\\)$"};
  // ui->
  m_currentlySettingInputControlValues = false;
}

void
MainWindow::withSelectedTracks(std::function<void(Track *)> code,
                               bool notIfAppending,
                               QWidget *widget) {
  if (m_currentlySettingInputControlValues)
    return;

  auto selection = ui->tracks->selectionModel()->selection();
  if (selection.isEmpty())
    return;

  if (nullptr == widget)
    widget = static_cast<QWidget *>(QObject::sender());

  bool withAudio     = m_audioControls.contains(widget);
  bool withVideo     = m_videoControls.contains(widget);
  bool withSubtitles = m_subtitleControls.contains(widget);
  bool withChapters  = m_chapterControls.contains(widget);
  bool withAll       = m_typeIndependantControls.contains(widget);

  for (auto &indexRange : selection) {
    auto idxs = indexRange.indexes();
    if (idxs.isEmpty() || !idxs.at(0).isValid())
      continue;

    auto track = static_cast<Track *>(idxs.at(0).internalPointer());
    if (!track || (track->m_appendedTo && notIfAppending))
      continue;

    if (   withAll
        || (track->isAudio()     && withAudio)
        || (track->isVideo()     && withVideo)
        || (track->isSubtitles() && withSubtitles)
        || (track->isChapters()  && withChapters))
      code(track);
  }
}

void
MainWindow::onTrackNameChanged(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_name = newValue; }, true);
}

void
MainWindow::onMuxThisChanged(int newValue) {
  mxinfo(boost::format("mux this changed\n"));  withSelectedTracks([&](Track *track) { mxinfo(boost::format("setting\n"));track->m_muxThis = 0 == newValue; });
}

void
MainWindow::onTrackLanguageChanged(int newValue) {
  auto code = ui->trackLanguage->itemData(newValue).toString();
  mxinfo(boost::format("lang changed to %1%\n") % code);
  if (code.isEmpty())
    return;

  if (code == Q("eng"))
    mxinfo(boost::format("muh\n"));

  withSelectedTracks([&](Track *track) { track->m_language = code; }, true);
}

void
MainWindow::onDefaultTrackFlagChanged(int newValue) {
  withSelectedTracks([&](Track *track) { track->m_defaultTrackFlag = newValue; }, true);
}

void
MainWindow::onForcedTrackFlagChanged(int newValue) {
  withSelectedTracks([&](Track *track) { track->m_forcedTrackFlag = newValue; }, true);
}

void
MainWindow::onCompressionChanged(int newValue) {
  withSelectedTracks([&](Track *track) {
      if (3 > newValue)
        track->m_compression = 0 == newValue ? Track::CompDefault
                             : 1 == newValue ? Track::CompNone
                             :                 Track::CompZlib;
      else
        track->m_compression = ui->compression->currentText() == Q("lzo") ? Track::CompLzo : Track::CompBz2;
    }, true);
}

void
MainWindow::onTrackTagsChanged(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_tags = newValue; }, true);
}

void
MainWindow::onDelayChanged(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_delay = newValue; });
}

void
MainWindow::onStretchByChanged(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_stretchBy = newValue; });
}

void
MainWindow::onDefaultDurationChanged(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_defaultDuration = newValue; }, true);
}

void
MainWindow::onTimecodesChanged(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_timecodes = newValue; });
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
