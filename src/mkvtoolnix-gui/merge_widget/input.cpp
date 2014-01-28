#include "common/common_pch.h"

#include "common/extern_data.h"
#include "common/iso639.h"
#include "common/qt.h"
#include "common/stereo_mode.h"
#include "mkvtoolnix-gui/merge_widget/merge_widget.h"
#include "mkvtoolnix-gui/forms/merge_widget.h"
#include "mkvtoolnix-gui/util/file_identifier.h"
#include "mkvtoolnix-gui/util/file_type_filter.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileDialog>
#include <QList>
#include <QMessageBox>
#include <QString>

void
MergeWidget::setupControlLists() {
  m_typeIndependantControls << ui->muxThisLabel << ui->muxThis << ui->miscellaneousBox << ui->userDefinedTrackOptionsLabel << ui->userDefinedTrackOptions;

  m_audioControls << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag << ui->forcedTrackFlagLabel << ui->forcedTrackFlag
                  << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTags << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                  << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes << ui->audioPropertiesBox << ui->aacIsSBR << ui->cuesLabel << ui->cues
                  << ui->propertiesLabel << ui->generalOptionsBox;

  m_videoControls << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag
                  << ui->forcedTrackFlagLabel << ui->forcedTrackFlag << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTags << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                  << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->defaultDurationLabel << ui->defaultDuration << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes
                  << ui->picturePropertiesBox << ui->setAspectRatio << ui->aspectRatio << ui->setDisplayWidthHeight << ui->displayWidth << ui->displayDimensionsXLabel << ui->displayHeight << ui->stereoscopyLabel
                  << ui->stereoscopy << ui->croppingLabel << ui->cropping << ui->cuesLabel << ui->cues
                  << ui->propertiesLabel << ui->generalOptionsBox << ui->fixBitstreamTimingInfo;

  m_subtitleControls << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag
                     << ui->forcedTrackFlagLabel << ui->forcedTrackFlag << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTags << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                     << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes
                     << ui->subtitleAndChapterPropertiesBox << ui->characterSetLabel << ui->subtitleCharacterSet << ui->cuesLabel << ui->cues
                     << ui->propertiesLabel << ui->generalOptionsBox;

  m_chapterControls << ui->subtitleAndChapterPropertiesBox << ui->characterSetLabel << ui->subtitleCharacterSet << ui->propertiesLabel << ui->generalOptionsBox;

  m_allInputControls << ui->muxThisLabel << ui->muxThis << ui->trackNameLabel << ui->trackName << ui->trackLanguageLabel << ui->trackLanguage << ui->defaultTrackFlagLabel << ui->defaultTrackFlag
                     << ui->forcedTrackFlagLabel << ui->forcedTrackFlag << ui->compressionLabel << ui->compression << ui->trackTagsLabel << ui->trackTags << ui->browseTrackTags << ui->timecodesAndDefaultDurationBox
                     << ui->delayLabel << ui->delay << ui->stretchByLabel << ui->stretchBy << ui->defaultDurationLabel << ui->defaultDuration << ui->timecodesLabel << ui->timecodes << ui->browseTimecodes
                     << ui->picturePropertiesBox << ui->setAspectRatio << ui->aspectRatio << ui->setDisplayWidthHeight << ui->displayWidth << ui->displayDimensionsXLabel << ui->displayHeight << ui->stereoscopyLabel
                     << ui->stereoscopy << ui->croppingLabel << ui->cropping << ui->audioPropertiesBox << ui->aacIsSBR << ui->subtitleAndChapterPropertiesBox << ui->characterSetLabel << ui->subtitleCharacterSet
                     << ui->miscellaneousBox << ui->cuesLabel << ui->cues << ui->userDefinedTrackOptionsLabel << ui->userDefinedTrackOptions
                     << ui->propertiesLabel << ui->generalOptionsBox << ui->fixBitstreamTimingInfo;

  m_comboBoxControls << ui->muxThis << ui->trackLanguage << ui->defaultTrackFlag << ui->forcedTrackFlag << ui->compression << ui->cues << ui->stereoscopy << ui->aacIsSBR << ui->subtitleCharacterSet;
}

