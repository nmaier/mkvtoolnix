/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   A Qt GUI for mkvinfo

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_QT_UI_H
#define MTX_QT_UI_H

#include "common/common_pch.h"

#include <QFile>
#include <QMainWindow>
#include <QString>
#include <QTreeWidgetItem>
#include <QVector>

#include "info/ui/mainwindow.h"

class main_window_c: public QMainWindow, public Ui_main_window {
  Q_OBJECT;

private slots:
  void open();
  void save_text_file();

  void show_all();

  void about();

private:
  int last_percent, num_elements;

  QVector<QTreeWidgetItem *> parent_items;
  QString current_file;
  QTreeWidgetItem *root;

  void expand_elements();
  void write_tree(QFile &file, QTreeWidgetItem *item, int level);

public:
  main_window_c();

  void show_error(const QString &message);
  void show_progress(int percentage, const QString &text);

  void add_item(int level, const QString &text);

  void expand_all_elements(QTreeWidgetItem *item, bool expand);

  void parse_file(const QString &file_name);
};

#endif  // MTX_QT_UI_H
