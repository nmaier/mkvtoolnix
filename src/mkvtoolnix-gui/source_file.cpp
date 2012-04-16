#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/mux_config.h"
#include "mkvtoolnix-gui/source_file.h"

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

  settings.beginGroup("tracks");
  settings.setValue("numberOfTracks", m_tracks.size());

  int idx = 0;
  for (auto &track : m_tracks) {
    settings.beginGroup(QString::number(idx));
    track->saveSettings(settings);
    settings.endGroup();
    ++idx;
  }

  settings.endGroup();

  settings.beginGroup("additionalParts");
  settings.setValue("numberOfAdditionalParts", m_additionalParts.size());

  idx = 0;
  for (auto &additionalPart : m_additionalParts) {
    settings.beginGroup(QString::number(idx));
    additionalPart->saveSettings(settings);
    settings.endGroup();
    ++idx;
  }

  settings.endGroup();

  settings.beginGroup("appendedFiles");
  settings.setValue("numberOfAppendedFiles", m_appendedFiles.size());

  idx = 0;
  for (auto &appendedFile : m_appendedFiles) {
    settings.beginGroup(QString::number(idx));
    appendedFile->saveSettings(settings);
    settings.endGroup();
    ++idx;
  }

  settings.endGroup();

  settings.setValue("objectID",       static_cast<void const *>(this));
  settings.setValue("fileName",       m_fileName);
  settings.setValue("container",      m_container);
  settings.setValue("type",           m_type);
  settings.setValue("appended",       m_appended);
  settings.setValue("additionalPart", m_additionalPart);
  settings.setValue("appendedTo",     static_cast<void const *>(this));
}
