#include "common/common_pch.h"

#include "common/qt.h"
#include "common/strings/editing.h"
#include "mkvtoolnix-gui/util/file_identifier.h"
#include "mkvtoolnix-gui/util/process.h"
#include "mkvtoolnix-gui/util/settings.h"

#include <QMessageBox>
#include <QStringList>

FileIdentifier::FileIdentifier(QWidget *parent,
                               QString const &fileName)
  : m_parent(parent)
  , m_exitCode(0)
  , m_fileName(fileName)
{
}

FileIdentifier::~FileIdentifier() {
}

bool
FileIdentifier::identify() {
  if (m_fileName.isEmpty())
    return false;

  QStringList args;
  args << "--output-charset" << "utf-8" << "--identify-for-mmg" << m_fileName;

  auto process  = Process::execute(Settings::get().m_mkvmergeExe, args);
  auto exitCode = process->process().exitCode();
  m_output      = process->output();

  if (0 == exitCode)
    return parseOutput();

  if (3 == exitCode) {
    auto pos       = m_output.isEmpty() ? -1            : m_output[0].indexOf("container:");
    auto container = -1 == pos          ? QY("unknown") : m_output[0].mid(pos + 11);

    QMessageBox::critical(m_parent, QY("Unsupported file format"), QY("The file is an unsupported container format (%1).").arg(container));

    return false;
  }

  QMessageBox::critical(m_parent, QY("Unrecognized file format"), QY("The file was not recognized as a supported format (exit code: %1).").arg(exitCode));

  return false;
}

QString const &
FileIdentifier::fileName()
  const {
  return m_fileName;
}

void
FileIdentifier::setFileName(QString const &fileName) {
  m_fileName = fileName;
}

int
FileIdentifier::exitCode()
  const {
  return m_exitCode;
}

QStringList const &
FileIdentifier::output()
  const {
  return m_output;
}

SourceFilePtr const &
FileIdentifier::file()
  const {
  return m_file;
}

bool
FileIdentifier::parseOutput() {
  m_file = std::make_shared<SourceFile>(m_fileName);

  for (auto &line : m_output) {
    if (line.startsWith("File"))
      parseContainerLine(line);

    else if (line.startsWith("Track"))
      parseTrackLine(line);

    else if (line.startsWith("Attachment"))
      parseAttachmentLine(line);

    else if (line.startsWith("Chapters"))
      parseChaptersLine(line);

    else if (line.startsWith("Global tags"))
      parseGlobalTagsLine(line);

    else if (line.startsWith("Tags"))
      parseTagsLine(line);

    else if (line.startsWith("Track"))
      parseTrackLine(line);
  }

  return m_file->isValid();
}

// Attachment ID 1: type "cue", size 1844 bytes, description "dummy", file name "cuewithtags2.cue"
void
FileIdentifier::parseAttachmentLine(QString const &line) {
  QRegExp re{"^Attachment ID (\\d+): type \"(.*)\", size (\\d+) bytes, description \"(.*)\", file name \"(.*)\"$"};

  if (-1 == re.indexIn(line))
    return;

  auto track                     = std::make_shared<Track>(m_file.get(), Track::Attachment);
  track->m_properties            = parseProperties(line);
  track->m_id                    = re.cap(1).toLongLong();
  track->m_codec                 = re.cap(2);
  track->m_size                  = re.cap(3).toLongLong();
  track->m_attachmentDescription = re.cap(4);
  track->m_name                  = re.cap(5);

  m_file->m_tracks << track;
}

// Chapters: 27 entries
void
FileIdentifier::parseChaptersLine(QString const &line) {
  QRegExp re{"^Chapters: (\\d+) entries$"};

  if (-1 == re.indexIn(line))
    return;

  auto track    = std::make_shared<Track>(m_file.get(), Track::Chapters);
  track->m_size = re.cap(1).toLongLong();

  m_file->m_tracks << track;
}

