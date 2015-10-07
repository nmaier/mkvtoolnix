/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   translation, locale handling

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if HAVE_NL_LANGINFO
# include <langinfo.h>
#elif HAVE_LOCALE_CHARSET
# include <libcharset.h>
#endif
#include <locale>
#include <locale.h>
#include <stdlib.h>

#include "common/locale_string.h"
#include "common/utf8_codecvt_facet.h"
#include "common/strings/editing.h"
#include "common/translation.h"

#if defined(SYS_WINDOWS)
# include <windows.h>
# include <winnls.h>

# include "common/fs_sys_helpers.h"
# include "common/memory.h"
#endif

std::vector<translation_c> translation_c::ms_available_translations;
int translation_c::ms_active_translation_idx = 0;

translation_c::translation_c(const std::string &unix_locale,
                             const std::string &windows_locale,
                             const std::string &windows_locale_sysname,
                             const std::string &english_name,
                             const std::string &translated_name,
                             bool line_breaks_anywhere,
                             int language_id,
                             int sub_language_id)
  : m_unix_locale(unix_locale)
  , m_windows_locale(windows_locale)
  , m_windows_locale_sysname(windows_locale_sysname)
  , m_english_name(english_name)
  , m_translated_name(translated_name)
  , m_line_breaks_anywhere(line_breaks_anywhere)
  , m_language_id(language_id)
  , m_sub_language_id(sub_language_id)
{
}

// See http://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx for the (sub) language IDs.
void
translation_c::initialize_available_translations() {
  ms_available_translations.clear();
  ms_available_translations.emplace_back("en_US", "en",    "english",    "English",              "English",             false, 0x0009, 0x00);
#if defined(HAVE_LIBINTL_H)
  ms_available_translations.emplace_back("cs_CZ", "cs",    "czech",      "Czech",                "Čeština",             false, 0x0005, 0x00);
  ms_available_translations.emplace_back("de_DE", "de",    "german",     "German",               "Deutsch",             false, 0x0007, 0x00);
  ms_available_translations.emplace_back("es_ES", "es",    "spanish",    "Spanish",              "Español",             false, 0x000a, 0x00);
  ms_available_translations.emplace_back("eu_ES", "eu",    "basque",     "Basque",               "Euskara",             false, 0x002d, 0x00);
  ms_available_translations.emplace_back("fr_FR", "fr",    "french",     "French",               "Français",            false, 0x000c, 0x00);
  ms_available_translations.emplace_back("it_IT", "it",    "italian",    "Italian",              "Italiano",            false, 0x0010, 0x00);
  ms_available_translations.emplace_back("ja_JP", "ja",    "japanese",   "Japanese",             "日本語",              true,  0x0011, 0x00);
  ms_available_translations.emplace_back("lt_LT", "lt",    "lithuanian", "Lithuanian",           "Lietuvių",            false, 0x0027, 0x00);
  ms_available_translations.emplace_back("nl_NL", "nl",    "dutch",      "Dutch",                "Nederlands",          false, 0x0013, 0x00);
  ms_available_translations.emplace_back("pl_PL", "pl",    "polish",     "Polish",               "Polski",              false, 0x0015, 0x00);
  ms_available_translations.emplace_back("pt_BR", "pt_BR", "portuguese", "Brazilian Portuguese", "Português do Brasil", false, 0x0016, 0x01);
  ms_available_translations.emplace_back("pt_PT", "pt",    "portuguese", "Portuguese",           "Português",           false, 0x0016, 0x02);
  ms_available_translations.emplace_back("ru_RU", "ru",    "russian",    "Russian",              "Русский",             false, 0x0019, 0x00);
  ms_available_translations.emplace_back("tr_TR", "tr",    "turkish",    "Turkish",              "Türkçe",              false, 0x001f, 0x00);
  ms_available_translations.emplace_back("uk_UA", "uk",    "ukrainian",  "Ukrainian",            "Український",         false, 0x0022, 0x00);
  ms_available_translations.emplace_back("zh_CN", "zh_CN", "chinese",    "Chinese Simplified",   "简体中文",            true,  0x0004, 0x02);
  ms_available_translations.emplace_back("zh_TW", "zh_TW", "chinese",    "Chinese Traditional",  "繁體中文",            true,  0x0004, 0x01);
#endif

  ms_active_translation_idx = 0;
}

