/*
    Description: settings dialog

    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#include <QTextCodec>
#include <QFileDialog>
#include <QFontDialog>
#include "common.h"
#include "git.h"
#include "settingsimpl.h"

#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

/*
By default, there are two entries in the search path:

   1. SYSCONF - where SYSCONF is a directory specified when configuring Qt;
            by default it is INSTALL/etc/settings.
   2. $HOME/.qt/ - where $HOME is the user's home directory.
*/

static const char* en[] = { "Latin1", "Big5 -- Chinese", "EUC-JP -- Japanese",
    "EUC-KR -- Korean", "GB18030 -- Chinese", "ISO-2022-JP -- Japanese",
    "Shift_JIS -- Japanese", "UTF-8 -- Unicode, 8-bit",
    "KOI8-R -- Russian", "KOI8-U -- Ukrainian", "ISO-8859-1 -- Western",
    "ISO-8859-2 -- Central European", "ISO-8859-3 -- Central European",
    "ISO-8859-4 -- Baltic", "ISO-8859-5 -- Cyrillic", "ISO-8859-6 -- Arabic",
    "ISO-8859-7 -- Greek", "ISO-8859-8 -- Hebrew, visually ordered",
    "ISO-8859-8-i -- Hebrew, logically ordered", "ISO-8859-9 -- Turkish",
    "ISO-8859-10", "ISO-8859-13", "ISO-8859-14", "ISO-8859-15 -- Western",
    "windows-1250 -- Central European", "windows-1251 -- Cyrillic",
    "windows-1252 -- Western", "windows-1253 -- Greek", "windows-1254 -- Turkish",
    "windows-1255 -- Hebrew", "windows-1256 -- Arabic", "windows-1257 -- Baltic",
    "windows-1258", 0 };

using namespace qgit;

