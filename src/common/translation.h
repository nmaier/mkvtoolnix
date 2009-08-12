/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   translation, locale handling

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_TRANSLATION_H
#define __MTX_COMMON_TRANSLATION_H

#include "common/os.h"

#include <string>
#include <vector>

class MTX_DLL_API translation_c {
public:
  static std::vector<translation_c> ms_available_translations;

public:
  std::string m_unix_locale, m_windows_locale, m_windows_locale_sysname, m_english_name, m_translated_name;

  translation_c(const std::string &unix_locale,
                const std::string &windows_locale,
                const std::string &windows_locale_sysname,
                const std::string &english_name,
                const std::string &translated_name);

  std::string get_locale();

  static void initialize_available_translations();
  static int look_up_translation(const std::string &locale);
  static std::string get_default_ui_locale();
};

class MTX_DLL_API translatable_string_c {
protected:
  std::string m_untranslated_string;

public:
  translatable_string_c();
  translatable_string_c(const std::string &untranslated_string);
  translatable_string_c(const char *untranslated_string);

  std::string get_translated() const;
  std::string get_untranslated() const;
};

#define YT(s) translatable_string_c(s)

void MTX_DLL_API init_locales(std::string locale = "");

#endif  // __MTX_COMMON_TRANSLATION_H
