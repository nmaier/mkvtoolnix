/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   A Qt GUI for mkvinfo

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <ebml/EbmlVersion.h>
#include <matroska/KaxVersion.h>

#include "common/common_pch.h"
#include "common/locale.h"
#include "common/qt.h"
#include "common/version.h"
#include "info/qt_ui.h"
#include "info/mkvinfo.h"

#include <QMessageBox>
#include <QMouseEvent>
#include <QFileDialog>

using namespace libebml;
using namespace libmatroska;

main_window_c::main_window_c():
  last_percent(-1), num_elements(0),
  root(nullptr) {

  setupUi(this);

  connect(action_Open, SIGNAL(triggered()), this, SLOT(open()));
  connect(action_Save_text_file, SIGNAL(triggered()), this,
          SLOT(save_text_file()));
  connect(action_Exit, SIGNAL(triggered()), this, SLOT(close()));

  connect(action_Show_all, SIGNAL(triggered()), this, SLOT(show_all()));

  connect(action_About, SIGNAL(triggered()), this, SLOT(about()));

  action_Save_text_file->setEnabled(false);

  action_Show_all->setCheckable(true);
  action_Show_all->setChecked(0 < g_options.m_verbose);

  action_Expand_important->setCheckable(true);
  action_Expand_important->setChecked(true);

  tree->setHeaderLabels(QStringList(Q(Y("Elements"))));
  tree->setRootIsDecorated(true);

  root = new QTreeWidgetItem(tree);
  root->setText(0, Q(Y("no file loaded")));
}

void
main_window_c::open() {
  QString file_name = QFileDialog::getOpenFileName(this, Q(Y("Open File")), "", Q(Y("Matroska files (*.mkv *.mka *.mks *.mk3d);;All files (*.*)")));
  if (!file_name.isEmpty())
    parse_file(file_name);
}

void
main_window_c::save_text_file() {
  QString file_name = QFileDialog::getSaveFileName(this, Q(Y("Save information as")), "", Q(Y("Text files (*.txt);;All files (*.*)")));
  if (file_name.isEmpty())
    return;

  QFile file(file_name);
  if (!file.open(QIODevice::WriteOnly)) {
    QMessageBox::critical(this, Q(Y("Error saving the information")), Q(Y("The file could not be opened for writing.")));
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
  g_options.m_verbose = action_Show_all->isChecked() ? 2 : 0;
  if (!current_file.isEmpty())
    parse_file(current_file);
}

void
main_window_c::about() {
  QString msg =
    Q(Y("%1.\n"
        "Compiled with libebml %2 + libmatroska %3.\n\n"
        "This program is licensed under the GPL v2 (see COPYING).\n"
        "It was written by Moritz Bunkus <moritz@bunkus.org>.\n"
        "Sources and the latest binaries are always available at\n"
        "http://www.bunkus.org/videotools/mkvtoolnix/"))
    .arg(Q(get_version_info("mkvinfo GUI").c_str()))
    .arg(Q(EbmlCodeVersion.c_str()))
    .arg(Q(KaxCodeVersion.c_str()));
  QMessageBox::about(this, Q(Y("About mkvinfo")), msg);
}

void
main_window_c::show_error(const QString &msg) {
  QMessageBox::critical(this, Q(Y("Error")), msg);
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

  statusBar()->showMessage(Q(Y("Ready")), 5000);

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
  const QString s_segment(Q(Y("Segment")));
  const QString s_info(Q(Y("Segment information")));
  const QString s_tracks(Q(Y("Segment tracks")));

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
  if (nullptr != item) {
#if QT_VERSION >= 0x040200
    gui->expand_all_elements(item, !item->isExpanded());
#else   // QT_VERSION >= 0x040200
    gui->expand_all_elements(item, !item->treeWidget()->isItemExpanded(item));
#endif  // QT_VERSION >= 0x040200
  }
}

void
ui_show_error(const std::string &error) {
  if (g_options.m_use_gui)
    gui->show_error(Q(error.c_str()));
  else
    console_show_error(error);
}

void
ui_show_element(int level,
                const std::string &text,
                int64_t position,
                int64_t size) {
  if (!g_options.m_use_gui)
    console_show_element(level, text, position, size);

  else if (0 <= position)
    gui->add_item(level, Q(create_element_text(text, position, size).c_str()));

  else
    gui->add_item(level, Q(text.c_str()));
}

void
ui_show_progress(int percentage,
                 const std::string &text) {
  gui->show_progress(percentage, Q(text.c_str()));
}

int
ui_run(int argc,
       char **argv) {
  QApplication app(argc, argv);
  main_window_c main_window;
  gui = &main_window;
  main_window.show();

  if (!g_options.m_file_name.empty())
    gui->parse_file(Q(g_options.m_file_name.c_str()));

  return app.exec();
}

bool
ui_graphical_available() {
  return true;
}
