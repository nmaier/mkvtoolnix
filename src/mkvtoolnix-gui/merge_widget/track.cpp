#include "common/common_pch.h"

#include "common/iso639.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"
#include "mkvtoolnix-gui/merge_widget/source_file.h"
#include "mkvtoolnix-gui/merge_widget/track.h"
#include "mkvtoolnix-gui/util/settings.h"

#include <QVariant>

Track::Track(SourceFile *file,
             Track::Type type)
  : m_properties{}
  , m_file{file}
  , m_appendedTo{nullptr}
  , m_appendedTracks{}
  , m_type{type}
  , m_id{-1}
  , m_muxThis{true}
  , m_setAspectRatio{true}
  , m_defaultTrackFlagWasSet{false}
  , m_defaultTrackFlag{0}
  , m_forcedTrackFlag{0}
  , m_stereoscopy{0}
  , m_cues{0}
  , m_aacIsSBR{0}
  , m_compression{CompDefault}
  , m_size{0}
{
}

Track::~Track() {
}

bool
Track::isType(Type type)
  const {
  return type == m_type;
}

bool
Track::isAudio()
  const {
  return Audio == m_type;
}

bool
Track::isVideo()
  const {
  return Video == m_type;
}

bool
Track::isSubtitles()
  const {
  return Subtitles == m_type;
}

bool
Track::isButtons()
  const {
  return Buttons == m_type;
}

bool
Track::isChapters()
  const {
  return Chapters == m_type;
}

bool
Track::isGlobalTags()
  const {
  return GlobalTags == m_type;
}

bool
Track::isTags()
  const {
  return Tags == m_type;
}

bool
Track::isAttachment()
  const {
  return Attachment == m_type;
}

bool
Track::isAppended()
  const {
  return !m_file ? false : m_file->m_appended;
}

bool
Track::isRegular()
  const {
  return isAudio() || isVideo() || isSubtitles() || isButtons();
}

void
Track::setDefaults() {
  auto &settings = Settings::get();

  if (isAudio() && settings.m_setAudioDelayFromFileName)
    m_delay = extractAudioDelayFromFileName();

  if (settings.m_disableAVCompression && (isVideo() || isAudio()))
    m_compression = CompNone;

  m_forcedTrackFlag        = m_properties[Q("forced_track")] == "1";
  m_defaultTrackFlagWasSet = m_properties[Q("default_track")] == "1";
  m_name                   = m_properties[Q("track_name")];
  m_cropping               = m_properties[Q("cropping")];
  if (!m_properties[Q("stereo_mode")].isEmpty())
    m_stereoscopy = m_properties[Q("stereo_mode")].toUInt() + 1;

  auto idx = map_to_iso639_2_code(to_utf8(m_properties[Q("language")]), true);
  if (0 <= idx)
    m_language = to_qs(iso639_languages[idx].iso639_2_code);

  QRegExp re_displayDimensions{"^(\\d+)x(\\d+)$"};
  if (-1 != re_displayDimensions.indexIn(m_properties[Q("display_dimensions")])) {
    m_displayWidth  = re_displayDimensions.cap(1);
    m_displayHeight = re_displayDimensions.cap(2);
  }
}

QString
Track::extractAudioDelayFromFileName()
  const {
  QRegExp re{"delay\\s+(-?\\d+)", Qt::CaseInsensitive};
  if (-1 == re.indexIn(m_file->m_fileName))
    return "";
  return re.cap(1);
}

