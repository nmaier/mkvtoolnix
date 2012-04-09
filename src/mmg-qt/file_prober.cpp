#include "common/common_pch.h"

#include <QMessageBox>
#include <QRegExp>
#include <QTemporaryFile>

#include "common/qt.h"
#include "mmg-qt/file_prober.h"

// file_prober_c::file_prober_c(main_window_c *parent)
//   : m_parent(parent)
//   , m_process(parent)
//   , m_file(new input_file_c)
//   , m_options_file(new QTemporaryFile)
// {
//   connect(&m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(data_available()));
// }

// file_prober_c::~file_prober_c() {
//   delete m_file;
//   delete m_options_file;
// }

// input_file_c *
// file_prober_c::get_file() {
//   return m_file;
// }

// int
// file_prober_c::run(const QString &input_file_name) {
//   m_file->m_name = input_file_name;

//   if (!m_options_file->open()) {
//     QMessageBox::critical(m_parent, Q(Y("Error querying mkvmerge")),
//                           Q(Y("The file cannot be used because mmg could not create a temporary file (reason: %1).")).arg(m_options_file->errorString()));
//     return -1;
//   }

//   static const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
//   m_options_file->write((const char *)utf8_bom, 3);
//   m_options_file->write(QByteArray("--output-charset\nUTF-8\n--identify-for-mmg\n"));
//   m_options_file->write(input_file_name.toUtf8());
//   m_options_file->write(QByteArray("\n"));
//   m_options_file->flush();

//   QStringList args;
//   args << Q("@%1").arg(m_options_file->fileName());
//   m_process.start(m_parent->get_mkvmerge_settings().executable, args);

//   while (!m_process.waitForFinished(10)) {
//     qApp->processEvents();
//     qApp->sendPostedEvents();
//   }

//   return process_output();
// }

// void
// file_prober_c::data_available() {
//   QByteArray output = m_process.readAllStandardOutput();
//   QByteArray raw_line;

//   int cur_pos;
//   for (cur_pos = 0; output.size() > cur_pos; ++cur_pos) {
//     if (('\n' == output[cur_pos]) || ('\r' == output[cur_pos])) {
//       m_output << QString::fromUtf8(raw_line);
//       raw_line.clear();

//     } else
//       raw_line += output[cur_pos];
//   }

//   if (!raw_line.isEmpty())
//     m_output << QString::fromUtf8(raw_line);
// }

// int
// file_prober_c::process_output() {
//   int exit_code = m_process.exitCode();

//   if (2 == exit_code) {
//     QMessageBox::warning(m_parent, Q(Y("Unknown file format")), Q(Y("The file is an unknown and unsupported file format.")));
//     return -2;
//   }

//   if (3 == exit_code) {
//     QString container = Q(Y("unknown"));

//     int pos;
//     if (!m_output.isEmpty() && ((pos = m_output[0].indexOf(Q("container:"))) >= 0))
//       container = m_output[0].mid(pos + 11);

//     QMessageBox::warning(m_parent, Q(Y("Unsupported file format")), Q(Y("The file is an unsupported container format (%1).")).arg(container));
//     return -3;
//   }

//   if (0 != exit_code) {
//     QMessageBox::warning(m_parent, Q(Y("Unknown error")), Q(Y("Determining the file type failed. Please make sure you have the correct mkvmerge executable selected in the settings.")));
//     return -4;
//   }

//   QRegExp re_container(Q("File\\s.*container:\\s+([^\\[]+)"));
//   QRegExp re_track(Q("Track\\s+ID\\s+(\\d+):\\s+(audio|video|subtitles|buttons)\\s+\\(([^\\)]+)\\)"));
//   QRegExp re_properties(Q("\\[(.*)\\]"));

//   int i;
//   for (i = 0; m_output.size() > i; ++i) {
//     int pos;

//     if (-1 != (pos = re_container.indexIn(m_output[i]))) {
//       DBG(Q("cont %1\n").arg(re_container.cap(1)));
//       m_file->m_container = re_container.cap(1);

//       if (-1 != re_properties.indexIn(m_output[i].mid(pos + re_container.matchedLength()))) {
//         DBG(Q("  props %2\n").arg(re_properties.cap(1)));
//         m_file->m_properties = unpack_properties(re_properties.cap(1));

//         QHash<QString, QString>::const_iterator prop = m_file->m_properties.begin();
//         while (prop != m_file->m_properties.end()) {
//           DBG(Q("    %1 = %2\n").arg(prop.key()).arg(prop.value()));
//           prop++;
//         }
//       }

//     } else if (-1 != (pos = re_track.indexIn(m_output[i]))) {
//       input_track_c *track = new input_track_c;
//       m_file->m_tracks << track;

//       DBG(Q("track id %1 type %2 codec %3\n").arg(re_track.cap(1)).arg(re_track.cap(2)).arg(re_track.cap(3)));
//       track->m_tid   = re_track.cap(1).toLongLong();
//       track->m_codec = re_track.cap(3);
//       QString type   = re_track.cap(2);
//       track->m_type  = Q("audio")     == type ? 'a'
//                      : Q("video")     == type ? 'v'
//                      : Q("subtitles") == type ? 's'
//                      :                          'b';

//       if (-1 != re_properties.indexIn(m_output[i].mid(pos + re_track.matchedLength()))) {
//         DBG(Q("  props %2\n").arg(re_properties.cap(1)));
//         track->m_properties = unpack_properties(re_properties.cap(1));

//         QHash<QString, QString>::const_iterator prop = track->m_properties.begin();
//         while (prop != track->m_properties.end()) {
//           DBG(Q("    %1 = %2\n").arg(prop.key()).arg(prop.value()));
//           prop++;
//         }
//       }
//     }
//   }

//   return 0;
// }

// QHash<QString, QString>
// file_prober_c::unpack_properties(const QString &packed) {
//   QHash<QString, QString> properties;

//   QStringList all_pairs = packed.split(Q(" "), QString::SkipEmptyParts);

//   int i;
//   for (i = 0; all_pairs.size() > i; ++i) {
//     QStringList pair = all_pairs[i].split(Q(":"));
//     properties[pair[0]] = pair[1].replace(Q("\\s"), Q(" ")).replace(Q("\\2"), Q("\"")).replace(Q("\\c"), Q(":")).replace(Q("\\\\"), Q("\\"));
//   }

//   return properties;
// }
