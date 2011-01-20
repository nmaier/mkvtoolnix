#include "os.h"

#include "common.h"
#include "qtcommon.h"

#include "mmg_qt.h"

#include "file_prober.h"
#include "main_window.h"

#include <QFileDialog>
#include <QList>
#include <QMessageBox>
#include <QString>

struct file_type_and_ext_t {
  QString description, extensions;

  file_type_and_ext_t(const QString &p_description,
                      const QString &p_extensions)
    : description(p_description)
    , extensions(p_extensions)
  {
  }
};

void
main_window_c::setup_input_file_filter() {
  QList<file_type_and_ext_t> raw_list;

  raw_list << file_type_and_ext_t(Q(Y("A/52 (aka AC3)")),                      Q("ac3"));
  raw_list << file_type_and_ext_t(Q(Y("AAC (Advanced Audio Coding)")),         Q("aac m4a mp4"));
  raw_list << file_type_and_ext_t(Q(Y("AVC/h.264 elementary streams")),        Q("264 avc h264 x264"));
  raw_list << file_type_and_ext_t(Q(Y("AVI (Audio/Video Interleaved)")),       Q("avi"));
  raw_list << file_type_and_ext_t(Q(Y("Dirac")),                               Q("drc"));
  raw_list << file_type_and_ext_t(Q(Y("DTS (Digital Theater System)")),        Q("dts"));
  if (m_capabilities[Q("FLAC")] == "true")
    raw_list << file_type_and_ext_t(Q(Y("FLAC (Free Lossless Audio Codec)")),  Q("flac ogg"));
  raw_list << file_type_and_ext_t(Q(Y("Matroska audio/video files")),          Q("mka mks mkv mk3d"));
  raw_list << file_type_and_ext_t(Q(Y("MP4 audio/video files")),               Q("mp4 m4v"));
  raw_list << file_type_and_ext_t(Q(Y("MPEG audio files")),                    Q("mp2 mp3"));
  raw_list << file_type_and_ext_t(Q(Y("MPEG program streams")),                Q("mpg mpeg m2v evo evob vob"));
  raw_list << file_type_and_ext_t(Q(Y("MPEG video elementary streams")),       Q("m1v m2v"));
  raw_list << file_type_and_ext_t(Q(Y("Ogg/OGM audio/video files")),           Q("ogg ogm"));
  raw_list << file_type_and_ext_t(Q(Y("QuickTime audio/video files")),         Q("mov"));
  raw_list << file_type_and_ext_t(Q(Y("RealMedia audio/video files")),         Q("ra ram rm rmvb rv"));
  raw_list << file_type_and_ext_t(Q(Y("SRT text subtitles")),                  Q("srt"));
  raw_list << file_type_and_ext_t(Q(Y("SSA/ASS text subtitles")),              Q("ass ssa"));
  raw_list << file_type_and_ext_t(Q(Y("TTA (The lossless True Audio codec)")), Q("tta"));
  raw_list << file_type_and_ext_t(Q(Y("USF text subtitles")),                  Q("usf xml"));
  raw_list << file_type_and_ext_t(Q(Y("VC1 elementary streams")),              Q("vc1"));
  raw_list << file_type_and_ext_t(Q(Y("VobButtons")),                          Q("btn"));
  raw_list << file_type_and_ext_t(Q(Y("VobSub subtitles")),                    Q("idx"));
  raw_list << file_type_and_ext_t(Q(Y("WAVE (uncompressed PCM audio)")),       Q("wav"));
  raw_list << file_type_and_ext_t(Q(Y("WAVPACK v4 audio")),                    Q("wv"));

  QHash<QString, bool> extension_map;
  QStringList filters;

  int i;
  for (i = 0; raw_list.size() > i; ++i) {
    file_type_and_ext_t &element = raw_list[i];
    QStringList extensions       = element.extensions.split(L' ');

    int k;
    for (k = 0; extensions.size() > k; ++k)
      extension_map[extensions[k]] = true;

    if (!m_input_file_filter.isEmpty())
      m_input_file_filter.append(Q(";;"));

    filters << Q("%1 (*.%2)").arg(element.description).arg(extensions.join(Q(" *.")));
  }

  filters << Q(Y("All files (*.*)"));

  QStringList all_extensions;
  QHash<QString, bool>::const_iterator idx = extension_map.begin();
  while (idx != extension_map.end()) {
    all_extensions << idx.key();
    ++idx;
  }

  all_extensions.sort();

  filters.prepend(Q(Y("All known file types (*.%1)")).arg(all_extensions.join(Q(" *."))));

  m_input_file_filter = filters.join(Q(";;"));
}

void
main_window_c::setup_input_tab() {
  setup_input_file_filter();
}

bool
main_window_c::select_input_file(bool for_appending) {
  QFileDialog dialog(this, for_appending ? Q(Y("Append a file")) : Q(Y("Add a file")), m_previous_directory, m_input_file_filter);
  dialog.setAcceptMode(QFileDialog::AcceptOpen);
  dialog.setFileMode(QFileDialog::ExistingFile);

  if (!dialog.exec())
    return false;

  m_previous_directory = dialog.directory().absolutePath();
  QString file_name    = dialog.selectedFiles()[0];

  file_prober_c prober(this);
  int exit_code = prober.run(file_name);

  if (0 > exit_code)
    return false;

  return true;
}

void
main_window_c::on_pb_add_file_clicked() {
  select_input_file(false);
}

void
main_window_c::on_pb_append_file_clicked() {
  select_input_file(true);
}
