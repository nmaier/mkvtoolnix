#ifndef MTX_MMGQT_FILE_IDENTIFIER_H
#define MTX_MMGQT_FILE_IDENTIFIER_H

#include "common/common_pch.h"

#include <QStringList>
#include <QWidget>

class FileIdentifier: public QObject {
  Q_OBJECT;

private:
  QWidget *m_parent;
  int m_exitCode;
  QStringList m_output;
  QString m_fileName;

public:
  FileIdentifier(QWidget *parent = nullptr, QString const &fileName = QString{});
  virtual ~FileIdentifier();

  virtual bool identify();

  virtual QString const &fileName() const;
  virtual void setFileName(QString const &fileName);

  virtual int exitCode() const;
  virtual QStringList const &output() const;
};

#endif // MTX_MMGQT_FILE_IDENTIFIER_H
