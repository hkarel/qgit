/*
    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#ifndef SETTINGSIMPL_H
#define SETTINGSIMPL_H

#include "ui_settings.h"

class QVariant;
class Git;

class SettingsImpl: public QDialog, public Ui_SettingsBase {
Q_OBJECT
public:
    SettingsImpl(QWidget* parent, Git* git, int defTab = 0);

    void loadGeometry();
    void saveGeometry();

signals:
    void typeWriterFontChanged();
    void flagChanged(uint);

protected slots:
    void on_chkNumbers_toggled(bool b);
    void on_chkSign_toggled(bool b);
    void on_chkRangeSelectDialog_toggled(bool b);
    void on_chkReopenLastRepo_toggled(bool b);
    void on_chkOpenInEditor_toggled(bool b);
    void on_chkCommitConfirm_toggled(bool b);
    void on_chkRelativeDate_toggled(bool b);
    void on_chkLogDiffTab_toggled(bool b);
    void on_chkSmartLabels_toggled(bool b);
    void on_chkMsgOnNewSHA_toggled(bool b);
    void on_chkEnableDragnDrop_toggled(bool b);
    void on_chkDiffCache_toggled(bool b);
    void on_chkCommitSign_toggled(bool b);
    void on_chkCommitVerify_toggled(bool b);
    void on_chkCommitUseDefMsg_toggled(bool b);
    void on_lineExternalDiffViewer_textChanged(const QString& s);
    void on_lineExternalEditor_textChanged(const QString& s);
    void on_lineApplyPatchExtraOptions_textChanged(const QString& s);
    void on_lineFormatPatchExtraOptions_textChanged(const QString& s);
    void on_lineExcludeFile_textChanged(const QString& s);
    void on_lineExcludePerDir_textChanged(const QString& s);
    void on_lineTemplate_textChanged(const QString& s);
    void on_lineCommitExtraOptions_textChanged(const QString& s);
    void on_cboxIconSize_activated(int i);
    void on_cboxCodecs_activated(int i);
    void on_cboxUserSrc_activated(int i);
    void on_cboxGitConfigSource_activated(int i);
    void on_treeGitConfig_itemChanged(QTreeWidgetItem*, int);
    void on_btnExtDiff_clicked();
    void on_btnExtEditor_clicked();
    void on_btnFont_clicked();

private:
    void done(int r) override;

    void addConfigOption(QTreeWidgetItem* parent, QStringList paths, const QString& value);
    void setupCodecList(QStringList& list);
    void setupCodecsCombo();
    void readGitConfig(const QString& source);
    void userInfo();
    void changeFlag(uint f, bool b);

    Git* git;
    QStringList _uInfo;
    bool populatingGitConfig;
};

#endif
