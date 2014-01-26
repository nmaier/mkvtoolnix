#ifndef MTX_MKVTOOLNIX_GUI_MUX_CONFIG_H
#define MTX_MKVTOOLNIX_GUI_MUX_CONFIG_H

#include "common/common_pch.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

#define MTXCFG_VERSION 1

namespace mtx {

class InvalidSettingsX: public exception {
public:
  virtual const char *what() const throw() {
    return "invalid settings in file";
  }
};

}

class Attachment;
typedef std::shared_ptr<Attachment> AttachmentPtr;

class Track;
typedef std::shared_ptr<Track> TrackPtr;

class SourceFile;
typedef std::shared_ptr<SourceFile> SourceFilePtr;

class MuxConfig;
typedef std::shared_ptr<MuxConfig> MuxConfigPtr;

class MuxConfig : public QObject {
  Q_OBJECT;

public:
  struct Loader {
    QSettings &settings;
    QHash<qlonglong, SourceFile *> &objectIDToSourceFile;
    QHash<qlonglong, Track *> &objectIDToTrack;

    Loader(QSettings &p_settings,
           QHash<qlonglong, SourceFile *> &p_objectIDToSourceFile,
           QHash<qlonglong, Track *> &p_objectIDToTrack)
      : settings(p_settings)
      , objectIDToSourceFile(p_objectIDToSourceFile)
      , objectIDToTrack(p_objectIDToTrack)
    {
    }
  };

  enum SplitMode {
    DoNotSplit = 0,
    SplitAfterSize,
    SplitAfterDuration,
    SplitAfterTimecodes,
    SplitByParts,
    SplitByPartsFrames,
    SplitByFrames,
    SplitAfterChapters,
  };

public:
  QString m_configFileName;

  QList<SourceFilePtr> m_files;
  QList<Track *> m_tracks;
  QList<AttachmentPtr> m_attachments;

  QString m_title, m_destination, m_globalTags, m_segmentInfo, m_splitOptions;
  QString m_segmentUIDs, m_previousSegmentUID, m_nextSegmentUID, m_chapters, m_chapterLanguage, m_chapterCharacterSet, m_chapterCueNameFormat, m_userDefinedOptions;
  SplitMode m_splitMode;
  unsigned int m_splitMaxFiles;
  bool m_linkFiles, m_webmMode, m_titleWasPresent;

public:
  MuxConfig(QString const &fileName = QString{""});
  MuxConfig(MuxConfig const &other);
  virtual ~MuxConfig();
  MuxConfig &operator =(MuxConfig const &other);

  virtual void load(QSettings &settings);
  virtual void load(QString const &fileName = QString{""});
  virtual void save(QSettings &settings) const;
  virtual void save(QString const &fileName = QString{""});
  virtual void reset();

  QStringList buildMkvmergeOptions() const;

protected:
  QHash<SourceFile *, unsigned int> buildFileNumbers() const;
  QStringList buildTrackOrder(QHash<SourceFile *, unsigned int> const &fileNumbers) const;
  QStringList buildAppendToMapping(QHash<SourceFile *, unsigned int> const &fileNumbers) const;

public:
  static MuxConfigPtr loadSettings(QString const &fileName);
  static void saveProperties(QSettings &settings, QHash<QString, QString> const &properties);
  static void loadProperties(QSettings &settings, QHash<QString, QString> &properties);
};

template<typename T>
void
loadSettingsGroup(char const *group,
                  QList<std::shared_ptr<T> > &container,
                  MuxConfig::Loader &l,
                  std::function<std::shared_ptr<T>()> create = []() { return std::make_shared<T>(); }) {
  l.settings.beginGroup(group);

  int numberOfEntries = std::max(l.settings.value("numberOfEntries").toInt(), 0);
  for (int idx = 0; idx < numberOfEntries; ++idx) {
    container << create();
    l.settings.beginGroup(QString::number(idx));
    container.back()->loadSettings(l);
    l.settings.endGroup();
  }

  l.settings.endGroup();
}

template<typename T>
void
saveSettingsGroup(char const *group,
                  QList<std::shared_ptr<T> > const &container,
                  QSettings &settings) {
  settings.beginGroup(group);

  int numberOfEntries = container.size();
  settings.setValue("numberOfEntries", numberOfEntries);

  for (int idx = 0; idx < numberOfEntries; ++idx) {
    settings.beginGroup(QString::number(idx));
    container.at(idx)->saveSettings(settings);
    settings.endGroup();
  }

  settings.endGroup();
}

#endif  // MTX_MKVTOOLNIX_GUI_MUX_CONFIG_H
