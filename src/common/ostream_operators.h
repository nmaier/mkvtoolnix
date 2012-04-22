/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   generic "operator <<()" for standard types

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_OSTREAM_OPERATORS_H
#define MTX_COMMON_OSTREAM_OPERATORS_H

namespace std {

template<typename CharT, typename TraitsT, typename ContT>
std::basic_ostream<CharT, TraitsT> &
operator <<(std::basic_ostream<CharT, TraitsT> &out,
            std::vector<ContT> const &vec) {
  out << "<";
  bool first = true;
  for (auto element : vec) {
    if (first)
      first = false;
    else
      out << ",";
    out << element;
  }
  out << ">";

  return out;
}

template<typename CharT, typename TraitsT, typename PairT1, typename PairT2>
std::basic_ostream<CharT, TraitsT> &
operator <<(std::basic_ostream<CharT, TraitsT> &out,
            std::pair<PairT1, PairT2> const &p) {
  out << "(" << p.first << "/" << p.second << ")";
  return out;
}

template<typename ElementT>
std::ostream &
operator <<(std::ostream &out,
            ElementT const &e) {
  return e.streamify(out);
}

template<typename ElementT>
std::wostream &
operator <<(std::wostream &out,
            ElementT const &e) {
  std::stringstream temp;
  e.streamify(temp);
  out << to_wide(temp.str());
  return out;
}

}

#endif  // MTX_COMMON_OSTREAM_OPERATORS_H
