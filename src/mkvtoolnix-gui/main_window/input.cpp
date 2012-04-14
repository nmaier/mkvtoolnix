#include "common/common_pch.h"

#include "common/extern_data.h"
#include "common/iso639.h"
#include "common/qt.h"
#include "common/stereo_mode.h"
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
                  << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTags << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                  << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes << ui->audioPropertiesBox << ui->aacIsSBR << ui->cuesLabel << ui->cues;

  m_videoControls << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag
                  << ui->forcedTrackFlagLabel << ui->forcedTrackFlag << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTags << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                  << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->defaultDurationLabel << ui->defaultDuration << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes
                  << ui->picturePropertiesBox << ui->setAspectRatio << ui->aspectRatio << ui->setDisplayWidthHeight << ui->displayWidth << ui->displayDimensionsXLabel << ui->displayHeight << ui->stereoscopyLabel
                  << ui->stereoscopy << ui->croppingLabel << ui->cropping << ui->cuesLabel << ui->cues;

  m_subtitleControls << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag
                     << ui->forcedTrackFlagLabel << ui->forcedTrackFlag << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTags << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                     << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes
                     << ui->subtitleAndChapterPropertiesBox << ui->characterSetLabel << ui->subtitleCharacterSet << ui->cuesLabel << ui->cues;

  m_chapterControls << ui->subtitleAndChapterPropertiesBox << ui->characterSetLabel << ui->subtitleCharacterSet;

  m_allInputControls << ui->generalBox << ui->muxThisLabel << ui->muxThis << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag
                     << ui->forcedTrackFlagLabel << ui->forcedTrackFlag << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTags << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                     << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->defaultDurationLabel << ui->defaultDuration << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes
                     << ui->picturePropertiesBox << ui->setAspectRatio << ui->aspectRatio << ui->setDisplayWidthHeight << ui->displayWidth << ui->displayDimensionsXLabel << ui->displayHeight << ui->stereoscopyLabel
                     << ui->stereoscopy << ui->croppingLabel << ui->cropping << ui->audioPropertiesBox << ui->aacIsSBR << ui->subtitleAndChapterPropertiesBox << ui->characterSetLabel << ui->subtitleCharacterSet
                     << ui->miscellaneousBox << ui->cuesLabel << ui->cues << ui->userDefinedTrackOptionsLabel << ui->userDefinedTrackOptions;

  m_comboBoxControls << ui->muxThis << ui->trackLanguage << ui->defaultTrackFlag << ui->forcedTrackFlag << ui->compression << ui->cues << ui->stereoscopy << ui->aacIsSBR << ui->subtitleCharacterSet;
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
  for (auto &subCharset : sub_charsets)
    characterSets << to_qs(subCharset);
  characterSets.sort();

  ui->subtitleCharacterSet->addItem(Q(""), Q(""));
  for (auto &characterSet : characterSets)
    ui->subtitleCharacterSet->addItem(characterSet, characterSet);

  // Stereoscopy
  ui->stereoscopy->addItem(Q(""), 0);
  for (auto idx = 0u, end = stereo_mode_c::max_index(); idx <= end; ++idx)
    ui->stereoscopy->addItem(QString{"%1 (%2; %3)"}.arg(to_qs(stereo_mode_c::translate(idx))).arg(idx).arg(to_qs(stereo_mode_c::s_modes[idx])), idx + 1);

  // Set item data to index for distinguishing between empty values
  // added by "multiple selection mode".
  for (auto control : std::vector<QComboBox *>{ui->defaultTrackFlag, ui->forcedTrackFlag, ui->cues, ui->compression, ui->muxThis, ui->aacIsSBR})
    for (auto idx = 0; control->count() > idx; ++idx)
      control->setItemData(idx, idx);
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
  for (auto &comboBox : m_comboBoxControls)
    if (add && comboBox->itemData(0).isValid())
      comboBox->insertItem(0, QY("<do not change>"));
    else if (!add && !comboBox->itemData(0).isValid())
      comboBox->removeItem(0);
}

