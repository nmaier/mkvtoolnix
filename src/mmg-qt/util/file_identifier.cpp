#include "common/common_pch.h"

#include "common/qt.h"
#include "mmg-qt/util/file_identifier.h"
#include "mmg-qt/util/process.h"
#include "mmg-qt/util/settings.h"

#include <QMessageBox>
#include <QStringList>

FileIdentifier::FileIdentifier(QWidget *parent,
                               QString const &fileName)
  : m_parent(parent)
  , m_exitCode(0)
  , m_fileName(fileName)
{
}

FileIdentifier::~FileIdentifier() {
}

bool
FileIdentifier::identify() {
  if (m_fileName.isEmpty())
    return false;

  QStringList args;
  args << "--output-charset" << "utf-8" << "--identify-for-mmg" << m_fileName;

  auto process  = Process::execute(Settings::get().m_mkvmergeExe, args);
  auto exitCode = process->process().exitCode();
  m_output      = process->output();

  mxinfo(boost::format("oh yeah, code %2%: %1%\n") % to_utf8(m_output.join(":::")) % exitCode);

  if (0 == exitCode)
    return true;

  if (3 == exitCode) {
    auto pos       = m_output.isEmpty() ? -1            : m_output[0].indexOf("container:");
    auto container = -1 == pos          ? QY("unknown") : m_output[0].mid(pos + 11);

    QMessageBox::critical(m_parent, QY("Unsupported file format"), QY("The file is an unsupported container format (%1).").arg(container));

    return false;
  }

  QMessageBox::critical(m_parent, QY("Unrecognized file format"), QY("The file was not recognized as a supported format (exit code: %1).").arg(exitCode));

  return false;
}

QString const &
FileIdentifier::fileName()
  const {
  return m_fileName;
}

void
FileIdentifier::setFileName(QString const &fileName) {
  m_fileName = fileName;
}

int
FileIdentifier::exitCode()
  const {
  return m_exitCode;
}

QStringList const &
FileIdentifier::output()
  const {
  return m_output;
}
