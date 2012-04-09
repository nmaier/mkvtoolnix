/********************************************************************************
** Form generated from reading UI file 'main_window.ui'
**
** Created: Mon Apr 9 17:38:31 2012
**      by: Qt User Interface Compiler version 4.7.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QColumnView>
#include <QtGui/QComboBox>
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QMainWindow>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>
#include <QtGui/QScrollArea>
#include <QtGui/QSpacerItem>
#include <QtGui/QSpinBox>
#include <QtGui/QStatusBar>
#include <QtGui/QTabWidget>
#include <QtGui/QTreeView>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionNew;
    QAction *actionSave_job_file;
    QAction *actionOpen_job_file;
    QAction *actionSave_as;
    QAction *actionExit;
    QAction *action_Start_muxing;
    QAction *actionSave_option_file;
    QAction *actionShow_mkvmerge_command_line;
    QAction *actionCopy_command_line_to_clipboard;
    QAction *actionAdd_to_job_queue;
    QAction *action_Job_manager;
    QAction *action_Header_editor;
    QAction *action_Chapter_editor;
    QAction *action_Options;
    QWidget *centralWidget;
    QVBoxLayout *verticalLayout_12;
    QTabWidget *mainTab;
    QWidget *inputTab;
    QHBoxLayout *horizontalLayout_2;
    QVBoxLayout *verticalLayout_3;
    QLabel *label;
    QHBoxLayout *horizontalLayout;
    QTreeView *riles;
    QVBoxLayout *verticalLayout;
    QPushButton *addFile;
    QPushButton *appendFile;
    QPushButton *removeFile;
    QPushButton *removeAll;
    QPushButton *multipleFileParts;
    QSpacerItem *verticalSpacer;
    QLabel *label_2;
    QHBoxLayout *horizontalLayout_3;
    QTreeView *tracks;
    QVBoxLayout *verticalLayout_2;
    QPushButton *trackUp;
    QPushButton *trackDown;
    QSpacerItem *verticalSpacer_2;
    QVBoxLayout *verticalLayout_4;
    QLabel *label_3;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *verticalLayout_9;
    QGroupBox *groupBox_7;
    QGridLayout *gridLayout_5;
    QLabel *label_16;
    QLineEdit *trackName;
    QLabel *label_17;
    QComboBox *trackLanguage;
    QLabel *label_18;
    QComboBox *defaultTrackFlag;
    QLabel *label_19;
    QComboBox *forcedTrackFlag;
    QLabel *label_31;
    QComboBox *compression;
    QLabel *label_20;
    QHBoxLayout *horizontalLayout_11;
    QLineEdit *trackTacks;
    QPushButton *browseTrackTags;
    QGroupBox *groupBox_8;
    QGridLayout *gridLayout_6;
    QLabel *label_22;
    QLineEdit *delay;
    QLabel *label_23;
    QLineEdit *stretchBy;
    QLabel *label_24;
    QComboBox *defaultDuration;
    QLabel *label_21;
    QHBoxLayout *horizontalLayout_12;
    QLineEdit *timecodes;
    QPushButton *browseTimecodes;
    QGroupBox *groupBox_9;
    QGridLayout *gridLayout_7;
    QRadioButton *setAspectRatio;
    QComboBox *aspectRatio;
    QRadioButton *setDisplayWidthHeight;
    QHBoxLayout *horizontalLayout_14;
    QLineEdit *displayWidth;
    QLabel *label_25;
    QLineEdit *displayHeight;
    QLabel *label_26;
    QComboBox *stereoscopy;
    QLabel *label_27;
    QLineEdit *cropping;
    QGroupBox *groupBox_10;
    QHBoxLayout *horizontalLayout_15;
    QCheckBox *aacIsSBR;
    QGroupBox *groupBox_11;
    QHBoxLayout *horizontalLayout_16;
    QLabel *label_28;
    QComboBox *subtitleCharacterSet;
    QGroupBox *groupBox_12;
    QGridLayout *gridLayout_9;
    QLabel *label_29;
    QComboBox *cues;
    QLabel *label_30;
    QLineEdit *userDefinedTrackOptions;
    QSpacerItem *verticalSpacer_5;
    QWidget *outputTab;
    QHBoxLayout *horizontalLayout_18;
    QVBoxLayout *verticalLayout_7;
    QGroupBox *groupBox;
    QGridLayout *gridLayout;
    QLabel *label_5;
    QLineEdit *title;
    QLabel *label_4;
    QHBoxLayout *horizontalLayout_6;
    QLineEdit *output;
    QPushButton *browseOutput;
    QLabel *label_6;
    QHBoxLayout *horizontalLayout_7;
    QLineEdit *globalTags;
    QPushButton *browseGlobalTags;
    QLabel *label_7;
    QHBoxLayout *horizontalLayout_8;
    QLineEdit *segmentinfo;
    QPushButton *browseSegmentInfo;
    QGroupBox *groupBox_2;
    QGridLayout *gridLayout_2;
    QRadioButton *doNotSplit;
    QRadioButton *doSplitAfterSize;
    QComboBox *splitSize;
    QRadioButton *doSplitAfterDuration;
    QComboBox *splitDuration;
    QRadioButton *doSplitAfterTimecodes;
    QLineEdit *splitTimecodes;
    QRadioButton *doSplitByParts;
    QLineEdit *splitParts;
    QLabel *label_8;
    QLabel *label_34;
    QSpinBox *splitMaxFiles;
    QCheckBox *linkFiles;
    QSpacerItem *verticalSpacer_3;
    QVBoxLayout *verticalLayout_8;
    QGroupBox *groupBox_3;
    QGridLayout *gridLayout_3;
    QLabel *label_9;
    QLineEdit *segmentUIDs;
    QLabel *label_10;
    QLineEdit *previousSegmentUID;
    QLabel *label_11;
    QLineEdit *nextSegmentUID;
    QGroupBox *groupBox_4;
    QGridLayout *gridLayout_4;
    QLabel *label_12;
    QHBoxLayout *horizontalLayout_9;
    QLineEdit *chapters;
    QPushButton *browseChapters;
    QLabel *label_13;
    QComboBox *chapterLanguage;
    QLabel *label_14;
    QComboBox *chapterCharacterSet;
    QLabel *label_15;
    QLineEdit *cueNameFormat;
    QGroupBox *groupBox_5;
    QVBoxLayout *verticalLayout_6;
    QCheckBox *webmMode;
    QHBoxLayout *horizontalLayout_10;
    QLabel *label_33;
    QLineEdit *userDefinedOptions;
    QPushButton *editUserDefinedOptions;
    QSpacerItem *verticalSpacer_4;
    QWidget *attachmentsTab;
    QVBoxLayout *verticalLayout_11;
    QLabel *label_32;
    QHBoxLayout *horizontalLayout_17;
    QColumnView *attachments;
    QVBoxLayout *verticalLayout_10;
    QPushButton *addAttachment;
    QPushButton *removeAttachment;
    QSpacerItem *verticalSpacer_6;
    QHBoxLayout *horizontalLayout_5;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *pbStartMuxing;
    QPushButton *pbAddToJobQueue;
    QPushButton *pbSaveSettings;
    QSpacerItem *horizontalSpacer_3;
    QMenuBar *menuBar;
    QMenu *menuFile;
    QMenu *menuMuxing;
    QMenu *menuTools;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(846, 571);
        actionNew = new QAction(MainWindow);
        actionNew->setObjectName(QString::fromUtf8("actionNew"));
        actionSave_job_file = new QAction(MainWindow);
        actionSave_job_file->setObjectName(QString::fromUtf8("actionSave_job_file"));
        actionOpen_job_file = new QAction(MainWindow);
        actionOpen_job_file->setObjectName(QString::fromUtf8("actionOpen_job_file"));
        actionSave_as = new QAction(MainWindow);
        actionSave_as->setObjectName(QString::fromUtf8("actionSave_as"));
        actionExit = new QAction(MainWindow);
        actionExit->setObjectName(QString::fromUtf8("actionExit"));
        action_Start_muxing = new QAction(MainWindow);
        action_Start_muxing->setObjectName(QString::fromUtf8("action_Start_muxing"));
        actionSave_option_file = new QAction(MainWindow);
        actionSave_option_file->setObjectName(QString::fromUtf8("actionSave_option_file"));
        actionShow_mkvmerge_command_line = new QAction(MainWindow);
        actionShow_mkvmerge_command_line->setObjectName(QString::fromUtf8("actionShow_mkvmerge_command_line"));
        actionCopy_command_line_to_clipboard = new QAction(MainWindow);
        actionCopy_command_line_to_clipboard->setObjectName(QString::fromUtf8("actionCopy_command_line_to_clipboard"));
        actionAdd_to_job_queue = new QAction(MainWindow);
        actionAdd_to_job_queue->setObjectName(QString::fromUtf8("actionAdd_to_job_queue"));
        action_Job_manager = new QAction(MainWindow);
        action_Job_manager->setObjectName(QString::fromUtf8("action_Job_manager"));
        action_Header_editor = new QAction(MainWindow);
        action_Header_editor->setObjectName(QString::fromUtf8("action_Header_editor"));
        action_Chapter_editor = new QAction(MainWindow);
        action_Chapter_editor->setObjectName(QString::fromUtf8("action_Chapter_editor"));
        action_Options = new QAction(MainWindow);
        action_Options->setObjectName(QString::fromUtf8("action_Options"));
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        verticalLayout_12 = new QVBoxLayout(centralWidget);
        verticalLayout_12->setSpacing(6);
        verticalLayout_12->setContentsMargins(11, 11, 11, 11);
        verticalLayout_12->setObjectName(QString::fromUtf8("verticalLayout_12"));
        mainTab = new QTabWidget(centralWidget);
        mainTab->setObjectName(QString::fromUtf8("mainTab"));
        mainTab->setTabPosition(QTabWidget::North);
        inputTab = new QWidget();
        inputTab->setObjectName(QString::fromUtf8("inputTab"));
        horizontalLayout_2 = new QHBoxLayout(inputTab);
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        label = new QLabel(inputTab);
        label->setObjectName(QString::fromUtf8("label"));

        verticalLayout_3->addWidget(label);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        riles = new QTreeView(inputTab);
        riles->setObjectName(QString::fromUtf8("riles"));

        horizontalLayout->addWidget(riles);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(6);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        addFile = new QPushButton(inputTab);
        addFile->setObjectName(QString::fromUtf8("addFile"));

        verticalLayout->addWidget(addFile);

        appendFile = new QPushButton(inputTab);
        appendFile->setObjectName(QString::fromUtf8("appendFile"));

        verticalLayout->addWidget(appendFile);

        removeFile = new QPushButton(inputTab);
        removeFile->setObjectName(QString::fromUtf8("removeFile"));

        verticalLayout->addWidget(removeFile);

        removeAll = new QPushButton(inputTab);
        removeAll->setObjectName(QString::fromUtf8("removeAll"));

        verticalLayout->addWidget(removeAll);

        multipleFileParts = new QPushButton(inputTab);
        multipleFileParts->setObjectName(QString::fromUtf8("multipleFileParts"));

        verticalLayout->addWidget(multipleFileParts);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        horizontalLayout->addLayout(verticalLayout);


        verticalLayout_3->addLayout(horizontalLayout);

        label_2 = new QLabel(inputTab);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        verticalLayout_3->addWidget(label_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setSpacing(6);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        tracks = new QTreeView(inputTab);
        tracks->setObjectName(QString::fromUtf8("tracks"));

        horizontalLayout_3->addWidget(tracks);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        trackUp = new QPushButton(inputTab);
        trackUp->setObjectName(QString::fromUtf8("trackUp"));

        verticalLayout_2->addWidget(trackUp);

        trackDown = new QPushButton(inputTab);
        trackDown->setObjectName(QString::fromUtf8("trackDown"));

        verticalLayout_2->addWidget(trackDown);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer_2);


        horizontalLayout_3->addLayout(verticalLayout_2);


        verticalLayout_3->addLayout(horizontalLayout_3);


        horizontalLayout_2->addLayout(verticalLayout_3);

        verticalLayout_4 = new QVBoxLayout();
        verticalLayout_4->setSpacing(6);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        label_3 = new QLabel(inputTab);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        verticalLayout_4->addWidget(label_3);

        scrollArea = new QScrollArea(inputTab);
        scrollArea->setObjectName(QString::fromUtf8("scrollArea"));
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 388, 757));
        verticalLayout_9 = new QVBoxLayout(scrollAreaWidgetContents);
        verticalLayout_9->setSpacing(6);
        verticalLayout_9->setContentsMargins(11, 11, 11, 11);
        verticalLayout_9->setObjectName(QString::fromUtf8("verticalLayout_9"));
        groupBox_7 = new QGroupBox(scrollAreaWidgetContents);
        groupBox_7->setObjectName(QString::fromUtf8("groupBox_7"));
        gridLayout_5 = new QGridLayout(groupBox_7);
        gridLayout_5->setSpacing(6);
        gridLayout_5->setContentsMargins(11, 11, 11, 11);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        label_16 = new QLabel(groupBox_7);
        label_16->setObjectName(QString::fromUtf8("label_16"));

        gridLayout_5->addWidget(label_16, 0, 0, 1, 1);

        trackName = new QLineEdit(groupBox_7);
        trackName->setObjectName(QString::fromUtf8("trackName"));

        gridLayout_5->addWidget(trackName, 0, 1, 1, 1);

        label_17 = new QLabel(groupBox_7);
        label_17->setObjectName(QString::fromUtf8("label_17"));

        gridLayout_5->addWidget(label_17, 1, 0, 1, 1);

        trackLanguage = new QComboBox(groupBox_7);
        trackLanguage->setObjectName(QString::fromUtf8("trackLanguage"));

        gridLayout_5->addWidget(trackLanguage, 1, 1, 1, 1);

        label_18 = new QLabel(groupBox_7);
        label_18->setObjectName(QString::fromUtf8("label_18"));

        gridLayout_5->addWidget(label_18, 2, 0, 1, 1);

        defaultTrackFlag = new QComboBox(groupBox_7);
        defaultTrackFlag->setObjectName(QString::fromUtf8("defaultTrackFlag"));

        gridLayout_5->addWidget(defaultTrackFlag, 2, 1, 1, 1);

        label_19 = new QLabel(groupBox_7);
        label_19->setObjectName(QString::fromUtf8("label_19"));

        gridLayout_5->addWidget(label_19, 3, 0, 1, 1);

        forcedTrackFlag = new QComboBox(groupBox_7);
        forcedTrackFlag->setObjectName(QString::fromUtf8("forcedTrackFlag"));

        gridLayout_5->addWidget(forcedTrackFlag, 3, 1, 1, 1);

        label_31 = new QLabel(groupBox_7);
        label_31->setObjectName(QString::fromUtf8("label_31"));

        gridLayout_5->addWidget(label_31, 4, 0, 1, 1);

        compression = new QComboBox(groupBox_7);
        compression->addItem(QString());
        compression->addItem(QString());
        compression->addItem(QString::fromUtf8("ZLIB"));
        compression->setObjectName(QString::fromUtf8("compression"));

        gridLayout_5->addWidget(compression, 4, 1, 1, 1);

        label_20 = new QLabel(groupBox_7);
        label_20->setObjectName(QString::fromUtf8("label_20"));

        gridLayout_5->addWidget(label_20, 5, 0, 1, 1);

        horizontalLayout_11 = new QHBoxLayout();
        horizontalLayout_11->setSpacing(6);
        horizontalLayout_11->setObjectName(QString::fromUtf8("horizontalLayout_11"));
        trackTacks = new QLineEdit(groupBox_7);
        trackTacks->setObjectName(QString::fromUtf8("trackTacks"));

        horizontalLayout_11->addWidget(trackTacks);

        browseTrackTags = new QPushButton(groupBox_7);
        browseTrackTags->setObjectName(QString::fromUtf8("browseTrackTags"));

        horizontalLayout_11->addWidget(browseTrackTags);


        gridLayout_5->addLayout(horizontalLayout_11, 5, 1, 1, 1);


        verticalLayout_9->addWidget(groupBox_7);

        groupBox_8 = new QGroupBox(scrollAreaWidgetContents);
        groupBox_8->setObjectName(QString::fromUtf8("groupBox_8"));
        gridLayout_6 = new QGridLayout(groupBox_8);
        gridLayout_6->setSpacing(6);
        gridLayout_6->setContentsMargins(11, 11, 11, 11);
        gridLayout_6->setObjectName(QString::fromUtf8("gridLayout_6"));
        label_22 = new QLabel(groupBox_8);
        label_22->setObjectName(QString::fromUtf8("label_22"));

        gridLayout_6->addWidget(label_22, 0, 0, 1, 1);

        delay = new QLineEdit(groupBox_8);
        delay->setObjectName(QString::fromUtf8("delay"));

        gridLayout_6->addWidget(delay, 0, 1, 1, 1);

        label_23 = new QLabel(groupBox_8);
        label_23->setObjectName(QString::fromUtf8("label_23"));

        gridLayout_6->addWidget(label_23, 1, 0, 1, 1);

        stretchBy = new QLineEdit(groupBox_8);
        stretchBy->setObjectName(QString::fromUtf8("stretchBy"));

        gridLayout_6->addWidget(stretchBy, 1, 1, 1, 1);

        label_24 = new QLabel(groupBox_8);
        label_24->setObjectName(QString::fromUtf8("label_24"));

        gridLayout_6->addWidget(label_24, 2, 0, 1, 1);

        defaultDuration = new QComboBox(groupBox_8);
        defaultDuration->insertItems(0, QStringList()
         << QString::fromUtf8("")
         << QString::fromUtf8("24p")
         << QString::fromUtf8("25p")
         << QString::fromUtf8("30p")
         << QString::fromUtf8("50i")
         << QString::fromUtf8("50p")
         << QString::fromUtf8("60i")
         << QString::fromUtf8("60p")
         << QString::fromUtf8("24000/1001p")
         << QString::fromUtf8("30000/1001p")
         << QString::fromUtf8("60000/1001i")
         << QString::fromUtf8("60000/1001p")
        );
        defaultDuration->setObjectName(QString::fromUtf8("defaultDuration"));
        defaultDuration->setEditable(true);

        gridLayout_6->addWidget(defaultDuration, 2, 1, 1, 1);

        label_21 = new QLabel(groupBox_8);
        label_21->setObjectName(QString::fromUtf8("label_21"));

        gridLayout_6->addWidget(label_21, 3, 0, 1, 1);

        horizontalLayout_12 = new QHBoxLayout();
        horizontalLayout_12->setSpacing(6);
        horizontalLayout_12->setObjectName(QString::fromUtf8("horizontalLayout_12"));
        timecodes = new QLineEdit(groupBox_8);
        timecodes->setObjectName(QString::fromUtf8("timecodes"));

        horizontalLayout_12->addWidget(timecodes);

        browseTimecodes = new QPushButton(groupBox_8);
        browseTimecodes->setObjectName(QString::fromUtf8("browseTimecodes"));

        horizontalLayout_12->addWidget(browseTimecodes);


        gridLayout_6->addLayout(horizontalLayout_12, 3, 1, 1, 1);


        verticalLayout_9->addWidget(groupBox_8);

        groupBox_9 = new QGroupBox(scrollAreaWidgetContents);
        groupBox_9->setObjectName(QString::fromUtf8("groupBox_9"));
        gridLayout_7 = new QGridLayout(groupBox_9);
        gridLayout_7->setSpacing(6);
        gridLayout_7->setContentsMargins(11, 11, 11, 11);
        gridLayout_7->setObjectName(QString::fromUtf8("gridLayout_7"));
        setAspectRatio = new QRadioButton(groupBox_9);
        setAspectRatio->setObjectName(QString::fromUtf8("setAspectRatio"));

        gridLayout_7->addWidget(setAspectRatio, 0, 0, 1, 1);

        aspectRatio = new QComboBox(groupBox_9);
        aspectRatio->insertItems(0, QStringList()
         << QString::fromUtf8("4/3")
         << QString::fromUtf8("1.66")
         << QString::fromUtf8("16/9")
         << QString::fromUtf8("1.85")
         << QString::fromUtf8("2.00")
         << QString::fromUtf8("2.21")
         << QString::fromUtf8("2.35")
        );
        aspectRatio->setObjectName(QString::fromUtf8("aspectRatio"));
        aspectRatio->setEditable(true);

        gridLayout_7->addWidget(aspectRatio, 0, 1, 1, 1);

        setDisplayWidthHeight = new QRadioButton(groupBox_9);
        setDisplayWidthHeight->setObjectName(QString::fromUtf8("setDisplayWidthHeight"));

        gridLayout_7->addWidget(setDisplayWidthHeight, 1, 0, 1, 1);

        horizontalLayout_14 = new QHBoxLayout();
        horizontalLayout_14->setSpacing(6);
        horizontalLayout_14->setObjectName(QString::fromUtf8("horizontalLayout_14"));
        displayWidth = new QLineEdit(groupBox_9);
        displayWidth->setObjectName(QString::fromUtf8("displayWidth"));

        horizontalLayout_14->addWidget(displayWidth);

        label_25 = new QLabel(groupBox_9);
        label_25->setObjectName(QString::fromUtf8("label_25"));

        horizontalLayout_14->addWidget(label_25);

        displayHeight = new QLineEdit(groupBox_9);
        displayHeight->setObjectName(QString::fromUtf8("displayHeight"));

        horizontalLayout_14->addWidget(displayHeight);


        gridLayout_7->addLayout(horizontalLayout_14, 1, 1, 1, 1);

        label_26 = new QLabel(groupBox_9);
        label_26->setObjectName(QString::fromUtf8("label_26"));

        gridLayout_7->addWidget(label_26, 2, 0, 1, 1);

        stereoscopy = new QComboBox(groupBox_9);
        stereoscopy->setObjectName(QString::fromUtf8("stereoscopy"));

        gridLayout_7->addWidget(stereoscopy, 2, 1, 2, 1);

        label_27 = new QLabel(groupBox_9);
        label_27->setObjectName(QString::fromUtf8("label_27"));

        gridLayout_7->addWidget(label_27, 3, 0, 2, 1);

        cropping = new QLineEdit(groupBox_9);
        cropping->setObjectName(QString::fromUtf8("cropping"));

        gridLayout_7->addWidget(cropping, 4, 1, 1, 1);


        verticalLayout_9->addWidget(groupBox_9);

        groupBox_10 = new QGroupBox(scrollAreaWidgetContents);
        groupBox_10->setObjectName(QString::fromUtf8("groupBox_10"));
        horizontalLayout_15 = new QHBoxLayout(groupBox_10);
        horizontalLayout_15->setSpacing(6);
        horizontalLayout_15->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_15->setObjectName(QString::fromUtf8("horizontalLayout_15"));
        aacIsSBR = new QCheckBox(groupBox_10);
        aacIsSBR->setObjectName(QString::fromUtf8("aacIsSBR"));

        horizontalLayout_15->addWidget(aacIsSBR);


        verticalLayout_9->addWidget(groupBox_10);

        groupBox_11 = new QGroupBox(scrollAreaWidgetContents);
        groupBox_11->setObjectName(QString::fromUtf8("groupBox_11"));
        horizontalLayout_16 = new QHBoxLayout(groupBox_11);
        horizontalLayout_16->setSpacing(6);
        horizontalLayout_16->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_16->setObjectName(QString::fromUtf8("horizontalLayout_16"));
        label_28 = new QLabel(groupBox_11);
        label_28->setObjectName(QString::fromUtf8("label_28"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(label_28->sizePolicy().hasHeightForWidth());
        label_28->setSizePolicy(sizePolicy);

        horizontalLayout_16->addWidget(label_28);

        subtitleCharacterSet = new QComboBox(groupBox_11);
        subtitleCharacterSet->setObjectName(QString::fromUtf8("subtitleCharacterSet"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(subtitleCharacterSet->sizePolicy().hasHeightForWidth());
        subtitleCharacterSet->setSizePolicy(sizePolicy1);

        horizontalLayout_16->addWidget(subtitleCharacterSet);


        verticalLayout_9->addWidget(groupBox_11);

        groupBox_12 = new QGroupBox(scrollAreaWidgetContents);
        groupBox_12->setObjectName(QString::fromUtf8("groupBox_12"));
        gridLayout_9 = new QGridLayout(groupBox_12);
        gridLayout_9->setSpacing(6);
        gridLayout_9->setContentsMargins(11, 11, 11, 11);
        gridLayout_9->setObjectName(QString::fromUtf8("gridLayout_9"));
        label_29 = new QLabel(groupBox_12);
        label_29->setObjectName(QString::fromUtf8("label_29"));

        gridLayout_9->addWidget(label_29, 0, 0, 1, 1);

        cues = new QComboBox(groupBox_12);
        cues->setObjectName(QString::fromUtf8("cues"));

        gridLayout_9->addWidget(cues, 0, 1, 1, 1);

        label_30 = new QLabel(groupBox_12);
        label_30->setObjectName(QString::fromUtf8("label_30"));

        gridLayout_9->addWidget(label_30, 1, 0, 1, 1);

        userDefinedTrackOptions = new QLineEdit(groupBox_12);
        userDefinedTrackOptions->setObjectName(QString::fromUtf8("userDefinedTrackOptions"));

        gridLayout_9->addWidget(userDefinedTrackOptions, 1, 1, 1, 1);


        verticalLayout_9->addWidget(groupBox_12);

        verticalSpacer_5 = new QSpacerItem(20, 19, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_9->addItem(verticalSpacer_5);

        scrollArea->setWidget(scrollAreaWidgetContents);

        verticalLayout_4->addWidget(scrollArea);


        horizontalLayout_2->addLayout(verticalLayout_4);

        mainTab->addTab(inputTab, QString());
        outputTab = new QWidget();
        outputTab->setObjectName(QString::fromUtf8("outputTab"));
        horizontalLayout_18 = new QHBoxLayout(outputTab);
        horizontalLayout_18->setSpacing(6);
        horizontalLayout_18->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_18->setObjectName(QString::fromUtf8("horizontalLayout_18"));
        verticalLayout_7 = new QVBoxLayout();
        verticalLayout_7->setSpacing(6);
        verticalLayout_7->setObjectName(QString::fromUtf8("verticalLayout_7"));
        groupBox = new QGroupBox(outputTab);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        gridLayout = new QGridLayout(groupBox);
        gridLayout->setSpacing(6);
        gridLayout->setContentsMargins(11, 11, 11, 11);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        label_5 = new QLabel(groupBox);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        gridLayout->addWidget(label_5, 0, 0, 1, 1);

        title = new QLineEdit(groupBox);
        title->setObjectName(QString::fromUtf8("title"));

        gridLayout->addWidget(title, 0, 1, 1, 1);

        label_4 = new QLabel(groupBox);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        gridLayout->addWidget(label_4, 1, 0, 1, 1);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setSpacing(6);
        horizontalLayout_6->setObjectName(QString::fromUtf8("horizontalLayout_6"));
        output = new QLineEdit(groupBox);
        output->setObjectName(QString::fromUtf8("output"));

        horizontalLayout_6->addWidget(output);

        browseOutput = new QPushButton(groupBox);
        browseOutput->setObjectName(QString::fromUtf8("browseOutput"));

        horizontalLayout_6->addWidget(browseOutput);


        gridLayout->addLayout(horizontalLayout_6, 1, 1, 1, 1);

        label_6 = new QLabel(groupBox);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        gridLayout->addWidget(label_6, 2, 0, 1, 1);

        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setSpacing(6);
        horizontalLayout_7->setObjectName(QString::fromUtf8("horizontalLayout_7"));
        globalTags = new QLineEdit(groupBox);
        globalTags->setObjectName(QString::fromUtf8("globalTags"));

        horizontalLayout_7->addWidget(globalTags);

        browseGlobalTags = new QPushButton(groupBox);
        browseGlobalTags->setObjectName(QString::fromUtf8("browseGlobalTags"));

        horizontalLayout_7->addWidget(browseGlobalTags);


        gridLayout->addLayout(horizontalLayout_7, 2, 1, 1, 1);

        label_7 = new QLabel(groupBox);
        label_7->setObjectName(QString::fromUtf8("label_7"));

        gridLayout->addWidget(label_7, 3, 0, 1, 1);

        horizontalLayout_8 = new QHBoxLayout();
        horizontalLayout_8->setSpacing(6);
        horizontalLayout_8->setObjectName(QString::fromUtf8("horizontalLayout_8"));
        segmentinfo = new QLineEdit(groupBox);
        segmentinfo->setObjectName(QString::fromUtf8("segmentinfo"));

        horizontalLayout_8->addWidget(segmentinfo);

        browseSegmentInfo = new QPushButton(groupBox);
        browseSegmentInfo->setObjectName(QString::fromUtf8("browseSegmentInfo"));

        horizontalLayout_8->addWidget(browseSegmentInfo);


        gridLayout->addLayout(horizontalLayout_8, 3, 1, 1, 1);


        verticalLayout_7->addWidget(groupBox);

        groupBox_2 = new QGroupBox(outputTab);
        groupBox_2->setObjectName(QString::fromUtf8("groupBox_2"));
        gridLayout_2 = new QGridLayout(groupBox_2);
        gridLayout_2->setSpacing(6);
        gridLayout_2->setContentsMargins(11, 11, 11, 11);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        doNotSplit = new QRadioButton(groupBox_2);
        doNotSplit->setObjectName(QString::fromUtf8("doNotSplit"));

        gridLayout_2->addWidget(doNotSplit, 0, 0, 1, 1);

        doSplitAfterSize = new QRadioButton(groupBox_2);
        doSplitAfterSize->setObjectName(QString::fromUtf8("doSplitAfterSize"));

        gridLayout_2->addWidget(doSplitAfterSize, 1, 0, 1, 1);

        splitSize = new QComboBox(groupBox_2);
        splitSize->insertItems(0, QStringList()
         << QString::fromUtf8("")
         << QString::fromUtf8("350M")
         << QString::fromUtf8("650M")
         << QString::fromUtf8("700M")
         << QString::fromUtf8("800M")
         << QString::fromUtf8("1000M")
         << QString::fromUtf8("4483M")
         << QString::fromUtf8("8142M")
        );
        splitSize->setObjectName(QString::fromUtf8("splitSize"));
        splitSize->setEditable(true);

        gridLayout_2->addWidget(splitSize, 1, 1, 1, 1);

        doSplitAfterDuration = new QRadioButton(groupBox_2);
        doSplitAfterDuration->setObjectName(QString::fromUtf8("doSplitAfterDuration"));

        gridLayout_2->addWidget(doSplitAfterDuration, 2, 0, 1, 1);

        splitDuration = new QComboBox(groupBox_2);
        splitDuration->insertItems(0, QStringList()
         << QString::fromUtf8("")
         << QString::fromUtf8("01:00:00")
         << QString::fromUtf8("1800s")
        );
        splitDuration->setObjectName(QString::fromUtf8("splitDuration"));
        splitDuration->setEditable(true);

        gridLayout_2->addWidget(splitDuration, 2, 1, 1, 1);

        doSplitAfterTimecodes = new QRadioButton(groupBox_2);
        doSplitAfterTimecodes->setObjectName(QString::fromUtf8("doSplitAfterTimecodes"));

        gridLayout_2->addWidget(doSplitAfterTimecodes, 3, 0, 1, 1);

        splitTimecodes = new QLineEdit(groupBox_2);
        splitTimecodes->setObjectName(QString::fromUtf8("splitTimecodes"));

        gridLayout_2->addWidget(splitTimecodes, 3, 1, 1, 1);

        doSplitByParts = new QRadioButton(groupBox_2);
        doSplitByParts->setObjectName(QString::fromUtf8("doSplitByParts"));

        gridLayout_2->addWidget(doSplitByParts, 4, 0, 1, 1);

        splitParts = new QLineEdit(groupBox_2);
        splitParts->setObjectName(QString::fromUtf8("splitParts"));

        gridLayout_2->addWidget(splitParts, 4, 1, 1, 1);

        label_8 = new QLabel(groupBox_2);
        label_8->setObjectName(QString::fromUtf8("label_8"));

        gridLayout_2->addWidget(label_8, 5, 0, 1, 1);

        label_34 = new QLabel(groupBox_2);
        label_34->setObjectName(QString::fromUtf8("label_34"));

        gridLayout_2->addWidget(label_34, 6, 0, 1, 1);

        splitMaxFiles = new QSpinBox(groupBox_2);
        splitMaxFiles->setObjectName(QString::fromUtf8("splitMaxFiles"));

        gridLayout_2->addWidget(splitMaxFiles, 6, 1, 1, 1);

        linkFiles = new QCheckBox(groupBox_2);
        linkFiles->setObjectName(QString::fromUtf8("linkFiles"));

        gridLayout_2->addWidget(linkFiles, 7, 0, 1, 1);


        verticalLayout_7->addWidget(groupBox_2);

        verticalSpacer_3 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_7->addItem(verticalSpacer_3);


        horizontalLayout_18->addLayout(verticalLayout_7);

        verticalLayout_8 = new QVBoxLayout();
        verticalLayout_8->setSpacing(6);
        verticalLayout_8->setObjectName(QString::fromUtf8("verticalLayout_8"));
        groupBox_3 = new QGroupBox(outputTab);
        groupBox_3->setObjectName(QString::fromUtf8("groupBox_3"));
        gridLayout_3 = new QGridLayout(groupBox_3);
        gridLayout_3->setSpacing(6);
        gridLayout_3->setContentsMargins(11, 11, 11, 11);
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        label_9 = new QLabel(groupBox_3);
        label_9->setObjectName(QString::fromUtf8("label_9"));

        gridLayout_3->addWidget(label_9, 0, 0, 1, 1);

        segmentUIDs = new QLineEdit(groupBox_3);
        segmentUIDs->setObjectName(QString::fromUtf8("segmentUIDs"));

        gridLayout_3->addWidget(segmentUIDs, 0, 1, 1, 1);

        label_10 = new QLabel(groupBox_3);
        label_10->setObjectName(QString::fromUtf8("label_10"));

        gridLayout_3->addWidget(label_10, 1, 0, 1, 1);

        previousSegmentUID = new QLineEdit(groupBox_3);
        previousSegmentUID->setObjectName(QString::fromUtf8("previousSegmentUID"));

        gridLayout_3->addWidget(previousSegmentUID, 1, 1, 1, 1);

        label_11 = new QLabel(groupBox_3);
        label_11->setObjectName(QString::fromUtf8("label_11"));

        gridLayout_3->addWidget(label_11, 2, 0, 1, 1);

        nextSegmentUID = new QLineEdit(groupBox_3);
        nextSegmentUID->setObjectName(QString::fromUtf8("nextSegmentUID"));

        gridLayout_3->addWidget(nextSegmentUID, 2, 1, 1, 1);


        verticalLayout_8->addWidget(groupBox_3);

        groupBox_4 = new QGroupBox(outputTab);
        groupBox_4->setObjectName(QString::fromUtf8("groupBox_4"));
        gridLayout_4 = new QGridLayout(groupBox_4);
        gridLayout_4->setSpacing(6);
        gridLayout_4->setContentsMargins(11, 11, 11, 11);
        gridLayout_4->setObjectName(QString::fromUtf8("gridLayout_4"));
        label_12 = new QLabel(groupBox_4);
        label_12->setObjectName(QString::fromUtf8("label_12"));

        gridLayout_4->addWidget(label_12, 0, 0, 1, 1);

        horizontalLayout_9 = new QHBoxLayout();
        horizontalLayout_9->setSpacing(6);
        horizontalLayout_9->setObjectName(QString::fromUtf8("horizontalLayout_9"));
        chapters = new QLineEdit(groupBox_4);
        chapters->setObjectName(QString::fromUtf8("chapters"));

        horizontalLayout_9->addWidget(chapters);

        browseChapters = new QPushButton(groupBox_4);
        browseChapters->setObjectName(QString::fromUtf8("browseChapters"));

        horizontalLayout_9->addWidget(browseChapters);


        gridLayout_4->addLayout(horizontalLayout_9, 0, 1, 1, 1);

        label_13 = new QLabel(groupBox_4);
        label_13->setObjectName(QString::fromUtf8("label_13"));

        gridLayout_4->addWidget(label_13, 1, 0, 1, 1);

        chapterLanguage = new QComboBox(groupBox_4);
        chapterLanguage->setObjectName(QString::fromUtf8("chapterLanguage"));

        gridLayout_4->addWidget(chapterLanguage, 1, 1, 1, 1);

        label_14 = new QLabel(groupBox_4);
        label_14->setObjectName(QString::fromUtf8("label_14"));

        gridLayout_4->addWidget(label_14, 2, 0, 1, 1);

        chapterCharacterSet = new QComboBox(groupBox_4);
        chapterCharacterSet->setObjectName(QString::fromUtf8("chapterCharacterSet"));

        gridLayout_4->addWidget(chapterCharacterSet, 2, 1, 1, 1);

        label_15 = new QLabel(groupBox_4);
        label_15->setObjectName(QString::fromUtf8("label_15"));

        gridLayout_4->addWidget(label_15, 3, 0, 1, 1);

        cueNameFormat = new QLineEdit(groupBox_4);
        cueNameFormat->setObjectName(QString::fromUtf8("cueNameFormat"));

        gridLayout_4->addWidget(cueNameFormat, 3, 1, 1, 1);


        verticalLayout_8->addWidget(groupBox_4);

        groupBox_5 = new QGroupBox(outputTab);
        groupBox_5->setObjectName(QString::fromUtf8("groupBox_5"));
        verticalLayout_6 = new QVBoxLayout(groupBox_5);
        verticalLayout_6->setSpacing(6);
        verticalLayout_6->setContentsMargins(11, 11, 11, 11);
        verticalLayout_6->setObjectName(QString::fromUtf8("verticalLayout_6"));
        webmMode = new QCheckBox(groupBox_5);
        webmMode->setObjectName(QString::fromUtf8("webmMode"));

        verticalLayout_6->addWidget(webmMode);

        horizontalLayout_10 = new QHBoxLayout();
        horizontalLayout_10->setSpacing(6);
        horizontalLayout_10->setObjectName(QString::fromUtf8("horizontalLayout_10"));
        label_33 = new QLabel(groupBox_5);
        label_33->setObjectName(QString::fromUtf8("label_33"));

        horizontalLayout_10->addWidget(label_33);

        userDefinedOptions = new QLineEdit(groupBox_5);
        userDefinedOptions->setObjectName(QString::fromUtf8("userDefinedOptions"));

        horizontalLayout_10->addWidget(userDefinedOptions);

        editUserDefinedOptions = new QPushButton(groupBox_5);
        editUserDefinedOptions->setObjectName(QString::fromUtf8("editUserDefinedOptions"));

        horizontalLayout_10->addWidget(editUserDefinedOptions);


        verticalLayout_6->addLayout(horizontalLayout_10);


        verticalLayout_8->addWidget(groupBox_5);

        verticalSpacer_4 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_8->addItem(verticalSpacer_4);


        horizontalLayout_18->addLayout(verticalLayout_8);

        mainTab->addTab(outputTab, QString());
        attachmentsTab = new QWidget();
        attachmentsTab->setObjectName(QString::fromUtf8("attachmentsTab"));
        verticalLayout_11 = new QVBoxLayout(attachmentsTab);
        verticalLayout_11->setSpacing(6);
        verticalLayout_11->setContentsMargins(11, 11, 11, 11);
        verticalLayout_11->setObjectName(QString::fromUtf8("verticalLayout_11"));
        label_32 = new QLabel(attachmentsTab);
        label_32->setObjectName(QString::fromUtf8("label_32"));

        verticalLayout_11->addWidget(label_32);

        horizontalLayout_17 = new QHBoxLayout();
        horizontalLayout_17->setSpacing(6);
        horizontalLayout_17->setObjectName(QString::fromUtf8("horizontalLayout_17"));
        attachments = new QColumnView(attachmentsTab);
        attachments->setObjectName(QString::fromUtf8("attachments"));

        horizontalLayout_17->addWidget(attachments);

        verticalLayout_10 = new QVBoxLayout();
        verticalLayout_10->setSpacing(6);
        verticalLayout_10->setObjectName(QString::fromUtf8("verticalLayout_10"));
        addAttachment = new QPushButton(attachmentsTab);
        addAttachment->setObjectName(QString::fromUtf8("addAttachment"));

        verticalLayout_10->addWidget(addAttachment);

        removeAttachment = new QPushButton(attachmentsTab);
        removeAttachment->setObjectName(QString::fromUtf8("removeAttachment"));

        verticalLayout_10->addWidget(removeAttachment);

        verticalSpacer_6 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_10->addItem(verticalSpacer_6);


        horizontalLayout_17->addLayout(verticalLayout_10);


        verticalLayout_11->addLayout(horizontalLayout_17);

        mainTab->addTab(attachmentsTab, QString());

        verticalLayout_12->addWidget(mainTab);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setSpacing(6);
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_5->addItem(horizontalSpacer_2);

        pbStartMuxing = new QPushButton(centralWidget);
        pbStartMuxing->setObjectName(QString::fromUtf8("pbStartMuxing"));

        horizontalLayout_5->addWidget(pbStartMuxing);

        pbAddToJobQueue = new QPushButton(centralWidget);
        pbAddToJobQueue->setObjectName(QString::fromUtf8("pbAddToJobQueue"));

        horizontalLayout_5->addWidget(pbAddToJobQueue);

        pbSaveSettings = new QPushButton(centralWidget);
        pbSaveSettings->setObjectName(QString::fromUtf8("pbSaveSettings"));

        horizontalLayout_5->addWidget(pbSaveSettings);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_5->addItem(horizontalSpacer_3);


        verticalLayout_12->addLayout(horizontalLayout_5);

        MainWindow->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(MainWindow);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 846, 24));
        menuFile = new QMenu(menuBar);
        menuFile->setObjectName(QString::fromUtf8("menuFile"));
        menuMuxing = new QMenu(menuBar);
        menuMuxing->setObjectName(QString::fromUtf8("menuMuxing"));
        menuTools = new QMenu(menuBar);
        menuTools->setObjectName(QString::fromUtf8("menuTools"));
        MainWindow->setMenuBar(menuBar);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        MainWindow->setStatusBar(statusBar);

        menuBar->addAction(menuFile->menuAction());
        menuBar->addAction(menuMuxing->menuAction());
        menuBar->addAction(menuTools->menuAction());
        menuFile->addAction(actionNew);
        menuFile->addAction(actionOpen_job_file);
        menuFile->addAction(actionSave_job_file);
        menuFile->addAction(actionSave_as);
        menuFile->addAction(actionSave_option_file);
        menuFile->addSeparator();
        menuFile->addAction(actionExit);
        menuMuxing->addAction(action_Start_muxing);
        menuMuxing->addSeparator();
        menuMuxing->addAction(actionAdd_to_job_queue);
        menuMuxing->addAction(action_Job_manager);
        menuMuxing->addSeparator();
        menuMuxing->addAction(actionShow_mkvmerge_command_line);
        menuMuxing->addAction(actionCopy_command_line_to_clipboard);
        menuTools->addAction(action_Header_editor);
        menuTools->addAction(action_Chapter_editor);
        menuTools->addSeparator();
        menuTools->addAction(action_Options);

        retranslateUi(MainWindow);
        QObject::connect(pbStartMuxing, SIGNAL(clicked()), MainWindow, SLOT(startMuxingPressed()));
        QObject::connect(pbAddToJobQueue, SIGNAL(clicked()), MainWindow, SLOT(onAddToJobQueue()));
        QObject::connect(pbSaveSettings, SIGNAL(clicked()), MainWindow, SLOT(onSaveSettings()));

        mainTab->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "MainWindow", 0, QApplication::UnicodeUTF8));
        actionNew->setText(QApplication::translate("MainWindow", "&New", 0, QApplication::UnicodeUTF8));
        actionSave_job_file->setText(QApplication::translate("MainWindow", "&Save settings", 0, QApplication::UnicodeUTF8));
        actionOpen_job_file->setText(QApplication::translate("MainWindow", "&Open", 0, QApplication::UnicodeUTF8));
        actionSave_as->setText(QApplication::translate("MainWindow", "S&ave settings as", 0, QApplication::UnicodeUTF8));
        actionExit->setText(QApplication::translate("MainWindow", "E&xit", 0, QApplication::UnicodeUTF8));
        action_Start_muxing->setText(QApplication::translate("MainWindow", "&Start muxing", 0, QApplication::UnicodeUTF8));
        actionSave_option_file->setText(QApplication::translate("MainWindow", "Create &option file", 0, QApplication::UnicodeUTF8));
        actionShow_mkvmerge_command_line->setText(QApplication::translate("MainWindow", "S&how command line", 0, QApplication::UnicodeUTF8));
        actionCopy_command_line_to_clipboard->setText(QApplication::translate("MainWindow", "&Copy command line to clipboard", 0, QApplication::UnicodeUTF8));
        actionAdd_to_job_queue->setText(QApplication::translate("MainWindow", "&Add to job queue", 0, QApplication::UnicodeUTF8));
        action_Job_manager->setText(QApplication::translate("MainWindow", "&Job manager", 0, QApplication::UnicodeUTF8));
        action_Header_editor->setText(QApplication::translate("MainWindow", "&Header editor", 0, QApplication::UnicodeUTF8));
        action_Chapter_editor->setText(QApplication::translate("MainWindow", "&Chapter editor", 0, QApplication::UnicodeUTF8));
        action_Options->setText(QApplication::translate("MainWindow", "&Options...", 0, QApplication::UnicodeUTF8));
        label->setText(QApplication::translate("MainWindow", "Files:", 0, QApplication::UnicodeUTF8));
        addFile->setText(QApplication::translate("MainWindow", "Add", 0, QApplication::UnicodeUTF8));
        appendFile->setText(QApplication::translate("MainWindow", "Append", 0, QApplication::UnicodeUTF8));
        removeFile->setText(QApplication::translate("MainWindow", "Remove", 0, QApplication::UnicodeUTF8));
        removeAll->setText(QApplication::translate("MainWindow", "Remove all", 0, QApplication::UnicodeUTF8));
        multipleFileParts->setText(QApplication::translate("MainWindow", "Multiple Parts", 0, QApplication::UnicodeUTF8));
        label_2->setText(QApplication::translate("MainWindow", "Tracks, chapters, tags and attachments:", 0, QApplication::UnicodeUTF8));
        trackUp->setText(QApplication::translate("MainWindow", "up", 0, QApplication::UnicodeUTF8));
        trackDown->setText(QApplication::translate("MainWindow", "down", 0, QApplication::UnicodeUTF8));
        label_3->setText(QApplication::translate("MainWindow", "Properties:", 0, QApplication::UnicodeUTF8));
        groupBox_7->setTitle(QApplication::translate("MainWindow", "General options", 0, QApplication::UnicodeUTF8));
        label_16->setText(QApplication::translate("MainWindow", "Track name:", 0, QApplication::UnicodeUTF8));
        label_17->setText(QApplication::translate("MainWindow", "Language:", 0, QApplication::UnicodeUTF8));
        label_18->setText(QApplication::translate("MainWindow", "\"Default track\" flag:", 0, QApplication::UnicodeUTF8));
        defaultTrackFlag->clear();
        defaultTrackFlag->insertItems(0, QStringList()
         << QApplication::translate("MainWindow", "determine automatically", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("MainWindow", "set to \"yes\"", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("MainWindow", "set to \"no\"", 0, QApplication::UnicodeUTF8)
        );
        label_19->setText(QApplication::translate("MainWindow", "\"Forced track\" flag:", 0, QApplication::UnicodeUTF8));
        forcedTrackFlag->clear();
        forcedTrackFlag->insertItems(0, QStringList()
         << QApplication::translate("MainWindow", "off", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("MainWindow", "on", 0, QApplication::UnicodeUTF8)
        );
        label_31->setText(QApplication::translate("MainWindow", "Compression:", 0, QApplication::UnicodeUTF8));
        compression->setItemText(0, QApplication::translate("MainWindow", "determine automatically", 0, QApplication::UnicodeUTF8));
        compression->setItemText(1, QApplication::translate("MainWindow", "no extra compression", 0, QApplication::UnicodeUTF8));

        label_20->setText(QApplication::translate("MainWindow", "Tags:", 0, QApplication::UnicodeUTF8));
        browseTrackTags->setText(QApplication::translate("MainWindow", "Browse", 0, QApplication::UnicodeUTF8));
        groupBox_8->setTitle(QApplication::translate("MainWindow", "Timecodes and default duration:", 0, QApplication::UnicodeUTF8));
        label_22->setText(QApplication::translate("MainWindow", "Delay (in ms):", 0, QApplication::UnicodeUTF8));
        label_23->setText(QApplication::translate("MainWindow", "Stretch by:", 0, QApplication::UnicodeUTF8));
        label_24->setText(QApplication::translate("MainWindow", "Default duration/FPS:", 0, QApplication::UnicodeUTF8));
        label_21->setText(QApplication::translate("MainWindow", "Timecode file:", 0, QApplication::UnicodeUTF8));
        browseTimecodes->setText(QApplication::translate("MainWindow", "Browse", 0, QApplication::UnicodeUTF8));
        groupBox_9->setTitle(QApplication::translate("MainWindow", "Picture properties", 0, QApplication::UnicodeUTF8));
        setAspectRatio->setText(QApplication::translate("MainWindow", "Set aspect ratio:", 0, QApplication::UnicodeUTF8));
        setDisplayWidthHeight->setText(QApplication::translate("MainWindow", "Display width/height:", 0, QApplication::UnicodeUTF8));
        label_25->setText(QApplication::translate("MainWindow", "x", 0, QApplication::UnicodeUTF8));
        label_26->setText(QApplication::translate("MainWindow", "Stereoscopy:", 0, QApplication::UnicodeUTF8));
        label_27->setText(QApplication::translate("MainWindow", "Cropping:", 0, QApplication::UnicodeUTF8));
        groupBox_10->setTitle(QApplication::translate("MainWindow", "Audio properties", 0, QApplication::UnicodeUTF8));
        aacIsSBR->setText(QApplication::translate("MainWindow", "AAC is SBR/HE-AAC/AAC+", 0, QApplication::UnicodeUTF8));
        groupBox_11->setTitle(QApplication::translate("MainWindow", "Subtitle and chapter properties", 0, QApplication::UnicodeUTF8));
        label_28->setText(QApplication::translate("MainWindow", "Character set:", 0, QApplication::UnicodeUTF8));
        groupBox_12->setTitle(QApplication::translate("MainWindow", "Miscellaneous", 0, QApplication::UnicodeUTF8));
        label_29->setText(QApplication::translate("MainWindow", "Indexing (cues):", 0, QApplication::UnicodeUTF8));
        cues->clear();
        cues->insertItems(0, QStringList()
         << QApplication::translate("MainWindow", "default", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("MainWindow", "only for I frames", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("MainWindow", "for all frames", 0, QApplication::UnicodeUTF8)
         << QApplication::translate("MainWindow", "no cues", 0, QApplication::UnicodeUTF8)
        );
        label_30->setText(QApplication::translate("MainWindow", "User defined options:", 0, QApplication::UnicodeUTF8));
        mainTab->setTabText(mainTab->indexOf(inputTab), QApplication::translate("MainWindow", "Input", 0, QApplication::UnicodeUTF8));
        groupBox->setTitle(QApplication::translate("MainWindow", "General", 0, QApplication::UnicodeUTF8));
        label_5->setText(QApplication::translate("MainWindow", "File title:", 0, QApplication::UnicodeUTF8));
        label_4->setText(QApplication::translate("MainWindow", "Destination:", 0, QApplication::UnicodeUTF8));
        browseOutput->setText(QApplication::translate("MainWindow", "Browse", 0, QApplication::UnicodeUTF8));
        label_6->setText(QApplication::translate("MainWindow", "Global tags:", 0, QApplication::UnicodeUTF8));
        browseGlobalTags->setText(QApplication::translate("MainWindow", "Browse", 0, QApplication::UnicodeUTF8));
        label_7->setText(QApplication::translate("MainWindow", "Segment info:", 0, QApplication::UnicodeUTF8));
        browseSegmentInfo->setText(QApplication::translate("MainWindow", "Browse", 0, QApplication::UnicodeUTF8));
        groupBox_2->setTitle(QApplication::translate("MainWindow", "Splitting", 0, QApplication::UnicodeUTF8));
        doNotSplit->setText(QApplication::translate("MainWindow", "Do not split", 0, QApplication::UnicodeUTF8));
        doSplitAfterSize->setText(QApplication::translate("MainWindow", "Split after this size:", 0, QApplication::UnicodeUTF8));
        doSplitAfterDuration->setText(QApplication::translate("MainWindow", "Split after this duration:", 0, QApplication::UnicodeUTF8));
        doSplitAfterTimecodes->setText(QApplication::translate("MainWindow", "Split after these timecodes:", 0, QApplication::UnicodeUTF8));
        doSplitByParts->setText(QApplication::translate("MainWindow", "Split by parts:", 0, QApplication::UnicodeUTF8));
        label_8->setText(QApplication::translate("MainWindow", "Options:", 0, QApplication::UnicodeUTF8));
        label_34->setText(QApplication::translate("MainWindow", "Maximum number of files:", 0, QApplication::UnicodeUTF8));
        linkFiles->setText(QApplication::translate("MainWindow", "Link files", 0, QApplication::UnicodeUTF8));
        groupBox_3->setTitle(QApplication::translate("MainWindow", "File/Segment linking", 0, QApplication::UnicodeUTF8));
        label_9->setText(QApplication::translate("MainWindow", "Segment UIDs:", 0, QApplication::UnicodeUTF8));
        label_10->setText(QApplication::translate("MainWindow", "Previous segment UID:", 0, QApplication::UnicodeUTF8));
        label_11->setText(QApplication::translate("MainWindow", "Next segment UID:", 0, QApplication::UnicodeUTF8));
        groupBox_4->setTitle(QApplication::translate("MainWindow", "Chapters", 0, QApplication::UnicodeUTF8));
        label_12->setText(QApplication::translate("MainWindow", "Chapter file:", 0, QApplication::UnicodeUTF8));
        browseChapters->setText(QApplication::translate("MainWindow", "Browse", 0, QApplication::UnicodeUTF8));
        label_13->setText(QApplication::translate("MainWindow", "Language:", 0, QApplication::UnicodeUTF8));
        label_14->setText(QApplication::translate("MainWindow", "Character set:", 0, QApplication::UnicodeUTF8));
        label_15->setText(QApplication::translate("MainWindow", "Cue name format:", 0, QApplication::UnicodeUTF8));
        groupBox_5->setTitle(QApplication::translate("MainWindow", "Miscellaneous", 0, QApplication::UnicodeUTF8));
        webmMode->setText(QApplication::translate("MainWindow", "Create WebM compliant file", 0, QApplication::UnicodeUTF8));
        label_33->setText(QApplication::translate("MainWindow", "User-defined options:", 0, QApplication::UnicodeUTF8));
        editUserDefinedOptions->setText(QApplication::translate("MainWindow", "Edit", 0, QApplication::UnicodeUTF8));
        mainTab->setTabText(mainTab->indexOf(outputTab), QApplication::translate("MainWindow", "Output", 0, QApplication::UnicodeUTF8));
        label_32->setText(QApplication::translate("MainWindow", "Attachments:", 0, QApplication::UnicodeUTF8));
        addAttachment->setText(QApplication::translate("MainWindow", "Add", 0, QApplication::UnicodeUTF8));
        removeAttachment->setText(QApplication::translate("MainWindow", "Remove", 0, QApplication::UnicodeUTF8));
        mainTab->setTabText(mainTab->indexOf(attachmentsTab), QApplication::translate("MainWindow", "Attachments", 0, QApplication::UnicodeUTF8));
        pbStartMuxing->setText(QApplication::translate("MainWindow", "Start muxing", 0, QApplication::UnicodeUTF8));
        pbAddToJobQueue->setText(QApplication::translate("MainWindow", "Add to job queue", 0, QApplication::UnicodeUTF8));
        pbSaveSettings->setText(QApplication::translate("MainWindow", "Save settings", 0, QApplication::UnicodeUTF8));
        menuFile->setTitle(QApplication::translate("MainWindow", "File", 0, QApplication::UnicodeUTF8));
        menuMuxing->setTitle(QApplication::translate("MainWindow", "Muxing", 0, QApplication::UnicodeUTF8));
        menuTools->setTitle(QApplication::translate("MainWindow", "Tools", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // MAIN_WINDOW_H
