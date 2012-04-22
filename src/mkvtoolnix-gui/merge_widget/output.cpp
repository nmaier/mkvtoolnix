#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/merge_widget/merge_widget.h"
#include "mkvtoolnix-gui/forms/merge_widget.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileDialog>

void
MergeWidget::onTitleEdited(QString newValue) {
  m_config.m_title = newValue;
}

void
MergeWidget::onOutputEdited(QString newValue) {
  m_config.m_destination = newValue;
}

void
MergeWidget::onBrowseOutput() {
  auto filter   = m_config.m_webmMode ? QY("WebM files") + Q(" (*.webm)") : QY("Matroska files") + Q(" (*.mkv *.mka *.mks *.mk3d)");
  auto fileName = getSaveFileName(QY("Select output file name"), filter, ui->output);
  if (!fileName.isEmpty())
    m_config.m_destination = fileName;
}

void
MergeWidget::onGlobalTagsEdited(QString newValue) {
  m_config.m_globalTags = newValue;
}

void
MergeWidget::onBrowseGlobalTags() {
  auto fileName = getOpenFileName(QY("Select tags file"), QY("XML files") + Q(" (*.xml)"), ui->globalTags);
  if (!fileName.isEmpty())
    m_config.m_globalTags = fileName;
}

void
MergeWidget::onSegmentinfoEdited(QString newValue) {
  m_config.m_segmentinfo = newValue;
}

void
MergeWidget::onBrowseSegmentinfo() {
  auto fileName = getOpenFileName(QY("Select segment info file"), QY("XML files") + Q(" (*.xml)"), ui->segmentinfo);
  if (!fileName.isEmpty())
    m_config.m_segmentinfo = fileName;
}

void
MergeWidget::onDoNotSplit() {
  m_config.m_splitMode = MuxConfig::DoNotSplit;
}

void
MergeWidget::onDoSplitAfterSize() {
  m_config.m_splitMode = MuxConfig::SplitAfterSize;
}

void
MergeWidget::onDoSplitAfterDuration() {
  m_config.m_splitMode = MuxConfig::SplitAfterDuration;
}

void
MergeWidget::onDoSplitAfterTimecodes() {
  m_config.m_splitMode = MuxConfig::SplitAfterTimecodes;
}

void
MergeWidget::onDoSplitByParts() {
  m_config.m_splitMode = MuxConfig::SplitByParts;
}

void
MergeWidget::onSplitSizeEdited(QString newValue) {
  m_config.m_splitAfterSize = newValue;
}

void
MergeWidget::onSplitDurationEdited(QString newValue) {
  m_config.m_splitAfterDuration = newValue;
}

void
MergeWidget::onSplitTimecodesEdited(QString newValue) {
  m_config.m_splitAfterTimecodes = newValue;
}

void
MergeWidget::onSplitPartsEdited(QString newValue) {
  m_config.m_splitByParts = newValue;
}

void
MergeWidget::onLinkFilesClicked(bool newValue) {
  m_config.m_linkFiles = newValue;
}

void
MergeWidget::onSplitMaxFilesChanged(int newValue) {
  m_config.m_splitMaxFiles = newValue;
}

void
MergeWidget::onSegmentUIDsEdited(QString newValue) {
  m_config.m_segmentUIDs = newValue;
}

void
MergeWidget::onPreviousSegmentUIDEdited(QString newValue) {
  m_config.m_previousSegmentUID = newValue;
}

void
MergeWidget::onNextSegmentUIDEdited(QString newValue) {
  m_config.m_nextSegmentUID = newValue;
}

void
MergeWidget::onChaptersEdited(QString newValue) {
  m_config.m_chapters = newValue;
}

void
MergeWidget::onBrowseChapters() {
  auto fileName = getOpenFileName(QY("Select chapter file"), QY("XML files") + Q(" (*.xml)"), ui->chapters);
  if (!fileName.isEmpty())
    m_config.m_chapters = fileName;
}

void
MergeWidget::onChapterLanguageChanged(int newValue) {
  auto data = ui->chapterLanguage->itemData(newValue);
  if (data.isValid())
    m_config.m_chapterLanguage = data.toString();
}

void
MergeWidget::onChapterCharacterSetChanged(QString newValue) {
  m_config.m_chapterCharacterSet = newValue;
}

void
MergeWidget::onChapterCueNameFormatChanged(QString newValue) {
  m_config.m_chapterCueNameFormat = newValue;
}

void
MergeWidget::onWebmClicked(bool newValue) {
  m_config.m_webmMode = newValue;
  // TODO: change output file extension
}

void
MergeWidget::onUserDefinedOptionsEdited(QString newValue) {
  m_config.m_userDefinedOptions = newValue;
}

void
MergeWidget::onEditUserDefinedOptions() {
  // TODO
}
