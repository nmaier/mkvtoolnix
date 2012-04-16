#ifndef MTX_MMGQT_FILE_IDENTIFIER_H
#define MTX_MMGQT_FILE_IDENTIFIER_H

#include "common/common_pch.h"

#include <QStringList>
#include <QWidget>

#include "mkvtoolnix-gui/source_file.h"

class FileIdentifier: public QObject {
  Q_OBJECT;

private:
  QWidget *m_parent;
  int m_exitCode;
  QStringList m_output;
  QString m_fileName;
  SourceFilePtr m_file;

public:
  FileIdentifier(QWidget *parent = nullptr, QString const &fileName = QString{});
  virtual ~FileIdentifier();

  virtual bool identify();
  virtual bool parseOutput();
  virtual QHash<QString, QString> parseProperties(QString const &line) const;
  virtual void parseAttachmentLine(QString const &line);
  virtual void parseChaptersLine(QString const &line);
  virtual void parseContainerLine(QString const &line);
  virtual void parseGlobalTagsLine(QString const &line);
  virtual void parseTagsLine(QString const &line);
  virtual void parseTrackLine(QString const &line);

  virtual QString const &fileName() const;
  virtual void setFileName(QString const &fileName);

  virtual int exitCode() const;
  virtual QStringList const &output() const;

  virtual SourceFilePtr const &file() const;
};

#endif // MTX_MMGQT_FILE_IDENTIFIER_H
