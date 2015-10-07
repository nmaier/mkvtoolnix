#include "common/common_pch.h"

#include "common/strings/editing.h"
#include "mkvtoolnix-gui/merge_widget/attachment.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"
#include "mkvtoolnix-gui/merge_widget/source_file.h"
#include "mkvtoolnix-gui/merge_widget/track.h"

#include <QFile>
#include <QStringList>

static void
addToMaps(SourceFile *oldFile,
          SourceFile *newFile,
          QHash<SourceFile *, SourceFile *> &fileMap,
          QHash<Track *, Track *> &trackMap) {
  assert(!!oldFile && !!newFile);

  fileMap[oldFile] = newFile;

  for (auto idx = 0, end = oldFile->m_tracks.size(); idx < end; ++idx)
    trackMap[ oldFile->m_tracks[idx].get() ] = newFile->m_tracks[idx].get();

  for (auto idx = 0, end = oldFile->m_additionalParts.size(); idx < end; ++idx)
    addToMaps(oldFile->m_additionalParts[idx].get(), newFile->m_additionalParts[idx].get(), fileMap, trackMap);

  for (auto idx = 0, end = oldFile->m_appendedFiles.size(); idx < end; ++idx)
    addToMaps(oldFile->m_appendedFiles[idx].get(), newFile->m_appendedFiles[idx].get(), fileMap, trackMap);
}

static void
fixMappings(SourceFile *oldFile,
            QHash<SourceFile *, SourceFile *> &fileMap,
            QHash<Track *, Track *> &trackMap) {
  auto newFile = fileMap[oldFile];

  assert(!!oldFile && !!newFile);

  for (auto idx = 0, end = oldFile->m_tracks.size(); idx < end; ++idx) {
    auto oldTrack = oldFile->m_tracks[idx].get();
    auto newTrack = trackMap[oldTrack];

    assert(!!oldTrack && !!newTrack);

    newTrack->m_file       = fileMap[oldTrack->m_file];
    newTrack->m_appendedTo = trackMap[oldTrack->m_appendedTo];

    assert((!!newTrack->m_file == !!oldTrack->m_file) && (!!newTrack->m_appendedTo == !!newTrack->m_appendedTo));

    newTrack->m_appendedTracks.empty();
    for (auto const &oldAppendedTrack : oldTrack->m_appendedTracks) {
      auto newAppendedTrack = trackMap[oldAppendedTrack];
      assert(!!newAppendedTrack);
      newTrack->m_appendedTracks << newAppendedTrack;
    }
  }

  for (auto const &oldAppendedFile : oldFile->m_appendedFiles)
    fixMappings(oldAppendedFile.get(), fileMap, trackMap);
}

MuxConfig::MuxConfig(QString const &fileName)
  : m_configFileName{fileName}
  , m_splitMode{DoNotSplit}
  , m_splitMaxFiles{0}
  , m_linkFiles{false}
  , m_webmMode{false}
  , m_titleWasPresent{}
{
}

MuxConfig::~MuxConfig() {
}

MuxConfig::MuxConfig(MuxConfig const &other)
{
  *this = other;
}

