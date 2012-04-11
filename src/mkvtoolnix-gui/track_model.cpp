#include "common/common_pch.h"

#include "mkvtoolnix-gui/track_model.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileInfo>

TrackModel::TrackModel(QObject *parent)
  : QAbstractItemModel(parent)
  , m_tracks(nullptr)
  , m_audioIcon(":/icons/16x16/knotify.png")
  , m_videoIcon(":/icons/16x16/tool-animator.png")
  , m_subtitleIcon(":/icons/16x16/")
  , m_attachmentIcon(":/icons/16x16/mail-attachment.png")
  , m_chaptersIcon(":/icons/16x16/clock.png")
  , m_tagsIcon(":/icons/16x16/irc-join-channel.png")
  , m_genericIcon(":/icons/16x16/application-octet-stream.png")
  , m_yesIcon(":/icons/16x16/dialog-ok-apply.png")
  , m_noIcon(":/icons/16x16/dialog-cancel.png")
{
}

TrackModel::~TrackModel() {
}

void
TrackModel::setTracks(QList<Track *> &tracks) {
  m_tracks = &tracks;
  reset();
}


QModelIndex
TrackModel::index(int row,
                  int column,
                  QModelIndex const &parent)
  const {
  if (!m_tracks || (0 > row) || (0 > column))
    return QModelIndex{};

  auto parentTrack = trackFromIndex(parent);
  if (!parentTrack)
    return row < m_tracks->size() ? createIndex(row, column, m_tracks->at(row)) : QModelIndex{};

  if (parentTrack->m_appendedTracks.size() >= row)
    return QModelIndex{};

  auto childTrack = parentTrack->m_appendedTracks[row];

  if (!childTrack)
    return QModelIndex{};

  return createIndex(row, column, childTrack);
}

QModelIndex
TrackModel::parent(QModelIndex const &child)
  const {
  auto track = trackFromIndex(child);
  if (!track)
    return QModelIndex{};

  auto parentTrack = track->m_appendedTo;
  if (!parentTrack) {
    for (int row = 0; m_tracks->size() > row; ++row)
      if (m_tracks->at(row) == track)
        return createIndex(row, 0, nullptr);
    return QModelIndex{};
  }

  for (int row = 0; parentTrack->m_appendedTracks.size() > row; ++row)
    if (parentTrack->m_appendedTracks[row] == track)
      return createIndex(row, 0, parentTrack);

  return QModelIndex{};
}

int
TrackModel::rowCount(QModelIndex const &parent)
  const {
  if (parent.column() > 0)
    return 0;
  if (!m_tracks)
    return 0;
  auto parentTrack = trackFromIndex(parent);
  if (!parentTrack)
    return m_tracks->size();
  return parentTrack->m_appendedTracks.size();
}

int
TrackModel::columnCount(QModelIndex const &)
  const {
  return NumberOfColumns;
}

QVariant
TrackModel::dataDecoration(QModelIndex const &index,
                           Track *track)
  const {
  if (CodecColumn == index.column())
    return track->isAudio()      ? m_audioIcon
         : track->isVideo()      ? m_videoIcon
         : track->isSubtitles()  ? m_subtitleIcon
         : track->isAttachment() ? m_attachmentIcon
         : track->isChapters()   ? m_chaptersIcon
         : track->isTags()       ? m_tagsIcon
         : track->isGlobalTags() ? m_tagsIcon
         :                         m_genericIcon;

  else if (MuxColumn == index.column())
    return track->m_muxThis ? m_yesIcon : m_noIcon;

  else
    return QVariant{};
}

QVariant
TrackModel::dataDisplay(QModelIndex const &index,
                        Track *track)
  const {
  if (CodecColumn == index.column())
    return track->m_codec;

  else if (TypeColumn == index.column())
    return track->isAudio()       ? QY("audio")
          : track->isVideo()      ? QY("video")
          : track->isSubtitles()  ? QY("subtitles")
          : track->isButtons()    ? QY("buttons")
          : track->isAttachment() ? QY("attachment")
          : track->isChapters()   ? QY("chapters")
          : track->isTags()       ? QY("tags")
          : track->isGlobalTags() ? QY("global tags")
          :                          Q("INTERNAL ERROR");

  else if (MuxColumn == index.column())
    return track->m_muxThis ? QY("yes") : QY("no");

  else if (LanguageColumn == index.column())
    return track->m_language;

  else if (SourceFileColumn == index.column())
    return QFileInfo{ track->m_file->m_fileName }.fileName();

  else
    return QVariant{};
}

QVariant
TrackModel::data(QModelIndex const &index,
                 int role)
  const {
  auto track = trackFromIndex(index);
  if (!track)
    return QVariant{};

  if (Qt::DecorationRole == role)
    return dataDecoration(index, track);

  if (Qt::DisplayRole == role)
    return dataDisplay(index, track);

  return QVariant{};
}

QVariant
TrackModel::headerData(int section,
                       Qt::Orientation orientation,
                       int role)
  const {
  if (Qt::Horizontal != orientation)
    return QVariant{};

  if (Qt::DisplayRole == role)
    return CodecColumn      == section ? QY("Codec")
         : MuxColumn        == section ? QY("Mux this")
         : LanguageColumn   == section ? QY("Language")
         : SourceFileColumn == section ? QY("Source file")
         : TypeColumn       == section ? QY("Type")
         :                                Q("INTERNAL ERROR");

  if (Qt::TextAlignmentRole == role)
    return 2 == section ? Qt::AlignRight : Qt::AlignLeft;

  return QVariant{};
}

Track *
TrackModel::trackFromIndex(QModelIndex const &index)
  const {
  if (index.isValid())
    return static_cast<Track *>(index.internalPointer());
  return nullptr;
}