int
translation_c::look_up_translation(const std::string &locale) {
  try {
    std::string locale_country_lang        = locale_string_c(locale).str(locale_string_c::half);
    std::vector<translation_c>::iterator i = ms_available_translations.begin();
    int idx                                = 0;

    while (i != ms_available_translations.end()) {
      if (   balg::iequals(locale_country_lang, i->get_locale())
#if defined(SYS_WINDOWS)
          || balg::iequals(locale_country_lang, i->m_windows_locale_sysname)
#endif
           )
        return idx;
      ++i;
      ++idx;
    }

  } catch (mtx::locale_string_format_x &) {
  }

  return -1;
}

int
translation_c::look_up_translation(int language_id, int sub_language_id) {
  auto ptr = brng::find_if(ms_available_translations, [language_id,sub_language_id](translation_c const &tr) {
      return (tr.m_language_id == language_id) && (!tr.m_sub_language_id || (tr.m_sub_language_id == sub_language_id));
    });

  int idx = ptr == ms_available_translations.end() ? -1 : std::distance(ms_available_translations.begin(), ptr);
  mxdebug_if(debugging_c::requested("locale"), boost::format("look_up_translation for 0x%|1$04x|/0x%|2$02x|: %3%\n") % language_id % sub_language_id % idx);

  return idx;
}

