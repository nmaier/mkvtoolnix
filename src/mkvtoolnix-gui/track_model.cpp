#include "common/common_pch.h"

#include "mkvtoolnix-gui/track_model.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileInfo>

TrackModel::TrackModel(QObject *parent)
  : QStandardItemModel{parent}
  , m_tracks{}
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
  auto labels = QStringList{};
  labels << QY("Codec") << QY("Mux this") << QY("Language") << QY("Source file") << QY("Type") << QY("Name") << QY("ID");
  setHorizontalHeaderLabels(labels);
  horizontalHeaderItem(6)->setTextAlignment(Qt::AlignRight);
}

TrackModel::~TrackModel() {
}

void
TrackModel::setTracks(QList<Track *> &tracks) {
  beginResetModel();
  m_tracks = &tracks;
  endResetModel();
}

QList<QStandardItem *>
TrackModel::createRow(Track *track) {
  auto items = QList<QStandardItem *>{};

  items << new QStandardItem{track->isChapters() || track->isGlobalTags() || track->isTags() ? QY("%1 entries").arg(track->m_size) : track->m_codec};
  items << new QStandardItem{  track->isAudio()      ? QY("audio")
                             : track->isVideo()      ? QY("video")
                             : track->isSubtitles()  ? QY("subtitles")
                             : track->isButtons()    ? QY("buttons")
                             : track->isAttachment() ? QY("attachment")
                             : track->isChapters()   ? QY("chapters")
                             : track->isTags()       ? QY("tags")
                             : track->isGlobalTags() ? QY("global tags")
                             :                          Q("INTERNAL ERROR")};
  items << new QStandardItem{track->m_muxThis ? QY("yes") : QY("no")};
  items << new QStandardItem{track->m_language};
  items << new QStandardItem{QFileInfo{ track->m_file->m_fileName }.fileName()};
  items << new QStandardItem{track->m_name};
  items << new QStandardItem{-1 == track->m_id ? Q("") : QString::number(track->m_id)};

  items[0]->setData(QVariant::fromValue(track), Util::TrackRole);
  items[1]->setIcon(  track->isAudio()      ? m_audioIcon
                    : track->isVideo()      ? m_videoIcon
                    : track->isSubtitles()  ? m_subtitleIcon
                    : track->isAttachment() ? m_attachmentIcon
                    : track->isChapters()   ? m_chaptersIcon
                    : track->isTags()       ? m_tagsIcon
                    : track->isGlobalTags() ? m_tagsIcon
                    :                         m_genericIcon);
  items[2]->setIcon(track->m_muxThis ? m_yesIcon : m_noIcon);
  items[6]->setTextAlignment(Qt::AlignRight);

  m_tracksToItems[track] = items[0];

  return items;
}

Track *
TrackModel::fromIndex(QModelIndex const &index) {
  if (index.isValid())
    return index.data(Util::TrackRole).value<Track *>();
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

  emit dataChanged(createIndex(row, 0, track), createIndex(row, columnCount() - 1, track));
}

void
TrackModel::addTracks(QList<TrackPtr> const &tracks) {
  for (auto &track : tracks) {
    *m_tracks << track.get();
    invisibleRootItem()->appendRow(createRow(track.get()));
  }
}

void
TrackModel::appendTracks(SourceFile *fileToAppendTo,
                         QList<TrackPtr> const &tracks) {
  if (tracks.isEmpty())
    return;

  auto lastTrack = boost::accumulate(*m_tracks, static_cast<Track *>(nullptr), [](Track *accu, Track *t) { return t->isRegular() ? t : accu; });
  assert(!!lastTrack);

  for (auto &newTrack : tracks) {
    // Things like tags, chapters and attachments aren't appended to a
    // specific track. Instead they're appended to the top list.
    if (!newTrack->isRegular()) {
      *m_tracks << newTrack.get();
      invisibleRootItem()->appendRow(createRow(newTrack.get()));
      continue;
    }

    newTrack->m_appendedTo = fileToAppendTo->findFirstTrackOfType(newTrack->m_type);
    if (!newTrack->m_appendedTo)
      newTrack->m_appendedTo = lastTrack;

    newTrack->m_appendedTo->m_appendedTracks << newTrack.get();
    m_tracksToItems[ newTrack->m_appendedTo ]->appendRow(createRow(newTrack.get()));
  }
}
