/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   A Qt GUI for mkvinfo

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __RIGHTCLICK_TREE_WIDGET_H
#define __RIGHTCLICK_TREE_WIDGET_H

#include "common/os.h"

#include <QTreeWidget>

class rightclick_tree_widget: public QTreeWidget {
  Q_OBJECT;

public slots:

public:
  rightclick_tree_widget(QWidget *parent = nullptr);

protected:
  virtual void mousePressEvent(QMouseEvent *event);
};

#endif // __RIGHTCLICK_TREE_WIDGET_H
