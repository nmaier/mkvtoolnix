#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/merge_widget/merge_widget.h"
#include "mkvtoolnix-gui/forms/merge_widget.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileDialog>

void
MergeWidget::setupOutputControls() {
  m_splitControls << ui->splitOptionsLabel << ui->splitOptions << ui->splittingOptionsLabel << ui->splitMaxFilesLabel << ui->splitMaxFiles << ui->linkFiles;

  auto comboBoxControls = QList<QComboBox *>{} << ui->splitMode << ui->chapterLanguage << ui->chapterCharacterSet;
  for (auto const &control : comboBoxControls)
    control->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  onSplitModeChanged(MuxConfig::DoNotSplit);
}

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
MergeWidget::onSegmentInfoEdited(QString newValue) {
  m_config.m_segmentInfo = newValue;
}

void
MergeWidget::onBrowseSegmentInfo() {
  auto fileName = getOpenFileName(QY("Select segment info file"), QY("XML files") + Q(" (*.xml)"), ui->segmentInfo);
  if (!fileName.isEmpty())
    m_config.m_segmentInfo = fileName;
}

void
MergeWidget::onSplitModeChanged(int newMode) {
  auto splitMode       = static_cast<MuxConfig::SplitMode>(newMode);
  m_config.m_splitMode = splitMode;

  if (MuxConfig::DoNotSplit == splitMode) {
    Util::enableWidgets(m_splitControls, false);
    return;
  }

  Util::enableWidgets(m_splitControls, true);

  auto tooltip = QStringList{};
  auto entries = QStringList{};
  auto label   = QString{};

  if (MuxConfig::SplitAfterSize == splitMode) {
    label    = QY("Size:");
    tooltip << QY("The size after which a new output file is started.")
            << QY("The letters 'G', 'M' and 'K' can be used to indicate giga/mega/kilo bytes respectively.")
            << QY("All units are based on 1024 (G = 1024^3, M = 1024^2, K = 1024).");
    entries << Q("")
            << Q("350M")
            << Q("650M")
            << Q("700M")
            << Q("703M")
            << Q("800M")
            << Q("1000M")
            << Q("4483M")
            << Q("8142M");

  } else if (MuxConfig::SplitAfterDuration == splitMode) {
    label    = QY("Duration:");
    tooltip << QY("The duration after which a new output file is started.")
            << QY("The time can be given either in the form HH:MM:SS.nnnnnnnnn or as the number of seconds followed by 's'.")
            << QY("You may omit the number of hours 'HH' and the number of nanoseconds 'nnnnnnnnn'.")
            << QY("If given then you may use up to nine digits after the decimal point.")
            << QY("Examples: 01:00:00 (after one hour) or 1800s (after 1800 seconds).");

    entries << Q("")
            << Q("01:00:00")
            << Q("1800s");

  } else if (MuxConfig::SplitAfterTimecodes == splitMode) {
    label    = QY("Timecodes:");
    tooltip << QY("The timecodes after which a new output file is started.")
            << QY("The timecodes refer to the whole stream and not to each individual output file.")
            << QY("The timecodes can be given either in the form HH:MM:SS.nnnnnnnnn or as the number of seconds followed by 's'.")
            << QY("You may omit the number of hours 'HH'.")
            << QY("You can specify up to nine digits for the number of nanoseconds 'nnnnnnnnn' or none at all.")
            << QY("If given then you may use up to nine digits after the decimal point.")
            << QY("If two or more timecodes are used then you have to separate them with commas.")
            << QY("The formats can be mixed, too.")
            << QY("Examples: 01:00:00,01:30:00 (after one hour and after one hour and thirty minutes) or 1800s,3000s,00:10:00 (after three, five and ten minutes).");

  } else if (MuxConfig::SplitByParts == splitMode) {
    label    = QY("Parts:");
    tooltip << QY("A comma-separated list of timecode ranges of content to keep.")
            << QY("Each range consists of a start and end timecode with a '-' in the middle, e.g. '00:01:15-00:03:20'.")
            << QY("If a start timecode is left out then the previous range's end timecode is used, or the start of the file if there was no previous range.")
            << QY("The timecodes can be given either in the form HH:MM:SS.nnnnnnnnn or as the number of seconds followed by 's'.")
            << QY("If a range's start timecode is prefixed with '+' then its content will be written to the same file as the previous range. Otherwise a new file will be created for this range.");

  } else if (MuxConfig::SplitByPartsFrames == splitMode) {
    label    = QY("Parts:");
    tooltip << QY("A comma-separated list of frame/field number ranges of content to keep.")
            << QY("Each range consists of a start and end frame/field number with a '-' in the middle, e.g. '157-238'.")
            << QY("The numbering starts at 1.")
            << QY("This mode considers only the first video track that is output.")
            << QY("If no video track is output no splitting will occur.")
            << QY("The numbers given with this argument are interpreted based on the number of Matroska blocks that are output.")
            << QY("A single Matroska block contains either a full frame (for progressive content) or a single field (for interlaced content).")
            << QY("mkvmerge does not distinguish between those two and simply counts the number of blocks.")
            << QY("If a start number is left out then the previous range's end number is used, or the start of the file if there was no previous range.")
            << QY("If a range's start number is prefixed with '+' then its content will be written to the same file as the previous range. Otherwise a new file will be created for this range.");

  } else if (MuxConfig::SplitByFrames == splitMode) {
    label    = QY("Frames/fields:");
    tooltip << QY("A comma-separated list of frame/field numbers after which to split.")
            << QY("The numbering starts at 1.")
            << QY("This mode considers only the first video track that is output.")
            << QY("If no video track is output no splitting will occur.")
            << QY("The numbers given with this argument are interpreted based on the number of Matroska blocks that are output.")
            << QY("A single Matroska block contains either a full frame (for progressive content) or a single field (for interlaced content).")
            << QY("mkvmerge does not distinguish between those two and simply counts the number of blocks.");

  } else if (MuxConfig::SplitAfterChapters == splitMode) {
    label    = QY("Chapter numbers:");
    tooltip << QY("Either the word 'all' which selects all chapters or a comma-separated list of chapter numbers before which to split.")
            << QY("The numbering starts at 1.")
            << QY("Splitting will occur right before the first key frame whose timecode is equal to or bigger than the start timecode for the chapters whose numbers are listed.")
            << QY("A chapter starting at 0s is never considered for splitting and discarded silently.")
            << QY("This mode only considers the top-most level of chapters across all edition entries.");

  }

  auto options = ui->splitOptions->currentText();

  ui->splitOptionsLabel->setText(label);
  ui->splitOptions->clear();
  ui->splitOptions->addItems(entries);
  ui->splitOptions->setCurrentText(options);
  ui->splitOptions->setToolTip(tooltip.join(Q(" ")));
}

