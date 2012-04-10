#include "common/common_pch.h"

#include <QRegExp>
#include <QTemporaryFile>

#include "common/qt.h"
#include "mkvtoolnix-gui/util/process.h"

Process::Process(QString const &command,
                 QStringList const &args)
  : m_command(command)
  , m_args(args)
{
  connect(&m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(dataAvailable()));
}

Process::~Process() {
}

void
Process::run() {
  m_process.start(m_command, m_args);
  m_process.waitForFinished(-1);
}

QStringList
Process::output()
  const {
  return m_output.split(QRegExp{ "\r?\n" });
}

QProcess const &
Process::process()
  const {
  return m_process;
}

void
Process::dataAvailable() {
  QByteArray output = m_process.readAllStandardOutput();
  m_output += QString::fromUtf8(output);
}

ProcessPtr
Process::execute(QString const &command,
                 QStringList const &args,
                 bool useTempFile) {
  auto runner = [](QString const &commandToUse, QStringList const &argsToUse) {
    auto pr = std::make_shared<Process>( commandToUse, argsToUse );
    pr->run();
    return pr;
  };

  if (!useTempFile)
    return runner(command, args);

  QTemporaryFile optFile;

  if (!optFile.open())
    throw ProcessX{ to_utf8(QY("Error creating a temporary file (reason: %1).").arg(optFile.errorString())) };

  static const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
  optFile.write(reinterpret_cast<char const *>(utf8_bom), 3);
  for (auto &arg : args)
    optFile.write(QString{"%1\n"}.arg(arg).toUtf8());
  optFile.close();

  QStringList argsToUse;
  argsToUse << QString{"@%1"}.arg(optFile.fileName());
  return runner(command, argsToUse);
}
