#ifndef MTX_MKVTOOLNIX_GUI_ATTACHMENT_H
#define MTX_MKVTOOLNIX_GUI_ATTACHMENT_H

#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"

#include <QObject>
#include <QSettings>
#include <QString>

class Attachment;
typedef std::shared_ptr<Attachment> AttachmentPtr;

class QStringList;

class Attachment: public QObject {
  Q_OBJECT;
public:
  enum Style {
    ToAllFiles = 1,
    ToFirstFile,
    StyleMax = ToFirstFile,
    StyleMin = ToAllFiles
  };

  QString m_fileName, m_name, m_description, m_MIMEType;
  Style m_style;

public:
  explicit Attachment(QString const &fileName = QString{""});
  virtual ~Attachment();

  virtual void saveSettings(QSettings &settings) const;
  virtual void loadSettings(MuxConfig::Loader &l);
  virtual void guessMIMEType();

  void buildMkvmergeOptions(QStringList &options) const;
};

#endif  // MTX_MKVTOOLNIX_GUI_ATTACHMENT_H
