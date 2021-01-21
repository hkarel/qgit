/*
    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#pragma once

#include "exceptionmanager.h"
#include "common.h"
#include "consoleimpl.h"
#include "ui_mainwindow.h"

#include "shared/defmac.h"

#include <QProcess>
#include <QRegExp>
#include <QDir>

class QAction;
class QCloseEvent;
class QComboBox;
class QEvent;
class QListWidgetItem;
class QModelIndex;
class QProgressBar;
class QShortcutEvent;
class QTextEdit;

class Domain;
class Git;
class FileHistory;
class FileView;
class RevsView;

class MainImpl : public QMainWindow, public Ui_MainWindow {
Q_OBJECT
public:
    MainImpl(const QString& curDir = "", QWidget* parent = 0);
    void updateContextActions(const QString& newRevSha, const QString& newFileName, bool isDir, bool found);
    const QString getRevisionDesc(const QString& sha);
	const QString currentDir() const {return curDir;}

    // not buildable with Qt designer, will be created manually
    QLineEdit* lineSHA;
    QLineEdit* lineFilter;

    enum ComboSearch {
        CS_SHORT_LOG,
        CS_LOG_MSG,
        CS_AUTHOR,
        CS_SHA1,
        CS_FILE,
        CS_PATCH,
        CS_PATCH_REGEXP
    };

    QComboBox* cboxSearch;

    void loadGeometry();
    void saveGeometry();

signals:
    void highlightPatch(const QString&, bool);
    void updateRevDesc();
    void closeAllWindows();
    void closeAllTabs();
    void changeFont(const QFont&);
    void typeWriterFontChanged();
    void flagChanged(uint);

private slots:
    void newRevsAdded(const FileHistory*, const QVector<ShaString>&);
    void fileNamesLoad(int, int);
	void applyRevisions(const QStringList& shas, const QString& remoteRepo);
	bool applyPatches(const QStringList &files);
	void rebase(const QString& from, const QString& to, const QString& onto);
	void merge(const QStringList& shas, const QString& into);
	void moveRef(const QString& refName, const QString& toSHA);
    void shortCutActivated();
    void consoleDestroyed(QObject*);

protected slots:
    void initWithEventLoopActive();
    void refreshRepo(bool setCurRevAfterLoad = true);
    void listViewLog_doubleClicked(const QModelIndex&);
    void fileList_itemDoubleClicked(QListWidgetItem*);
    void histListView_doubleClicked(const QModelIndex&);
    void customAction_triggered(QAction*);
    void customAction_exited(qgit::CustomActionData::Ptr);
    void goRef_triggered(QAction*);
    void openRecent_triggered(QAction*);
    void changesCommitted(bool);

    void lineSHA_returnPressed();
    void lineFilter_returnPressed();

    void on_tabWidget_currentChanged(int);
    void on_tabWidget_tabCloseRequested(int index);
    void on_treeView_itemDoubleClicked(QTreeWidgetItem*, int);

    void on_actBack_triggered(bool);
    void on_actForward_triggered(bool);
    void on_actFind_triggered(bool);
    void on_actFindNext_triggered(bool);
    void on_actRangeDlg_triggered(bool);
    void on_actViewRev_triggered(bool);
    void on_actViewFile_triggered(bool);
    void on_actViewFileNewTab_triggered(bool);
    void on_actViewDiff_triggered(bool);
    void on_actViewDiffNewTab_triggered(bool);
    void on_actExternalDiff_triggered(bool);
    void on_actExternalEditor_triggered(bool);
    void on_actSplitView_triggered(bool);
    void on_actToggleLogsDiff_triggered(bool);
    void on_actShowDescHeader_triggered(bool);
    void on_actOpenRepo_triggered(bool);
    void on_actOpenRepoNewWindow_triggered(bool);
    void on_actRefresh_triggered(bool);
    void on_actSaveFile_triggered(bool);
    void on_actMailFormatPatch_triggered(bool);
    void on_actMailApplyPatch_triggered(bool);
    void on_actSettings_triggered(bool);
    void on_actCommit_triggered(bool);
    void on_actAmend_triggered(bool);
    void on_actCheckout_triggered(bool);
    void on_actBranch_triggered(bool);
    void on_actTag_triggered(bool);
    void on_actDelete_triggered(bool);
    void on_actPush_triggered(bool);
    void on_actPop_triggered(bool);
    void on_actClose_triggered(bool);
    void on_actExit_triggered(bool);
    void on_actSearchAndFilter_triggered(bool);
    void on_actSearchAndHighlight_triggered(bool);
    void on_actCustomActionSetup_triggered(bool);
    void on_actCheckWorkDir_triggered(bool);
    void on_actShowTree_triggered(bool);
    void on_actFilterTree_triggered(bool);
    void on_actAbout_triggered(bool);
    void on_actHelp_triggered(bool);
    void on_actMarkDiffToSha_triggered(bool);

private:
    bool event(QEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void closeEvent(QCloseEvent* ce) override;

    void updateGlobalActions(bool b);
    void updateRevVariables(const QString& sha);
    void setupShortcuts();
    int currentTabType(Domain** t);
	int tabType(Domain** t, int index);
    void filterList(bool isOn, bool onlyHighlight);
    bool isMatch(const QString& sha, const QString& f, int cn, const QMap<QString,bool>& sm);
    void highlightAbbrevSha(const QString& abbrevSha);
    void setRepository(const QString& wd, bool = false, bool = false, const QStringList* = NULL, bool = false);
    void getExternalDiffArgs(QStringList* args, QStringList* filenames);
	QString copyFileToDiffIfNeeded(QStringList* filenames, QString sha);
    QStringList getExternalEditorArgs();
    void lineEditSHASetText(const QString& text);
    void updateCommitMenu(bool isStGITStack);
    void updateRecentRepoMenu(const QString& newEntry = "");
    void doUpdateRecentRepoMenu(const QString& newEntry);
    void doUpdateCustomActionMenu();
    void doBranchOrTag(bool isTag);
    void actCommit_setEnabled(bool b);
    void doContexPopup(const QString& sha);
    void doFileContexPopup(const QString& fileName, int type);
    void adjustFontSize(int delta);
    void scrollTextEdit(int delta);
    void goMatch(int delta);
    bool askApplyPatchParameters(bool* commit, bool* fold);
    QTextEdit* getCurrentTextEdit();
    template<class X> QList<X*>* getTabs(QWidget* tabPage = NULL);
    template<class X> X* firstTab(QWidget* startPage = NULL);
    void openFileTab(FileView* fv = NULL);

    EM_DECLARE(exExiting);

    Git* git;
    RevsView* rv;
    QProgressBar* pbFileNamesLoading;

    ConsoleImpl* console;

    // curDir is the repository working directory, could be different from qgit running
    // directory QDir::current(). Note that qgit could be run from subdirectory
    // so only after git->isArchive() that updates curDir to point to working directory
    // we are sure is correct.
    QString curDir;
    QString startUpDir;
	QString startUpFile;
    QString textToFind;
    QRegExp shortLogRE;
    QRegExp longLogRE;
	static const QRegExp emptySha;
    QMap<QString, QVariant> revision_variables; // variables used in generic input dialogs
    bool setRepositoryBusy;
};

class ExternalDiffProc : public QProcess {
Q_OBJECT
public:
    ExternalDiffProc(const QStringList& f, QObject* p)
        : QProcess(p), filenames(f) {

        chk_connect_a(this, SIGNAL(finished(int, QProcess::ExitStatus)),
                      this, SLOT(on_finished(int, QProcess::ExitStatus)));
    }
    ~ExternalDiffProc() {

        terminate();
        removeFiles();
    }
    QStringList filenames;

private slots:
    void on_finished(int, QProcess::ExitStatus) { deleteLater(); }

private:
    void removeFiles() {

        if (!filenames.empty()) {
            QDir d; // remove temporary files to diff on
			for (int i = 0; i < filenames.size(); i++)
			{
				d.remove(filenames[i]);
			}
        }
    }
};

class ExternalEditorProc : public QProcess {
Q_OBJECT
public:
    ExternalEditorProc(QObject* p)
        : QProcess(p) {

        chk_connect_a(this, SIGNAL(finished(int, QProcess::ExitStatus)),
                      this, SLOT(on_finished(int, QProcess::ExitStatus)));
    }
    ~ExternalEditorProc() {
        terminate();
    }

private slots:
    void on_finished(int, QProcess::ExitStatus) { deleteLater(); }

private:
};

//----------------------------------------------------------------------------

template<class X>
QList<X*>* MainImpl::getTabs(QWidget* tabPage)
{
    QList<X*> l = this->findChildren<X*>();
    QList<X*>* ret = new QList<X*>;

    for (int i = 0; i < l.size(); ++i) {
        if (!tabPage || l.at(i)->tabPage() == tabPage)
            ret->append(l.at(i));
    }
    return ret; // 'ret' must be deleted by caller
}

template<class X> X*
MainImpl::firstTab(QWidget* startPage)
{
    int minVal = 99, firstVal = 99;
    int startPos = tabWidget->indexOf(startPage);
    X* min = NULL;
    X* first = NULL;
    QList<X*>* l = getTabs<X>();
    for (int i = 0; i < l->size(); ++i) {

        X* d = l->at(i);
        int idx = tabWidget->indexOf(d->tabPage());
        if (idx < minVal) {
            minVal = idx;
            min = d;
        }
        if (idx < firstVal && idx > startPos) {
            firstVal = idx;
            first = d;
        }
    }
    delete l;
    return (first ? first : min);
}
