#ifndef __CAPABILITIES_READER_H
#define __CAPABILITIES_READER_H

#include "config.h"

#include <QHash>
#include <QProcess>
#include <QString>

#include "mmg_qt.h"

#include "main_window.h"

class capabilities_reader_c: public QObject {
  Q_OBJECT;
private:
  main_window_c *m_parent;
  QProcess m_process;
  QHash<QString, QString> m_capabilities;

public:
  capabilities_reader_c(main_window_c *parent);
  virtual ~capabilities_reader_c();

  virtual void run();

public slots:
  virtual void data_available();

protected:
  virtual void process_line(const QString &line);
};

#endif  // __CAPABILITIES_READER_H
