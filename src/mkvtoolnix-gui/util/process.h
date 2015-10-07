#ifndef MTX_MKVTOOLNIX_GUI_PROCESS_H
#define MTX_MKVTOOLNIX_GUI_PROCESS_H

#include "common/common_pch.h"

#include <QProcess>
#include <QString>

class ProcessX : public mtx::exception {
protected:
  std::string m_message;
public:
  explicit ProcessX(std::string const &message)  : m_message(message)       { }
  explicit ProcessX(boost::format const &message): m_message(message.str()) { }
  virtual ~ProcessX() throw() { }

  virtual char const *what() const throw() {
    return m_message.c_str();
  }
};

class Process;
typedef std::shared_ptr<Process> ProcessPtr;

class Process: public QObject {
  Q_OBJECT;
private:
  QProcess m_process;
  QString m_command, m_output;
  QStringList m_args;

public:
  Process(QString const &command, QStringList const &args);
  virtual ~Process();

  virtual QStringList output() const;
  virtual QProcess const &process() const;
  virtual void run();

public slots:
  virtual void dataAvailable();

public:
  static ProcessPtr execute(QString const &command, QStringList const &args, bool useTempFile = true);
};

#endif  // MTX_MKVTOOLNIX_GUI_PROCESS_H
