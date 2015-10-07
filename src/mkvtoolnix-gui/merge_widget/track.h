#ifndef MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_TRACK_H
#define MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_TRACK_H

#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"

#include <QHash>
#include <QSettings>
#include <QString>

class SourceFile;
class MkvmergeOptionBuilder;

class Track;
typedef std::shared_ptr<Track> TrackPtr;

class Track {
public:
  enum Type {
    Audio = 0,
    Video,
    Subtitles,
    Buttons,
    Chapters,
    GlobalTags,
    Tags,
    Attachment,
    TypeMin = Audio,
    TypeMax = Attachment
  };

  enum Compression {
    CompDefault = 0,
    CompNone,
    CompZlib,
    CompMin = CompDefault,
    CompMax = CompZlib
  };

  QHash<QString, QString> m_properties;

  SourceFile *m_file;
  Track *m_appendedTo;
  QList<Track *> m_appendedTracks;

  Type m_type;
  int64_t m_id;

  bool m_muxThis, m_setAspectRatio, m_defaultTrackFlagWasSet, m_forcedTrackFlagWasSet, m_aacSbrWasDetected, m_nameWasPresent, m_fixBitstreamTimingInfo;
  QString m_name, m_codec, m_language, m_tags, m_delay, m_stretchBy, m_defaultDuration, m_timecodes, m_aspectRatio, m_displayWidth, m_displayHeight, m_cropping, m_characterSet, m_userDefinedOptions;
  unsigned int m_defaultTrackFlag, m_forcedTrackFlag, m_stereoscopy, m_cues, m_aacIsSBR;
  Compression m_compression;

  int64_t m_size;
  QString m_attachmentDescription;

public:
  explicit Track(SourceFile *file = nullptr, Type = Audio);
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
  virtual bool isRegular() const;
  virtual bool isAppended() const;

  virtual void setDefaults();
  virtual QString extractAudioDelayFromFileName() const;

  virtual void saveSettings(QSettings &settings) const;
  virtual void loadSettings(MuxConfig::Loader &l);
  virtual void fixAssociations(MuxConfig::Loader &l);

  virtual QString nameForType() const;

  virtual std::string debugInfo() const;

  void buildMkvmergeOptions(MkvmergeOptionBuilder &opt) const;
};

#endif  // MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_TRACK_H
