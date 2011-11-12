/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   run code at scope exit

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_AT_SCOPE_EXIT_H
#define MTX_COMMON_AT_SCOPE_EXIT_H

class at_scope_exit_c {
private:
  std::function<void()> m_code;
public:
  at_scope_exit_c(const std::function<void()> &code) : m_code(code) {}
  ~at_scope_exit_c() {
    m_code();
  }
};

#endif  // MTX_COMMON_AT_SCOPE_EXIT_H
