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

#include <QString>

#define Q(s)  to_qs(s)
#define QY(s) to_qs(Y(s))
#define QNY(singular, plural, count) to_qs(NY(singular, plural, count))

inline QString
to_qs(std::string const &source) {
  return QString::fromUtf8(source.c_str());
}

inline QString
to_qs(std::wstring const &source) {
  return QString::fromStdWString(source);
}

inline QString
to_qs(boost::format const &source) {
  return QString::fromUtf8(source.str().c_str());
}

inline std::string
to_utf8(QString const &source) {
  return std::string{ source.toUtf8().data() };
}

inline std::wstring
to_wide(QString const &source) {
  return source.toStdWString();
}

inline void
mxinfo(QString const &s) {
  mxinfo(to_utf8(s));
}

inline std::ostream &
operator <<(std::ostream &out,
            QString const &string) {
  out << std::string{string.toUtf8().data()};
  return out;
}

inline std::wostream &
operator <<(std::wostream &out,
            QString const &string) {
  out << string.toStdWString();
  return out;
}

#endif  // MTX_COMMON_QT_H