void
MergeWidget::setupInputControls() {
  setupControlLists();

  ui->files->setModel(m_filesModel);
  ui->tracks->setModel(m_tracksModel);

  // Track & chapter language
  std::vector<std::pair<QString, QString> > languages;
  for (auto &language : iso639_languages)
    languages.push_back(std::make_pair(QString{"%1 (%2)"}.arg(to_qs(language.english_name)).arg(to_qs(language.iso639_2_code)), to_qs(language.iso639_2_code)));

  brng::sort(languages, [](std::pair<QString, QString> const &a, std::pair<QString, QString> const &b) { return a.first < b.first; });

  ui->chapterLanguage->addItem(Q(""), Q(""));

  for (auto &language: languages) {
    ui->trackLanguage->addItem(language.first, language.second);
    ui->chapterLanguage->addItem(language.first, language.second);
  }

  // Track & chapter character set
  QStringList characterSets;
  for (auto &subCharset : sub_charsets)
    characterSets << to_qs(subCharset);
  characterSets.sort();

  ui->subtitleCharacterSet->addItem(Q(""), Q(""));
  ui->chapterCharacterSet->addItem(Q(""), Q(""));
  for (auto &characterSet : characterSets) {
    ui->subtitleCharacterSet->addItem(characterSet, characterSet);
    ui->chapterCharacterSet->addItem(characterSet, characterSet);
  }

  // Stereoscopy
  ui->stereoscopy->addItem(Q(""), 0);
  for (auto idx = 0u, end = stereo_mode_c::max_index(); idx <= end; ++idx)
    ui->stereoscopy->addItem(QString{"%1 (%2; %3)"}.arg(to_qs(stereo_mode_c::translate(idx))).arg(idx).arg(to_qs(stereo_mode_c::s_modes[idx])), idx + 1);

  // Set item data to index for distinguishing between empty values
  // added by "multiple selection mode".
  for (auto control : std::vector<QComboBox *>{ui->defaultTrackFlag, ui->forcedTrackFlag, ui->cues, ui->compression, ui->muxThis, ui->aacIsSBR})
    for (auto idx = 0; control->count() > idx; ++idx)
      control->setItemData(idx, idx);

  for (auto const &control : m_comboBoxControls)
    control->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  // Connect signals & slots.
  connect(ui->files->selectionModel(),  SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(onFileSelectionChanged()));
  connect(ui->tracks->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(onTrackSelectionChanged()));

  // "files" context menu
  ui->files->addAction(m_addFilesAction);
  ui->files->addAction(m_appendFilesAction);
  ui->files->addAction(m_addAdditionalPartsAction);
  ui->files->addAction(m_removeFilesAction);
  ui->files->addAction(m_removeAllFilesAction);

  connect(m_addFilesAction,           SIGNAL(triggered()), this, SLOT(onAddFiles()));
  connect(m_appendFilesAction,        SIGNAL(triggered()), this, SLOT(onAppendFiles()));
  connect(m_addAdditionalPartsAction, SIGNAL(triggered()), this, SLOT(onAddAdditionalParts()));
  connect(m_removeFilesAction,        SIGNAL(triggered()), this, SLOT(onRemoveFiles()));
  connect(m_removeAllFilesAction,     SIGNAL(triggered()), this, SLOT(onRemoveAllFiles()));

  onTrackSelectionChanged();
}

void
MergeWidget::onFileSelectionChanged() {
  enableFilesActions();
}

