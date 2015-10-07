#ifndef MTX_MKVTOOLNIX_GUI_TRACK_MODEL_H
#define MTX_MKVTOOLNIX_GUI_TRACK_MODEL_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/merge_widget/source_file.h"

#include <QStandardItemModel>
#include <QIcon>
#include <QList>
#include <QSet>

class TrackModel;
typedef std::shared_ptr<TrackModel> TrackModelPtr;

class TrackModel : public QStandardItemModel {
  Q_OBJECT;

protected:
  QList<Track *> *m_tracks;
  QMap<Track *, QStandardItem *> m_tracksToItems;
  QIcon m_audioIcon, m_videoIcon, m_subtitleIcon, m_attachmentIcon, m_chaptersIcon, m_tagsIcon, m_genericIcon, m_yesIcon, m_noIcon;

  debugging_option_c m_debug;

public:
  TrackModel(QObject *parent);
  virtual ~TrackModel();

  virtual void setTracks(QList<Track *> &tracks);
  virtual void addTracks(QList<TrackPtr> const &tracks);
  virtual void appendTracks(SourceFile *fileToAppendTo, QList<TrackPtr> const &tracks);
  virtual void removeTrack(Track *trackToBeRemoved);
  virtual void removeTracks(QSet<Track *> const &tracks);

  virtual void trackUpdated(Track *track);

  virtual Track *fromIndex(QModelIndex const &idx);

protected:
  QList<QStandardItem *>createRow(Track *track);
  void setItemsFromTrack(QList<QStandardItem *> items, Track *track);

public:                         // static
  static int rowForTrack(QList<Track *> const &tracks, Track *trackToLookFor);
};

Q_DECLARE_METATYPE(Track *)

#endif  // MTX_MKVTOOLNIX_GUI_TRACK_MODEL_H