MuxConfig &
MuxConfig::operator =(MuxConfig const &other) {
  if (&other == this)
    return *this;

  m_configFileName       = other.m_configFileName;
  m_title                = other.m_title;
  m_destination          = other.m_destination;
  m_globalTags           = other.m_globalTags;
  m_segmentInfo          = other.m_segmentInfo;
  m_splitOptions         = other.m_splitOptions;
  m_segmentUIDs          = other.m_segmentUIDs;
  m_previousSegmentUID   = other.m_previousSegmentUID;
  m_nextSegmentUID       = other.m_nextSegmentUID;
  m_chapters             = other.m_chapters;
  m_chapterLanguage      = other.m_chapterLanguage;
  m_chapterCharacterSet  = other.m_chapterCharacterSet;
  m_chapterCueNameFormat = other.m_chapterCueNameFormat;
  m_userDefinedOptions   = other.m_userDefinedOptions;
  m_splitMode            = other.m_splitMode;
  m_splitMaxFiles        = other.m_splitMaxFiles;
  m_linkFiles            = other.m_linkFiles;
  m_webmMode             = other.m_webmMode;

  m_files.empty();
  m_tracks.empty();
  m_attachments.empty();

  for (auto const &attachment : other.m_attachments)
    m_attachments << std::make_shared<Attachment>(*attachment);

  auto fileMap  = QHash<SourceFile *, SourceFile *>{};
  auto trackMap = QHash<Track *, Track *>{};

  for (auto const &oldFile : other.m_files) {
    auto newFile = std::make_shared<SourceFile>(*oldFile);
    m_files << newFile;

    addToMaps(oldFile.get(), newFile.get(), fileMap, trackMap);
  }

  for (auto const &oldFile : other.m_files)
    fixMappings(oldFile.get(), fileMap, trackMap);

  for (auto const &oldTrack : other.m_tracks) {
    auto newTrack = trackMap[oldTrack];
    assert(!!newTrack);
    m_tracks << newTrack;
  }

  return *this;
}

void
MuxConfig::loadProperties(QSettings &settings,
                          QHash<QString, QString> &properties) {
  properties.clear();

  settings.beginGroup("properties");
  for (auto &key : settings.childKeys())
    properties[key] = settings.value(key).toString();
  settings.endGroup();
}

void
MuxConfig::load(QString const &fileName) {
  reset();

  if (!fileName.isEmpty())
    m_configFileName = fileName;

  if (m_configFileName.isEmpty())
    throw mtx::InvalidSettingsX{};

  QSettings settings{m_configFileName, QSettings::IniFormat};
  load(settings);
}

void
MuxConfig::load(QSettings &settings) {
  reset();

  // Check supported config file version
  if (settings.value("version", std::numeric_limits<int>::max()).toInt() > MTXCFG_VERSION)
    throw mtx::InvalidSettingsX{};

  settings.beginGroup("input");

  QHash<qlonglong, SourceFile *> objectIDToSourceFile;
  QHash<qlonglong, Track *> objectIDToTrack;
  Loader l{settings, objectIDToSourceFile, objectIDToTrack};

  loadSettingsGroup<SourceFile>("files",       m_files,       l, [](){ return std::make_shared<SourceFile>(); });
  loadSettingsGroup<Attachment>("attachments", m_attachments, l);

  settings.beginGroup("files");
  auto idx = 0u;
  for (auto &file : m_files) {
    settings.beginGroup(QString::number(idx++));
    file->fixAssociations(l);
    settings.endGroup();
  }
  settings.endGroup();

  // Load track list.
  for (auto trackID : settings.value("trackOrder").toList()) {
    if (!objectIDToTrack.contains(trackID.toLongLong()))
      throw mtx::InvalidSettingsX{};
    m_tracks << objectIDToTrack.value(trackID.toLongLong());
  }
  settings.endGroup();

  // Load global settings
  settings.beginGroup("global");
  m_title                = settings.value("title").toString();
  m_destination          = settings.value("destination").toString();
  m_globalTags           = settings.value("globalTags").toString();
  m_segmentInfo          = settings.value("segmentInfo").toString();
  m_splitOptions         = settings.value("splitOptions").toString();
  m_segmentUIDs          = settings.value("segmentUIDs").toString();
  m_previousSegmentUID   = settings.value("previousSegmentUID").toString();
  m_nextSegmentUID       = settings.value("nextSegmentUID").toString();
  m_chapters             = settings.value("chapters").toString();
  m_chapterLanguage      = settings.value("chapterLanguage").toString();
  m_chapterCharacterSet  = settings.value("chapterCharacterSet").toString();
  m_chapterCueNameFormat = settings.value("chapterCueNameFormat").toString();
  m_userDefinedOptions   = settings.value("userDefinedOptions").toString();
  m_splitMode            = static_cast<SplitMode>(settings.value("splitMode").toInt());
  m_splitMaxFiles        = settings.value("splitMaxFiles").toInt();
  m_linkFiles            = settings.value("linkFiles").toBool();
  m_webmMode             = settings.value("webmMode").toBool();
  settings.endGroup();
}

