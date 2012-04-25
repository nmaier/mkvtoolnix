/**************************************************************************
**
** This file is adapted from stylehelper.h which is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU General Public License Usage
**
** This file may be used under the terms of the GNU General Public
** License version 2 as published by the Free Software Foundation and
** appearing in the file COPYING included in the packaging of this file.
** Please review the following information to ensure the GNU General
** Public License version 2 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/gpl-2.0.html.
**
**************************************************************************/

#ifndef MTX_MKVTOOLNIX_GUI_UTIL_STYLE_HELPER_H
#define MTX_MKVTOOLNIX_GUI_UTIL_STYLE_HELPER_H

#include <QtGui/QColor>
#include <QtGui/QStyle>

class QPalette;
class QPainter;
class QRect;

// Helper class holding all custom color values

class StyleHelper
{
public:
    static const unsigned int DEFAULT_BASE_COLOR = 0x666666;

    // Height of the project explorer navigation bar
    static int navigationWidgetHeight() { return 24; }
    static qreal sidebarFontSize();
    static QPalette sidebarFontPalette(const QPalette &original);

    // This is our color table, all colors derive from baseColor
    static QColor requestedBaseColor() { return m_requestedBaseColor; }
    static QColor baseColor(bool lightColored = false);
    static QColor panelTextColor(bool lightColored = false);
    static QColor highlightColor(bool lightColored = false);
    static QColor shadowColor(bool lightColored = false);
    static QColor borderColor(bool lightColored = false);
    static QColor buttonTextColor() { return QColor(0x4c4c4c); }
    static QColor mergedColors(const QColor &colorA, const QColor &colorB, int factor = 50);

    static QColor sidebarHighlight() { return QColor(255, 255, 255, 40); }
    static QColor sidebarShadow() { return QColor(0, 0, 0, 40); }

    // Sets the base color and makes sure all top level widgets are updated
    static void setBaseColor(const QColor &color);

    // Draws a shaded anti-aliased arrow
    static void drawArrow(QStyle::PrimitiveElement element, QPainter *painter, const QStyleOption *option);

    // Gradients used for panels
    static void horizontalGradient(QPainter *painter, const QRect &spanRect, const QRect &clipRect, bool lightColored = false);
    static void verticalGradient(QPainter *painter, const QRect &spanRect, const QRect &clipRect, bool lightColored = false);
    static void menuGradient(QPainter *painter, const QRect &spanRect, const QRect &clipRect);
    static bool usePixmapCache() { return true; }

    static void drawIconWithShadow(const QIcon &icon, const QRect &rect, QPainter *p, QIcon::Mode iconMode,
                                   int radius = 3, const QColor &color = QColor(0, 0, 0, 130),
                                   const QPoint &offset = QPoint(1, -2));
    static void drawCornerImage(const QImage &img, QPainter *painter, QRect rect,
                         int left = 0, int top = 0, int right = 0, int bottom = 0);

    static void tintImage(QImage &img, const QColor &tintColor);

private:
    static QColor m_baseColor;
    static QColor m_requestedBaseColor;
};

#endif // MTX_MKVTOOLNIX_GUI_UTIL_STYLE_HELPER_H
