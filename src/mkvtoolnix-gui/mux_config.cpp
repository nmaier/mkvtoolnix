#include "common/common_pch.h"

#include "mkvtoolnix-gui/mux_config.h"

#include <QFile>
#include <QStringList>

MuxConfig::MuxConfig()
  : QObject{}
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
  // m_attachments          = other.m_attachments;
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
MuxConfig::load(QSettings const &) {
  // TODO
}

void
MuxConfig::saveProperties(QSettings &settings,
                          QHash<QString, QString> const &properties) {
  QStringList propertyKeys;
  for (auto key_value = properties.begin() ; properties.end() != key_value ; ++key_value)
    propertyKeys << key_value.key();
  settings.setValue("propertyKeys", propertyKeys.join("\t"));

  settings.beginGroup("properties");
  for (auto key_value = properties.begin() ; properties.end() != key_value ; ++key_value)
    settings.setValue(key_value.key(), key_value.value());
  settings.endGroup();
}

void
MuxConfig::save(QSettings &settings)
  const {
  settings.beginGroup("files");
  settings.setValue("numberOfFiles", m_files.size());

  int idx = 0;
  for (auto &file : m_files) {
    settings.beginGroup(QString::number(idx));
    file->saveSettings(settings);
    settings.endGroup();
    ++idx;
  }

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
MuxConfig::load(QString const &fileName) {
  auto config = std::make_shared<MuxConfig>();
  config->load(QSettings{fileName, QSettings::IniFormat});

  return config;
}
