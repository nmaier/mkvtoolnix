#ifndef MTX_MKVTOOLNIX_GUI_TRACK_H
#define MTX_MKVTOOLNIX_GUI_TRACK_H

#include "common/common_pch.h"

#include "common/qt.h"

#include <QHash>
#include <QObject>
#include <QSettings>
#include <QString>

class SourceFile;

class Track;
typedef std::shared_ptr<Track> TrackPtr;

class Track: public QObject {
  Q_OBJECT;
public:
  enum Type {
    Audio,
    Video,
    Subtitles,
    Buttons,
    Chapters,
    GlobalTags,
    Tags,
    Attachment,
  };

  enum Compression {
    CompDefault,
    CompNone,
    CompZlib,
    CompLzo,
    CompBz2,
  };

  QHash<QString, QString> m_properties;

  SourceFile *m_file;
  Track *m_appendedTo;
  QList<Track *> m_appendedTracks;

  Type m_type;
  int64_t m_id;

  bool m_muxThis, m_setAspectRatio, m_defaultTrackFlagWasSet;
  QString m_name, m_codec, m_language, m_tags, m_delay, m_stretchBy, m_defaultDuration, m_timecodes, m_aspectRatio, m_displayWidth, m_displayHeight, m_cropping, m_characterSet, m_userDefinedOptions;
  unsigned int m_defaultTrackFlag, m_forcedTrackFlag, m_stereoscopy, m_cues, m_aacIsSBR;
  Compression m_compression;

  int64_t m_size;
  QString m_attachmentDescription;

public:
  explicit Track(Type type, SourceFile *file);
  virtual ~Track();

  virtual bool isType(Type type) const;
  virtual bool isAudio() const;
  virtual bool isVideo() const;
  virtual bool isSubtitles() const;
  virtual bool isButtons() const;
  virtual bool isChapters() const;
  virtual bool isGlobalTags() const;
  virtual bool isTags() const;
  virtual bool isAttachment() const;
  virtual bool isAppended() const;

  virtual void setDefaults();
  virtual QString extractAudioDelayFromFileName() const;

  virtual void saveSettings(QSettings &settings) const;
};

#endif  // MTX_MKVTOOLNIX_GUI_TRACK_H