SettingsImpl::SettingsImpl(QWidget* p, Git* g, int defTab) : QDialog(p), git(g) {

    setupUi(this);

//    int f = flags(FLAGS_KEY);
//    checkBoxDiffCache->setChecked(f & DIFF_INDEX_F);
//    checkBoxNumbers->setChecked(f & NUMBERS_F);
//    checkBoxSign->setChecked(f & SIGN_PATCH_F);
//    checkBoxCommitSign->setChecked(f & SIGN_CMT_F);
//    checkBoxCommitVerify->setChecked(f & VERIFY_CMT_F);
//    checkBoxCommitUseDefMsg->setChecked(f & USE_CMT_MSG_F);
//    checkBoxRangeSelectDialog->setChecked(f & RANGE_SELECT_F);
//    checkBoxReopenLastRepo->setChecked(f & REOPEN_REPO_F);
//    checkBoxOpenInEditor->setChecked(f & OPEN_IN_EDITOR_F);
//    checkBoxRelativeDate->setChecked(f & REL_DATE_F);
//    checkBoxLogDiffTab->setChecked(f & LOG_DIFF_TAB_F);
//    checkBoxSmartLabels->setChecked(f & SMART_LBL_F);
//    checkBoxMsgOnNewSHA->setChecked(f & MSG_ON_NEW_F);
//    checkBoxEnableDragnDrop->setChecked(f & ENABLE_DRAGNDROP_F);

    qgit::FlagType f = qgit::flags();

    checkBoxDiffCache->setChecked(f & DIFF_INDEX_F);
    checkBoxNumbers->setChecked(f & NUMBERS_F);
    checkBoxSign->setChecked(f & SIGN_PATCH_F);
    checkBoxCommitSign->setChecked(f & SIGN_CMT_F);
    checkBoxCommitVerify->setChecked(f & VERIFY_CMT_F);
    checkBoxCommitUseDefMsg->setChecked(f & USE_CMT_MSG_F);
    checkBoxRangeSelectDialog->setChecked(f & RANGE_SELECT_F);
    checkBoxReopenLastRepo->setChecked(f & REOPEN_REPO_F);
    checkBoxOpenInEditor->setChecked(f & OPEN_IN_EDITOR_F);
    checkBoxRelativeDate->setChecked(f & REL_DATE_F);
    checkBoxLogDiffTab->setChecked(f & LOG_DIFF_TAB_F);
    checkBoxSmartLabels->setChecked(f & SMART_LBL_F);
    checkBoxMsgOnNewSHA->setChecked(f & MSG_ON_NEW_F);
    checkBoxEnableDragnDrop->setChecked(f & ENABLE_DRAGNDROP_F);

    QString FPOpt;
    config::base().getValue("patch.args", FPOpt);

    QString APOpt;
    config::base().getValue("patch.args_2", APOpt);

    QString extDiff;
    config::base().getValue("general.external_diff_viewer", extDiff);

    QString extEditor;
    config::base().getValue("general.external_editor", extEditor);

    int iconSizeIndex = 0;
    config::base().getValue("general.icon_size_index", iconSizeIndex);

    QString CMArgs;
    config::base().getValue("commit.args", CMArgs);

    QString exFile;
    config::base().getValue("working_dir.exclude.from", exFile);

    QString exPerDir;
    config::base().getValue("working_dir.exclude.per_directory", exPerDir);

    QString commitTmpl;
    config::base().getValue("commit.template_file_path", commitTmpl);

    lineEditApplyPatchExtraOptions->setText(APOpt);
    lineEditFormatPatchExtraOptions->setText(FPOpt);
    lineEditExternalDiffViewer->setText(extDiff);
    lineEditExternalEditor->setText(extEditor);
    lineEditExcludeFile->setText(exFile);
    lineEditExcludePerDir->setText(exPerDir);
    lineEditTemplate->setText(commitTmpl);
    lineEditCommitExtraOptions->setText(CMArgs);
    lineEditTypeWriterFont->setText(TYPE_WRITER_FONT.toString());
    lineEditTypeWriterFont->setCursorPosition(0); // font description could be long

    comboBoxIconSize->setCurrentIndex(iconSizeIndex);

    setupCodecsCombo();
    checkBoxDiffCache_toggled(checkBoxDiffCache->isChecked());
    tabDialog->setCurrentIndex(defTab);
    userInfo();
    comboBoxGitConfigSource_activated(0);

    loadGeometry();
}

void SettingsImpl::loadGeometry()
{
    QVector<int> v;
    config::base().getValue("geometry.settings.window", v);

    if (v.count() == 4) {
        move(v[0], v[1]);
        resize(v[2], v[3]);
    }
}

void SettingsImpl::saveGeometry()
{
    QPoint p = pos();
    QVector<int> v {p.x(), p.y(), width(), height()};
    config::base().setValue("geometry.settings.window", v);
}

void SettingsImpl::userInfo() {
/*
    QGit::userInfo() returns a QStringList formed by
    triples (defined in, user, email)
*/
    git->userInfo(_uInfo);
    if (_uInfo.count() % 3 != 0) {
        log_error << "bad info returned";
        return;
    }
    bool found = false;
    int idx = 0;
    FOREACH(it, _uInfo) {
        comboBoxUserSrc->addItem(*it);
        ++it;
        if (!found && !(*it).isEmpty())
            found = true;
        if (!found)
            idx++;
        ++it;
    }
    if (!found)
        idx = 0;

    comboBoxUserSrc->setCurrentIndex(idx);
    comboBoxUserSrc_activated(idx);
}

void SettingsImpl::addConfigOption(QTreeWidgetItem* parent, QStringList paths, const QString& value) {

    if (paths.isEmpty()) {
        parent->setText(1, value);
        return;
    }
    QString name(paths.first());
    paths.removeFirst();

    // Options list is already ordered
    if (parent->childCount() == 0 || name != parent->child(0)->text(0))
        parent->addChild(new QTreeWidgetItem(parent, QStringList(name)));

    addConfigOption(parent->child(parent->childCount() - 1), paths, value);
}

