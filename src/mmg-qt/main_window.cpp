#include "os.h"

#include <stdio.h>

#include "common.h"
#include "qtcommon.h"
#include "common/version.h"

#include "mmg_qt.h"

#include "capabilities_reader.h"
#include "main_window.h"

#include <ebml/EbmlVersion.h>
#include <matroska/KaxVersion.h>

#include <QMessageBox>

using namespace libmatroska;

main_window_c *g_main_window = nullptr;

main_window_c::main_window_c()
  : m_previous_directory(Q("/home/mosu/prog/video/mkvtoolnix/data"))
{
  setupUi(this);

  g_main_window = this;

  query_mkvmerge_capabilities();
  setup_input_tab();
}

main_window_c::~main_window_c() {
}

void
main_window_c::query_mkvmerge_capabilities() {
  capabilities_reader_c reader(this);
  reader.run();
}

mkvmerge_settings_t &
main_window_c::get_mkvmerge_settings() {
  return m_mkvmerge_settings;
}

void
main_window_c::set_capabilities(QHash<QString, QString> &new_capabilities) {
  m_capabilities = new_capabilities;
}

QString
main_window_c::get_capability(const QString &key) {
  return m_capabilities[key];
}

void
main_window_c::add_debug_message(const QString &message) {
  QFile file;
  file.open(stdout, QIODevice::WriteOnly);
  file.write(message.toUtf8());
  file.close();
}

// Menu "Help"

void
main_window_c::on_action_about_activated() {
  QString msg =
    Q(Y("%1.\n"
        "Compiled with libebml %2 + libmatroska %3.\n\n"
        "This program is licensed under the GPL v2 (see COPYING).\n"
        "It was written by Moritz Bunkus <moritz@bunkus.org>.\n\n"
        "Sources and the latest binaries are always available at\n"
        "http://www.bunkus.org/videotools/mkvtoolnix/"))
    .arg(Q(get_version_info("mkvmerge GUI (Qt version)").c_str()))
    .arg(Q(EbmlCodeVersion.c_str()))
    .arg(Q(KaxCodeVersion.c_str()));

  QMessageBox::about(this, Q(Y("About mmg/Qt")), msg);
}

void
main_window_c::on_action_show_help_activated() {
  query_mkvmerge_capabilities();
}

// Menu "Window"

void
main_window_c::on_action_show_input_tab_activated() {
  tw_main_window->setCurrentIndex(0);
}

void
main_window_c::on_action_show_global_options_tab_activated() {
  tw_main_window->setCurrentIndex(1);
}

void
main_window_c::on_action_show_splitting_and_linking_tab_activated() {
  tw_main_window->setCurrentIndex(2);
}

void
main_window_c::on_action_show_attachments_tab_activated() {
  tw_main_window->setCurrentIndex(3);
}
