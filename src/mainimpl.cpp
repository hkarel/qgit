/*
    Description: qgit main view

    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#include <assert.h>
#include "config.h" // defines PACKAGE_VERSION
#include "commitimpl.h"
#include "common.h"
#include "customactionimpl.h"
#include "fileview.h"
#include "git.h"
#include "help.h"
#include "listview.h"
#include "mainimpl.h"
#include "inputdialog.h"
#include "patchview.h"
#include "rangeselectimpl.h"
#include "revdesc.h"
#include "revsview.h"
#include "settingsimpl.h"
#include "treeview.h"
#include "ui_help.h"
#include "ui_revsview.h"
#include "ui_fileview.h"
#include "ui_patchview.h"

#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

#include <QCloseEvent>
#include <QEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QScrollBar>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>
#include <QWheelEvent>
#include <QTextCodec>
#include <QUuid>

using namespace qgit;

MainImpl::MainImpl(const QString& cd, QWidget* p) : QMainWindow(p) {

    EM_INIT(exExiting, "Exiting");

    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(this);

    // Add alternative shortcut for 'Amend' action
    QList<QKeySequence> actAmendShort;
    actAmendShort << QKeySequence("@") << QKeySequence("\"");
    actAmend->setShortcuts(actAmendShort);

    if (qgit::flags().test(SHOW_CLOSE_BTN_F))
    {
        toolBar->insertAction(actSearchAndFilter, actExit);
        toolBar->insertSeparator(actSearchAndFilter);
        QToolButton* btnExit = dynamic_cast<QToolButton*>(toolBar->widgetForAction(actExit));
        btnExit->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btnExit->setToolTip("");
    }

    // manual setup widgets not buildable with Qt designer
    lineSHA = new QLineEdit(NULL);
    lineFilter = new QLineEdit(NULL);
    cboxSearch = new QComboBox(NULL);
    QString list {"Short log,Log msg,Author,SHA1,File,Patch (-S),Patch (-G),Patch (regExp)"};
    cboxSearch->addItems(list.split(","));
    toolBar->addWidget(lineSHA);
    QAction* act = toolBar->insertWidget(actSearchAndFilter, lineFilter);
    toolBar->insertWidget(act, cboxSearch);
    chk_connect_a(lineSHA, SIGNAL(returnPressed()), this, SLOT(lineSHA_returnPressed()));
    chk_connect_a(lineFilter, SIGNAL(returnPressed()), this, SLOT(lineFilter_returnPressed()));

    // our interface to git world
    git = new Git(this);
    setupShortcuts();
    qApp->installEventFilter(this);

    // init native types
    setRepositoryBusy = false;

    // init filter match highlighters
    shortLogRE.setMinimal(true);
    shortLogRE.setCaseSensitivity(Qt::CaseInsensitive);
    longLogRE.setMinimal(true);
    longLogRE.setCaseSensitivity(Qt::CaseInsensitive);

    // set-up standard revisions and files list font
    QString fontDescr; // (settings.value(STD_FNT_KEY).toString());
    if (fontDescr.isEmpty()) {
        fontDescr = QFontDatabase::systemFont(QFontDatabase::GeneralFont).toString();
    }
    qgit::STD_FONT.fromString(fontDescr);

    int iconSizeIndex = 0;
    config::base().getValue("general.icon_size_index", iconSizeIndex);

    switch (iconSizeIndex) {
    case 1:
        toolBar->setIconSize(QSize(16, 16));
        break;
    case 2:
        toolBar->setIconSize(QSize(24, 24));
        break;
    case 3:
        toolBar->setIconSize(QSize(32, 32));
        break;
    case 4:
        toolBar->setIconSize(QSize(48, 48));
        break;
    case 5:
        toolBar->setIconSize(QSize(64, 64));
        break;
    }

    // set-up typewriter (fixed width) font
    fontDescr.clear();
    config::base().getValue("general.typewriter_font", fontDescr);

    if (fontDescr.isEmpty()) { // choose a sensible default
        QFont fnt = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        fontDescr = fnt.toString();              // to current style hint
    }
    qgit::TYPE_WRITER_FONT.fromString(fontDescr);

    // set-up tab view
    delete tabWidget->currentWidget(); // cannot be done in Qt Designer
    rv = new RevsView(this, git, true); // set has main domain
    tabWidget->addTab(rv->tabPage(), "&Rev list");

    // hide close button for rev list tab
    QTabBar* const tabBar = tabWidget->tabBar();
    tabBar->setTabButton(0, QTabBar::RightSide, NULL);
    tabBar->setTabButton(0, QTabBar::LeftSide, NULL);
    //chk_connect_a(tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(tabWidget_tabCloseRequested(int)));

    // set-up file names loading progress bar
    pbFileNamesLoading = new QProgressBar(statusBar());
    pbFileNamesLoading->setTextVisible(false);
    pbFileNamesLoading->setToolTip("Background file names loading");
    pbFileNamesLoading->hide();
    statusBar()->addPermanentWidget(pbFileNamesLoading);

    loadGeometry();
    treeView->hide();

    // set-up menu for recent visited repositories
    chk_connect_a(mnuFile, SIGNAL(triggered(QAction*)), this, SLOT(openRecent_triggered(QAction*)));
    doUpdateRecentRepoMenu("");

    // set-up menu for custom actions
    chk_connect_a(mnuActions, SIGNAL(triggered(QAction*)), this, SLOT(customAction_triggered(QAction*)));
    doUpdateCustomActionMenu();

    // manual adjust lineSHA width
    QString tmp(qgit::SHA_END_LENGTH, '8');
    int wd = lineSHA->fontMetrics().boundingRect(tmp).width();
    lineSHA->setMinimumWidth(wd);

    // disable all actions
    updateGlobalActions(false);

    chk_connect_a(git, SIGNAL(fileNamesLoad(int, int)), this, SLOT(fileNamesLoad(int, int)));

    chk_connect_a(git, SIGNAL(newRevsAdded(const FileHistory*, const QVector<ShaString>&)),
            this, SLOT(newRevsAdded(const FileHistory*, const QVector<ShaString>&)));

    chk_connect_a(this, SIGNAL(typeWriterFontChanged()), this, SIGNAL(updateRevDesc()));

    chk_connect_a(this, SIGNAL(changeFont(const QFont&)), git, SIGNAL(changeFont(const QFont&)));

    // connect cross-domain update signals
    chk_connect_a(rv->tab()->listViewLog, SIGNAL(doubleClicked(const QModelIndex&)),
                  this, SLOT(listViewLog_doubleClicked(const QModelIndex&)));
    chk_connect_a(rv->tab()->listViewLog, SIGNAL(showStatusMessage(const QString&,int)),
                  statusBar(), SLOT(showMessage(QString,int)));

    chk_connect_a(rv->tab()->fileList, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
                  this, SLOT(fileList_itemDoubleClicked(QListWidgetItem*)));

    //chk_connect_a(treeView, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
    //              this, SLOT(treeView_doubleClicked(QTreeWidgetItem*, int)));

    // use most recent repo as startup dir if it exists and user opted to do so
    QStringList recents;
    config::base().getValue("general.recent_open_repos", (QList<QString>&)recents);

    QDir checkRepo;
    if ( recents.size() >= 1
         && qgit::flags().test(REOPEN_REPO_F)
         && checkRepo.exists(recents.at(0)))
    {
        startUpDir = recents.at(0);
    }
    else {
        startUpDir = (cd.isEmpty() ? QDir::current().absolutePath() : cd);
    }

    // handle --view-file=* or --view-file * argument
    QStringList arglist = qApp->arguments();
    // Remove first argument which is the path of the current executable
    arglist.removeFirst();
    bool retainNext = false;
    foreach (QString arg, arglist) {
        if (retainNext) {
            retainNext = false;
            startUpFile = arg;
        } else if (arg == "--view-file")
            retainNext = true;
        else if (arg.startsWith("--view-file="))
            startUpFile = arg.mid(12);
    }

    // MainImpl c'tor is called before to enter event loop,
    // but some stuff requires event loop to init properly
    QTimer::singleShot(10, this, &MainImpl::initWithEventLoopActive);
}

void MainImpl::loadGeometry()
{
    QVector<int> v;
    config::base().getValue("geometry.mainwin.window", v);

    if (v.count() == 4) {
        move(v[0], v[1]);
        resize(v[2], v[3]);
    }

    QString sval;
    if (config::base().getValue("geometry.mainwin.spliter", sval)) {
        QByteArray ba = QByteArray::fromBase64(sval.toLatin1());
        treeSplitter->restoreState(ba);
    }
}

void MainImpl::saveGeometry()
{
    QPoint p = pos();
    QVector<int> v {p.x(), p.y(), width(), height()};
    config::base().setValue("geometry.mainwin.window", v);

    QByteArray ba = treeSplitter->saveState().toBase64();
    config::base().setValue("geometry.mainwin.spliter", QString::fromLatin1(ba));
}

void MainImpl::initWithEventLoopActive() {

    emit flagChanged(qgit::ENABLE_DRAGNDROP_F);
    git->checkEnvironment();
    setRepository(startUpDir);
    startUpDir = ""; // one shot

    // handle --view-file=* or --view-file * argument
    if (!startUpFile.isEmpty()) {
        rv->st.setSha("HEAD");
        rv->st.setFileName(startUpFile);
        openFileTab();
        startUpFile = QString(); // one shot
    }
}

void MainImpl::highlightAbbrevSha(const QString& abbrevSha) {
    // reset any previous highlight
    if (actSearchAndHighlight->isChecked())
        actSearchAndHighlight->toggle();

    // set to highlight on SHA matching
    cboxSearch->setCurrentIndex(CS_SHA1);

    // set substring to search for
    lineFilter->setText(abbrevSha);

    // go with highlighting
    actSearchAndHighlight->toggle();
}

void MainImpl::lineSHA_returnPressed() {

    QString text = lineSHA->text().trimmed();
    if (text.isEmpty())
        return;

    QString sha = git->getRefSha(text);
    if (!sha.isEmpty()) // good, we can resolve to an unique sha
    {
        rv->st.setSha(sha);
        UPDATE_DOMAIN(rv);
    } else { // try a multiple match search
        highlightAbbrevSha(lineSHA->text());
        goMatch(0);
    }
}

void MainImpl::on_actBack_triggered(bool) {

    lineSHA->undo(); // first for insert(text)
    if (lineSHA->text().isEmpty())
        lineSHA->undo(); // double undo, see RevsView::updatelineSHA()

    lineSHA_returnPressed();
}

void MainImpl::on_actForward_triggered(bool) {

    lineSHA->redo();
    if (lineSHA->text().isEmpty())
        lineSHA->redo();

    lineSHA_returnPressed();
}

// *************************** ExternalDiffViewer ***************************

void MainImpl::on_actExternalDiff_triggered(bool) {

    QStringList args;
    QStringList filenames;
    getExternalDiffArgs(&args, &filenames);
    ExternalDiffProc* externalDiff = new ExternalDiffProc(filenames, this);
    externalDiff->setWorkingDirectory(curDir);

    if (!qgit::startProcess(externalDiff, args)) {
        QString text("Cannot start external viewer: ");
        text.append(args[0]);
        QMessageBox::warning(this, "Error - QGit", text);
        delete externalDiff;
    }
}

const QRegExp MainImpl::emptySha("0*");

QString MainImpl::copyFileToDiffIfNeeded(QStringList* filenames, QString sha) {
    if (emptySha.exactMatch(sha))
    {
        return QString(curDir + "/" + rv->st.fileName());
    }

    QFileInfo f(rv->st.fileName());
    QFileInfo fi(f);

    QString fName(curDir + "/" + sha.left(6) + "_" + fi.fileName());

    QByteArray fileContent;
    QTextCodec* tc = QTextCodec::codecForLocale();

    QString fileSha(git->getFileSha(rv->st.fileName(), sha));
    git->getFile(fileSha, NULL, &fileContent, rv->st.fileName());
    if (!writeToFile(fName, tc->toUnicode(fileContent)))
    {
        statusBar()->showMessage("Unable to save " + fName);
    }

    filenames->append(fName);

    return fName;

}

void MainImpl::getExternalDiffArgs(QStringList* args, QStringList* filenames) {

    QString prevRevSha(rv->st.diffToSha());
    if (prevRevSha.isEmpty()) { // default to first parent
        const Rev* r = git->revLookup(rv->st.sha());
        prevRevSha = (r && r->parentsCount() > 0 ? r->parent(0) : rv->st.sha());
    }
    // save files to diff in working directory,
    // will be removed by ExternalDiffProc on exit
    QString fName1 = copyFileToDiffIfNeeded(filenames, rv->st.sha());
    QString fName2 = copyFileToDiffIfNeeded(filenames, prevRevSha);

    // get external diff viewer command
    QString extDiff;
    config::base().getValue("general.external_diff_viewer", extDiff);

    // if command doesn't have %1 and %2 to denote filenames, add them to end
    if (!extDiff.contains("%1")) {
        extDiff.append(" %1");
    }
    if (!extDiff.contains("%2")) {
        extDiff.append(" %2");
    }

    // set process arguments
    QStringList extDiffArgs = extDiff.split(' ');
    QString curArg;
    for (int i = 0; i < extDiffArgs.count(); i++) {
        curArg = extDiffArgs.value(i);

        // perform any filename replacements that are necessary
        // (done inside the loop to handle whitespace in paths properly)
        curArg.replace("%1", fName2);
        curArg.replace("%2", fName1);

        args->append(curArg);
    }

}

// *************************** ExternalEditor ***************************

void MainImpl::on_actExternalEditor_triggered(bool) {

    const QStringList &args = getExternalEditorArgs();
    ExternalEditorProc* externalEditor = new ExternalEditorProc(this);
    externalEditor->setWorkingDirectory(curDir);

    if (!qgit::startProcess(externalEditor, args)) {
        QString text("Cannot start external editor: ");
        text.append(args[0]);
        QMessageBox::warning(this, "Error - QGit", text);
        delete externalEditor;
    }
}

QStringList MainImpl::getExternalEditorArgs() {

    QString fName1(curDir + "/" + rv->st.fileName());

    // get external diff viewer command
    QString extEditor;
    config::base().getValue("general.external_editor", extEditor);

    // if command doesn't have %1 to denote filename, add to end
    if (!extEditor.contains("%1")) extEditor.append(" %1");

    // set process arguments
    QStringList args = extEditor.split(' ');
    for (int i = 0; i < args.count(); i++) {
        QString &curArg = args[i];

        // perform any filename replacements that are necessary
        // (done inside the loop to handle whitespace in paths properly)
        curArg.replace("%1", fName1);
    }
    return args;
}
// ********************** Repository open or changed *************************

void MainImpl::setRepository(const QString& newDir, bool refresh, bool keepSelection,
                             const QStringList* passedArgs, bool overwriteArgs) {

    /*
       Because Git::init calls processEvents(), if setRepository() is called in
       a tight loop (as example keeping pressed F5 refresh button) then a lot
       of pending init() calls would be stacked.
       On returning from processEvents() an exception is trown and init is exited,
       so we end up with a long list of 'exception thrown' messages.
       But the worst thing is that we have to wait for _all_ the init call to exit
           and this could take a long time as example in case of working directory refreshing
       'git update-index' of a big tree.
       So we use a guard flag to guarantee we have only one init() call 'in flight'
    */
    if (setRepositoryBusy)
        return;

    setRepositoryBusy = true;

    // check for a refresh or open of a new repository while in filtered view
    if (actFilterTree->isChecked() && passedArgs == NULL)
        // toggle() triggers a refresh and a following setRepository()
        // call that is filtered out by setRepositoryBusy guard flag
        actFilterTree->toggle(); // triggers actFilterTree_toggled()

    try {
        EM_REGISTER(exExiting);

        bool archiveChanged;
        git->getBaseDir(newDir, curDir, archiveChanged);

        git->stop(archiveChanged); // stop all pending processes, non blocking

        if (archiveChanged && refresh)
            log_warn << "Different dir with no range select";

        // now we can clear all our data
        bool complete = !refresh || !keepSelection;
        rv->clear(complete);
        if (archiveChanged)
            emit closeAllTabs();

        // disable all actions
        updateGlobalActions(false);
        updateContextActions("", "", false, false);
        actCommit_setEnabled(false);

        // tree name should be set before init because in case of
        // StGIT archives the first revs are sent before init returns
        QString n(curDir);
        treeView->setTreeName(n.prepend('/').section('/', -1, -1));

        QString curBranch;

        bool quit;
        bool ok = git->init(curDir, !refresh, passedArgs, overwriteArgs, &quit); // blocking call
        if (quit)
            goto exit;

        curBranch = git->getCurrentBranchName();
        if (curBranch.length()) curBranch = " [" + curBranch + "]";
        setWindowTitle(curDir + curBranch + " - QGit");

        if (actFilterTree->isChecked() && passedArgs) {
            QString msg = " - FILTER ON < %1 >";
            setWindowTitle(windowTitle() + msg.arg(passedArgs->join(" ")));
        }

        updateCommitMenu(ok && git->isStGITStack());
        actCheckWorkDir->setChecked(qgit::flags().test(DIFF_INDEX_F)); // could be changed in Git::init()

        if (ok) {
            updateGlobalActions(true);
            if (archiveChanged)
                updateRecentRepoMenu(curDir);
        } else
            statusBar()->showMessage("Not a git archive");

exit:
        setRepositoryBusy = false;
        EM_REMOVE(exExiting);

        if (quit && !startUpDir.isEmpty())
            close();

    }
    catch (int i) {
        EM_REMOVE(exExiting);

        if (EM_MATCH(i, exExiting, "loading repository")) {
            EM_THROW_PENDING;
            return;
        }
        log_warn << log_format("Exception '%?' not handled"
                               ". It will be re-throw", EM_DESC(i));
        alog::logger().flush();
        alog::logger().waitingFlush();
        throw;
    }
}

