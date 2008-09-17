/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   A Qt GUI for mkvinfo

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "config.h"

#include <ebml/EbmlVersion.h>
#include <matroska/KaxVersion.h>

#include "common.h"
#include "mkvinfo.h"

#include "qt_ui.h"

#include <QMessageBox>
#include <QMouseEvent>
#include <QFileDialog>

using namespace libebml;
using namespace libmatroska;

main_window_c::main_window_c():
  last_percent(-1), num_elements(0),
  root(NULL) {

  setupUi(this);

  connect(action_Open, SIGNAL(triggered()), this, SLOT(open()));
  connect(action_Save_text_file, SIGNAL(triggered()), this,
          SLOT(save_text_file()));
  connect(action_Exit, SIGNAL(triggered()), this, SLOT(close()));

  connect(action_Show_all, SIGNAL(triggered()), this, SLOT(show_all()));

  connect(action_About, SIGNAL(triggered()), this, SLOT(about()));

  action_Save_text_file->setEnabled(false);

  action_Show_all->setCheckable(true);
  action_Show_all->setChecked(0 < verbose);

  action_Expand_important->setCheckable(true);
  action_Expand_important->setChecked(true);

  tree->setHeaderLabels(QStringList(tr("Elements")));
  tree->setRootIsDecorated(true);

  root = new QTreeWidgetItem(tree);
  root->setText(0, tr("no file loaded"));
}

void
main_window_c::open() {
  QString file_name =
    QFileDialog::getOpenFileName(this, tr("Open File"), "",
                                 tr("Matroska files (*.mkv *.mka *.mks)"
                                    ";;All files (*.*)"));
  if (!file_name.isEmpty())
    parse_file(file_name);
}

void
main_window_c::save_text_file() {
  QString file_name =
    QFileDialog::getSaveFileName(this, tr("Save information as"), "",
                                 tr("Text files (*.txt);;All files (*.*)"));
  if (file_name.isEmpty())
    return;

  QFile file(file_name);
  if (!file.open(QIODevice::WriteOnly)) {
    QMessageBox::critical(this, tr("Error saving the information"),
                          tr("The file could not be opened for writing."));
    return;
  }

  write_tree(file, root, 0);

  file.close();
}

void
main_window_c::write_tree(QFile &file,
                          QTreeWidgetItem *item,
                          int level) {
  int i;

  for (i = 0; item->childCount() > i; ++i) {
    QTreeWidgetItem *child = item->child(i);

    char *level_buffer = new char[level + 1];
    level_buffer[0] = '|';
    memset(&level_buffer[1], ' ', level);
    level_buffer[level] = 0;

    file.write(level_buffer, level);
    file.write(QString("+ %1\n").arg(child->text(0)).toUtf8());
    write_tree(file, child, level + 1);
  }
}

void
main_window_c::show_all() {
  verbose = action_Show_all->isChecked() ? 2 : 0;
  if (!current_file.isEmpty())
    parse_file(current_file);
}

void
main_window_c::about() {
  QString msg =
    QString(VERSIONINFO ".\n"
            "Compiled with libebml %1 + libmatroska %2.\n\n"
            "This program is licensed under the GPL v2 (see COPYING).\n"
            "It was written by Moritz Bunkus <moritz@bunkus.org>.\n"
            "Sources and the latest binaries are always available at\n"
            "http://www.bunkus.org/videotools/mkvtoolnix/")
    .arg(QString::fromUtf8(EbmlCodeVersion.c_str()))
    .arg(QString::fromUtf8(KaxCodeVersion.c_str()));
  QMessageBox::about(this, tr("About " NAME), msg);
}

void
main_window_c::show_error(const QString &msg) {
  QMessageBox::critical(this, tr("Error"), msg);
}