void
Track::saveSettings(QSettings &settings)
  const {
  MuxConfig::saveProperties(settings, m_properties);

  QStringList appendedTracks;
  for (auto &track : m_appendedTracks)
    appendedTracks << QString::number(reinterpret_cast<qlonglong>(track));

  settings.setValue("objectID",               reinterpret_cast<qlonglong>(this));
  settings.setValue("appendedTo",             reinterpret_cast<qlonglong>(m_appendedTo));
  settings.setValue("appendedTracks",         appendedTracks);
  settings.setValue("type",                   m_type);
  settings.setValue("id",                     static_cast<qlonglong>(m_id));
  settings.setValue("muxThis",                m_muxThis);
  settings.setValue("setAspectRatio",         m_setAspectRatio);
  settings.setValue("aacIsSBR",               m_aacIsSBR);
  settings.setValue("defaultTrackFlagWasSet", m_defaultTrackFlagWasSet);
  settings.setValue("name",                   m_name);
  settings.setValue("codec",                  m_codec);
  settings.setValue("language",               m_language);
  settings.setValue("tags",                   m_tags);
  settings.setValue("delay",                  m_delay);
  settings.setValue("stretchBy",              m_stretchBy);
  settings.setValue("defaultDuration",        m_defaultDuration);
  settings.setValue("timecodes",              m_timecodes);
  settings.setValue("aspectRatio",            m_aspectRatio);
  settings.setValue("displayWidth",           m_displayWidth);
  settings.setValue("displayHeight",          m_displayHeight);
  settings.setValue("cropping",               m_cropping);
  settings.setValue("characterSet",           m_characterSet);
  settings.setValue("userDefinedOptions",     m_userDefinedOptions);
  settings.setValue("defaultTrackFlag",       m_defaultTrackFlag);
  settings.setValue("forcedTrackFlag",        m_forcedTrackFlag);
  settings.setValue("stereoscopy",            m_stereoscopy);
  settings.setValue("cues",                   m_cues);
  settings.setValue("compression",            m_compression);
  settings.setValue("size",                   static_cast<qlonglong>(m_size));
  settings.setValue("attachmentDescription",  m_attachmentDescription);
}

void
Track::loadSettings(MuxConfig::Loader &l) {
  MuxConfig::loadProperties(l.settings, m_properties);

  auto objectID = l.settings.value("objectID").toLongLong();
  if ((0 >= objectID) || l.objectIDToTrack.contains(objectID))
    throw mtx::InvalidSettingsX{};

  l.objectIDToTrack[objectID] = this;
  m_type                      = static_cast<Type>(l.settings.value("type").toInt());
  m_id                        = l.settings.value("id").toLongLong();
  m_muxThis                   = l.settings.value("muxThis").toBool();
  m_setAspectRatio            = l.settings.value("setAspectRatio").toBool();
  m_defaultTrackFlagWasSet    = l.settings.value("defaultTrackFlagWasSet").toBool();
  m_name                      = l.settings.value("name").toString();
  m_codec                     = l.settings.value("codec").toString();
  m_language                  = l.settings.value("language").toString();
  m_tags                      = l.settings.value("tags").toString();
  m_delay                     = l.settings.value("delay").toString();
  m_stretchBy                 = l.settings.value("stretchBy").toString();
  m_defaultDuration           = l.settings.value("defaultDuration").toString();
  m_timecodes                 = l.settings.value("timecodes").toString();
  m_aspectRatio               = l.settings.value("aspectRatio").toString();
  m_displayWidth              = l.settings.value("displayWidth").toString();
  m_displayHeight             = l.settings.value("displayHeight").toString();
  m_cropping                  = l.settings.value("cropping").toString();
  m_characterSet              = l.settings.value("characterSet").toString();
  m_userDefinedOptions        = l.settings.value("userDefinedOptions").toString();
  m_defaultTrackFlag          = l.settings.value("defaultTrackFlag").toInt();
  m_forcedTrackFlag           = l.settings.value("forcedTrackFlag").toInt();
  m_stereoscopy               = l.settings.value("stereoscopy").toInt();
  m_cues                      = l.settings.value("cues").toInt();
  m_aacIsSBR                  = l.settings.value("aacIsSBR").toInt();
  m_compression               = static_cast<Compression>(l.settings.value("compression").toInt());
  m_size                      = l.settings.value("size").toLongLong();
  m_attachmentDescription     = l.settings.value("attachmentDescription").toString();

  if (   (TypeMin > m_type)        || (TypeMax < m_type)
      || (CompMin > m_compression) || (CompMax < m_compression))
    throw mtx::InvalidSettingsX{};
}

void
Track::fixAssociations(MuxConfig::Loader &l) {
  if (isAppended()) {
    auto appendedToID = l.settings.value("appendedTo").toLongLong();
    if ((0 >= appendedToID) || !l.objectIDToTrack.contains(appendedToID))
      throw mtx::InvalidSettingsX{};
    m_appendedTo = l.objectIDToTrack.value(appendedToID);
  }

  m_appendedTracks.clear();
  for (auto &appendedTrackID : l.settings.value("appendedTracks").toStringList()) {
    if (!l.objectIDToTrack.contains(appendedTrackID.toLongLong()))
      throw mtx::InvalidSettingsX{};
    m_appendedTracks << l.objectIDToTrack.value(appendedTrackID.toLongLong());
  }
}

std::string
Track::debugInfo()
  const {
  return (boost::format("%1%/%2%:%3%@%4%") % static_cast<unsigned int>(m_type) % m_id % m_codec % this).str();
}