void MainImpl::updateGlobalActions(bool b) {

    actRefresh->setEnabled(b);
    actCheckWorkDir->setEnabled(b);
    actViewRev->setEnabled(b);
    actViewDiff->setEnabled(b);
    actViewDiffNewTab->setEnabled(b && firstTab<PatchView>());
    actShowTree->setEnabled(b);
    actMailApplyPatch->setEnabled(b);
    actMailFormatPatch->setEnabled(b);

    rv->setEnabled(b);
}

const QString REV_LOCAL_BRANCHES("REV_LOCAL_BRANCHES");
const QString REV_REMOTE_BRANCHES("REV_REMOTE_BRANCHES");
const QString REV_TAGS("REV_TAGS");
const QString CURRENT_BRANCH("CURRENT_BRANCH");
const QString SELECTED_NAME("SELECTED_NAME");

void MainImpl::updateRevVariables(const QString& sha) {
    QMap<QString, QVariant> &v = revision_variables;
    v.clear();

    const QStringList &remote_branches = git->getRefNames(sha, Git::RMT_BRANCH);
    QString curBranch;
    v.insert(REV_LOCAL_BRANCHES, git->getRefNames(sha, Git::BRANCH));
    v.insert(CURRENT_BRANCH, git->getCurrentBranchName());
    v.insert(REV_REMOTE_BRANCHES, remote_branches);
    v.insert(REV_TAGS, git->getRefNames(sha, Git::TAG));
    v.insert("SHA", sha);

    // determine which name the user clicked on
    ListView* lv = rv->tab()->listViewLog;
    v.insert(SELECTED_NAME, lv->selectedRefName());
}

void MainImpl::updateContextActions(const QString& newRevSha, const QString& newFileName,
                                    bool isDir, bool found) {

    bool pathActionsEnabled = !newFileName.isEmpty();
    bool fileActionsEnabled = (pathActionsEnabled && !isDir);

    actViewFile->setEnabled(fileActionsEnabled);
    actViewFileNewTab->setEnabled(fileActionsEnabled && firstTab<FileView>());
    actExternalDiff->setEnabled(fileActionsEnabled);
    actExternalEditor->setEnabled(fileActionsEnabled);
    actSaveFile->setEnabled(fileActionsEnabled);
    actFilterTree->setEnabled(pathActionsEnabled || actFilterTree->isChecked());

//    bool isTag       = false;
    bool isUnApplied = false;
    bool isApplied   = false;

    uint ref_type = 0;

    if (found) {
        const Rev* r = git->revLookup(newRevSha);
        ref_type = git->checkRef(newRevSha, Git::ANY_REF);
//        isTag = ref_type & Git::TAG;
        isUnApplied = r->isUnApplied;
        isApplied = r->isApplied;
    }
    actMarkDiffToSha->setEnabled(newRevSha != ZERO_SHA);
    actCheckout->setEnabled(found && (newRevSha != ZERO_SHA) && !isUnApplied);
    actBranch->setEnabled(found && (newRevSha != ZERO_SHA) && !isUnApplied);
    actTag->setEnabled(found && (newRevSha != ZERO_SHA) && !isUnApplied);
    actDelete->setEnabled(ref_type != 0);
    actPush->setEnabled(found && isUnApplied && git->isNothingToCommit());
    actPop->setEnabled(found && isApplied && git->isNothingToCommit());
}