void
MergeWidget::onSplitOptionsEdited(QString newValue) {
  m_config.m_splitOptions = newValue;
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

void
MergeWidget::setOutputControlValues() {
  ui->title->setText(m_config.m_title);
  ui->output->setText(m_config.m_destination);
  ui->globalTags->setText(m_config.m_globalTags);
  ui->segmentInfo->setText(m_config.m_segmentInfo);
  ui->splitMode->setCurrentIndex(m_config.m_splitMode);
  ui->splitOptions->setEditText(m_config.m_splitOptions);
  ui->splitMaxFiles->setValue(m_config.m_splitMaxFiles);
  ui->linkFiles->setChecked(m_config.m_linkFiles);
  ui->segmentUIDs->setText(m_config.m_segmentUIDs);
  ui->previousSegmentUID->setText(m_config.m_previousSegmentUID);
  ui->nextSegmentUID->setText(m_config.m_nextSegmentUID);
  ui->chapters->setText(m_config.m_chapters);
  ui->chapterCueNameFormat->setText(m_config.m_chapterCueNameFormat);
  ui->userDefinedOptions->setText(m_config.m_userDefinedOptions);
  ui->webmMode->setChecked(m_config.m_webmMode);

  Util::setComboBoxIndexIf(ui->chapterLanguage,     [&](QString const &, QVariant const &data) { return data.isValid() && (data.toString() == m_config.m_chapterLanguage);     });
  Util::setComboBoxIndexIf(ui->chapterCharacterSet, [&](QString const &, QVariant const &data) { return data.isValid() && (data.toString() == m_config.m_chapterCharacterSet); });
}