std::string
translation_c::get_default_ui_locale() {
  std::string locale;
  bool debug = debugging_c::requested("locale");

#if defined(HAVE_LIBINTL_H)
# if defined(SYS_WINDOWS)
  std::string env_var = get_environment_variable("LC_MESSAGES");
  if (!env_var.empty() && (-1 != look_up_translation(env_var)))
    return env_var;

  env_var = get_environment_variable("LANG");
  if (!env_var.empty() && (-1 != look_up_translation(env_var)))
    return env_var;

  auto lang_id = GetUserDefaultUILanguage();
  int idx      = translation_c::look_up_translation(lang_id & 0x3ff, (lang_id >> 10) & 0x3f);
  if (-1 != idx)
    locale = ms_available_translations[idx].get_locale();

  mxdebug_if(debug, boost::format("[lang_id %|1$04x| idx %2% locale %3%\n") % lang_id % idx % locale);

# else  // SYS_WINDOWS

  char *data = setlocale(LC_MESSAGES, nullptr);
  if (data) {
    std::string previous_locale = data;
    mxdebug_if(debug, boost::format("[get_default_ui_locale previous %1%]\n") % previous_locale);
    setlocale(LC_MESSAGES, "");
    data = setlocale(LC_MESSAGES, nullptr);

    if (data)
      locale = data;

    mxdebug_if(debug, boost::format("[get_default_ui_locale new %1%]\n") % locale);

    setlocale(LC_MESSAGES, previous_locale.c_str());
  } else
    mxdebug_if(debug, boost::format("[get_default_ui_locale get previous failed]\n"));

# endif // SYS_WINDOWS
#endif  // HAVE_LIBINTL_H

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

translation_c &
translation_c::get_active_translation() {
  return ms_available_translations[ms_active_translation_idx];
}

void
translation_c::set_active_translation(const std::string &locale) {
  int idx                   = look_up_translation(locale);
  ms_active_translation_idx = std::max(idx, 0);

  if (debugging_c::requested("locale"))
    mxinfo(boost::format("[translation_c::set_active_translation() active_translation_idx %1% for locale %2%]\n") % ms_active_translation_idx % locale);
}

// ------------------------------------------------------------

translatable_string_c::translatable_string_c()
{
}

translatable_string_c::translatable_string_c(const std::string &untranslated_string)
  : m_untranslated_string(untranslated_string)
{
}

translatable_string_c::translatable_string_c(const char *untranslated_string)
  : m_untranslated_string(untranslated_string)
{
}

std::string
translatable_string_c::get_translated()
  const
{
  return m_untranslated_string.empty() ? "" : Y(m_untranslated_string.c_str());
}

std::string
translatable_string_c::get_untranslated()
  const
{
  return m_untranslated_string;
}

// ------------------------------------------------------------

#if defined(HAVE_LIBINTL_H)

void
init_locales(std::string locale) {
  translation_c::initialize_available_translations();

  if (debugging_c::requested("locale"))
    mxinfo(boost::format("[init_locales start: locale %1%]\n") % locale);

  std::string locale_dir;
  std::string default_locale = translation_c::get_default_ui_locale();

  if (-1 == translation_c::look_up_translation(locale)) {
    if (debugging_c::requested("locale"))
      mxinfo(boost::format("[init_locales lookup failed; clearing locale]\n"));
    locale = "";
  }

  if (locale.empty()) {
    locale = default_locale;
    if (debugging_c::requested("locale"))
      mxinfo(boost::format("[init_locales setting to default locale %1%]\n") % locale);
  }

# if defined(SYS_WINDOWS)
  set_environment_variable("LANGUAGE", "");

  if (!locale.empty()) {
    // The Windows system headers define LC_MESSAGES but
    // Windows itself doesn't know it and treats "set_locale(LC_MESSAGES, ...)"
    // as an error. gettext uses the LANG and LC_MESSAGE environment variables instead.

    // Windows knows two environments: the system environment that's
    // modified by SetEnvironmentVariable() and the C library's cache
    // of said environment which is modified via _putenv().

    set_environment_variable("LANG",        locale);
    set_environment_variable("LC_MESSAGES", locale);

    translation_c::set_active_translation(locale);

    // Boost's path class uses wide chars on Windows for path
    // names. Tell that all narrow strings are encoded in UTF-8.
    std::locale utf8_locale(std::locale(), new mtx::utf8_codecvt_facet);
    std::locale::global(utf8_locale);
    boost::filesystem::path::imbue(utf8_locale);
  }

  locale_dir = (mtx::get_installation_path() / "locale").string();

# else  // SYS_WINDOWS
  std::string chosen_locale;

  try {
    locale_string_c loc_default(default_locale);
    std::string loc_req_with_default_codeset(locale_string_c(locale).set_codeset_and_modifier(loc_default).str());

    if (setlocale(LC_MESSAGES, loc_req_with_default_codeset.c_str()))
      chosen_locale = loc_req_with_default_codeset;

    else if (setlocale(LC_MESSAGES, locale.c_str()))
      chosen_locale = locale;

    else {
      std::string loc_req_with_utf8 = locale_string_c(locale).set_codeset_and_modifier(locale_string_c("dummy.UTF-8")).str();
      if (setlocale(LC_MESSAGES, loc_req_with_utf8.c_str()))
        chosen_locale = loc_req_with_utf8;
    }

  } catch (mtx::locale_string_format_x &error) {
    if (debugging_c::requested("locale"))
      mxinfo(boost::format("[init_locales format error in %1%]\n") % error.error());
  }

  if (debugging_c::requested("locale"))
    mxinfo(boost::format("[init_locales chosen locale %1%]\n") % chosen_locale);

  // Hard fallback to "C" locale if no suitable locale was
  // selected. This can happen if the system has no locales for
  // "en_US" or "en_US.UTF-8" compiled.
  if (chosen_locale.empty() && setlocale(LC_MESSAGES, "C"))
    chosen_locale = "C";

  if (chosen_locale.empty())
    mxerror(Y("The locale could not be set properly. Check the LANG, LC_ALL and LC_MESSAGES environment variables.\n"));

  std::locale utf8_locale(std::locale(), new mtx::utf8_codecvt_facet);
  std::locale::global(utf8_locale);

  translation_c::set_active_translation(chosen_locale);

  locale_dir = MTX_LOCALE_DIR;
# endif  // SYS_WINDOWS

# if defined(SYS_APPLE)
  int result = setenv("LC_MESSAGES", chosen_locale.c_str(), 1);
  if (debugging_c::requested("locale"))
    mxinfo(boost::format("[init_locales setenv() return code: %1%]\n") % result);
# endif

  bindtextdomain("mkvtoolnix", locale_dir.c_str());
  textdomain("mkvtoolnix");
  bind_textdomain_codeset("mkvtoolnix", "UTF-8");
}

#else  // HAVE_LIBINTL_H

void
init_locales(std::string) {
  translation_c::initialize_available_translations();
}

#endif  // HAVE_LIBINTL_H