// ************************* cross-domain update Actions ***************************

void MainImpl::listViewLog_doubleClicked(const QModelIndex& index) {

    if (index.isValid() && actViewDiff->isEnabled())
        actViewDiff->activate(QAction::Trigger);
}

void MainImpl::histListView_doubleClicked(const QModelIndex& index) {

    if (index.isValid() && actViewRev->isEnabled())
        actViewRev->activate(QAction::Trigger);
}

void MainImpl::fileList_itemDoubleClicked(QListWidgetItem* item) {

    bool isFirst = (item && item->listWidget()->item(0) == item);
    if (isFirst && rv->st.isMerge())
        return;

    if (qgit::flags().test(OPEN_IN_EDITOR_F)) {
        if (item && actExternalEditor->isEnabled())
            actExternalEditor->activate(QAction::Trigger);
    } else {
        bool isMainView = (item && item->listWidget() == rv->tab()->fileList);
        if (isMainView && actViewDiff->isEnabled())
            actViewDiff->activate(QAction::Trigger);

        if (item && !isMainView && actViewFile->isEnabled())
            actViewFile->activate(QAction::Trigger);
    }
}

void MainImpl::on_treeView_itemDoubleClicked(QTreeWidgetItem* item, int) {

    if (qgit::flags().test(OPEN_IN_EDITOR_F)) {
        if (item && actExternalEditor->isEnabled())
            actExternalEditor->activate(QAction::Trigger);
    } else {
        if (item && actViewFile->isEnabled())
            actViewFile->activate(QAction::Trigger);
    }
}

void MainImpl::on_tabWidget_tabCloseRequested(int index) {

    Domain* t;
    switch (tabType(&t, index)) {
    case TAB_REV:
        break;
    case TAB_PATCH:
        t->deleteWhenDone();
        actViewDiffNewTab->setEnabled(actViewDiff->isEnabled() && firstTab<PatchView>());
        break;
    case TAB_FILE:
        t->deleteWhenDone();
        actViewFileNewTab->setEnabled(actViewFile->isEnabled() && firstTab<FileView>());
        break;
    default:
        log_warn << "Unknown current page";
    }
}

void MainImpl::on_actRangeDlg_triggered(bool) {

    QString args;
    RangeSelectImpl rs(this, &args, false, git);
    bool quit = (rs.exec() == QDialog::Rejected); // modal execution
    if (!quit) {
        const QStringList l(args.split(" "));
        setRepository(curDir, true, true, &l, true);
    }
}

void MainImpl::on_actViewRev_triggered(bool) {

    Domain* t;
    if (currentTabType(&t) == TAB_FILE) {
        rv->st = t->st;
        UPDATE_DOMAIN(rv);
    }
    tabWidget->setCurrentWidget(rv->tabPage());
}

void MainImpl::on_actViewFile_triggered(bool) {

    openFileTab(firstTab<FileView>());
}

void MainImpl::on_actViewFileNewTab_triggered(bool) {

    openFileTab();
}

void MainImpl::openFileTab(FileView* fv) {

    if (!fv) {
        fv = new FileView(this, git);
        tabWidget->addTab(fv->tabPage(), "File");

        chk_connect_a(fv->tab()->histListView, SIGNAL(doubleClicked(const QModelIndex&)),
                      this, SLOT(histListView_doubleClicked(const QModelIndex&)));

        chk_connect_a(this, SIGNAL(closeAllTabs()), fv, SLOT(on_closeAllTabs()));

        actViewFileNewTab->setEnabled(actViewFile->isEnabled());
    }
    tabWidget->setCurrentWidget(fv->tabPage());
    fv->st = rv->st;
    UPDATE_DOMAIN(fv);
}

void MainImpl::on_actViewDiff_triggered(bool) {

    Domain* t;
    if (currentTabType(&t) == TAB_FILE) {
        rv->st = t->st;
        UPDATE_DOMAIN(rv);
    }
    rv->viewPatch(false);
    actViewDiffNewTab->setEnabled(true);

    if (actSearchAndFilter->isChecked() || actSearchAndHighlight->isChecked()) {
        bool isRegExp = (cboxSearch->currentIndex() == CS_PATCH_REGEXP);
        emit highlightPatch(lineFilter->text(), isRegExp);
    }
}

void MainImpl::on_actViewDiffNewTab_triggered(bool) {

    rv->viewPatch(true);
}

