#include "common/common_pch.h"

#include "common/file_types.h"
#include "common/qt.h"
#include "mkvtoolnix-gui/util/file_type_filter.h"

#include <QStringList>

QStringList FileTypeFilter::s_filter;

QStringList const &
FileTypeFilter::get() {
  if (!s_filter.isEmpty())
    return s_filter;

  auto &file_types = file_type_t::get_supported();

  std::map<QString, bool> all_extensions_map;
  for (auto &file_type : file_types) {
    auto extensions = to_qs(file_type.extensions).split(" ");
    QStringList extensions_full;

    for (auto &extension : extensions) {
      all_extensions_map[extension] = true;
      extensions_full << QString{"*.%1"}.arg(extension);

#if !defined(SYS_WINDOWS)
      auto extension_upper = extension.toUpper();
      all_extensions_map[extension_upper] = true;
      if (extension_upper != extension)
        extensions_full << QString("*.%1").arg(extension_upper);
#endif  // !SYS_WINDOWS
    }

    s_filter << QString("%1 (%2)").arg(to_qs(file_type.title)).arg(extensions_full.join(" "));
  }

  QStringList all_extensions;
  for (auto &extension : all_extensions_map)
    all_extensions << QString("*.%1").arg(extension.first);

  std::sort(s_filter.begin(), s_filter.end());

  s_filter.push_front(QY("All files (*)"));
  s_filter.push_front(QY("All supported media files (%1)").arg(all_extensions.join(" ")));

  return s_filter;
}