void
main_window_c::parse_file(const QString &file_name) {
  tree->setEnabled(false);
  tree->clear();

  root = new QTreeWidgetItem(tree);
  root->setText(0, file_name);

  last_percent = -1;
  num_elements = 0;
  action_Save_text_file->setEnabled(false);

  parent_items.clear();
  parent_items.append(root);

  if (process_file(file_name.toUtf8().data())) {
    action_Save_text_file->setEnabled(true);
    current_file = file_name;
    if (action_Expand_important->isChecked())
      expand_elements();
  }

  statusBar()->showMessage(tr("Ready"), 5000);

  tree->setEnabled(true);
}

void
main_window_c::expand_all_elements(QTreeWidgetItem *item,
                                   bool expand) {
  int i;

  if (expand)
    tree->expandItem(item);
  else
    tree->collapseItem(item);
  for (i = 0; item->childCount() > i; ++i)
    expand_all_elements(item->child(i), expand);
}

void
main_window_c::expand_elements() {
  int l0, l1, c0, c1;
  QTreeWidgetItem *i0, *i1;
  const QString s_segment(tr("Segment"));
  const QString s_info(tr("Segment information"));
  const QString s_tracks(tr("Segment tracks"));

  setUpdatesEnabled(false);

  expand_all_elements(root, false);
  tree->expandItem(root);

  c0 = root->childCount();
  for (l0 = 0; l0 < c0; ++l0) {
    i0 = root->child(l0);
    tree->expandItem(i0);
    if (i0->text(0).left(7) == s_segment) {
      c1 = i0->childCount();
      for (l1 = 0; l1 < c1; ++l1) {
        i1 = i0->child(l1);
        if ((i1->text(0).left(19) == s_info) ||
            (i1->text(0).left(14) == s_tracks))
          expand_all_elements(i1, true);
      }
    }
  }

  setUpdatesEnabled(true);
}

void
main_window_c::add_item(int level,
                        const QString &text) {
  ++level;

  while (parent_items.count() > level)
    parent_items.erase(parent_items.end() - 1);
  QTreeWidgetItem *item = new QTreeWidgetItem(parent_items.last());
  item->setText(0, text);
  parent_items.append(item);
}

void
main_window_c::show_progress(int percentage,
                             const QString &text) {
  if ((percentage / 5) != (last_percent / 5)) {
    statusBar()->showMessage(QString("%1: %2%").arg(text).arg(percentage));
    last_percent = percentage;
    QCoreApplication::processEvents();
  }
}

static main_window_c *gui;

rightclick_tree_widget::rightclick_tree_widget(QWidget *parent):
  QTreeWidget(parent) {
}

void
rightclick_tree_widget::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::RightButton) {
    QTreeWidget::mousePressEvent(event);
    return;
  }

  QTreeWidgetItem *item = itemAt(event->pos());
  if (NULL != item) {
#if QT_VERSION >= 0x040200
    gui->expand_all_elements(item, !item->isExpanded());
#else   // QT_VERSION >= 0x040200
    gui->expand_all_elements(item, !item->treeWidget()->isItemExpanded(item));
#endif  // QT_VERSION >= 0x040200
  }
}

void
ui_show_error(const char *text) {
  if (use_gui)
    gui->show_error(QString::fromUtf8(text));
  else
    console_show_error(text);
}

void
ui_show_element(int level,
                const char *text,
                int64_t position) {
  if (!use_gui)
    console_show_element(level, text, position);

  else if (0 <= position) {
    string buffer = mxsprintf("%s at " LLD, text, position);
    gui->add_item(level, QString::fromUtf8(buffer.c_str()));

  } else
    gui->add_item(level, QString::fromUtf8(text));
}

void
ui_show_progress(int percentage,
                 const char *text) {
  gui->show_progress(percentage, QString::fromUtf8(text));
}

int
ui_run(int argc,
       char **argv) {
  vector<string> args;
  string initial_file;

  QApplication app(argc, argv);
  main_window_c main_window;
  gui = &main_window;
  main_window.show();

  args = command_line_utf8(argc, argv);
  parse_args(args, initial_file);

  if (initial_file != "")
    gui->parse_file(QString::fromUtf8(initial_file.c_str()));

  return app.exec();
}

bool
ui_graphical_available() {
  return true;
}

