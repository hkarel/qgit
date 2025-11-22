/*
    Description: settings dialog

    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#include "settingsimpl.h"
#include "common.h"
#include "git.h"
#include "spellcheck/spellcheck.h"

#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

#include <QTextCodec>
#include <QFileDialog>
#include <QFontDialog>

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

    qgit::FlagType f = qgit::flags();

    chkDiffCache->setChecked(f & DIFF_INDEX_F);
    chkNumbers->setChecked(f & NUMBERS_F);
    chkSign->setChecked(f & SIGN_PATCH_F);
    chkCommitSign->setChecked(f & SIGN_CMT_F);
    chkCommitVerify->setChecked(f & VERIFY_CMT_F);
    chkCommitUseDefMsg->setChecked(f & USE_CMT_MSG_F);
    chkRangeSelectDialog->setChecked(f & RANGE_SELECT_F);
    chkReopenLastRepo->setChecked(f & REOPEN_REPO_F);
    chkOpenInEditor->setChecked(f & OPEN_IN_EDITOR_F);
    chkCommitConfirm->setChecked(f & COMMIT_CONFIRM_F);
    chkShowCloseButton->setChecked(f & SHOW_CLOSE_BTN_F);
    chkRelativeDate->setChecked(f & REL_DATE_F);
    chkLogDiffTab->setChecked(f & LOG_DIFF_TAB_F);
    chkSmartLabels->setChecked(f & SMART_LBL_F);
    chkMsgOnNewSHA->setChecked(f & MSG_ON_NEW_F);
    chkEnableDragnDrop->setChecked(f & ENABLE_DRAGNDROP_F);
    chkShortCommitHash->setChecked(f & ENABLE_SHORTREF_F);
    chkEnableDragnDrop->setChecked(f & ENABLE_DRAGNDROP_F);
    chkSpellCheck->setChecked(f & SPELL_CHECK_F);

    QString FPOpt;
    config::base().getValue("patch.args", FPOpt);

    QString APOpt;
    config::base().getValue("patch.args2", APOpt);

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

    lineApplyPatchExtraOptions->setText(APOpt);
    lineFormatPatchExtraOptions->setText(FPOpt);
    lineExternalDiffViewer->setText(extDiff);
    lineExternalEditor->setText(extEditor);
    lineExcludeFile->setText(exFile);
    lineExcludePerDir->setText(exPerDir);
    lineTemplate->setText(commitTmpl);
    lineCommitExtraOptions->setText(CMArgs);
    lineTypeWriterFont->setText(TYPE_WRITER_FONT.toString());
    lineTypeWriterFont->setCursorPosition(0); // font description could be long

    cboxIconSize->setCurrentIndex(iconSizeIndex);

    setupCodecsCombo();
    on_chkDiffCache_toggled(chkDiffCache->isChecked());
    tabDialog->setCurrentIndex(defTab);
    userInfo();
    on_cboxGitConfigSource_activated(0);

    spellCheck().deinit();
    spellCheck().init();

    for (const QString& dict : spellCheck().dictNames())
    {
        QListWidgetItem* item = new QListWidgetItem(dict);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        //item->setData(Qt::UserRole, qVariantFromValue(cad));
        lstSpellLang->addItem(item);
    }

    QList<QString> langs;
    config::base().getValue("spell_check.langs", langs);
    for (int i = 0; i < lstSpellLang->count(); ++i) {
        QListWidgetItem* item = lstSpellLang->item(i);
        if (langs.contains(item->text()))
            item->setCheckState(Qt::Checked);
    }

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
        cboxUserSrc->addItem(*it);
        ++it;
        if (!found && !(*it).isEmpty())
            found = true;
        if (!found)
            idx++;
        ++it;
    }
    if (!found)
        idx = 0;

    cboxUserSrc->setCurrentIndex(idx);
    on_cboxUserSrc_activated(idx);
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
    treeGitConfig->clear();
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
        QList<QTreeWidgetItem*> items = treeGitConfig->findItems(name, Qt::MatchExactly);
        QTreeWidgetItem* item;

        if (items.isEmpty())
            item = new QTreeWidgetItem(treeGitConfig, QStringList(name));
        else
            item = items.first();

        addConfigOption(item, paths, value);
    }
    populatingGitConfig = false;
}

void SettingsImpl::on_treeGitConfig_itemChanged(QTreeWidgetItem* item, int i) {

    if (populatingGitConfig)
        return;
    //dbs(item->text(0));
    //dbs(item->text(1));
    //dbp("column %1", i);
}

void SettingsImpl::on_cboxUserSrc_activated(int i) {

    lineAuthor->setText(_uInfo[i * 3 + 1]);
    lineMail->setText(_uInfo[i * 3 + 2]);
}

void SettingsImpl::on_cboxGitConfigSource_activated(int) {

    readGitConfig(cboxGitConfigSource->currentText());
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
    cboxCodecs->insertItems(0, codecs);

    bool isGitArchive;
    QTextCodec* tc = git->getTextCodec(&isGitArchive);
    if (!isGitArchive) {
        cboxCodecs->setEnabled(false);
        return;
    }
    const QString curCodec(tc != 0 ? tc->name() : "Latin1");
    QString curCodecWld = QRegularExpression::wildcardToRegularExpression("*" + curCodec + "*");
    QRegularExpression re {curCodecWld, QRegularExpression::CaseInsensitiveOption};
    int idx = codecs.indexOf(re);
    if (idx == -1) {
        log_warn << log_format("Codec <%?> not available, using local codec", curCodec);
        idx = 0;
    }
    cboxCodecs->setCurrentIndex(idx);
    if (idx == 0) // signal activated() will not fire in this case
        on_cboxCodecs_activated(0);
}

void SettingsImpl::on_cboxIconSize_activated(int i) {

    config::base().setValue("general.icon_size_index", i);
}

void SettingsImpl::on_cboxCodecs_activated(int idx) {

    QString codecName(QTextCodec::codecForLocale()->name());
    if (idx != 0)
        codecName = cboxCodecs->currentText().section(" --", 0, 0);

    git->setTextCodec(QTextCodec::codecForName(codecName.toLatin1()));
}

void SettingsImpl::on_btnExtDiff_clicked() {

    QString extDiffName(QFileDialog::getOpenFileName(this,
                        "Select the patch viewer"));
    if (!extDiffName.isEmpty())
        lineExternalDiffViewer->setText(extDiffName);
}

void SettingsImpl::on_btnExtEditor_clicked() {

    QString extEditorName(QFileDialog::getOpenFileName(this,
                        "Select the external editor"));
    if (!extEditorName.isEmpty())
        lineExternalEditor->setText(extEditorName);
}

void SettingsImpl::on_btnFont_clicked() {

    bool ok;
    QFont fnt = QFontDialog::getFont(&ok, TYPE_WRITER_FONT, this);
    if (ok && TYPE_WRITER_FONT != fnt) {

        TYPE_WRITER_FONT = fnt;
        lineTypeWriterFont->setText(fnt.toString());
        lineTypeWriterFont->setCursorPosition(0);

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
        if (lstSpellLang->count()) {
            QList<QString> langs;
            for (int i = 0; i < lstSpellLang->count(); ++i) {
                QListWidgetItem* item = lstSpellLang->item(i);
                if (item->checkState() == Qt::Checked)
                    langs.append(item->text());
            }
            config::base().setValue("spell_check.langs", langs);
        }
        qgit::flags().save(); // Call config::base().save() inside
    }
    else
        config::base().rereadFile();

    spellCheck().deinit();
    saveGeometry();
}

void SettingsImpl::on_chkDiffCache_toggled(bool b) {

    lineExcludeFile->setEnabled(b);
    lineExcludePerDir->setEnabled(b);
    changeFlag(DIFF_INDEX_F, b);
}

void SettingsImpl::on_chkNumbers_toggled(bool b) {

    changeFlag(NUMBERS_F, b);
}

void SettingsImpl::on_chkSign_toggled(bool b) {

    changeFlag(SIGN_PATCH_F, b);
}

void SettingsImpl::on_chkRangeSelectDialog_toggled(bool b) {

    changeFlag(RANGE_SELECT_F, b);
}

void SettingsImpl::on_chkReopenLastRepo_toggled(bool b) {

    changeFlag(REOPEN_REPO_F, b);
}

void SettingsImpl::on_chkOpenInEditor_toggled(bool b) {

    changeFlag(OPEN_IN_EDITOR_F, b);
}

void SettingsImpl::on_chkCommitConfirm_toggled(bool b) {

    changeFlag(COMMIT_CONFIRM_F, b);
}

void SettingsImpl::on_chkShowCloseButton_toggled(bool b) {

    changeFlag(SHOW_CLOSE_BTN_F, b);
}

void SettingsImpl::on_chkRelativeDate_toggled(bool b) {

    changeFlag(REL_DATE_F, b);
}

void SettingsImpl::on_chkLogDiffTab_toggled(bool b) {

    changeFlag(LOG_DIFF_TAB_F, b);
}

void SettingsImpl::on_chkSmartLabels_toggled(bool b) {

    changeFlag(SMART_LBL_F, b);
}

void SettingsImpl::on_chkMsgOnNewSHA_toggled(bool b) {

    changeFlag(MSG_ON_NEW_F, b);
}

void SettingsImpl::on_chkEnableDragnDrop_toggled(bool b) {

    changeFlag(ENABLE_DRAGNDROP_F, b);
}

void SettingsImpl::on_chkShortCommitHash_toggled(bool b) {

    changeFlag(ENABLE_SHORTREF_F, b);
}

void SettingsImpl::on_chkCommitSign_toggled(bool b) {

    changeFlag(SIGN_CMT_F, b);
}

void SettingsImpl::on_chkCommitVerify_toggled(bool b) {

    changeFlag(VERIFY_CMT_F, b);
}

void SettingsImpl::on_chkCommitUseDefMsg_toggled(bool b) {

    changeFlag(USE_CMT_MSG_F, b);
}

void SettingsImpl::on_chkSpellCheck_toggled(bool b) {

    changeFlag(SPELL_CHECK_F, b);
}

void SettingsImpl::on_lineExternalDiffViewer_textChanged(const QString& s) {

    config::base().setValue("general.external_diff_viewer", s);
}

void SettingsImpl::on_lineExternalEditor_textChanged(const QString& s) {

    config::base().setValue("general.external_editor", s);
}

void SettingsImpl::on_lineApplyPatchExtraOptions_textChanged(const QString& s) {

    config::base().setValue("patch.args2", s);
}

void SettingsImpl::on_lineFormatPatchExtraOptions_textChanged(const QString& s) {

    config::base().setValue("patch.args", s);
}

void SettingsImpl::on_lineExcludeFile_textChanged(const QString& s) {

    config::base().setValue("working_dir.exclude.from", s);
}

void SettingsImpl::on_lineExcludePerDir_textChanged(const QString& s) {

    config::base().setValue("working_dir.exclude.per_directory", s);
}

void SettingsImpl::on_lineTemplate_textChanged(const QString& s) {

    config::base().setValue("commit.template_file_path", s);
}

void SettingsImpl::on_lineCommitExtraOptions_textChanged(const QString& s) {

    config::base().setValue("commit.args", s);
}