// File 'complex.mkv': container: Matroska [duration:106752000000 segment_uid:00000000000000000000000000000000]
void
FileIdentifier::parseContainerLine(QString const &line) {
  QRegExp re{"^File\\s.*container:\\s+([^\\[]+)"};

  if (-1 == re.indexIn(line))
    return;

  m_file->setContainer(re.cap(1));
  m_file->m_properties       = parseProperties(line);
  m_file->m_isPlaylist       = m_file->m_properties["playlist"] == "1";
  m_file->m_playlistDuration = m_file->m_properties["playlist_duration"].toULongLong();
  m_file->m_playlistSize     = m_file->m_properties["playlist_size"].toULongLong();
  m_file->m_playlistChapters = m_file->m_properties["playlist_chapters"].toULongLong();

  if (m_file->m_isPlaylist && !m_file->m_properties["playlist_file"].isEmpty())
    for (auto const &fileName : m_file->m_properties["playlist_file"].split("\t"))
      m_file->m_playlistFiles << QFileInfo{fileName};

  if (m_file->m_properties["other_file"].isEmpty())
    return;

  for (auto &fileName : m_file->m_properties["other_file"].split("\t")) {
    auto additionalPart              = std::make_shared<SourceFile>(fileName);
    additionalPart->m_additionalPart = true;
    additionalPart->m_appendedTo     = m_file.get();
    m_file->m_additionalParts       << additionalPart;
  }
}

// Global tags: 3 entries
void
FileIdentifier::parseGlobalTagsLine(QString const &line) {
  QRegExp re{"^Global tags: (\\d+) entries$"};

  if (-1 == re.indexIn(line))
    return;

  auto track    = std::make_shared<Track>(m_file.get(), Track::GlobalTags);
  track->m_size = re.cap(1).toLongLong();

  m_file->m_tracks << track;
}

// Tags for track ID 1: 2 entries
void
FileIdentifier::parseTagsLine(QString const &line) {
  QRegExp re{"^Tags for track ID (\\d+): (\\d+) entries$"};

  if (-1 == re.indexIn(line))
    return;

  auto track    = std::make_shared<Track>(m_file.get(), Track::Tags);
  track->m_id   = re.cap(1).toLongLong();
  track->m_size = re.cap(2).toLongLong();

  m_file->m_tracks << track;
}

// Track ID 0: video (V_MS/VFW/FOURCC, DIV3) [number:1 ...]
// Track ID 7: audio (A_PCM/INT/LIT) [number:8 uid:289972206 codec_id:A_PCM/INT/LIT codec_private_length:0 language:und default_track:0 forced_track:0 enabled_track:1 default_duration:31250000 audio_sampling_frequency:48000 audio_channels:2]
// Track ID 8: subtitles (S_TEXT/UTF8) [number:9 ...]
void
FileIdentifier::parseTrackLine(QString const &line) {
  QRegExp re{"Track\\s+ID\\s+(\\d+):\\s+(audio|video|subtitles|buttons)\\s+\\(([^\\)]+)\\)", Qt::CaseInsensitive};
  if (-1 == re.indexIn(line))
    return;

  auto type                       = re.cap(2) == "audio"     ? Track::Audio
                                  : re.cap(2) == "video"     ? Track::Video
                                  : re.cap(2) == "subtitles" ? Track::Subtitles
                                  :                            Track::Buttons;
  auto track                      = std::make_shared<Track>(m_file.get(), type);
  track->m_id                     = re.cap(1).toLongLong();
  track->m_codec                  = re.cap(3);
  track->m_properties             = parseProperties(line);

  m_file->m_tracks << track;

  track->setDefaults();
}

QHash<QString, QString>
FileIdentifier::parseProperties(QString const &line)
  const {
  QHash<QString, QString> properties;

  QRegExp re{"\\[(.+)\\]"};
  if (-1 == re.indexIn(line))
    return properties;

  for (auto &pair : re.cap(1).split(QRegExp{"\\s+"}, QString::SkipEmptyParts)) {
    QRegExp re{"(.+):(.+)"};
    if (-1 == re.indexIn(pair))
      continue;

    auto key = to_qs(unescape(to_utf8(re.cap(1))));
    if (!properties[key].isEmpty())
      properties[key] += Q("\t");
    properties[key] += to_qs(unescape(to_utf8(re.cap(2))));
  }

  return properties;
}