void
MergeWidget::onTrackSelectionChanged() {
  Util::enableWidgets(m_allInputControls, false);

  auto selection = ui->tracks->selectionModel()->selection();
  if (selection.isEmpty())
    return;

  if (1 < selection.size()) {
    setInputControlValues(nullptr);
    Util::enableWidgets(m_allInputControls, true);
    return;
  }

  Util::enableWidgets(m_typeIndependantControls, true);

  auto idxs = selection[0].indexes();
  if (idxs.isEmpty() || !idxs[0].isValid())
    return;

  auto track = m_tracksModel->fromIndex(idxs[0]);
  if (!track)
    return;

  setInputControlValues(track);

  if (track->isAudio())
    Util::enableWidgets(m_audioControls, true);

  else if (track->isVideo())
    Util::enableWidgets(m_videoControls, true);

  else if (track->isSubtitles())
    Util::enableWidgets(m_subtitleControls, true);

  else if (track->isChapters())
    Util::enableWidgets(m_chapterControls, true);
}

void
MergeWidget::addOrRemoveEmptyComboBoxItem(bool add) {
  for (auto &comboBox : m_comboBoxControls)
    if (add && comboBox->itemData(0).isValid())
      comboBox->insertItem(0, QY("<do not change>"));
    else if (!add && !comboBox->itemData(0).isValid())
      comboBox->removeItem(0);
}

void
MergeWidget::clearInputControlValues() {
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
MergeWidget::setInputControlValues(Track *track) {
  m_currentlySettingInputControlValues = true;

  addOrRemoveEmptyComboBoxItem(!track);

  if (!track) {
    clearInputControlValues();
    m_currentlySettingInputControlValues = false;
    return;
  }

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

QList<SourceFile *>
MergeWidget::selectedSourceFiles()
  const {
  auto sourceFiles = QList<SourceFile *>{};
  Util::withSelectedIndexes(ui->files, [&sourceFiles, this](QModelIndex const &idx) {
      auto sourceFile = m_filesModel->fromIndex(idx);
      if (sourceFile)
        sourceFiles << sourceFile;
    });

  return sourceFiles;
}

QList<Track *>
MergeWidget::selectedTracks()
  const {
  auto tracks = QList<Track *>{};
  Util::withSelectedIndexes(ui->tracks, [&tracks, this](QModelIndex const &idx) {
      auto track = m_tracksModel->fromIndex(idx);
      if (track)
        tracks << track;
    });

  return tracks;
}

void
MergeWidget::withSelectedTracks(std::function<void(Track *)> code,
                                bool notIfAppending,
                                QWidget *widget) {
  if (m_currentlySettingInputControlValues)
    return;

  auto tracks = selectedTracks();
  if (tracks.isEmpty())
    return;

  if (!widget)
    widget = static_cast<QWidget *>(QObject::sender());

  bool withAudio     = m_audioControls.contains(widget);
  bool withVideo     = m_videoControls.contains(widget);
  bool withSubtitles = m_subtitleControls.contains(widget);
  bool withChapters  = m_chapterControls.contains(widget);
  bool withAll       = m_typeIndependantControls.contains(widget);

  for (auto &track : tracks) {
    if (track->m_appendedTo && notIfAppending)
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
MergeWidget::onTrackNameEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_name = newValue; }, true);
}

void
MergeWidget::onMuxThisChanged(int newValue) {
  auto data = ui->muxThis->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_muxThis = 0 == newValue; });
}

void
MergeWidget::onTrackLanguageChanged(int newValue) {
  auto code = ui->trackLanguage->itemData(newValue).toString();
  if (code.isEmpty())
    return;

  withSelectedTracks([&](Track *track) { track->m_language = code; }, true);
}

void
MergeWidget::onDefaultTrackFlagChanged(int newValue) {
  auto data = ui->defaultTrackFlag->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_defaultTrackFlag = newValue; }, true);
}

void
MergeWidget::onForcedTrackFlagChanged(int newValue) {
  auto data = ui->forcedTrackFlag->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_forcedTrackFlag = newValue; }, true);
}

void
MergeWidget::onCompressionChanged(int newValue) {
  auto data = ui->compression->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  Track::Compression compression;
  if (3 > newValue)
    compression = 0 == newValue ? Track::CompDefault
                : 1 == newValue ? Track::CompNone
                :                 Track::CompZlib;

  withSelectedTracks([&](Track *track) { track->m_compression = compression; }, true);
}