void
MainWindow::clearInputControlValues() {
  for (auto comboBox : m_comboBoxControls)
    comboBox->setCurrentIndex(0);

  for (auto control : std::vector<QLineEdit *>{ui->trackName, ui->trackTags, ui->delay, ui->stretchBy, ui->timecodes, ui->displayWidth, ui->displayHeight, ui->cropping, ui->userDefinedTrackOptions})
    control->setText(Q(""));

  ui->defaultDuration->setEditText(Q(""));
  ui->aspectRatio->setEditText(Q(""));

  ui->setAspectRatio->setChecked(false);
  ui->setDisplayWidthHeight->setChecked(false);
}

void
MainWindow::setInputControlValues(Track *track) {
  m_currentlySettingInputControlValues = true;

  addOrRemoveEmptyComboBoxItem(nullptr == track);

  if (nullptr == track) {
    clearInputControlValues();
    m_currentlySettingInputControlValues = false;
    return;
  }

  // TODO
  ui->trackName->setText(track->m_name);

  Util::setComboBoxIndexIf(ui->muxThis,              [&](QString const &, QVariant const &data) { return data.isValid() && (data.toInt()    == (track->m_muxThis ? 0 : 1)); });
  Util::setComboBoxIndexIf(ui->trackLanguage,        [&](QString const &, QVariant const &data) { return data.isValid() && (data.toString() == track->m_language);          });
  Util::setComboBoxIndexIf(ui->defaultTrackFlag,     [&](QString const &, QVariant const &data) { return data.isValid() && (data.toUInt()   == track->m_defaultTrackFlag);  });
  Util::setComboBoxIndexIf(ui->forcedTrackFlag,      [&](QString const &, QVariant const &data) { return data.isValid() && (data.toUInt()   == track->m_forcedTrackFlag);   });
  Util::setComboBoxIndexIf(ui->compression,          [&](QString const &, QVariant const &data) { return data.isValid() && (data.toUInt()   == track->m_compression);       });
  Util::setComboBoxIndexIf(ui->cues,                 [&](QString const &, QVariant const &data) { return data.isValid() && (data.toUInt()   == track->m_cues);              });
  Util::setComboBoxIndexIf(ui->stereoscopy,          [&](QString const &, QVariant const &data) { return data.isValid() && (data.toUInt()   == track->m_stereoscopy);       });
  Util::setComboBoxIndexIf(ui->aacIsSBR,             [&](QString const &, QVariant const &data) { return data.isValid() && (data.toUInt()   == track->m_aacIsSBR);          });
  Util::setComboBoxIndexIf(ui->subtitleCharacterSet, [&](QString const &, QVariant const &data) { return data.isValid() && (data.toString() == track->m_characterSet);      });

  ui->trackName->setText(                track->m_name);
  ui->trackTags->setText(                track->m_tags);
  ui->delay->setText(                    track->m_delay);
  ui->stretchBy->setText(                track->m_stretchBy);
  ui->timecodes->setText(                track->m_timecodes);
  ui->displayWidth->setText(             track->m_displayWidth);
  ui->displayHeight->setText(            track->m_displayHeight);
  ui->cropping->setText(                 track->m_cropping);
  ui->userDefinedTrackOptions->setText(  track->m_userDefinedOptions);
  ui->defaultDuration->setEditText(      track->m_defaultDuration);
  ui->aspectRatio->setEditText(          track->m_aspectRatio);

  ui->setAspectRatio->setChecked(        track->m_setAspectRatio);
  ui->setDisplayWidthHeight->setChecked(!track->m_setAspectRatio);

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
        || (track->isChapters()  && withChapters)) {
      code(track);
      m_tracksModel->trackUpdated(track);
    }
  }
}

void
MainWindow::onTrackNameEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_name = newValue; }, true);
}

void
MainWindow::onMuxThisChanged(int newValue) {
  auto data = ui->muxThis->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_muxThis = 0 == newValue; });
}

void
MainWindow::onTrackLanguageChanged(int newValue) {
  auto code = ui->trackLanguage->itemData(newValue).toString();
  if (code.isEmpty())
    return;

  withSelectedTracks([&](Track *track) { track->m_language = code; }, true);
}

