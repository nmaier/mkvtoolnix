#include "common/common_pch.h"

#include "mkvtoolnix-gui/track_model.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileInfo>

TrackModel::TrackModel(QObject *parent)
  : QAbstractItemModel(parent)
  , m_tracks(nullptr)
  , m_audioIcon(":/icons/16x16/knotify.png")
  , m_videoIcon(":/icons/16x16/tool-animator.png")
  , m_subtitleIcon(":/icons/16x16/subtitles.png")
  , m_attachmentIcon(":/icons/16x16/mail-attachment.png")
  , m_chaptersIcon(":/icons/16x16/clock.png")
  , m_tagsIcon(":/icons/16x16/mail-tagged.png")
  , m_genericIcon(":/icons/16x16/application-octet-stream.png")
  , m_yesIcon(":/icons/16x16/dialog-ok-apply.png")
  , m_noIcon(":/icons/16x16/dialog-cancel.png")
  , m_debug{"track_model"}
{
}

TrackModel::~TrackModel() {
}

void
TrackModel::setTracks(QList<Track *> &tracks) {
  beginResetModel();
  m_tracks = &tracks;
  endResetModel();
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
  if (0 != child.column())
    return QModelIndex{};

  auto track = trackFromIndex(child);
  if (!track || !track->m_appendedTo)
    return QModelIndex{};

  auto grandparentTrack = track->m_appendedTo->m_appendedTo;
  int row               = (grandparentTrack ? grandparentTrack->m_appendedTracks : *m_tracks).indexOf(track->m_appendedTo);
  if (-1 != row)
    return createIndex(row, 0, track->m_appendedTo);

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
TrackModel::dataTextAlignment(QModelIndex const &index)
  const {
  return IDColumn == index.column() ? Qt::AlignRight : Qt::AlignLeft;
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
    return track->isChapters() || track->isGlobalTags() || track->isTags() ? QString(QY("%1 entries")).arg(track->m_size)
         :                                                                   track->m_codec;

  else if (TypeColumn == index.column())
    return track->isAudio()      ? QY("audio")
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
    return  QFileInfo{ track->m_file->m_fileName }.fileName();

  else if (NameColumn == index.column())
    return track->m_name;

  else if (IDColumn == index.column())
    return -1 == track->m_id ? Q("") : QString::number(track->m_id);

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

  if (Qt::TextAlignmentRole == role)
    return dataTextAlignment(index);

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
         : NameColumn       == section ? QY("Name")
         : IDColumn         == section ? QY("ID")
         :                                Q("INTERNAL ERROR");

  if (Qt::TextAlignmentRole == role)
    return IDColumn == section ? Qt::AlignRight : Qt::AlignLeft;

  return QVariant{};
}

Track *
TrackModel::trackFromIndex(QModelIndex const &index)
  const {
  if (index.isValid())
    return static_cast<Track *>(index.internalPointer());
  return nullptr;
}

int
TrackModel::rowForTrack(QList<Track *> const &tracks,
                        Track *trackToLookFor) {
  int idx = 0;
  for (auto track : tracks) {
    if (track == trackToLookFor)
      return idx;

    int result = rowForTrack(track->m_appendedTracks, trackToLookFor);
    if (-1 != result)
      return result;

    ++idx;
  }

  return -1;
}

void
TrackModel::trackUpdated(Track *track) {
  if (!m_tracks) {
    mxdebug_if(m_debug, boost::format("trackUpdated() called but !m_tracks!?\n"));
    return;
  }

  int row = rowForTrack(*m_tracks, track);
  mxdebug(boost::format("trackUpdated(): row is %1%\n") % row);
  if (-1 == row)
    return;

  emit dataChanged(createIndex(row, 0, track), createIndex(row, NumberOfColumns - 1, track));
}

void
TrackModel::addTracks(QList<TrackPtr> tracks) {
  if (tracks.isEmpty())
    return;

  beginInsertRows(QModelIndex{}, m_tracks->size(), m_tracks->size() + tracks.size() - 1);
  for (auto &track : tracks)
    *m_tracks << track.get();
  endInsertRows();
}

void
TrackModel::clear() {
  if (m_tracks->isEmpty())
    return;

  beginResetModel();
  m_tracks->clear();
  endResetModel();
}
