#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/merge_widget/mkvmerge_option_builder.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"
#include "mkvtoolnix-gui/merge_widget/source_file.h"

namespace {

template<typename T>
void
fixAssociationsFor(char const *group,
                   QList<std::shared_ptr<T> > &container,
                   MuxConfig::Loader &l) {
  l.settings.beginGroup(group);

  int idx = 0;
  for (auto &entry : container) {
    l.settings.beginGroup(QString::number(idx));
    entry->fixAssociations(l);
    l.settings.endGroup();
    ++idx;
  }

  l.settings.endGroup();
}

}

SourceFile::SourceFile(QString const &fileName)
  : m_properties{}
  , m_fileName{fileName}
  , m_container{}
  , m_tracks{}
  , m_additionalParts{}
  , m_appendedFiles{}
  , m_type{FILE_TYPE_IS_UNKNOWN}
  , m_appended{false}
  , m_additionalPart{false}
  , m_appendedTo{nullptr}
{

}

SourceFile::~SourceFile() {
}

bool
SourceFile::isValid()
  const {
  return !m_container.isEmpty() || m_additionalPart;
}


bool
SourceFile::isRegular()
  const {
  return !m_appended;
}

bool
SourceFile::isAppended()
  const {
  return m_appended && m_additionalPart;
}

bool
SourceFile::isAdditionalPart()
  const {
  return m_appended && !m_additionalPart;
}

bool
SourceFile::hasRegularTrack()
  const {
  return m_tracks.end() != brng::find_if(m_tracks, [](TrackPtr const &track) { return track->isRegular(); });
}

void
SourceFile::setContainer(QString const &container) {
  m_container = container;
  m_type      = container == "AAC"                          ? FILE_TYPE_AAC
              : container == "AC3"                          ? FILE_TYPE_AC3
              : container == "AVC/h.264"                    ? FILE_TYPE_AVC_ES
              : container == "AVI"                          ? FILE_TYPE_AVI
              : container == "Dirac"                        ? FILE_TYPE_DIRAC
              : container == "DTS"                          ? FILE_TYPE_DTS
              : container == "FLAC"                         ? FILE_TYPE_FLAC
              : container == "IVF (VP8)"                    ? FILE_TYPE_IVF
              : container == "Matroska"                     ? FILE_TYPE_MATROSKA
              : container == "MP2/MP3"                      ? FILE_TYPE_MP3
              : container == "MPEG video elementary stream" ? FILE_TYPE_MPEG_ES
              : container == "MPEG program stream"          ? FILE_TYPE_MPEG_PS
              : container == "MPEG transport stream"        ? FILE_TYPE_MPEG_TS
              : container == "Ogg/OGM"                      ? FILE_TYPE_OGM
              : container == "PGSSUP"                       ? FILE_TYPE_PGSSUP
              : container == "QuickTime/MP4"                ? FILE_TYPE_QTMP4
              : container == "RealMedia"                    ? FILE_TYPE_REAL
              : container == "SRT subtitles"                ? FILE_TYPE_SRT
              : container == "SSA/ASS subtitles"            ? FILE_TYPE_SSA
              : container == "TrueHD"                       ? FILE_TYPE_TRUEHD
              : container == "TTA"                          ? FILE_TYPE_TTA
              : container == "USF subtitles"                ? FILE_TYPE_USF
              : container == "VC1 elementary stream"        ? FILE_TYPE_VC1
              : container == "VobBtn"                       ? FILE_TYPE_VOBBTN
              : container == "VobSub"                       ? FILE_TYPE_VOBSUB
              : container == "WAV"                          ? FILE_TYPE_WAV
              : container == "WAVPACK"                      ? FILE_TYPE_WAVPACK4
              :                                               FILE_TYPE_IS_UNKNOWN;
}

void
SourceFile::saveSettings(QSettings &settings)
  const {
  MuxConfig::saveProperties(settings, m_properties);

  saveSettingsGroup("tracks",          m_tracks,          settings);
  saveSettingsGroup("additionalParts", m_additionalParts, settings);
  saveSettingsGroup("appendedFiles",   m_appendedFiles,   settings);

  settings.setValue("objectID",       reinterpret_cast<qlonglong>(this));
  settings.setValue("fileName",       m_fileName);
  settings.setValue("container",      m_container);
  settings.setValue("type",           m_type);
  settings.setValue("appended",       m_appended);
  settings.setValue("additionalPart", m_additionalPart);
  settings.setValue("appendedTo",     reinterpret_cast<qlonglong>(m_appendedTo));
}