bool MainImpl::eventFilter(QObject* obj, QEvent* ev) {

    if (ev->type() == QEvent::Wheel) {

        QWheelEvent* e = static_cast<QWheelEvent*>(ev);
        if (e->modifiers() == Qt::AltModifier) {

            int idx = tabWidget->currentIndex();
            if (e->angleDelta().y() < 0)
                idx = (++idx == tabWidget->count() ? 0 : idx);
            else
                idx = (--idx < 0 ? tabWidget->count() - 1 : idx);

            tabWidget->setCurrentIndex(idx);
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void MainImpl::applyRevisions(const QStringList& remoteRevs, const QString& remoteRepo) {
    // remoteRevs is already sanity checked to contain some possible valid data

    QDir dr(curDir + qgit::PATCHES_DIR);
    dr.setFilter(QDir::Files);
    if (!dr.exists(remoteRepo)) {
        statusBar()->showMessage("Remote repository missing: " + remoteRepo);
        return;
    }
    if (dr.exists() && dr.count()) {
        statusBar()->showMessage(QString("Please remove stale import directory " + dr.absolutePath()));
        return;
    }
    bool workDirOnly, fold;
    if (!askApplyPatchParameters(&workDirOnly, &fold))
        return;

    // ok, let's go
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    raise();
    EM_PROCESS_EVENTS;

    uint revNum = 0;
    QStringList::const_iterator it(remoteRevs.constEnd());
    do {
        --it;
        QString sha = *it;
        QString msg = QString("Importing revision %1 of %2: %3");
        statusBar()->showMessage(msg.arg(++revNum).arg(remoteRevs.count()).arg(sha));

        // we create patches one by one
        if (!git->formatPatch(QStringList(sha), dr.absolutePath(), remoteRepo))
            break;

        dr.refresh();
        if (dr.count() != 1) {
            qDebug("ASSERT in on_droppedRevisions: found %i files "
                   "in %s", dr.count(), qPrintable(dr.absolutePath()));
            break;
        }
        const QString& fn(dr.absoluteFilePath(dr[0]));
        bool is_applied = git->applyPatchFile(fn, fold, Git::optDragDrop);
        dr.remove(fn);
        if (!is_applied) {
            statusBar()->showMessage(QString("Failed to import revision %1 of %2: %3")
                                     .arg(revNum).arg(remoteRevs.count()).arg(sha));
            break;
        }

    } while (it != remoteRevs.constBegin());

    if (it == remoteRevs.constBegin())
        statusBar()->clearMessage();

    if (workDirOnly && (revNum > 0))
        git->resetCommits(revNum);

    dr.rmdir(dr.absolutePath()); // 'dr' must be already empty
    QApplication::restoreOverrideCursor();
    refreshRepo();
}

bool MainImpl::applyPatches(const QStringList &files) {
    bool workDirOnly, fold;
    if (!askApplyPatchParameters(&workDirOnly, &fold))
        return false;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QStringList::const_iterator it=files.begin(), end=files.end();
    for(; it!=end; ++it) {
        statusBar()->showMessage("Applying " + *it);
        if (!git->applyPatchFile(*it, fold, Git::optDragDrop))
            statusBar()->showMessage("Failed to apply " + *it);
    }
    if (it == end) statusBar()->clearMessage();

    if (workDirOnly && (files.count() > 0))
        git->resetCommits(files.count());

    QApplication::restoreOverrideCursor();
    refreshRepo();
    return true;
}

void MainImpl::rebase(const QString &from, const QString &to, const QString &onto)
{
    bool success = false;
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    if (from.isEmpty()) {
        success = git->run(QString("git checkout -q %1").arg(to)) &&
                  git->run(QString("git rebase %1").arg(onto));
    } else {
        success = git->run(QString("git rebase --onto %3 %1^ %2").arg(from, to, onto));
    }
    if (!success) {
        // TODO say something about rebase failure
    }
    refreshRepo(true);
    QApplication::restoreOverrideCursor();
}

void MainImpl::merge(const QStringList &shas, const QString &into)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QString output;
    if (git->merge(into, shas, &output)) {
        refreshRepo(true);
        statusBar()->showMessage(QString("Successfully merged into %1").arg(into));
        on_actCommit_triggered(false);
    } else if (!output.isEmpty()) {
        QMessageBox::warning(this, "git merge failed",
                             QString("\n\nGit says: \n\n" + output));
    }
    refreshRepo(true);
    QApplication::restoreOverrideCursor();
}

void MainImpl::moveRef(const QString &target, const QString &toSHA)
{
    QString cmd;
    if (target.startsWith("remotes/")) {
        QString remote = target.section("/", 1, 1);
        QString name = target.section("/", 2);
        cmd = QString("git push -q %1 %2:%3").arg(remote, toSHA, name);
    } else if (target.startsWith("tags/")) {
        cmd = QString("git tag -f %1 %2").arg(target.section("/",1), toSHA);
    } else if (!target.isEmpty()) {
        const QString &sha = git->getRefSha(target, Git::BRANCH, false);
        if (sha.isEmpty()) return;
        const QStringList &children = git->getChildren(sha);
        if ((children.count() == 0 || (children.count() == 1 && children.front() == ZERO_SHA)) && // no children
            git->getRefNames(sha, Git::ANY_REF).count() == 1 && // last ref name
            QMessageBox::question(this, "move branch",
                                  QString("This is the last reference to this branch.\n"
                                          "Do you really want to move '%1'?").arg(target))
            == QMessageBox::No)
            return;

        if (target == git->getCurrentBranchName()) // move current branch
            cmd = QString("git checkout -q -B %1 %2").arg(target, toSHA);
        else // move any other local branch
            cmd = QString("git branch -f %1 %2").arg(target, toSHA);
    }
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    if (git->run(cmd)) refreshRepo(true);
    QApplication::restoreOverrideCursor();
}

// ******************************* Filter ******************************

void MainImpl::newRevsAdded(const FileHistory* fh, const QVector<ShaString>&) {

    if (!git->isMainHistory(fh))
        return;

    if (actSearchAndFilter->isChecked())
        on_actSearchAndFilter_triggered(true); // filter again on new arrived data

    if (actSearchAndHighlight->isChecked())
        on_actSearchAndHighlight_triggered(true); // filter again on new arrived data

    // first rev could be a StGIT unapplied patch so check more then once
    if (   !actCommit->isEnabled()
        && (!git->isNothingToCommit() || git->isUnknownFiles()))
        actCommit_setEnabled(true);
}

void MainImpl::lineFilter_returnPressed() {

    actSearchAndFilter->setChecked(true);
    on_actSearchAndFilter_triggered(true);
}

void MainImpl::on_actSearchAndFilter_triggered(bool isOn) {

    actSearchAndHighlight->setEnabled(!isOn);
    actSearchAndFilter->setEnabled(false);
    filterList(isOn, false); // blocking call
    actSearchAndFilter->setEnabled(true);
}

void MainImpl::on_actSearchAndHighlight_triggered(bool isOn) {

    actSearchAndFilter->setEnabled(!isOn);
    actSearchAndHighlight->setEnabled(false);
    filterList(isOn, true); // blocking call
    actSearchAndHighlight->setEnabled(true);
}

void MainImpl::filterList(bool isOn, bool onlyHighlight) {

    lineFilter->setEnabled(!isOn);
    cboxSearch->setEnabled(!isOn);

    const QString& filter(lineFilter->text());
    if (filter.isEmpty())
        return;

    ShaSet shaSet;
    bool patchNeedsUpdate, isRegExp;
    patchNeedsUpdate = isRegExp = false;
    int idx = cboxSearch->currentIndex(), colNum = 0;
    if (isOn) {
        switch (idx) {
        case CS_SHORT_LOG:
            colNum = LOG_COL;
            shortLogRE.setPattern(filter);
            break;
        case CS_LOG_MSG:
            colNum = LOG_MSG_COL;
            longLogRE.setPattern(filter);
            break;
        case CS_AUTHOR:
            colNum = AUTH_COL;
            break;
        case CS_SHA1:
            colNum = COMMIT_COL;
            break;
        case CS_FILE:
        case CS_PATCH:
        case CS_PATCH_GKEY:
        case CS_PATCH_REGEXP:
            colNum = SHA_MAP_COL;
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
            EM_PROCESS_EVENTS; // to paint wait cursor
            if (idx == CS_FILE)
                git->getFileFilter(filter, shaSet);
            else {
                isRegExp = (idx == CS_PATCH_REGEXP);
                bool useGKey = (idx == CS_PATCH_GKEY);
                if (!git->getPatchFilter(filter, useGKey, isRegExp, shaSet)) {
                    QApplication::restoreOverrideCursor();
                    actSearchAndFilter->toggle();
                    cboxSearch->setEnabled(true);
                    lineFilter->setEnabled(true);
                    return;
                }
                patchNeedsUpdate = (shaSet.count() > 0);
            }
            QApplication::restoreOverrideCursor();
            break;
        }
    } else {
        patchNeedsUpdate = (idx == CS_PATCH || idx == CS_PATCH_REGEXP);
        shortLogRE.setPattern("");
        longLogRE.setPattern("");
    }
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    ListView* lv = rv->tab()->listViewLog;
    int matchedCnt = lv->filterRows(isOn, onlyHighlight, filter, colNum, &shaSet);

    QApplication::restoreOverrideCursor();

    emit updateRevDesc(); // could be highlighted
    if (patchNeedsUpdate)
        emit highlightPatch(isOn ? filter : "", isRegExp);

    QString msg;
    if (isOn && !onlyHighlight)
        msg = QString("Found %1 matches. Toggle filter/highlight "
                      "button to remove the filter").arg(matchedCnt);
    QApplication::postEvent(rv, new MessageEvent(msg)); // deferred message, after update
}

bool MainImpl::event(QEvent* e) {

    BaseEvent* de = dynamic_cast<BaseEvent*>(e);
    if (!de)
        return QWidget::event(e);

    const QString& data = de->myData();
    bool ret = true;

    switch ((EventType)e->type()) {
    case ERROR_EV: {
        QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
        EM_PROCESS_EVENTS;
        MainExecErrorEvent* me = (MainExecErrorEvent*)e;
        QString text("An error occurred while executing command:\n\n");
        text.append(me->command() + "\n\n" + me->report());

        // Display message only if console window is not shown
        if (console == 0)
            QMessageBox::warning(this, "Error - QGit", text);

        QApplication::restoreOverrideCursor(); }
        break;
    case MSG_EV:
        statusBar()->showMessage(data);
        break;
    case POPUP_LIST_EV:
        doContexPopup(data);
        break;
    case POPUP_FILE_EV:
    case POPUP_TREE_EV:
        doFileContexPopup(data, e->type());
        break;
    default:
        log_warn << log_format("Unhandled event %?", e->type());
        ret = false;
    }
    return ret;
}

int MainImpl::currentTabType(Domain** t) {

    return tabType(t, tabWidget->currentIndex());
}

int MainImpl::tabType(Domain** t, int index) {

    *t = NULL;
    QWidget* curPage = tabWidget->widget(index);
    if (curPage == rv->tabPage()) {
        *t = rv;
        return TAB_REV;
    }
    QList<PatchView*>* l = getTabs<PatchView>(curPage);
    if (l->count() > 0) {
        *t = l->first();
        delete l;
        return TAB_PATCH;
    }
    delete l;
    QList<FileView*>* l2 = getTabs<FileView>(curPage);
    if (l2->count() > 0) {
        *t = l2->first();
        delete l2;
        return TAB_FILE;
    }
    if (l2->count() > 0)
        log_warn << "File not found";

    delete l2;
    return -1;
}

void MainImpl::on_tabWidget_currentChanged(int w) {

    if (w == -1)
        return;

    // set correct focus for keyboard browsing
    Domain* t;
    switch (currentTabType(&t)) {
    case TAB_REV:
        static_cast<RevsView*>(t)->tab()->listViewLog->setFocus();
        break;
    case TAB_PATCH:
        static_cast<PatchView*>(t)->tab()->textEditDiff->setFocus();
        break;
    case TAB_FILE:
        static_cast<FileView*>(t)->tab()->histListView->setFocus();
        break;
    default:
        log_warn << "Unknown current page";
    }
}

void MainImpl::setupShortcuts() {

    new QShortcut(Qt::Key_I,     this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_K,     this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_N,     this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_Left,  this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_Right, this, SLOT(shortCutActivated()));

    new QShortcut(Qt::Key_Delete,    this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_Backspace, this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_Space,     this, SLOT(shortCutActivated()));

    new QShortcut(Qt::Key_B, this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_D, this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_F, this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_P, this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_R, this, SLOT(shortCutActivated()));
    new QShortcut(Qt::Key_U, this, SLOT(shortCutActivated()));

    new QShortcut(Qt::SHIFT | Qt::Key_Up,    this, SLOT(shortCutActivated()));
    new QShortcut(Qt::SHIFT | Qt::Key_Down,  this, SLOT(shortCutActivated()));
    new QShortcut(Qt::CTRL  | Qt::Key_Plus,  this, SLOT(shortCutActivated()));
    new QShortcut(Qt::CTRL  | Qt::Key_Minus, this, SLOT(shortCutActivated()));
}

void MainImpl::shortCutActivated() {

    QShortcut* se = dynamic_cast<QShortcut*>(sender());

    if (se) {
        const QKeySequence& key = se->key();

        if (key == Qt::Key_I) {
            rv->tab()->listViewLog->on_keyUp();
        }
        else if ((key == Qt::Key_K) || (key == Qt::Key_N)) {
            rv->tab()->listViewLog->on_keyDown();
        }
        else if (key == (Qt::SHIFT | Qt::Key_Up)) {
            goMatch(-1);
        }
        else if (key == (Qt::SHIFT | Qt::Key_Down)) {
            goMatch(1);
        }
        else if (key == Qt::Key_Left) {
            on_actBack_triggered(false);
        }
        else if (key == Qt::Key_Right) {
            on_actForward_triggered(false);
        }
        else if (key == (Qt::CTRL | Qt::Key_Plus)) {
            adjustFontSize(1); //TODO replace magic constant
        }
        else if (key == (Qt::CTRL | Qt::Key_Minus)) {
            adjustFontSize(-1); //TODO replace magic constant
        }
        else if (key == Qt::Key_U) {
            scrollTextEdit(-18); //TODO replace magic constant
        }
        else if (key == Qt::Key_D) {
            scrollTextEdit(18); //TODO replace magic constant
        }
        else if (key == Qt::Key_Delete || key == Qt::Key_B || key == Qt::Key_Backspace) {
            scrollTextEdit(-1); //TODO replace magic constant
        }
        else if (key == Qt::Key_Space) {
            scrollTextEdit(1);
        }
        else if (key == Qt::Key_R) {
            tabWidget->setCurrentWidget(rv->tabPage());
        }
        else if (key == Qt::Key_P || key == Qt::Key_F) {
            QWidget* cp = tabWidget->currentWidget();
            Domain* d = (key == Qt::Key_P)
                        ? static_cast<Domain*>(firstTab<PatchView>(cp))
                        : static_cast<Domain*>(firstTab<FileView>(cp));
            if (d) tabWidget->setCurrentWidget(d->tabPage());
        }
    }
}

void MainImpl::consoleDestroyed(QObject*)
{
    console = 0;
}

void MainImpl::goMatch(int delta) {

    if (actSearchAndHighlight->isChecked())
        rv->tab()->listViewLog->scrollToNextHighlighted(delta);
    else
        rv->tab()->listViewLog->scrollToNext(delta);
}

QTextEdit* MainImpl::getCurrentTextEdit() {

    QTextEdit* te = NULL;
    Domain* t;
    switch (currentTabType(&t)) {
    case TAB_REV:
        te = static_cast<RevsView*>(t)->tab()->textBrowserDesc;
        if (!te->isVisible())
            te = static_cast<RevsView*>(t)->tab()->textEditDiff;
        break;
    case TAB_PATCH:
        te = static_cast<PatchView*>(t)->tab()->textEditDiff;
        break;
    case TAB_FILE:
        te = static_cast<FileView*>(t)->tab()->textEditFile;
        break;
    default:
        break;
    }
    return te;
}

void MainImpl::scrollTextEdit(int delta) {

    QTextEdit* te = getCurrentTextEdit();
    if (!te)
        return;

    QScrollBar* vs = te->verticalScrollBar();
    if (delta == 1 || delta == -1)
        vs->setValue(vs->value() + delta * (vs->pageStep() - vs->singleStep()));
    else
        vs->setValue(vs->value() + delta * vs->singleStep());
}

void MainImpl::adjustFontSize(int delta) {
// font size is changed on a 'per instance' base and only on list views

    int ps = qgit::STD_FONT.pointSize() + delta;
    if (ps < 2)
        return;

    qgit::STD_FONT.setPointSize(ps);

    //QSettings settings;
    //settings.setValue(qgit::STD_FNT_KEY, qgit::STD_FONT.toString());
    emit changeFont(qgit::STD_FONT);
}

void MainImpl::fileNamesLoad(int status, int value) {

    switch (status) {
    case 1: // stop
        pbFileNamesLoading->hide();
        break;
    case 2: // update
        pbFileNamesLoading->setValue(value);
        break;
    case 3: // start
        if (value > 200) { // don't show for few revisions
            pbFileNamesLoading->reset();
            pbFileNamesLoading->setMaximum(value);
            pbFileNamesLoading->show();
        }
        break;
    }
}

// ****************************** Menu *********************************

void MainImpl::updateCommitMenu(bool isStGITStack) {

    actCommit->setText(isStGITStack ? "Commit St&GIT patch..." : "&Commit...");
    actAmend->setText(isStGITStack ? "Refresh St&GIT patch..." : "&Amend commit...");
}

void MainImpl::updateRecentRepoMenu(const QString& newEntry) {

    // update menu of all windows
    foreach (QWidget* widget, QApplication::topLevelWidgets()) {

        MainImpl* w = dynamic_cast<MainImpl*>(widget);
        if (w)
            w->doUpdateRecentRepoMenu(newEntry);
    }
}

void MainImpl::doUpdateRecentRepoMenu(const QString& newEntry) {

    for (QAction* act : mnuFile->actions()) {
        if (act->data().toString().startsWith("RECENT"))
            mnuFile->removeAction(act);
    }

    QStringList recents;
    config::base().getValue("general.recent_open_repos", (QList<QString>&)recents);

    int idx = recents.indexOf(newEntry);
    if (idx != -1)
        recents.removeAt(idx);

    if (!newEntry.isEmpty())
        recents.prepend(newEntry);

    idx = 1;
    QStringList newRecents;
    for (const QString& s : recents) {
        QAction* newAction = mnuFile->addAction(QString::number(idx++) + " " + s);
        newAction->setData(QString("RECENT ") + s);
        newRecents << s;
        if (idx > MAX_RECENT_REPOS)
            break;
    }
    config::base().setValue("general.recent_open_repos", (QList<QString>&)newRecents);
    config::base().setNodeStyle("general.recent_open_repos", YAML::EmitterStyle::Block);
}

static void prepareRefSubmenu(QMenu* menu, const QStringList& refs, const QChar sep = '/') {

    for (const QString& ref : refs) {
        QStringList parts = ref.split(sep, QGIT_SPLITBEHAVIOR(SkipEmptyParts));
        QMenu* add_here = menu;
        for (const QString& pit : parts) {
            if (pit == parts.last())
                break;

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
            QMenu* found = add_here->findChild<QMenu*>(pit, Qt::FindDirectChildrenOnly);
#else
            QMenu* found = add_here->findChild<QMenu*>(pit);
#endif
            if(!found) {
                found = add_here->addMenu(pit);
                found->setObjectName(pit);
            }
            add_here = found;
        }
        QAction* act = add_here->addAction(ref);
        act->setData("Ref");
    }
}

void MainImpl::doContexPopup(const QString& sha) {

    QMenu contextMenu(this);
    QMenu contextBrnMenu("Branches...", this);
    QMenu contextRmtMenu("Remote branches...", this);
    QMenu contextTagMenu("Tags...", this);

    chk_connect_a(&contextMenu, SIGNAL(triggered(QAction*)), this, SLOT(goRef_triggered(QAction*)));

    Domain* t;
    int tt = currentTabType(&t);
    bool isRevPage = (tt == TAB_REV);
    bool isPatchPage = (tt == TAB_PATCH);
    bool isFilePage = (tt == TAB_FILE);

    if (isFilePage && actViewRev->isEnabled())
        contextMenu.addAction(actViewRev);

    if (!isPatchPage && actViewDiff->isEnabled())
        contextMenu.addAction(actViewDiff);

    if (isRevPage && actViewDiffNewTab->isEnabled())
        contextMenu.addAction(actViewDiffNewTab);

    if (!isFilePage && actExternalDiff->isEnabled())
        contextMenu.addAction(actExternalDiff);

    if (isFilePage && actExternalEditor->isEnabled())
        contextMenu.addAction(actExternalEditor);

    if (isRevPage) {
        updateRevVariables(sha);

        if (actCommit->isEnabled() && (sha == ZERO_SHA))
            contextMenu.addAction(actCommit);
        if (actCheckout->isEnabled())
            contextMenu.addAction(actCheckout);
        if (actBranch->isEnabled())
            contextMenu.addAction(actBranch);
        if (actTag->isEnabled())
            contextMenu.addAction(actTag);
        if (actDelete->isEnabled())
            contextMenu.addAction(actDelete);
        if (actMailFormatPatch->isEnabled())
            contextMenu.addAction(actMailFormatPatch);
        if (actPush->isEnabled())
            contextMenu.addAction(actPush);
        if (actPop->isEnabled())
            contextMenu.addAction(actPop);

        contextMenu.addSeparator();

        QStringList bn(git->getAllRefNames(Git::BRANCH, Git::optOnlyLoaded));
        bn.sort();
        prepareRefSubmenu(&contextBrnMenu, bn);
        contextMenu.addMenu(&contextBrnMenu);
        contextBrnMenu.setEnabled(bn.size() > 0);

        QStringList rbn(git->getAllRefNames(Git::RMT_BRANCH, Git::optOnlyLoaded));
        rbn.sort();
        prepareRefSubmenu(&contextRmtMenu, rbn);
        contextMenu.addMenu(&contextRmtMenu);
        contextRmtMenu.setEnabled(rbn.size() > 0);

        QStringList tn(git->getAllRefNames(Git::TAG, Git::optOnlyLoaded));
        tn.sort();
        prepareRefSubmenu(&contextTagMenu, tn);
        contextMenu.addSeparator();
        contextMenu.addMenu(&contextTagMenu);
        contextTagMenu.setEnabled(tn.size() > 0);

    }

    QPoint p = QCursor::pos();
    p += QPoint(10, 10);
    contextMenu.exec(p);

    // remove selected ref name after showing the popup
    revision_variables.remove(SELECTED_NAME);
}

void MainImpl::doFileContexPopup(const QString& fileName, int type) {

    QMenu contextMenu(this);

    Domain* t;
    int tt = currentTabType(&t);
    bool isRevPage = (tt == TAB_REV);
    bool isPatchPage = (tt == TAB_PATCH);
    bool isDir = treeView->isDir(fileName);

    if (type == POPUP_FILE_EV)
        if (!isPatchPage && actViewDiff->isEnabled())
            contextMenu.addAction(actViewDiff);

    if (!isDir && actViewFile->isEnabled())
        contextMenu.addAction(actViewFile);

    if (!isDir && actViewFileNewTab->isEnabled())
        contextMenu.addAction(actViewFileNewTab);

    if (!isRevPage && (type == POPUP_FILE_EV) && actViewRev->isEnabled())
        contextMenu.addAction(actViewRev);

    if (actFilterTree->isEnabled())
        contextMenu.addAction(actFilterTree);

    if (!isDir) {
        if (actSaveFile->isEnabled())
            contextMenu.addAction(actSaveFile);
        if ((type == POPUP_FILE_EV) && actExternalDiff->isEnabled())
            contextMenu.addAction(actExternalDiff);
        if ((type == POPUP_FILE_EV) && actExternalEditor->isEnabled())
            contextMenu.addAction(actExternalEditor);
        if (actExternalEditor->isEnabled())
            contextMenu.addAction(actExternalEditor);
    }
    contextMenu.exec(QCursor::pos());
}

void MainImpl::goRef_triggered(QAction* act) {

    if (!act || act->data() != "Ref")
        return;

    const QString& refSha(git->getRefSha(act->iconText()));
    rv->st.setSha(refSha);
    UPDATE_DOMAIN(rv);
}

void MainImpl::on_actSplitView_triggered(bool) {

    Domain* t;
    switch (currentTabType(&t)) {
    case TAB_REV: {
        RevsView* rv = static_cast<RevsView*>(t);
        QWidget* w = rv->tab()->fileList;
        QSplitter* sp = static_cast<QSplitter*>(w->parent());
        sp->setHidden(w->isVisible()); }
        break;
    case TAB_PATCH: {
        PatchView* pv = static_cast<PatchView*>(t);
        QWidget* w = pv->tab()->textBrowserDesc;
        w->setHidden(w->isVisible()); }
        break;
    case TAB_FILE: {
        FileView* fv = static_cast<FileView*>(t);
        QWidget* w = fv->tab()->histListView;
        w->setHidden(w->isVisible()); }
        break;
    default:
        log_warn << "Unknown current page";
    }
}

void MainImpl::on_actToggleLogsDiff_triggered(bool) {

    Domain* t;
    if (currentTabType(&t) == TAB_REV) {
        RevsView* rv = static_cast<RevsView*>(t);
        rv->toggleDiffView();
    }
}

const QString MainImpl::getRevisionDesc(const QString& sha) {

    bool showHeader = actShowDescHeader->isChecked();
    return git->getDesc(sha, shortLogRE, longLogRE, showHeader, NULL);
}

void MainImpl::on_actShowDescHeader_triggered(bool) {

    // each open tab get his description,
    // could be different for each tab
    emit updateRevDesc();
}

void MainImpl::on_actShowTree_triggered(bool b) {

    if (b) {
        treeView->show();
        UPDATE_DOMAIN(rv);
    } else {
        //saveGeometry();
        treeView->hide();
    }
}

void MainImpl::on_actSaveFile_triggered(bool) {

    QFileInfo f(rv->st.fileName());
    const QString fileName(QFileDialog::getSaveFileName(this, "Save file as", f.fileName()));
    if (fileName.isEmpty())
        return;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QString fileSha(git->getFileSha(rv->st.fileName(), rv->st.sha()));
    if (!git->saveFile(fileSha, rv->st.fileName(), fileName))
        statusBar()->showMessage("Unable to save " + fileName);

    QApplication::restoreOverrideCursor();
}

void MainImpl::openRecent_triggered(QAction* act) {

    const QString dataString = act->data().toString();
    if (!dataString.startsWith("RECENT"))
        // only recent repos entries have "RECENT" in data field
        return;

    const QString workDir = dataString.mid(7);
    if (!workDir.isEmpty()) {
        QDir d(workDir);
        if (d.exists())
            setRepository(workDir);
        else
            statusBar()->showMessage("Directory '" + workDir +
                                     "' does not seem to exist anymore");
    }
}

void MainImpl::on_actOpenRepo_triggered(bool) {

    const QString dirName(QFileDialog::getExistingDirectory(this, "Choose a directory", curDir));
    if (!dirName.isEmpty()) {
        QDir d(dirName);
        setRepository(d.absolutePath());
    }
}

void MainImpl::on_actOpenRepoNewWindow_triggered(bool) {

    const QString dirName(QFileDialog::getExistingDirectory(this, "Choose a directory", curDir));
    if (!dirName.isEmpty()) {
        QDir d(dirName);
        MainImpl* newWin = new MainImpl(d.absolutePath());
        newWin->show();
    }
}

void MainImpl::refreshRepo(bool b) {

    setRepository(curDir, true, b);
}

void MainImpl::on_actRefresh_triggered(bool) {

    refreshRepo(true);
}

void MainImpl::on_actMailFormatPatch_triggered(bool) {

    QStringList selectedItems;
    rv->tab()->listViewLog->getSelectedItems(selectedItems);
    if (selectedItems.isEmpty()) {
        statusBar()->showMessage("At least one selected revision needed");
        return;
    }
    if (selectedItems.contains(ZERO_SHA)) {
        statusBar()->showMessage("Unable to save a patch for not committed content");
        return;
    }

    QString outDir = curDir;
    config::base().getValue("patch.last_dir", outDir);
    QString dirPath(QFileDialog::getExistingDirectory(this,
                    "Choose destination directory - Save Patch", outDir));
    if (dirPath.isEmpty())
        return;

    QDir d(dirPath);
    config::base().setValue("patch.last_dir", d.absolutePath());
    config::base().saveFile();

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    git->formatPatch(selectedItems, d.absolutePath());
    QApplication::restoreOverrideCursor();
}

bool MainImpl::askApplyPatchParameters(bool* workDirOnly, bool* fold) {

    int ret = 0;
    if (!git->isStGITStack()) {
        ret = QMessageBox::question(this, "Apply Patch",
              "Do you want to commit or just to apply changes to "
                      "working directory?", "&Cancel", "&Working directory", "&Commit", 0, 0);
        *workDirOnly = (ret == 1);
        *fold = false;
    } else {
        ret = QMessageBox::question(this, "Apply Patch", "Do you want to "
              "import or fold the patch?", "&Cancel", "&Fold", "&Import", 0, 0);
        *workDirOnly = false;
        *fold = (ret == 1);
    }
    return (ret != 0);
}

void MainImpl::on_actMailApplyPatch_triggered(bool) {

    QString outDir = curDir;
    config::base().getValue("patch.last_dir", outDir);

    QString patchName(QFileDialog::getOpenFileName(this,
                      "Choose the patch file - Apply Patch", outDir,
                      "Patches (*.patch *.diff *.eml)\nAll Files (*.*)"));
    if (patchName.isEmpty())
        return;

    QFileInfo f(patchName);
    config::base().setValue("patch.last_dir", f.absolutePath());
    config::base().saveFile();

    bool workDirOnly, fold;
    if (!askApplyPatchParameters(&workDirOnly, &fold))
        return;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    bool ok = git->applyPatchFile(f.absoluteFilePath(), fold, !Git::optDragDrop);
    if (workDirOnly && ok)
        git->resetCommits(1);

    QApplication::restoreOverrideCursor();
    refreshRepo(false);
}

void MainImpl::on_actCheckWorkDir_triggered(bool b) {

    if (!actCheckWorkDir->isEnabled()) // to avoid looping with setChecked()
        return;

    qgit::flags().set(DIFF_INDEX_F, b);
    bool keepSelection = (rv->st.sha() != ZERO_SHA);
    refreshRepo(keepSelection);
}

void MainImpl::on_actSettings_triggered(bool) {

    SettingsImpl setings {this, git};
    chk_connect_a(&setings, SIGNAL(typeWriterFontChanged()),
                  this, SIGNAL(typeWriterFontChanged()));

    chk_connect_a(&setings, SIGNAL(flagChanged(uint)),
                  this, SIGNAL(flagChanged(uint)));

    setings.exec();

    // update actCheckWorkDir if necessary
    if (actCheckWorkDir->isChecked() != qgit::flags().test(DIFF_INDEX_F))
        actCheckWorkDir->toggle();
}

void MainImpl::on_actCustomActionSetup_triggered(bool) {

    CustomActionImpl ca {this};
    if (ca.exec() == QDialog::Accepted)
        doUpdateCustomActionMenu();
}

void MainImpl::doUpdateCustomActionMenu() {

    QAction* setupAct = mnuActions->actions().first(); // is never empty
    mnuActions->removeAction(setupAct);
    mnuActions->clear();
    mnuActions->addAction(setupAct);
    mnuActions->addSeparator();

    YamlConfig::Func loadFunc =
        [this](YamlConfig* conf, YAML::Node& actions, bool /*logWarn*/)
    {
        for (size_t i = 0; i < actions.size(); ++i)
        {
            CustomActionData::Ptr cad {new CustomActionData};
            conf->getValue(actions[i], "name",    cad->name);
            conf->getValue(actions[i], "command", cad->command);
            conf->getValue(actions[i], "refresh", cad->refresh);

            QAction* act = mnuActions->addAction(cad->name);
            act->setMenuRole(QAction::NoRole);
            act->setData(QVariant::fromValue(cad));
        }
        return true;
    };
    config::base().getValue("custom_actions", loadFunc);
}

void MainImpl::customAction_triggered(QAction* act) {

    QVariant var = act->data();
    if (!var.canConvert<CustomActionData::Ptr>())
        return;

    CustomActionData::Ptr cad = var.value<CustomActionData::Ptr>();
    QString cmd = cad->command;

    QString actionName = act->text();
    //actionName.remove("&");

//    QString cmd = set.value(ACT_GROUP_KEY + actionName + ACT_TEXT_KEY).toString().trimmed();
//    if (testFlag(ACT_CMD_LINE_F, ACT_GROUP_KEY + actionName + ACT_FLAGS_KEY)) {
//        // for backwards compatibility: if ACT_CMD_LINE_F is set, insert a dialog token in first line
//        int pos = cmd.indexOf('\n');
//        if (pos < 0) pos = cmd.length();
//        cmd.insert(pos, " %lineedit:cmdline args%");
//    }

    updateRevVariables(lineSHA->text());
    InputDialog dlg(cmd, revision_variables, "Run custom action: " + actionName, this);
    if (!dlg.empty() && dlg.exec() != QDialog::Accepted) return;
    try {
        cmd = dlg.replace(revision_variables); // replace variables
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Custom action command", e.what());
        return;
    }

    if (cmd.isEmpty())
        return;

    console = new ConsoleImpl(cad, git); // has Qt::WA_DeleteOnClose attribute

    chk_connect_a(this, SIGNAL(typeWriterFontChanged()),
                  console, SLOT(typeWriterFontChanged()));

    chk_connect_a(this, SIGNAL(closeAllWindows()),
                  console, SLOT(close()));
    chk_connect_a(console, SIGNAL(customAction_exited(qgit::CustomActionData::Ptr)),
                  this, SLOT(customAction_exited(qgit::CustomActionData::Ptr)));
    chk_connect_a(console, SIGNAL(destroyed(QObject*)),
                  this, SLOT(consoleDestroyed(QObject*)));

    if (console->start(cmd))
        console->show();
}

void MainImpl::customAction_exited(qgit::CustomActionData::Ptr cad) {

    if (cad->refresh)
        QTimer::singleShot(10, this, [this]() {refreshRepo(true);}); // outside of event handler
}

void MainImpl::on_actCommit_triggered(bool) {

    CommitImpl* c = new CommitImpl(git, false); // has Qt::WA_DeleteOnClose attribute
    chk_connect_a(this, SIGNAL(closeAllWindows()), c, SLOT(close()));
    chk_connect_a(c, SIGNAL(changesCommitted(bool)), this, SLOT(changesCommitted(bool)));
    c->show();
}

void MainImpl::on_actAmend_triggered(bool) {

    CommitImpl* c = new CommitImpl(git, true); // has Qt::WA_DeleteOnClose attribute
    chk_connect_a(this, SIGNAL(closeAllWindows()), c, SLOT(close()));
    chk_connect_a(c, SIGNAL(changesCommitted(bool)), this, SLOT(changesCommitted(bool)));
    c->show();
}

void MainImpl::changesCommitted(bool ok) {

    if (ok)
        refreshRepo(false);
    else
        statusBar()->showMessage("Failed to commit changes");
}

void MainImpl::actCommit_setEnabled(bool b) {

    // pop and push commands fail if there are local changes,
    // so in this case we disable ActPop and ActPush
    if (b) {
        actPush->setEnabled(false);
        actPop->setEnabled(false);
    }
    actCommit->setEnabled(b);
}

/** Checkout supports various operation modes:
 *  - switching to an existing branch (standard use case)
 *  - create and checkout a new branch
 *  - resetting an existing branch to a new sha
 */
void MainImpl::on_actCheckout_triggered(bool)
{
    QString sha = lineSHA->text(), rev = sha;
    const QString branchKey("local branch name");
    QString cmd = "git checkout -q ";

    const QString &selected_name = revision_variables.value(SELECTED_NAME).toString();
    const QString &current_branch = revision_variables.value(CURRENT_BRANCH).toString();
    const QStringList &local_branches = revision_variables.value(REV_LOCAL_BRANCHES).toStringList();

    if (!selected_name.isEmpty() &&
        local_branches.contains(selected_name) &&
        selected_name != current_branch) {
        // standard branch switching: directly checkout selected branch
        rev = selected_name;
    } else {
        // ask for (new) local branch name
        QString title = QString("Checkout ");
        if (selected_name.isEmpty()) {
            title += QString("revision ") + sha.mid(0, 8);
        } else {
            title    += QString("branch ") + selected_name;
            rev = selected_name;
        }
        // merge all reference names into a single list
        const QStringList &rmts = revision_variables.value(REV_REMOTE_BRANCHES).toStringList();
        QStringList all_names;
        all_names << revision_variables.value(REV_LOCAL_BRANCHES).toStringList();
        for(QStringList::const_iterator it=rmts.begin(), end=rmts.end(); it!=end; ++it) {
            // drop initial <origin>/ from name
            int pos = it->indexOf('/'); if (pos < 0) continue;
            all_names << it->mid(pos+1);
        }
        revision_variables.insert("ALL_NAMES", all_names);

        InputDialog dlg(QString("%combobox[editable,ref,empty]:%1=$ALL_NAMES%").arg(branchKey), revision_variables, title, this);
        if (dlg.exec() != QDialog::Accepted) return;

        QString branch = dlg.value(branchKey).toString();
        if (!branch.isEmpty()) {
            const QString& refsha = git->getRefSha(branch, Git::BRANCH, true);
            if (refsha == sha)
                rev = branch; // checkout existing branch, even if name wasn't directly selected
            else if (!refsha.isEmpty()) {
                if (QMessageBox::warning(this, "Checkout " + branch,
                                         QString("Branch %1 already exists. Reset?").arg(branch),
                                         QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                    != QMessageBox::Yes)
                    return;
                else
                    cmd.append("-B ").append(branch); // reset an existing branch
            } else {
                cmd.append("-b ").append(branch); // create new local branch
            }
        } // if new branch name is empty, checkout detached
    }

    cmd.append(" ").append(rev);
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    if (!git->run(cmd)) statusBar()->showMessage("Failed to checkout " + rev);
    refreshRepo(true);
    QApplication::restoreOverrideCursor();
}

void MainImpl::on_actBranch_triggered(bool) {

    doBranchOrTag(false);
}

void MainImpl::on_actTag_triggered(bool) {

    doBranchOrTag(true);
}

const QStringList& stripNames(QStringList& names) {
    for(QStringList::iterator it=names.begin(), end=names.end(); it!=end; ++it)
        *it = it->section('/', -1);
    return names;
}

void MainImpl::doBranchOrTag(bool isTag) {
    const QString sha = lineSHA->text();
    QString refDesc = isTag ? "tag" : "branch";
    QString dlgTitle = "Create " + refDesc + " - QGit";

    QString dlgDesc = "%lineedit[ref]:name=$ALL_NAMES%";
    InputDialog::VariableMap dlgVars;
    QStringList allNames = git->getAllRefNames(Git::BRANCH | Git::RMT_BRANCH | Git::TAG, false);
    stripNames(allNames);
    allNames.removeDuplicates();
    allNames.sort();
    dlgVars.insert("ALL_NAMES", allNames);

    if (isTag) {
        QString revDesc(rv->tab()->listViewLog->currentText(LOG_COL));
        dlgDesc += "%textedit:message=$MESSAGE%";
        dlgVars.insert("MESSAGE", revDesc);
    }

    InputDialog dlg(dlgDesc, dlgVars, dlgTitle, this);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString& ref = dlg.value("name").toString();

    bool force = false;
    if (!git->getRefSha(ref, isTag ? Git::TAG : Git::BRANCH, false).isEmpty()) {
        if (QMessageBox::warning(this, dlgTitle,
                                 refDesc + " name '" + ref + "' already exists.\n"
                                 "Force reset?", QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) != QMessageBox::Yes)
            return;
        force = true;
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QString cmd;
    if (isTag) {
        const QString& msg = dlg.value("message").toString();
        cmd = "git tag ";
        if (!msg.isEmpty()) cmd += "-m \"" + msg + "\" ";
    } else {
        cmd = "git branch ";
    }
    if (force) cmd += "-f ";
    cmd += ref + " " + sha;

    if (git->run(cmd))
        refreshRepo(true);
    else
        statusBar()->showMessage("Failed to create " + refDesc + " " + ref);

    QApplication::restoreOverrideCursor();
}

// put a ref name into a corresponding StringList for tags, remotes, and local branches
typedef QMap<QString, QStringList> RefGroupMap;
static void groupRef(const QString& ref, RefGroupMap& groups) {
    QString group, name;
    if (ref.startsWith("tags/")) { group = ref.left(5); name = ref.mid(5); }
    else if (ref.startsWith("remotes/")) { group = ref.section('/', 1, 1); name = ref.section('/', 2); }
    else { group = ""; name = ref; }
    if (!groups.contains(group))
        groups.insert(group, QStringList());
    QStringList &l = groups[group];
    l << name;
}

void MainImpl::on_actDelete_triggered(bool) {

    const QString &selected_name = revision_variables.value(SELECTED_NAME).toString();
    const QStringList &tags = revision_variables.value(REV_TAGS).toStringList();
    const QStringList &rmts = revision_variables.value(REV_REMOTE_BRANCHES).toStringList();

    // merge all reference names into a single list
    QStringList all_names;
    all_names << revision_variables.value(REV_LOCAL_BRANCHES).toStringList();
    for (QStringList::const_iterator it=rmts.begin(), end=rmts.end(); it!=end; ++it)
        all_names << "remotes/" + *it;
    for (QStringList::const_iterator it=tags.begin(), end=tags.end(); it!=end; ++it)
        all_names << "tags/" + *it;

    // group selected names by origin and determine which ref names will remain
    QMap <QString, QStringList> groups;
    QStringList remaining = all_names;
    if (!selected_name.isEmpty()) {
        groupRef(selected_name, groups);
        remaining.removeOne(selected_name);
    } else if (all_names.size() == 1) {
        const QString &name = all_names.first();
        groupRef(name, groups);
        remaining.removeOne(name);
    } else {
        revision_variables.insert("ALL_NAMES", all_names);
        InputDialog dlg("%listbox:_refs=$ALL_NAMES%", revision_variables,
                        "Delete references - QGit", this);
        QListView *w = dynamic_cast<QListView*>(dlg.widget("_refs"));
        w->setSelectionMode(QAbstractItemView::ExtendedSelection);
        if (dlg.exec() != QDialog::Accepted) return;

        QModelIndexList selected = w->selectionModel()->selectedIndexes();
        for (QModelIndexList::const_iterator it=selected.begin(), end=selected.end(); it!=end; ++it) {
            const QString &name = it->data().toString();
            groupRef(name, groups);
            remaining.removeOne(name);
        }
    }
    if (groups.empty()) return;

    // check whether all refs will be removed
    const QString sha = revision_variables.value("SHA").toString();
    const QStringList &children = git->getChildren(sha);
    if ((children.count() == 0 || (children.count() == 1 && children.front() == ZERO_SHA)) && // no children
        remaining.count() == 0 && // all refs will be removed
        QMessageBox::warning(this, "remove references",
                             "Do you really want to remove all\nremaining references to this branch?",
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        == QMessageBox::No)
        return;

    // group selected names by origin
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    bool ok = true;
    for (RefGroupMap::const_iterator g = groups.begin(), gend = groups.end(); g != gend; ++g) {
        QString cmd;
        if (g.key() == "") // local branches
            cmd = "git branch -D " + g.value().join(" ");
        else if (g.key() == "tags/") // tags
            cmd = "git tag -d " + g.value().join(" ");
        else // remote branches
            cmd = "git push -q " + g.key() + " :" + g.value().join(" :");
        ok &= git->run(cmd);
    }
    refreshRepo(true);
    QApplication::restoreOverrideCursor();
    if (!ok) statusBar()->showMessage("Failed, to remove some refs.");
}

void MainImpl::on_actPush_triggered(bool) {

    QStringList selectedItems;
    rv->tab()->listViewLog->getSelectedItems(selectedItems);
    for (int i = 0; i < selectedItems.count(); i++) {
        if (!git->checkRef(selectedItems[i], Git::UN_APPLIED)) {
            statusBar()->showMessage("Please, select only unapplied patches");
            return;
        }
    }
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    bool ok = true;
    for (int i = 0; i < selectedItems.count(); i++) {
        const QString tmp(QString("Pushing patch %1 of %2")
                          .arg(i+1).arg(selectedItems.count()));
        statusBar()->showMessage(tmp);
        const QString& sha = selectedItems[selectedItems.count() - i - 1];
        if (!git->stgPush(sha)) {
            statusBar()->showMessage("Failed to push patch " + sha);
            ok = false;
            break;
        }
    }
    if (ok)
        statusBar()->clearMessage();

    QApplication::restoreOverrideCursor();
    refreshRepo(false);
}

void MainImpl::on_actPop_triggered(bool) {

    QStringList selectedItems;
    rv->tab()->listViewLog->getSelectedItems(selectedItems);
    if (selectedItems.count() > 1) {
        statusBar()->showMessage("Please, select one revision only");
        return;
    }
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    git->stgPop(selectedItems[0]);
    QApplication::restoreOverrideCursor();
    refreshRepo(false);
}

void MainImpl::on_actFilterTree_triggered(bool b) {

    if (!actFilterTree->isEnabled()) {
        //dbs("ASSERT ActFilterTree_toggled while disabled");
        return;
    }
    if (b) {
        QStringList selectedItems;
        if (!treeView->isVisible())
            treeView->updateTree(); // force tree updating

        treeView->getTreeSelectedItems(selectedItems);
        if (selectedItems.count() == 0) {
            log_warn << "Tree filter action activated with no selected items";
            return;
        }
        statusBar()->showMessage("Filter view on " + selectedItems.join(" "));
        setRepository(curDir, true, true, &selectedItems);
    } else
        refreshRepo(true);
}

void MainImpl::on_actFindNext_triggered(bool) {

    QTextEdit* te = getCurrentTextEdit();
    if (!te || textToFind.isEmpty())
        return;

    bool endOfDocument = false;
    while (true) {
        if (te->find(textToFind))
            return;

        if (endOfDocument) {
            QMessageBox::warning(this, "Find text - QGit", "Text \"" +
                         textToFind + "\" not found!", QMessageBox::Ok, 0);
            return;
        }
        if (QMessageBox::question(this, "Find text - QGit", "End of document "
            "reached\n\nDo you want to continue from beginning?", QMessageBox::Yes,
            QMessageBox::No | QMessageBox::Escape) == QMessageBox::No)
            return;

        endOfDocument = true;
        te->moveCursor(QTextCursor::Start);
    }
}

void MainImpl::on_actFind_triggered(bool) {

    QTextEdit* te = getCurrentTextEdit();
    if (!te)
        return;

    QString def(textToFind);
    if (te->textCursor().hasSelection())
        def = te->textCursor().selectedText().section('\n', 0, 0);
    else
        te->moveCursor(QTextCursor::Start);

    bool ok;
    QString str(QInputDialog::getText(this, "Find text - QGit", "Text to find:",
                                      QLineEdit::Normal, def, &ok));
    if (!ok || str.isEmpty())
        return;

    // Highlight all occurrences.
    const QTextCursor origCursor = te->textCursor();
    te->moveCursor(QTextCursor::Start);

    QList<QTextEdit::ExtraSelection> extras;
    while(te->find(str)) {
        QTextEdit::ExtraSelection extra;
        extra.format.setBackground(Qt::yellow);
        extra.cursor = te->textCursor();
        extras.append(extra);
    }
    te->setExtraSelections(extras);

    te->setTextCursor(origCursor);

    // Do the normal find().
    textToFind = str; // update with valid data only
    on_actFindNext_triggered(false);
}

void MainImpl::on_actHelp_triggered(bool) {

    QDialog* dlg = new QDialog();
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    Ui::HelpBase ui;
    ui.setupUi(dlg);
    ui.textEditHelp->setHtml(QString::fromLatin1(helpInfo)); // defined in help.h
    chk_connect_a(this, SIGNAL(closeAllWindows()), dlg, SLOT(close()));
    dlg->show();
    dlg->raise();
}

void MainImpl::on_actMarkDiffToSha_triggered(bool)
{
    ListView* lv = rv->tab()->listViewLog;
    lv->markDiffToSha(lineSHA->text());
}

void MainImpl::on_actAbout_triggered(bool) {

    static const char* aboutMsg =
    "<p><b>QGit version " VERSION_PROJECT " (gitrev:&nbsp;" GIT_REVISION ") </b></p>"
    "<p>Copyright (c) 2005-2008 Marco Costalba<br>"
	"Copyright (c) 2011-2025 <a href='mailto:tibirna@kde.org'>Cristian Tibirna</a></p>"
    "<p>Use and redistribute under the terms of the<br>"
    "<a href=\"http://www.gnu.org/licenses/old-licenses/gpl-2.0.html\">GNU General Public License Version 2</a></p>"
    "<p>Contributors:<br>"
    "Copyright (c) "
    "<nobr>2007 Andy Parkins,</nobr> "
    "<nobr>2007 Pavel Roskin,</nobr> "
    "<nobr>2007 Peter Oberndorfer,</nobr> "
    "<nobr>2007 Yaacov Akiba,</nobr> "
    "<nobr>2007 James McKaskill,</nobr> "
    "<nobr>2008 Jan Hudec,</nobr> "
    "<nobr>2008 Paul Gideon Dann,</nobr> "
    "<nobr>2008 Oliver Bock,</nobr> "
    "<nobr>2010 <a href='mailto:cyp561@gmail.com'>Cyp</a>,</nobr> "
    "<nobr>2011 <a href='dagenaisj@sonatest.com'>Jean-Fran&ccedil;ois Dagenais</a>,</nobr> "
    "<nobr>2011 <a href='mailto:pavtih@gmail.com'>Pavel Tikhomirov</a>,</nobr> "
    "<nobr>2011 <a href='mailto:tim@klingt.org'>Tim Blechmann</a>,</nobr> "
    "<nobr>2014 <a href='mailto:codestruct@posteo.org'>Gregor Mi</a>,</nobr> "
    "<nobr>2014 <a href='mailto:sbytnn@gmail.com'>Sbytov N.N</a>,</nobr> "
    "<nobr>2015 <a href='mailto:dendy.ua@gmail.com'>Daniel Levin</a>,</nobr> "
    "<nobr>2017 <a href='mailto:luigi.toscano@tiscali.it'>Luigi Toscano</a>,</nobr> "
    "<nobr>2016 <a href='mailto:hkarel@yandex.ru'>Pavel Karelin</a>,</nobr> "
    "<nobr>2016 <a href='mailto:zbitter@redhat.com'>Zane Bitter</a>,</nobr> "
    "<nobr>2017 <a href='mailto:wrar@wrar.name'>Andrey Rahmatullin</a>,</nobr> "
    "<nobr>2017 <a href='mailto:alex-github@wenlex.nl'>Alex Hermann</a>,</nobr> "
    "<nobr>2017 <a href='mailto:shalokshalom@protonmail.ch'>Matthias Schuster</a>,</nobr> "
    "<nobr>2017 <a href='mailto:u.joss@calltrade.ch'>Urs Joss</a>,</nobr> "
    "<nobr>2017 <a href='mailto:patrick.m.lacasse@gmail.com'>Patrick Lacasse</a>,</nobr> "
    "<nobr>2018 <a href='mailto:deveee@gmail.com'>Deve</a>,</nobr> "
    "<nobr>2018 <a href='mailto:asturm@gentoo.org'>Andreas Sturmlechner</a>,</nobr> "
    "<nobr>2018 <a href='mailto:kde@davidedmundson.co.uk'>David Edmundson</a>,</nobr> "
    "<nobr>2016-2018 <a href='mailto:rhaschke@techfak.uni-bielefeld.de'>Robert Haschke</a>,</nobr> "
    "<nobr>2018-2024 <a href='mailto:filipe.rinaldi@gmail.com'>Filipe Rinaldi</a>,</nobr> "
    "<nobr>2018 <a href='mailto:balbusm@gmail.com'>Mateusz Balbus</a>,</nobr> "
    "<nobr>2019-2022 <a href='mailto:sebastian@pipping.org'>Sebastian Pipping</a>,</nobr> "
    "<nobr>2019-2020 <a href='mailto:mvf@gmx.eu'>Matthias von Faber</a>,</nobr> "
    "<nobr>2019 <a href='mailto:Kevin@tigcc.ticalc.org'>Kevin Kofler</a>,</nobr> "
    "<nobr>2020 <a href='mailto:cortexspam-github@yahoo.fr'>Matthieu Muffato</a>,</nobr> "
    "<nobr>2020 <a href='mailto:brent@mbari.org'>Brent Roman</a>,</nobr> "
    "<nobr>2020 <a href='mailto:jjm@keelhaul.demon.co.uk'>Jonathan Marten</a>,</nobr> "
    "<nobr>2020 <a href='mailto:yyc1992@gmail.com'>Yichao Yu</a>,</nobr> "
    "<nobr>2021 <a href='mailto:wickedsmoke@users.sourceforge.net'>Karl Robillard</a></nobr> "
    "<nobr>2021 <a href='mailto:vchesn@gmail.com'>Vitaly Chesnokov</a></nobr> "
    "<nobr>2022 <a href='mailto:bits_n_bytes@gmx.de'>Frank Dietrich</a></nobr> "
    "<nobr>2023 <a href='mailto:urban82@gmail.com'>Danilo Treffiletti</a></nobr> "
    "<nobr>2023 <a href='mailto:urban82@gmail.com'>Magnus Holmgren</a></nobr> "
	"<nobr>2025 <a href='mailto:tim@siosm.fr'>Thimoth&eacute;e Ravier</a></nobr> "
    "</p>"
    "<p>This version was compiled against Qt " QT_VERSION_STR "</p>";

    QMessageBox::about(this, "About QGit", QString::fromLatin1(aboutMsg));
}

void MainImpl::closeEvent(QCloseEvent* ce) {

    saveGeometry();

    // lastWindowClosed() signal is emitted by close(), after sending
    // closeEvent(), so we need to close _here_ all secondary windows before
    // the close() method checks for lastWindowClosed flag to avoid missing
    // the signal and stay in the main loop forever, because lastWindowClosed()
    // signal is connected to qApp->quit()
    //
    // note that we cannot rely on setting 'this' parent in secondary windows
    // because when close() is called children are still alive and, finally,
    // when children are deleted, d'tor do not call close() anymore. So we miss
    // lastWindowClosed() signal in this case.
    emit closeAllWindows();
    hide();

    EM_RAISE(exExiting);

    git->stop(Git::optSaveCache);

    if (!git->findChildren<QProcess*>().isEmpty()) {
        // if not all processes have been deleted, there is
        // still some run() call not returned somewhere, it is
        // not safe to delete run() callers objects now
        QTimer::singleShot(100, this, [this]() {on_actClose_triggered(false);});
        ce->ignore();
        return;
    }
    emit closeAllTabs();
    delete rv;
    QWidget::closeEvent(ce);
}

void MainImpl::on_actClose_triggered(bool) {

    //close();
    on_tabWidget_tabCloseRequested(tabWidget->currentIndex());
}

void MainImpl::on_actExit_triggered(bool) {

    qApp->closeAllWindows();
}