void
MergeWidget::onTrackTagsEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_tags = newValue; }, true);
}

void
MergeWidget::onDelayEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_delay = newValue; });
}

void
MergeWidget::onStretchByEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_stretchBy = newValue; });
}

void
MergeWidget::onDefaultDurationEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_defaultDuration = newValue; }, true);
}

void
MergeWidget::onTimecodesEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_timecodes = newValue; });
}

void
MergeWidget::onBrowseTimecodes() {
  auto fileName = getOpenFileName(QY("Select timecode file"), QY("Text files") + Q(" (*.txt)"), ui->timecodes);
  if (!fileName.isEmpty())
    withSelectedTracks([&](Track *track) { track->m_timecodes = fileName; });
}

void
MergeWidget::onFixBitstreamTimingInfoChanged(bool newValue) {
  withSelectedTracks([&](Track *track) { track->m_fixBitstreamTimingInfo = newValue; });
}

void
MergeWidget::onBrowseTrackTags() {
  auto fileName = getOpenFileName(QY("Select tags file"), QY("XML files") + Q(" (*.xml)"), ui->trackTags);
  if (!fileName.isEmpty())
    withSelectedTracks([&](Track *track) { track->m_tags = fileName; }, true);
}

void
MergeWidget::onSetAspectRatio() {
  withSelectedTracks([&](Track *track) { track->m_setAspectRatio = true; }, true);
}

void
MergeWidget::onSetDisplayDimensions() {
  withSelectedTracks([&](Track *track) { track->m_setAspectRatio = false; }, true);
}

void
MergeWidget::onAspectRatioEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_aspectRatio = newValue; }, true);
}

void
MergeWidget::onDisplayWidthEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_displayWidth = newValue; }, true);
}

void
MergeWidget::onDisplayHeightEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_displayHeight = newValue; }, true);
}

void
MergeWidget::onStereoscopyChanged(int newValue) {
  auto data = ui->stereoscopy->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_stereoscopy = newValue; }, true);
}

void
MergeWidget::onCroppingEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_cropping = newValue; }, true);
}

void
MergeWidget::onAacIsSBRChanged(int newValue) {
  auto data = ui->aacIsSBR->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_aacIsSBR = newValue; }, true);
}

void
MergeWidget::onSubtitleCharacterSetChanged(int newValue) {
  auto characterSet = ui->subtitleCharacterSet->itemData(newValue).toString();
  if (characterSet.isEmpty())
    return;

  withSelectedTracks([&](Track *track) { track->m_characterSet = characterSet; }, true);
}

void
MergeWidget::onCuesChanged(int newValue) {
  auto data = ui->cues->itemData(newValue);
  if (!data.isValid())
    return;
  newValue = data.toInt();

  withSelectedTracks([&](Track *track) { track->m_cues = newValue; }, true);
}

void
MergeWidget::onUserDefinedTrackOptionsEdited(QString newValue) {
  withSelectedTracks([&](Track *track) { track->m_userDefinedOptions = newValue; }, true);
}

void
MergeWidget::onAddFiles() {
  addOrAppendFiles(false);
}

void
MergeWidget::onAppendFiles() {
  // auto selectedFile = m_filesModel->fromIndex(selectedSourceFile());
  // if (selectedFile && !selectedFile->m_tracks.size()) {
  //   QMessageBox::critical(this, QY("Unable to append files"), QY("You cannot append tracks or add additional parts to files that contain tracks."));
  //   return;
  // }

  addOrAppendFiles(true);
}

void
MergeWidget::addOrAppendFiles(bool append) {
  auto fileNames = selectFilesToAdd(append ? QY("Append media files") : QY("Add media files"));
  if (fileNames.empty())
    return;

  QList<SourceFilePtr> identifiedFiles;
  for (auto &fileName : fileNames) {
    FileIdentifier identifier{ this, fileName };
    if (identifier.identify())
      identifiedFiles << identifier.file();
  }

  if (identifiedFiles.isEmpty())
    return;

  m_filesModel->addOrAppendFilesAndTracks(selectedSourceFile(), identifiedFiles, append);
  reinitFilesTracksControls();
}

