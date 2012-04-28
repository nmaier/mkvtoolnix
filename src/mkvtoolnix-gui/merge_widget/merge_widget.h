#ifndef MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_MERGE_WIDGET_H
#define MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_MERGE_WIDGET_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/attachment_model.h"
#include "mkvtoolnix-gui/mux_config.h"
#include "mkvtoolnix-gui/source_file_model.h"
#include "mkvtoolnix-gui/track_model.h"

#include <QComboBox>
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

  QList<QWidget *> m_audioControls, m_videoControls, m_subtitleControls, m_chapterControls, m_typeIndependantControls, m_allInputControls;
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
  virtual void onSegmentinfoEdited(QString newValue);
  virtual void onBrowseSegmentinfo();
  virtual void onDoNotSplit();
  virtual void onDoSplitAfterSize();
  virtual void onDoSplitAfterDuration();
  virtual void onDoSplitAfterTimecodes();
  virtual void onDoSplitByParts();
  virtual void onSplitSizeEdited(QString newValue);
  virtual void onSplitDurationEdited(QString newValue);
  virtual void onSplitTimecodesEdited(QString newValue);
  virtual void onSplitPartsEdited(QString newValue);
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

protected:
  virtual void setupAttachmentsControls();
  virtual void setupControlLists();
  virtual void setupInputControls();
  virtual void setupMenu();

  virtual void retranslateUI();
  virtual void retranslateInputUI();
  virtual void retranslateAttachmentsUI();

  virtual QStringList selectFilesToAdd();
  virtual QStringList selectAttachmentsToAdd();
  virtual void addFile(QString const &fileName, bool append);
  virtual void enableInputControls(QList<QWidget *> const &controls, bool enable);
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

  virtual void resizeViewColumnsToContents(QTreeView *view) const;

  virtual QModelIndex selectedSourceFile() const;
};

#endif // MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_MERGE_WIDGET_H
