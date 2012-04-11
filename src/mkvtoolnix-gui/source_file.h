#ifndef MTX_MMGQT_SOURCE_FILE_H
#define MTX_MMGQT_SOURCE_FILE_H

#include "common/common_pch.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include "common/file_types.h"
#include "common/qt.h"

#include "mkvtoolnix-gui/track.h"

class SourceFile;
typedef std::shared_ptr<SourceFile> SourceFilePtr;

class SourceFile: public QObject {
  Q_OBJECT;
public:
  QHash<QString, QString> m_properties;
  QString m_fileName, m_container;
  QList<TrackPtr> m_tracks;
  QList<SourceFilePtr> m_additionalParts, m_appendedFiles;

  file_type_e m_type;
  bool m_appended, m_additionalPart;
  SourceFile *m_appendedTo;

public:
  explicit SourceFile(QString const &fileName);
  virtual ~SourceFile();

  virtual void setContainer(QString const &container);
  virtual bool isValid() const;
};

#endif  // MTX_MMGQT_SOURCE_FILE_H