void
MuxConfig::saveProperties(QSettings &settings,
                          QHash<QString, QString> const &properties) {
  QStringList keys{ properties.keys() };
  keys.sort();
  settings.beginGroup("properties");
  for (auto &key : keys)
    settings.setValue(key, properties.value(key));
  settings.endGroup();
}

void
MuxConfig::save(QSettings &settings)
  const {
  settings.setValue("version", MTXCFG_VERSION);

  settings.beginGroup("input");
  saveSettingsGroup("files",       m_files,       settings);
  saveSettingsGroup("attachments", m_attachments, settings);

  settings.setValue("trackOrder", std::accumulate(m_tracks.begin(), m_tracks.end(), QList<QVariant>{}, [](QList<QVariant> &accu, Track *track) { accu << QVariant{reinterpret_cast<qlonglong>(track)}; return accu; }));
  settings.endGroup();

  settings.beginGroup("global");
  settings.setValue("title",                m_title);
  settings.setValue("destination",          m_destination);
  settings.setValue("globalTags",           m_globalTags);
  settings.setValue("segmentInfo",          m_segmentInfo);
  settings.setValue("splitOptions",         m_splitOptions);
  settings.setValue("segmentUIDs",          m_segmentUIDs);
  settings.setValue("previousSegmentUID",   m_previousSegmentUID);
  settings.setValue("nextSegmentUID",       m_nextSegmentUID);
  settings.setValue("chapters",             m_chapters);
  settings.setValue("chapterLanguage",      m_chapterLanguage);
  settings.setValue("chapterCharacterSet",  m_chapterCharacterSet);
  settings.setValue("chapterCueNameFormat", m_chapterCueNameFormat);
  settings.setValue("userDefinedOptions",   m_userDefinedOptions);
  settings.setValue("splitMode",            m_splitMode);
  settings.setValue("splitMaxFiles",        m_splitMaxFiles);
  settings.setValue("linkFiles",            m_linkFiles);
  settings.setValue("webmMode",             m_webmMode);
  settings.endGroup();
}

void
MuxConfig::save(QString const &fileName) {
  if (!fileName.isEmpty())
    m_configFileName = fileName;
  if (m_configFileName.isEmpty())
    return;

  QFile::remove(m_configFileName);
  QSettings settings{m_configFileName, QSettings::IniFormat};
  save(settings);
}

void
MuxConfig::reset() {
  *this = MuxConfig{};
}

MuxConfigPtr
MuxConfig::loadSettings(QString const &fileName) {
  auto config = std::make_shared<MuxConfig>(fileName);
  config->load();

  return config;
}

QHash<SourceFile *, unsigned int>
MuxConfig::buildFileNumbers()
  const {
  auto fileNumbers = QHash<SourceFile *, unsigned int>{};
  auto number      = 0u;
  for (auto const &file : m_files) {
    fileNumbers[file.get()] = number++;
    for (auto const &appendedFile : file->m_appendedFiles)
      fileNumbers[appendedFile.get()] = number++;
  }

  return fileNumbers;
}

QStringList
MuxConfig::buildTrackOrder(QHash<SourceFile *, unsigned int> const &fileNumbers)
  const {
  auto trackOrder = QStringList{};
  for (auto const &track : m_tracks)
    if (track->m_muxThis && (track->isAudio() || track->isVideo() || track->isSubtitles() || track->isButtons()))
      trackOrder << Q("%1:%2").arg(fileNumbers.value(track->m_file)).arg(track->m_id);

  if (trackOrder.size() > 1)
    return QStringList{} << Q("--track-order") << trackOrder.join(Q(","));
  return QStringList{};
}

