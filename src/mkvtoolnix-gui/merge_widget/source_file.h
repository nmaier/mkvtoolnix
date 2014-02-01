#ifndef MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_SOURCE_FILE_H
#define MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_SOURCE_FILE_H

#include "common/common_pch.h"

#include "common/file_types.h"
#include "common/qt.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"
#include "mkvtoolnix-gui/merge_widget/track.h"

#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>

class SourceFile;
typedef std::shared_ptr<SourceFile> SourceFilePtr;

class SourceFile: public QObject {
  Q_OBJECT;
public:
  QHash<QString, QString> m_properties;
  QString m_fileName, m_container;
  QList<TrackPtr> m_tracks;
  QList<SourceFilePtr> m_additionalParts, m_appendedFiles;
  QList<QFileInfo> m_playlistFiles;

  file_type_e m_type;
  bool m_appended, m_additionalPart, m_isPlaylist;
  SourceFile *m_appendedTo;

  uint64_t m_playlistDuration, m_playlistSize, m_playlistChapters;

public:
  explicit SourceFile(QString const &fileName = QString{""});
  virtual ~SourceFile();

  virtual void setContainer(QString const &container);
  virtual bool isValid() const;
  virtual bool isRegular() const;
  virtual bool isAppended() const;
  virtual bool isAdditionalPart() const;
  virtual bool isPlaylist() const;
  virtual bool hasRegularTrack() const;

  virtual void saveSettings(QSettings &settings) const;
  virtual void loadSettings(MuxConfig::Loader &l);
  virtual void fixAssociations(MuxConfig::Loader &l);

  virtual Track *findFirstTrackOfType(Track::Type type) const;

  void buildMkvmergeOptions(QStringList &options) const;
};

#endif  // MTX_MKVTOOLNIX_GUI_SOURCE_FILE_H
