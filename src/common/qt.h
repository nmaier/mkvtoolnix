/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used by Qt GUIs

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_QT_H
#define MTX_COMMON_QT_H

#define Q(s)  to_qs(s)
#define QY(s) to_qs(Y(s))

inline QString
to_qs(std::string const &source) {
  return QString::fromUtf8(source.c_str());
}

inline QString
to_qs(std::wstring const &source) {
  return QString::fromStdWString(source);
}

inline std::string
to_utf8(QString const &source) {
  return std::string{ source.toUtf8() };
}

inline std::wstring
to_wide(QString const &source) {
  return source.toStdWString();
}

#endif  // MTX_COMMON_QT_H
