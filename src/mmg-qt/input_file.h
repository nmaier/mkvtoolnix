#ifndef __INPUT_FILE_H
#define __INPUT_FILE_H

#include "os.h"

#include <QHash>
#include <QList>
#include <QSettings>
#include <QString>

class input_track_c {
public:
  char m_type;
  QString m_codec;
  QHash<QString, QString> m_properties;

  int64_t m_tid;

  bool m_enabled;

public:
  input_track_c();
  virtual ~input_track_c();

  virtual void save_settings(QSettings &settings);
  virtual void load_settings(QSettings &settings);
};

class input_file_c {
public:
  QString m_name, m_container;
  QHash<QString, QString> m_properties;

  QList<input_track_c *> m_tracks;

  bool m_no_attachments, m_no_chapters, m_no_tags;

public:
  input_file_c();
  virtual ~input_file_c();

  virtual void save_settings(QSettings &settings);
  virtual void load_settings(QSettings &settings);
};

#endif  // __INPUT_FILE_H
