#ifndef MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_MKVMERGE_OPTION_BUILDER_H
#define MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_MKVMERGE_OPTION_BUILDER_H

#include "common/common_pch.h"

#include <QString>
#include <QHash>

#include "mkvtoolnix-gui/merge_widget/track.h"

struct MkvmergeOptionBuilder {
  QStringList options;
  QHash<Track::Type, unsigned int> numTracksOfType;
  QHash<Track::Type, QStringList> enabledTrackIds;
};

#endif // MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_MKVMERGE_OPTION_BUILDER_H
