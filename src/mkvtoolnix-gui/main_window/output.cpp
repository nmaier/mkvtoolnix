#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/main_window/main_window.h"
#include "mkvtoolnix-gui/forms/main_window.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileDialog>

void
MainWindow::onTitleEdited(QString newValue) {
  m_config.m_title = newValue;
}

void
MainWindow::onOutputEdited(QString newValue) {
  m_config.m_destination = newValue;
}

void
MainWindow::onBrowseOutput() {
  auto filter   = m_config.m_webmMode ? QY("WebM files") + Q(" (*.webm)") : QY("Matroska files") + Q(" (*.mkv *.mka *.mks *.mk3d)");
  auto fileName = getSaveFileName(QY("Select output file name"), filter, ui->output);
  if (!fileName.isEmpty())
    m_config.m_destination = fileName;
}

void
MainWindow::onGlobalTagsEdited(QString newValue) {
  m_config.m_globalTags = newValue;
}

void
MainWindow::onBrowseGlobalTags() {
  auto fileName = getOpenFileName(QY("Select tags file"), QY("XML files") + Q(" (*.xml)"), ui->globalTags);
  if (!fileName.isEmpty())
    m_config.m_globalTags = fileName;
}

void
MainWindow::onSegmentinfoEdited(QString newValue) {
  m_config.m_segmentinfo = newValue;
}

void
MainWindow::onBrowseSegmentinfo() {
  auto fileName = getOpenFileName(QY("Select segment info file"), QY("XML files") + Q(" (*.xml)"), ui->segmentinfo);
  if (!fileName.isEmpty())
    m_config.m_segmentinfo = fileName;
}

void
MainWindow::onDoNotSplit() {
  m_config.m_splitMode = MuxConfig::DoNotSplit;
}

void
MainWindow::onDoSplitAfterSize() {
  m_config.m_splitMode = MuxConfig::SplitAfterSize;
}

void
MainWindow::onDoSplitAfterDuration() {
  m_config.m_splitMode = MuxConfig::SplitAfterDuration;
}

void
MainWindow::onDoSplitAfterTimecodes() {
  m_config.m_splitMode = MuxConfig::SplitAfterTimecodes;
}

void
MainWindow::onDoSplitByParts() {
  m_config.m_splitMode = MuxConfig::SplitByParts;
}

void
MainWindow::onSplitSizeEdited(QString newValue) {
  m_config.m_splitAfterSize = newValue;
}

void
MainWindow::onSplitDurationEdited(QString newValue) {
  m_config.m_splitAfterDuration = newValue;
}

void
MainWindow::onSplitTimecodesEdited(QString newValue) {
  m_config.m_splitAfterTimecodes = newValue;
}

void
MainWindow::onSplitPartsEdited(QString newValue) {
  m_config.m_splitByParts = newValue;
}

void
MainWindow::onLinkFilesClicked(bool newValue) {
  m_config.m_linkFiles = newValue;
}

void
MainWindow::onSplitMaxFilesChanged(int newValue) {
  m_config.m_splitMaxFiles = newValue;
}

void
MainWindow::onSegmentUIDsEdited(QString newValue) {
  m_config.m_segmentUIDs = newValue;
}

void
MainWindow::onPreviousSegmentUIDEdited(QString newValue) {
  m_config.m_previousSegmentUID = newValue;
}

void
MainWindow::onNextSegmentUIDEdited(QString newValue) {
  m_config.m_nextSegmentUID = newValue;
}

void
MainWindow::onChaptersEdited(QString newValue) {
  m_config.m_chapters = newValue;
}

void
MainWindow::onBrowseChapters() {
  auto fileName = getOpenFileName(QY("Select chapter file"), QY("XML files") + Q(" (*.xml)"), ui->chapters);
  if (!fileName.isEmpty())
    m_config.m_chapters = fileName;
}

void
MainWindow::onChapterLanguageChanged(int newValue) {
  auto data = ui->chapterLanguage->itemData(newValue);
  if (data.isValid())
    m_config.m_chapterLanguage = data.toString();
}

void
MainWindow::onChapterCharacterSetChanged(QString newValue) {
  m_config.m_chapterCharacterSet = newValue;
}

void
MainWindow::onChapterCueNameFormatChanged(QString newValue) {
  m_config.m_chapterCueNameFormat = newValue;
}

void
MainWindow::onWebmClicked(bool newValue) {
  m_config.m_webmMode = newValue;
  // TODO: change output file extension
}

void
MainWindow::onUserDefinedOptionsEdited(QString newValue) {
  m_config.m_userDefinedOptions = newValue;
}

void
MainWindow::onEditUserDefinedOptions() {
  // TODO
}
