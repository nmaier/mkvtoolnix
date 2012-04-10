#include "common/common_pch.h"

#include "common/iso639.h"
#include "mkvtoolnix-gui/source_file.h"
#include "mkvtoolnix-gui/track.h"
#include "mkvtoolnix-gui/util/settings.h"

Track::Track(Track::Type type)
  : m_file(nullptr)
  , m_type(type)
  , m_id(-1)
  , m_muxThis(true)
  , m_setAspectRatio(true)
  , m_aacIsSBR(false)
  , m_defaultTrackFlagWasSet(false)
  , m_defaultTrackFlag(0)
  , m_forcedTrackFlag(0)
  , m_stereoscopy(0)
  , m_cues(0)
  , m_compression(CompDefault)
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
  return nullptr == m_file ? false : m_file->m_appended;
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
  // m_stereoscopy            = mapToStereoMode(m_properties[Q("stereo_mode")];

  auto idx = map_to_iso639_2_code(to_utf8(m_language), true);
  if (0 <= idx)
    m_language = to_qs(iso639_languages[idx].iso639_2_code);

  QRegExp re_displayDimensions{"^(\\d+)x(\\d+)$"};
  if (-1 != re_displayDimensions.indexIn(m_properties[Q("display_dimensions")])) {
    m_displayWidth  = re_displayDimensions.cap(1);
    m_displayHeight = re_displayDimensions.cap(1);
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
