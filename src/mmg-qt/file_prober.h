#ifndef MTX_MMGQT_FILE_PROBER_H
#define MTX_MMGQT_FILE_PROBER_H

#include "common/common_pch.h"

#include <QHash>
#include <QProcess>
#include <QString>

#include "mmg-qt/mmg_qt.h"

// class file_prober_c: public QObject {
//   Q_OBJECT;
// private:
//   main_window_c *m_parent;
//   QProcess m_process;
//   QStringList m_output;
//   input_file_c *m_file;
//   QTemporaryFile *m_options_file;

// public:
//   file_prober_c(main_window_c *parent);
//   virtual ~file_prober_c();

//   virtual int run(const QString &input_file_name);
//   virtual input_file_c *get_file();

// public slots:
//   virtual void data_available();

// protected:
//   virtual int process_output();
//   virtual QHash<QString, QString> unpack_properties(const QString &packed);
// };

#endif  // MTX_MMGQT_FILE_PROBER_H
