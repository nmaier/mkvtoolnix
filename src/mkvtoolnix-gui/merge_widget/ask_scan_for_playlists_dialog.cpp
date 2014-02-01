#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/forms/ask_scan_for_playlists_dialog.h"
#include "mkvtoolnix-gui/merge_widget/ask_scan_for_playlists_dialog.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QAbstractButton>
#include <QPushButton>

AskScanForPlaylistsDialog::AskScanForPlaylistsDialog(QWidget *parent)
  : QDialog{parent}
  , ui{new Ui::AskScanForPlaylistsDialog}
{
  // Setup UI controls.
  ui->setupUi(this);

  auto style    = QApplication::style();
  auto iconSize = style->pixelMetric(QStyle::PM_MessageBoxIconSize);

  ui->iconLabel->setPixmap(style->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(iconSize, iconSize));
  ui->scanPolicy->setCurrentIndex(static_cast<int>(Settings::get().m_scanForPlaylistsPolicy));

  auto yesButton = Util::buttonForRole(ui->buttonBox, QDialogButtonBox::YesRole);
  yesButton->setText(QY("&Scan for other playlists"));
  yesButton->setDefault(true);

  Util::buttonForRole(ui->buttonBox, QDialogButtonBox::NoRole)->setText(QY("&Don't scan, just add the file"));

  adjustSize();
}

AskScanForPlaylistsDialog::~AskScanForPlaylistsDialog() {
  delete ui;
}

bool
AskScanForPlaylistsDialog::ask(SourceFile const &file,
                               unsigned int numOtherFiles) {
  auto info1 = QStringList{
    QY("The file '%1' you've added is a playlist.").arg(QFileInfo{file.m_fileName}.fileName()),
    QNY("The directory it is located in contains %1 other file with the same extension.",
        "The directory it is located in contains %1 other files with the same extension.",
        numOtherFiles).arg(numOtherFiles),
    QY("The GUI can scan these files, present the results including duration and number of tracks of each playlist found and let you chose which one to add."),
  };

  auto minimumDuration = Settings::get().m_minimumPlaylistDuration;
  auto info2           = QStringList {
    QNY("Playlists shorter than %1 second will be ignored.",
        "Playlists shorter than %1 seconds will be ignored.",
        minimumDuration).arg(minimumDuration),
    QY("You can change this value in the preferences."),
  };

  ui->questionLabel->setText(Util::joinSentences(info1) + "\n\n" + Util::joinSentences(info2));

  auto const result = exec();

  Settings::get().m_scanForPlaylistsPolicy = static_cast<Settings::ScanForPlaylistsPolicy>(ui->scanPolicy->currentIndex());
  Settings::get().save();

  return result == QDialogButtonBox::YesRole;
}
