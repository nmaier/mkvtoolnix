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
                             const std::string &windows_locale_sysname,
                             const std::string &english_name,
                             const std::string &translated_name)
  : m_unix_locale(unix_locale)
  , m_windows_locale(windows_locale)
  , m_windows_locale_sysname(windows_locale_sysname)
  , m_english_name(english_name)
  , m_translated_name(translated_name)
{
}

void
translation_c::initialize_available_translations() {
  ms_available_translations.clear();
#if defined(HAVE_LIBINTL_H)
  ms_available_translations.push_back(translation_c("en_US", "en", "english", "English", "English"));
  ms_available_translations.push_back(translation_c("de_DE", "de", "german",  "German",  "Deutsch"));
#endif
}

int
translation_c::look_up_translation(const std::string &locale) {
  string locale_lower                    = downcase(locale);
  std::vector<translation_c>::iterator i = ms_available_translations.begin();
  int idx                                = 0;

  while (i != ms_available_translations.end()) {
    if (   (downcase(i->get_locale()) == locale_lower)
#if defined(SYS_WINDOWS)
        || (downcase(i->m_windows_locale_sysname) == locale_lower)
#endif
         )
      return idx;
    ++i;
    ++idx;
  }

  return -1;
}

std::string
translation_c::get_default_ui_locale() {
  std::string locale;

#if defined(HAVE_LIBINTL_H)
# if defined(SYS_WINDOWS)
  char *data;
  int len = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, NULL, 0);
  if (0 < len) {
    data = (char *)safemalloc(len);
    memset(data, 0, len);
    if (0 != GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, data, len))
      locale = data;
    safefree(data);

    int idx = translation_c::look_up_translation(locale);
    if (-1 != idx)
      locale = ms_available_translations[idx].get_locale();
  }

# else  // SYS_WINDOWS

  char *data = setlocale(LC_MESSAGES, NULL);
  if (NULL != data) {
    std::string previous_locale = data;
    setlocale(LC_MESSAGES, "");
    data = setlocale(LC_MESSAGES, NULL);

    if (NULL != data) {
      std::vector<std::string> parts = split(data, ".");
      locale                         = parts[0];
    }

    setlocale(LC_MESSAGES, previous_locale.c_str());
  }

# endif // SYS_WINDOWS
#endif  // HAVE_LIBINTL_H

  if (!locale.empty() && (-1 == look_up_translation(locale)))
    locale = "";

  return locale;
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

  if (-1 == translation_c::look_up_translation(locale))
    locale = "";

  if (locale.empty())
    locale = translation_c::get_default_ui_locale();

# if defined(SYS_WINDOWS)
  if (!locale.empty()) {
    // The Windows system headers define LC_MESSAGES but
    // Windows itself doesn't know it and treats "set_locale(LC_MESSAGES, ...)"
    // as an error. gettext uses the LANG and LC_MESSAGE environment variables instead.

    // Windows knows two environments: the system environment that's
    // modified by SetEnvironmentVariable() and the C library's cache
    // of said environment which is modified via _putenv().

    set_environment_variable("LANG",        locale);
    set_environment_variable("LC_MESSAGES", locale);
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
