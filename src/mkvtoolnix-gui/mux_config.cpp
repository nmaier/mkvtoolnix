#include "common/common_pch.h"

#include "mkvtoolnix-gui/attachment.h"
#include "mkvtoolnix-gui/mux_config.h"
#include "mkvtoolnix-gui/source_file.h"
#include "mkvtoolnix-gui/track.h"

#include <QFile>
#include <QStringList>

MuxConfig::MuxConfig(QString const &fileName)
  : QObject{}
  , m_configFileName{fileName}
  , m_splitMode{DoNotSplit}
  , m_splitMaxFiles{0}
  , m_linkFiles{false}
  , m_webmMode{false}
{
}

MuxConfig::~MuxConfig() {
}

MuxConfig::MuxConfig(MuxConfig const &other)
  : QObject{}
{
  *this = other;
}

MuxConfig &
MuxConfig::operator =(MuxConfig const &other) {
  if (&other == this)
    return *this;

  m_configFileName       = other.m_configFileName;
  m_files                = other.m_files;
  m_tracks               = other.m_tracks;
  m_attachments          = other.m_attachments;
  m_title                = other.m_title;
  m_destination          = other.m_destination;
  m_globalTags           = other.m_globalTags;
  m_segmentinfo          = other.m_segmentinfo;
  m_splitAfterSize       = other.m_splitAfterSize;
  m_splitAfterDuration   = other.m_splitAfterDuration;
  m_splitAfterTimecodes  = other.m_splitAfterTimecodes;
  m_splitByParts         = other.m_splitByParts;
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

  // Need to give different argument for last parameter even though
  // it's the same as the default argument due to a bug in gcc 4.6.1.
  loadSettingsGroup<SourceFile>("files",       m_files,       l, []() { return std::make_shared<SourceFile>(); });
  loadSettingsGroup<Attachment>("attachments", m_attachments, l);

  for (auto &file : m_files)
    file->fixAssociations(l);

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
  m_segmentinfo          = settings.value("segmentinfo").toString();
  m_splitAfterSize       = settings.value("splitAfterSize").toString();
  m_splitAfterDuration   = settings.value("splitAfterDuration").toString();
  m_splitAfterTimecodes  = settings.value("splitAfterTimecodes").toString();
  m_splitByParts         = settings.value("splitByParts").toString();
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
  settings.setValue("segmentinfo",          m_segmentinfo);
  settings.setValue("splitAfterSize",       m_splitAfterSize);
  settings.setValue("splitAfterDuration",   m_splitAfterDuration);
  settings.setValue("splitAfterTimecodes",  m_splitAfterTimecodes);
  settings.setValue("splitByParts",         m_splitByParts);
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
