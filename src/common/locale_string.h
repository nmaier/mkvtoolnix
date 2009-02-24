/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   locale string splitting and creation

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <string>

#include "error.h"

class locale_string_format_error_c: public error_c {
public:
  std::string m_format;

public:
  locale_string_format_error_c(std::string format)
    : m_format(format)
  {
  };
};

class locale_string_c {
protected:
  std::string m_language, m_territory, m_codeset, m_modifier;

public:
  enum eval_type_e {
    territory = 1,
    codeset   = 2,
    modifier  = 4,

    half      = 1,
    full      = 7,
  };

public:
  locale_string_c(std::string locale);

  locale_string_c &set_codeset_and_modifier(const locale_string_c &src);
  std::string str(eval_type_e type = full);
};