QStringList
MergeWidget::selectFilesToAdd(QString const &title) {
  QFileDialog dlg{this};
  dlg.setNameFilters(FileTypeFilter::get());
  dlg.setFileMode(QFileDialog::ExistingFiles);
  dlg.setDirectory(Settings::get().m_lastOpenDir);
  dlg.setWindowTitle(title);

  if (!dlg.exec())
    return QStringList{};

  Settings::get().m_lastOpenDir = dlg.directory();

  return dlg.selectedFiles();
}

void
MergeWidget::onAddAdditionalParts() {
  auto currentIdx = selectedSourceFile();
  auto sourceFile = m_filesModel->fromIndex(currentIdx);
  if (sourceFile && !sourceFile->m_tracks.size()) {
    QMessageBox::critical(this, QY("Unable to append files"), QY("You cannot append tracks or add additional parts to files that contain tracks."));
    return;
  }

  m_filesModel->addAdditionalParts(currentIdx, selectFilesToAdd(QY("Add media files as additional parts")));
}

void
MergeWidget::onRemoveFiles() {
  auto selectedFiles = selectedSourceFiles();
  if (selectedFiles.isEmpty())
    return;

  m_filesModel->removeFiles(selectedFiles);

  reinitFilesTracksControls();
}

void
MergeWidget::onRemoveAllFiles() {
  if (m_config.m_files.isEmpty())
    return;

  m_filesModel->removeRows(0, m_filesModel->rowCount());
  m_tracksModel->removeRows(0, m_tracksModel->rowCount());
  m_config.m_files.clear();
  m_config.m_tracks.clear();

  reinitFilesTracksControls();
}

void
MergeWidget::reinitFilesTracksControls() {
  resizeFilesColumnsToContents();
  resizeTracksColumnsToContents();
  onFileSelectionChanged();
  onTrackSelectionChanged();
  enableFilesActions();
}

void
MergeWidget::resizeFilesColumnsToContents()
  const {
  Util::resizeViewColumnsToContents(ui->files);
}

void
MergeWidget::resizeTracksColumnsToContents()
  const {
  Util::resizeViewColumnsToContents(ui->tracks);
}

void
MergeWidget::enableFilesActions() {
  int numSelected      = ui->files->selectionModel()->selection().size();
  bool hasRegularTrack = false;
  if (1 == numSelected)
    hasRegularTrack = m_config.m_files.end() != brng::find_if(m_config.m_files, [](SourceFilePtr const &file) { return file->hasRegularTrack(); });

  m_addFilesAction->setEnabled(true);
  m_appendFilesAction->setEnabled((1 == numSelected) && hasRegularTrack);
  m_addAdditionalPartsAction->setEnabled(1 == numSelected);
  m_removeFilesAction->setEnabled(0 < numSelected);
  m_removeAllFilesAction->setEnabled(!m_config.m_files.isEmpty());
}

void
MergeWidget::retranslateInputUI() {
  m_addFilesAction->setText(QY("&Add files"));
  m_appendFilesAction->setText(QY("A&ppend files"));
  m_addAdditionalPartsAction->setText(QY("Add files as a&dditional parts"));
  m_removeFilesAction->setText(QY("&Remove files"));
  m_removeAllFilesAction->setText(QY("Remove a&ll files"));

  for (auto &comboBox : m_comboBoxControls)
    if (!((0 == comboBox->count()) || comboBox->itemData(0).isValid()))
      comboBox->setItemText(0, QY("<do not change>"));
}

QModelIndex
MergeWidget::selectedSourceFile()
  const {
  auto idx = ui->files->selectionModel()->currentIndex();
  return m_filesModel->index(idx.row(), 0, idx.parent());
}
