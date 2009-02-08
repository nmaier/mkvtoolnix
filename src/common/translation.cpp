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

#if defined(SYS_WINDOWS)
# include <windows.h>
# include "os_windows.h"
#endif

std::vector<translation_c> translation_c::ms_available_translations;

translation_c::translation_c(const std::string &unix_locale,
                             const std::string &windows_locale,
                             const std::string &english_name,
                             const std::string &translated_name)
  : m_unix_locale(unix_locale)
  , m_windows_locale(windows_locale)
  , m_english_name(english_name)
  , m_translated_name(translated_name)
{
}

void
translation_c::initialize_available_translations() {
  ms_available_translations.clear();
  ms_available_translations.push_back(translation_c("en_US", "english", "English", "English"));
  ms_available_translations.push_back(translation_c("de_DE", "german",  "German",  "Deutsch"));
}

int
translation_c::look_up_translation(const std::string &locale) {
  string locale_lower                    = downcase(locale);
  std::vector<translation_c>::iterator i = ms_available_translations.begin();
  int idx                                = 0;

  while (i != ms_available_translations.end()) {
    if (downcase(i->get_locale()) == locale_lower)
      return idx;
    ++i;
    ++idx;
  }

  return -1;
}

std::string
translation_c::get_locale() {
#if defined(SYS_WINDOWS)
  return m_windows_locale;
#else
  return m_unix_locale;
#endif
}

#if defined(HAVE_LIBINTL_H)

void
init_locales(std::string locale) {
  translation_c::initialize_available_translations();

  string locale_dir;

  if (!locale.empty()) {
    int idx = translation_c::look_up_translation(locale);
    if (-1 == idx)
      locale = "";
  }

# if defined(SYS_WINDOWS)
  if (locale.empty()) {
    char *data;
    int len = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, NULL, 0);
    if (0 < len) {
      data = (char *)safemalloc(len);
      memset(data, 0, len);
      if (0 != GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, data, len))
        locale = data;
      safefree(data);
    }
  }

  if (!locale.empty()) {
    // The Windows system headers define LC_MESSAGES but
    // Windows itself doesn't know it and treats "set_locale(LC_MESSAGES, ...)"
    // as an error. gettext uses the LANG environment variable instead.

    // Windows knows two environments: the system environment that's
    // modified by SetEnvironmentVariable() and the C library's cache
    // of said environment which is modified via _putenv().

    SetEnvironmentVariable("LANG", locale.c_str());
    std::string env_buf = (boost::format("LANG=%1%") % locale).str();
    _putenv(env_buf.c_str());
  }

  locale_dir = get_installation_path() + "\\locale";

# else  // SYS_WINDOWS
  if (setlocale(LC_MESSAGES, locale.c_str()) == NULL)
    mxerror(Y("The locale could not be set properly. Check the LANG, LC_ALL and LC_MESSAGES environment variables.\n"));

  locale_dir = MTX_LOCALE_DIR;
# endif  // SYS_WINDOWS

  bindtextdomain("mkvtoolnix", locale_dir.c_str());
  textdomain("mkvtoolnix");
  bind_textdomain_codeset("mkvtoolnix", "UTF-8");
}

#else  // HAVE_LIBINTL_H

void
init_locales(std::string) {
}

#endif  // HAVE_LIBINTL_H