void SettingsImpl::readGitConfig(const QString& source) {
 
    populatingGitConfig = true;
    treeWidgetGitConfig->clear();
    QStringList options = git->getGitConfigList(source == "Global");
    options.sort();

    for (const QString& s : options) {

        QStringList paths = s.split("=").at(0).split(".");
        QString value = s.split("=").at(1);

        if (paths.isEmpty() || value.isEmpty()) {
            log_error << log_format("Unable to parse line %?", s);
            continue;
        }
        QString name(paths.first());
        paths.removeFirst();
        QList<QTreeWidgetItem*> items = treeWidgetGitConfig->findItems(name, Qt::MatchExactly);
        QTreeWidgetItem* item;

        if (items.isEmpty())
            item = new QTreeWidgetItem(treeWidgetGitConfig, QStringList(name));
        else
            item = items.first();

        addConfigOption(item, paths, value);
    }
    populatingGitConfig = false;
}

void SettingsImpl::treeWidgetGitConfig_itemChanged(QTreeWidgetItem* item, int i) {

    if (populatingGitConfig)
        return;
    //dbs(item->text(0));
    //dbs(item->text(1));
    //dbp("column %1", i);
}

void SettingsImpl::comboBoxUserSrc_activated(int i) {

    lineEditAuthor->setText(_uInfo[i * 3 + 1]);
    lineEditMail->setText(_uInfo[i * 3 + 2]);
}

void SettingsImpl::comboBoxGitConfigSource_activated(int) {

    readGitConfig(comboBoxGitConfigSource->currentText());
}

void SettingsImpl::setupCodecList(QStringList& list) {

    int i = 0;
    while (en[i] != 0)
        list.append(QString::fromLatin1(en[i++]));
}

void SettingsImpl::setupCodecsCombo() {

    const QString localCodec(QTextCodec::codecForLocale()->name());
    QStringList codecs;
    codecs.append(QString("Local Codec (" + localCodec + ")"));
    setupCodecList(codecs);
    comboBoxCodecs->insertItems(0, codecs);

    bool isGitArchive;
    QTextCodec* tc = git->getTextCodec(&isGitArchive);
    if (!isGitArchive) {
        comboBoxCodecs->setEnabled(false);
        return;
    }
    const QString curCodec(tc != 0 ? tc->name() : "Latin1");
    QRegExp re("*" + curCodec + "*", Qt::CaseInsensitive, QRegExp::Wildcard);
    int idx = codecs.indexOf(re);
    if (idx == -1) {
        log_warn << log_format("Codec <%?> not available, using local codec", curCodec);
        idx = 0;
    }
    comboBoxCodecs->setCurrentIndex(idx);
    if (idx == 0) // signal activated() will not fire in this case
        comboBoxCodecs_activated(0);
}

void SettingsImpl::comboBoxIconSize_activated(int i) {

    //writeSetting(ICON_SIZE_INDEX, i);
    config::base().setValue("general.icon_size_index", i);
}

void SettingsImpl::comboBoxCodecs_activated(int idx) {

    QString codecName(QTextCodec::codecForLocale()->name());
    if (idx != 0)
        codecName = comboBoxCodecs->currentText().section(" --", 0, 0);

    git->setTextCodec(QTextCodec::codecForName(codecName.toLatin1()));
}

void SettingsImpl::pushButtonExtDiff_clicked() {

    QString extDiffName(QFileDialog::getOpenFileName(this,
                        "Select the patch viewer"));
    if (!extDiffName.isEmpty())
        lineEditExternalDiffViewer->setText(extDiffName);
}

void SettingsImpl::pushButtonExtEditor_clicked() {

    QString extEditorName(QFileDialog::getOpenFileName(this,
                        "Select the external editor"));
    if (!extEditorName.isEmpty())
        lineEditExternalEditor->setText(extEditorName);
}