QStringList
MuxConfig::buildAppendToMapping(QHash<SourceFile *, unsigned int> const &fileNumbers)
  const {
  auto appendToMapping = QStringList{};
  for (auto const &destinationTrack : m_tracks) {
    for (auto const &sourceTrack : destinationTrack->m_appendedTracks)
      if (sourceTrack->m_muxThis && (sourceTrack->isAudio() || sourceTrack->isVideo() || sourceTrack->isSubtitles() || sourceTrack->isButtons()))
        appendToMapping << Q("%1:%2:%3:%4").arg(fileNumbers.value(sourceTrack->m_file)).arg(sourceTrack->m_id).arg(fileNumbers.value(destinationTrack->m_file)).arg(destinationTrack->m_id);
  }

  return appendToMapping.isEmpty() ? QStringList{} : QStringList{} << Q("--append-to") << appendToMapping.join(Q(","));
}

QStringList
MuxConfig::buildMkvmergeOptions()
  const {
  auto options = QStringList{};

  // TODO: buildMkvmergeOptions get ui locale from prefs
  // if (!preferences.m_uiLocale.isEmpty())
  //   options << Q("--ui-language") << preferences.m_uiLocale;

  options << Q("--output") << m_destination;

  if (m_webmMode)
    options << Q("--webm");

  for (auto const &file : m_files)
    file->buildMkvmergeOptions(options);

  for (auto const &attachment : m_attachments)
    attachment->buildMkvmergeOptions(options);

  if (DoNotSplit != m_splitMode) {
    auto mode = SplitAfterSize      == m_splitMode ? Q("size:")
              : SplitAfterDuration  == m_splitMode ? Q("duration:")
              : SplitAfterTimecodes == m_splitMode ? Q("timecodes:")
              : SplitByParts        == m_splitMode ? Q("parts:")
              : SplitByPartsFrames  == m_splitMode ? Q("parts-frames:")
              : SplitByFrames       == m_splitMode ? Q("frames:")
              : SplitAfterChapters  == m_splitMode ? Q("chapters:")
              :                                      Q("PROGRAM EROR");
    options << Q("--split") << (mode + m_splitOptions);

    if (m_splitMaxFiles)
      options << Q("--split-max-files") << QString::number(m_splitMaxFiles);
    if (m_linkFiles)
      options << Q("--link");
  }

  auto add = [&](QString const &arg, QString const &value, boost::logic::tribool predicate = boost::indeterminate) {
    if (boost::logic::indeterminate(predicate))
      predicate = !value.isEmpty();
    if (predicate)
      options << arg << value;
  };

  add(Q("--title"), m_title, m_titleWasPresent || !m_title.isEmpty());
  add(Q("--segment-uid"), m_segmentUIDs);
  add(Q("--previous-segment-uid"), m_previousSegmentUID);
  add(Q("--next-segment-uid"), m_nextSegmentUID);
  add(Q("--segmentinfo"), m_segmentInfo);

  if (!m_chapters.isEmpty()) {
    add(Q("--chapter-language"), m_chapterLanguage);
    add(Q("--chapter-charset"), m_chapterCharacterSet);
    add(Q("--cue-chapter-name-format"), m_chapterCueNameFormat);
    options << Q("--chapters") << m_chapters;
  }

  add(Q("--global-tags"), m_globalTags);

  auto userDefinedOptions = Q(strip_copy(to_utf8(m_userDefinedOptions)));
  if (!userDefinedOptions.isEmpty())
    options += userDefinedOptions.split(QRegExp{" +"});

  auto fileNumbers  = buildFileNumbers();
  options          += buildTrackOrder(fileNumbers);
  options          += buildAppendToMapping(fileNumbers);

  return options;
}
