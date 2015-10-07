#ifndef MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_MERGE_WIDGET_H
#define MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_MERGE_WIDGET_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/merge_widget/attachment_model.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"
#include "mkvtoolnix-gui/merge_widget/source_file_model.h"
#include "mkvtoolnix-gui/merge_widget/track_model.h"

#include <QComboBox>
#include <QFileInfo>
#include <QList>
#include <QMenu>
#include <QWidget>

class QTreeView;

namespace Ui {
class MergeWidget;
}

class MergeWidget : public QWidget {
  Q_OBJECT;

protected:
  // non-UI stuff:
  MuxConfig m_config;

  // UI stuff:
  Ui::MergeWidget *ui;

  // "Input" tab:
  SourceFileModel *m_filesModel;
  TrackModel *m_tracksModel;

  QList<QWidget *> m_audioControls, m_videoControls, m_subtitleControls, m_chapterControls, m_typeIndependantControls, m_allInputControls, m_splitControls;
  QList<QComboBox *> m_comboBoxControls;
  bool m_currentlySettingInputControlValues;

  QAction *m_addFilesAction, *m_appendFilesAction, *m_addAdditionalPartsAction, *m_removeFilesAction, *m_removeAllFilesAction;

  // "Attachments" tab:
  AttachmentModel *m_attachmentsModel;
  QAction *m_addAttachmentsAction, *m_removeAttachmentsAction;

public:
  explicit MergeWidget(QWidget *parent = nullptr);
  ~MergeWidget();

public slots:
  // Input tab:
  virtual void onSaveConfig();
  virtual void onSaveConfigAs();
  virtual void onOpenConfig();
  virtual void onAddFiles();
  virtual void onAddAdditionalParts();
  virtual void onAppendFiles();
  virtual void onRemoveFiles();
  virtual void onRemoveAllFiles();
  virtual void onNew();
  virtual void onAddToJobQueue();
  virtual void onStartMuxing();

  virtual void onFileSelectionChanged();
  virtual void onTrackSelectionChanged();

  virtual void onTrackNameEdited(QString newValue);
  virtual void onMuxThisChanged(int newValue);
  virtual void onTrackLanguageChanged(int newValue);
  virtual void onDefaultTrackFlagChanged(int newValue);
  virtual void onForcedTrackFlagChanged(int newValue);
  virtual void onCompressionChanged(int newValue);
  virtual void onTrackTagsEdited(QString newValue);
  virtual void onDelayEdited(QString newValue);
  virtual void onStretchByEdited(QString newValue);
  virtual void onDefaultDurationEdited(QString newValue);
  virtual void onTimecodesEdited(QString newValue);
  virtual void onBrowseTimecodes();
  virtual void onFixBitstreamTimingInfoChanged(bool newValue);
  virtual void onBrowseTrackTags();
  virtual void onSetAspectRatio();
  virtual void onSetDisplayDimensions();
  virtual void onAspectRatioEdited(QString newValue);
  virtual void onDisplayWidthEdited(QString newValue);
  virtual void onDisplayHeightEdited(QString newValue);
  virtual void onStereoscopyChanged(int newValue);
  virtual void onCroppingEdited(QString newValue);
  virtual void onAacIsSBRChanged(int newValue);
  virtual void onSubtitleCharacterSetChanged(int newValue);
  virtual void onCuesChanged(int newValue);
  virtual void onUserDefinedTrackOptionsEdited(QString newValue);

  virtual void resizeFilesColumnsToContents() const;
  virtual void resizeTracksColumnsToContents() const;
  virtual void reinitFilesTracksControls();

  // Output tab:
  virtual void onTitleEdited(QString newValue);
  virtual void onOutputEdited(QString newValue);
  virtual void onBrowseOutput();
  virtual void onGlobalTagsEdited(QString newValue);
  virtual void onBrowseGlobalTags();
  virtual void onSegmentInfoEdited(QString newValue);
  virtual void onBrowseSegmentInfo();
  virtual void onSplitModeChanged(int newMode);
  virtual void onSplitOptionsEdited(QString newValue);
  virtual void onLinkFilesClicked(bool newValue);
  virtual void onSplitMaxFilesChanged(int newValue);
  virtual void onSegmentUIDsEdited(QString newValue);
  virtual void onPreviousSegmentUIDEdited(QString newValue);
  virtual void onNextSegmentUIDEdited(QString newValue);
  virtual void onChaptersEdited(QString newValue);
  virtual void onBrowseChapters();
  virtual void onChapterLanguageChanged(int newValue);
  virtual void onChapterCharacterSetChanged(QString newValue);
  virtual void onChapterCueNameFormatChanged(QString newValue);
  virtual void onWebmClicked(bool newValue);
  virtual void onUserDefinedOptionsEdited(QString newValue);
  virtual void onEditUserDefinedOptions();

  // Attachments tab:
  virtual void onAttachmentSelectionChanged();
  virtual void onAttachmentNameEdited(QString newValue);
  virtual void onAttachmentDescriptionEdited(QString newValue);
  virtual void onAttachmentMIMETypeEdited(QString newValue);
  virtual void onAttachmentStyleChanged(int newValue);
  virtual void onAddAttachments();
  virtual void onRemoveAttachments();

  virtual void resizeAttachmentsColumnsToContents() const;

  virtual void onShowMkvmergeOptions();

protected:
  virtual void setupAttachmentsControls();
  virtual void setupControlLists();
  virtual void setupInputControls();
  virtual void setupOutputControls();
  virtual void setupMenu();

  virtual void retranslateUI();
  virtual void retranslateInputUI();
  virtual void retranslateAttachmentsUI();

  virtual QStringList selectFilesToAdd(QString const &title);
  virtual QStringList selectAttachmentsToAdd();
  virtual void addOrAppendFiles(bool append);
  virtual void enableFilesActions();
  virtual void enableAttachmentControls(bool enable);
  virtual void setInputControlValues(Track *track);
  virtual void setOutputControlValues();
  virtual void setAttachmentControlValues(Attachment *attachment);
  virtual void clearInputControlValues();
  virtual void setControlValuesFromConfig();
  virtual void withSelectedTracks(std::function<void(Track *)> code, bool notIfAppending = false, QWidget *widget = nullptr);
  virtual void withSelectedAttachments(std::function<void(Attachment *)> code);
  virtual void addOrRemoveEmptyComboBoxItem(bool add);
  virtual QString getOpenFileName(QString const &title, QString const &filter, QLineEdit *lineEdit);
  virtual QString getSaveFileName(QString const &title, QString const &filter, QLineEdit *lineEdit);

  virtual QModelIndex selectedSourceFile() const;
  virtual QList<SourceFile *> selectedSourceFiles() const;
  virtual QList<Track *> selectedTracks() const;

  virtual void addToJobQueue(bool startNow);
};

#endif // MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_MERGE_WIDGET_H