void
SourceFile::loadSettings(MuxConfig::Loader &l) {
  auto objectID = l.settings.value("objectID").toLongLong();
  if ((0 >= objectID) || l.objectIDToSourceFile.contains(objectID))
    throw mtx::InvalidSettingsX{};

  l.objectIDToSourceFile[objectID] = this;
  m_fileName                       = l.settings.value("fileName").toString();
  m_container                      = l.settings.value("container").toString();
  m_type                           = static_cast<file_type_e>(l.settings.value("type").toInt());
  m_appended                       = l.settings.value("appended").toBool();
  m_additionalPart                 = l.settings.value("additionalPart").toBool();
  m_appendedTo                     = reinterpret_cast<SourceFile *>(l.settings.value("appendedTo").toLongLong());

  if ((FILE_TYPE_IS_UNKNOWN > m_type) || (FILE_TYPE_MAX < m_type))
    throw mtx::InvalidSettingsX{};

  MuxConfig::loadProperties(l.settings, m_properties);

  loadSettingsGroup<Track>     ("tracks",          m_tracks,          l, [](){ return std::make_shared<Track>(); });
  loadSettingsGroup<SourceFile>("additionalParts", m_additionalParts, l);
  loadSettingsGroup<SourceFile>("appendedFiles",   m_appendedFiles,   l);
}

void
SourceFile::fixAssociations(MuxConfig::Loader &l) {
  if (m_appended || m_additionalPart) {
    auto appendedToID = reinterpret_cast<qlonglong>(m_appendedTo);
    if ((0 >= appendedToID) || !l.objectIDToSourceFile.contains(appendedToID))
      throw mtx::InvalidSettingsX{};
    m_appendedTo = l.objectIDToSourceFile.value(appendedToID);

  } else
    m_appendedTo = nullptr;

  for (auto &track : m_tracks)
    track->m_file = this;

  fixAssociationsFor("tracks",          m_tracks,          l);
  fixAssociationsFor("additionalParts", m_additionalParts, l);
  fixAssociationsFor("appendedFiles",   m_appendedFiles,   l);
}

Track *
SourceFile::findFirstTrackOfType(Track::Type type)
  const {
  auto itr = brng::find_if(m_tracks, [type](TrackPtr const &track) { return type == track->m_type; });
  return itr != m_tracks.end() ? itr->get() : nullptr;
}

void
SourceFile::buildMkvmergeOptions(QStringList &options)
  const {
  assert(!m_additionalPart);

  auto opt = MkvmergeOptionBuilder{};

  for (auto const &track : m_tracks)
    track->buildMkvmergeOptions(opt);

  auto buildTrackIdArg = [&options,&opt](Track::Type type, QString const &enabled, QString const &disabled) {
    if (!enabled.isEmpty() && !opt.enabledTrackIds[type].isEmpty() && (static_cast<unsigned int>(opt.enabledTrackIds[type].size()) < opt.numTracksOfType[type]))
      options << enabled << opt.enabledTrackIds[type].join(Q(","));

    else if (opt.enabledTrackIds[type].isEmpty() && opt.numTracksOfType[type])
      options << disabled;
  };

  buildTrackIdArg(Track::Audio,      Q("--audio-tracks"),    Q("--no-audio"));
  buildTrackIdArg(Track::Video,      Q("--video-tracks"),    Q("--no-video"));
  buildTrackIdArg(Track::Subtitles,  Q("--subtitle-tracks"), Q("--no-subtitles"));
  buildTrackIdArg(Track::Buttons,    Q("--button-tracks"),   Q("--no-buttons"));
  buildTrackIdArg(Track::GlobalTags, Q("--attachments"),     Q("--no-attachments"));
  buildTrackIdArg(Track::Tags,       Q("--track-tags"),      Q("--no-track-tags"));
  buildTrackIdArg(Track::GlobalTags, Q(""),                  Q("--no-global-tags"));
  buildTrackIdArg(Track::Chapters,   Q(""),                  Q("--no-chapters"));

  options += opt.options;
  options << m_fileName;
}