void
MainWindow::onDefaultTrackFlagChanged(int newValue) {
  auto data = ui->defaultTrackFlag->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_defaultTrackFlag = newValue; }, true);
}

void
MainWindow::onForcedTrackFlagChanged(int newValue) {
  auto data = ui->forcedTrackFlag->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_forcedTrackFlag = newValue; }, true);
}

void
MainWindow::onCompressionChanged(int newValue) {
  auto data = ui->compression->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  Track::Compression compression;
  if (3 > newValue)
    compression = 0 == newValue ? Track::CompDefault
                : 1 == newValue ? Track::CompNone
                :                 Track::CompZlib;
  else
    compression = ui->compression->currentText() == Q("lzo") ? Track::CompLzo : Track::CompBz2;

  withSelectedTracks([&](Track *track) { track->m_compression = compression; }, true);
}

void
MainWindow::onTrackTagsEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_tags = newValue; }, true);
}

void
MainWindow::onDelayEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_delay = newValue; });
}

void
MainWindow::onStretchByEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_stretchBy = newValue; });
}

void
MainWindow::onDefaultDurationEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_defaultDuration = newValue; }, true);
}

void
MainWindow::onTimecodesEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_timecodes = newValue; });
}

void
MainWindow::onBrowseTimecodes() {
  auto dir      = ui->timecodes->text().isEmpty() ? Settings::get().m_lastOpenDir.path() : QFileInfo{ ui->timecodes->text() }.path();
  auto fileName = QFileDialog::getOpenFileName(this, QY("Select timecode file"), dir, QY("Text files (*.txt)") + Q(";;") + QY("All files (*)"));
  if (fileName.isEmpty())
    return;

  Settings::get().m_lastOpenDir = QFileInfo{fileName}.path();
  Settings::get().save();

  ui->timecodes->setText(fileName);

  withSelectedTracks([&](Track *track) { track->m_timecodes = fileName; });
}

void
MainWindow::onBrowseTrackTags() {
  auto dir      = ui->trackTags->text().isEmpty() ? Settings::get().m_lastOpenDir.path() : QFileInfo{ ui->trackTags->text() }.path();
  auto fileName = QFileDialog::getOpenFileName(this, QY("Select tags file"), dir, QY("XML files (*.xml)") + Q(";;") + Q("All files (*)"));
  if (fileName.isEmpty())
    return;

  Settings::get().m_lastOpenDir = QFileInfo{fileName}.path();
  Settings::get().save();

  ui->trackTags->setText(fileName);

  withSelectedTracks([&](Track *track) { track->m_tags = fileName; }, true);
}

void
MainWindow::onSetAspectRatio() {
  withSelectedTracks([&](Track *track) { track->m_setAspectRatio = true; }, true);
}

void
MainWindow::onSetDisplayDimensions() {
  withSelectedTracks([&](Track *track) { track->m_setAspectRatio = false; }, true);
}

void
MainWindow::onAspectRatioEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_aspectRatio = newValue; }, true);
}

void
MainWindow::onDisplayWidthEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_displayWidth = newValue; }, true);
}

void
MainWindow::onDisplayHeightEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_displayHeight = newValue; }, true);
}

void
MainWindow::onStereoscopyChanged(int newValue) {
  auto data = ui->stereoscopy->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_stereoscopy = newValue; }, true);
}

void
MainWindow::onCroppingEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_cropping = newValue; }, true);
}

void
MainWindow::onAacIsSBRChanged(int newValue) {
  auto data = ui->aacIsSBR->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_aacIsSBR = newValue; }, true);
}

void
MainWindow::onSubtitleCharacterSetChanged(int newValue) {
  auto characterSet = ui->subtitleCharacterSet->itemData(newValue).toString();
  if (characterSet.isEmpty())
    return;

  withSelectedTracks([&](Track *track) { track->m_characterSet = characterSet; }, true);
}

void
MainWindow::onCuesChanged(int newValue) {
  auto data = ui->cues->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_cues = newValue; }, true);
}

void
MainWindow::onUserDefinedTrackOptionsEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_userDefinedOptions = newValue; }, true);
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

void
MainWindow::onFilesContextMenu()
  const {
}

void
MainWindow::onTracksContextMenu()
  const {
}
