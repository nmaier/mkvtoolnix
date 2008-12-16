#include "config.h"

#include "capabilities_reader.h"
#include "qtcommon.h"

capabilities_reader_c::capabilities_reader_c(main_window_c *parent)
  : m_parent(parent)
  , m_process(parent)
{
  connect(&m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(data_available()));
}

capabilities_reader_c::~capabilities_reader_c() {
}

void
capabilities_reader_c::run() {
  QStringList args;
  args << Q("--capabilities");
  m_process.start(m_parent->get_mkvmerge_settings().executable, args);

  while (!m_process.waitForFinished(10)) {
    qApp->processEvents();
    qApp->sendPostedEvents();
  }

  m_parent->set_capabilities(m_capabilities);
}

void
capabilities_reader_c::data_available() {
  QByteArray output = m_process.readAllStandardOutput();
  QByteArray raw_line;

  int cur_pos;
  for (cur_pos = 0; output.size() > cur_pos; ++cur_pos) {
    if (('\n' == output[cur_pos]) || ('\r' == output[cur_pos])) {
      process_line(QString::fromUtf8(raw_line));
      raw_line.clear();

    } else
      raw_line += output[cur_pos];
  }

  if (!raw_line.isEmpty())
    process_line(QString::fromUtf8(raw_line));
}

void
capabilities_reader_c::process_line(const QString &line) {
  int pos = line.indexOf(L'=');
  if (-1 == pos)
    m_capabilities[line] = Q("true");

  else
    m_capabilities[line.mid(0, pos)] = line.mid(pos + 1);
}