void SettingsImpl::pushButtonFont_clicked() {

    bool ok;
    QFont fnt = QFontDialog::getFont(&ok, TYPE_WRITER_FONT, this);
    if (ok && TYPE_WRITER_FONT != fnt) {

        TYPE_WRITER_FONT = fnt;
        lineEditTypeWriterFont->setText(fnt.toString());
        lineEditTypeWriterFont->setCursorPosition(0);

        //writeSetting(TYPWRT_FNT_KEY, fnt.toString());
        config::base().setValue("general.typewriter_font", fnt.toString());

        emit typeWriterFontChanged();
    }
}

void SettingsImpl::changeFlag(uint f, bool b) {

    qgit::flags().set(f, b);
    emit flagChanged(f);
}

void SettingsImpl::done(int r)
{
    QDialog::done(r);
    if (r == QDialog::Accepted) {
        qgit::flags().save(); // Call config::base().save() inside
    }
    else
        config::base().rereadFile();

    saveGeometry();
}

void SettingsImpl::checkBoxDiffCache_toggled(bool b) {

    lineEditExcludeFile->setEnabled(b);
    lineEditExcludePerDir->setEnabled(b);
    changeFlag(DIFF_INDEX_F, b);
}

void SettingsImpl::checkBoxNumbers_toggled(bool b) {

    changeFlag(NUMBERS_F, b);
}

void SettingsImpl::checkBoxSign_toggled(bool b) {

    changeFlag(SIGN_PATCH_F, b);
}

void SettingsImpl::checkBoxRangeSelectDialog_toggled(bool b) {

    changeFlag(RANGE_SELECT_F, b);
}

void SettingsImpl::checkBoxReopenLastRepo_toggled(bool b) {

    changeFlag(REOPEN_REPO_F, b);
}

void SettingsImpl::checkBoxOpenInEditor_toggled(bool b) {

    changeFlag(OPEN_IN_EDITOR_F, b);
}

void SettingsImpl::checkBoxRelativeDate_toggled(bool b) {

    changeFlag(REL_DATE_F, b);
}

void SettingsImpl::checkBoxLogDiffTab_toggled(bool b) {

    changeFlag(LOG_DIFF_TAB_F, b);
}

void SettingsImpl::checkBoxSmartLabels_toggled(bool b) {

    changeFlag(SMART_LBL_F, b);
}

void SettingsImpl::checkBoxMsgOnNewSHA_toggled(bool b) {

    changeFlag(MSG_ON_NEW_F, b);
}

void SettingsImpl::checkBoxEnableDragnDrop_toggled(bool b) {

	changeFlag(ENABLE_DRAGNDROP_F, b);
}

void SettingsImpl::checkBoxCommitSign_toggled(bool b) {

    changeFlag(SIGN_CMT_F, b);
}

void SettingsImpl::checkBoxCommitVerify_toggled(bool b) {

    changeFlag(VERIFY_CMT_F, b);
}

void SettingsImpl::checkBoxCommitUseDefMsg_toggled(bool b) {

    changeFlag(USE_CMT_MSG_F, b);
}

void SettingsImpl::lineEditExternalDiffViewer_textChanged(const QString& s) {

    config::base().setValue("general.external_diff_viewer", s);
}

void SettingsImpl::lineEditExternalEditor_textChanged(const QString& s) {

    config::base().setValue("general.external_editor", s);
}

void SettingsImpl::lineEditApplyPatchExtraOptions_textChanged(const QString& s) {

    config::base().setValue("patch.args_2", s);
}

void SettingsImpl::lineEditFormatPatchExtraOptions_textChanged(const QString& s) {

    config::base().setValue("patch.args", s);
}

void SettingsImpl::lineEditExcludeFile_textChanged(const QString& s) {

    config::base().setValue("working_dir.exclude.from", s);
}

void SettingsImpl::lineEditExcludePerDir_textChanged(const QString& s) {

    config::base().setValue("working_dir.exclude.per_directory", s);
}

void SettingsImpl::lineEditTemplate_textChanged(const QString& s) {

    config::base().setValue("commit.template_file_path", s);
}

void SettingsImpl::lineEditCommitExtraOptions_textChanged(const QString& s) {

    config::base().setValue("commit/args", s);
}
