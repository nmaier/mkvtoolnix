#ifndef __MAIN_WINDOW_H
#define __MAIN_WINDOW_H

#include "config.h"

#include <QFile>
#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QTreeWidgetItem>
#include <QVector>

#include "mmg_qt.h"

#include "forms/main_window.h"

class main_window_c: public QMainWindow, public Ui_main_window {
  Q_OBJECT;
protected:
  mkvmerge_settings_t m_mkvmerge_settings;

  QHash<QString, QString> m_capabilities;

  QString m_previous_directory;
  QString m_input_file_filter;

public:
  main_window_c();
  virtual ~main_window_c();

  void set_capabilities(QHash<QString, QString> &new_capabilities);
  QString get_capability(const QString &key);

  mkvmerge_settings_t &get_mkvmerge_settings();

  void add_debug_message(const QString &message);

public slots:
  // Menu "Help"
  void on_action_about_activated();
  void on_action_show_help_activated();

  // Menu "Window"
  void on_action_show_input_tab_activated();
  void on_action_show_global_options_tab_activated();
  void on_action_show_splitting_and_linking_tab_activated();
  void on_action_show_attachments_tab_activated();

  // Tab "Input"
  void on_pb_add_file_clicked();
  void on_pb_append_file_clicked();

protected:
  void query_mkvmerge_capabilities();

  void setup_input_tab();
  void setup_input_file_filter();
  bool select_input_file(bool for_appending);
};

extern main_window_c *g_main_window;

#define DBG(s) g_main_window->add_debug_message(s)

#endif  // __MAIN_WINDOW_H
