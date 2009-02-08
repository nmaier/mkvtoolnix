/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   translation, locale handling

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#if HAVE_NL_LANGINFO
# include <langinfo.h>
#elif HAVE_LOCALE_CHARSET
# include <libcharset.h>
#endif
#if defined(HAVE_LIBINTL_H)
# include <libintl.h>
#endif
#include <locale.h>

#include "common.h"
#include "translation.h"

std::vector<translation_c> translation_c::ms_available_translations;

translation_c::translation_c(const std::string &locale,
                             const std::string &english_name,
                             const std::string &translated_name)
  : m_locale(locale)
  , m_english_name(english_name)
  , m_translated_name(translated_name)
{
}

void
translation_c::initialize_available_translations() {
  ms_available_translations.push_back(translation_c("en_US", "English", "English"));
  ms_available_translations.push_back(translation_c("de_DE", "German",  "Deutsch"));
}

bool
translation_c::is_translation_available(const std::string &locale) {
  std::vector<translation_c>::iterator i = ms_available_translations.begin();
  while (i != ms_available_translations.end()) {
    if (i->m_locale == locale)
      return true;
    ++i;
  }

  return false;
}

void
init_locales(std::string locale) {
#if ! defined(SYS_WINDOWS) && defined(HAVE_LIBINTL_H)
  translation_c::initialize_available_translations();

  if (!locale.empty() && !translation_c::is_translation_available(locale))
    locale = "";

  if (setlocale(LC_MESSAGES, locale.c_str()) == NULL)
    mxerror("The locale could not be set properly. Check the LANG, LC_ALL and LC_MESSAGES environment variables.\n");
  bindtextdomain("mkvtoolnix", MTX_LOCALE_DIR);
  textdomain("mkvtoolnix");
  bind_textdomain_codeset("mkvtoolnix", "UTF-8");
#endif
}

