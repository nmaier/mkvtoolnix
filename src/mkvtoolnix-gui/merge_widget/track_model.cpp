#include "common/common_pch.h"

#include "mkvtoolnix-gui/merge_widget/track_model.h"
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
  labels << QY("Codec") << QY("Type") << QY("Mux this") << QY("Language") << QY("Name") << QY("Source file") << QY("ID");
  setHorizontalHeaderLabels(labels);
  horizontalHeaderItem(6)->setTextAlignment(Qt::AlignRight);
}

TrackModel::~TrackModel() {
}

void
TrackModel::setTracks(QList<Track *> &tracks) {
  removeRows(0, rowCount());

  m_tracks = &tracks;
  auto row = 0u;

  for (auto const &track : *m_tracks) {
    invisibleRootItem()->appendRow(createRow(track));

    for (auto const &appendedTrack : track->m_appendedTracks)
      item(row)->appendRow(createRow(appendedTrack));

    ++row;
  }
}

QList<QStandardItem *>
TrackModel::createRow(Track *track) {
  auto items = QList<QStandardItem *>{};
  for (int idx = 0; idx < 7; ++idx)
    items << new QStandardItem{};

  setItemsFromTrack(items, track);

  m_tracksToItems[track] = items[0];

  return items;
}

void
TrackModel::setItemsFromTrack(QList<QStandardItem *> items,
                              Track *track) {
  items[0]->setText(track->isChapters() || track->isGlobalTags() || track->isTags() ? QY("%1 entries").arg(track->m_size) : track->m_codec);
  items[1]->setText(  track->isAudio()      ? QY("audio")
                    : track->isVideo()      ? QY("video")
                    : track->isSubtitles()  ? QY("subtitles")
                    : track->isButtons()    ? QY("buttons")
                    : track->isAttachment() ? QY("attachment")
                    : track->isChapters()   ? QY("chapters")
                    : track->isTags()       ? QY("tags")
                    : track->isGlobalTags() ? QY("global tags")
                    :                          Q("INTERNAL ERROR"));
  items[2]->setText(track->m_muxThis ? QY("yes") : QY("no"));
  items[3]->setText(track->m_language);
  items[4]->setText(track->m_name);
  items[5]->setText(QFileInfo{ track->m_file->m_fileName }.fileName());
  items[6]->setText(-1 == track->m_id ? Q("") : QString::number(track->m_id));

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
  assert(m_tracks);

  int row = rowForTrack(*m_tracks, track);
  if (-1 == row)
    return;

  auto items = QList<QStandardItem *>{};
  for (int column = 0; column < columnCount(); ++column)
    items << item(row, column);

  setItemsFromTrack(items, track);
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
