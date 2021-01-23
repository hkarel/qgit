/*
    Description: changes commit dialog

    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution
*/

#include "commitimpl.h"
#include "git.h"
#include "settingsimpl.h"
#include "exceptionmanager.h"
#include "spellcheck/spellcheck.h"

#include "shared/defmac.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

#include <QTextCodec>
#include <QMenu>
#include <QRegExp>
#include <QDir>
#include <QMessageBox>
#include <QInputDialog>
#include <QToolTip>
#include <QScrollBar>
#include <QKeyEvent>

using namespace qgit;

QString CommitImpl::lastMsgBeforeError;

CommitImpl::CommitImpl(Git* g, bool amend) : git(g) {

    // adjust GUI
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    textMsg->setFont(TYPE_WRITER_FONT);

    QString commitTmpl;
    config::base().getValue("commit.template_file_path", commitTmpl);

    QString msg;
    QDir d;
    if (d.exists(commitTmpl))
        readFromFile(commitTmpl, msg);

    // set-up files list
    const RevFile* f = git->getFiles(ZERO_SHA);
    for (int i = 0; f && i < f->count(); ++i) { // in case of amend f could be null

        bool inIndex = f->statusCmp(i, RevFile::IN_INDEX);
        bool isNew = (f->statusCmp(i, RevFile::NEW) || f->statusCmp(i, RevFile::UNKNOWN));
        QColor myColor = QPalette().color(QPalette::WindowText);
        if (isNew)
            myColor = Qt::darkGreen;
        else if (f->statusCmp(i, RevFile::DELETED))
            myColor = Qt::red;

        QTreeWidgetItem* item = new QTreeWidgetItem(treeFiles);
        item->setText(0, git->filePath(*f, i));
        item->setText(1, inIndex ? "Updated in index" : "Not updated in index");
        item->setCheckState(0, inIndex || !isNew ? Qt::Checked : Qt::Unchecked);
        item->setForeground(0, myColor);
    }
    treeFiles->resizeColumnToContents(0);

    // compute cursor offsets. Take advantage of fixed width font
    textMsg->setPlainText("\nx\nx"); // cursor doesn't move on empty text
    textMsg->moveCursor(QTextCursor::Start);
    textMsg->verticalScrollBar()->setValue(0);
    textMsg->horizontalScrollBar()->setValue(0);
    int y0 = textMsg->cursorRect().y();
    int x0 = textMsg->cursorRect().x();
    textMsg->moveCursor(QTextCursor::Down);
    textMsg->moveCursor(QTextCursor::Right);
    textMsg->verticalScrollBar()->setValue(0);
    int y1 = textMsg->cursorRect().y();
    int x1 = textMsg->cursorRect().x();
    ofsX = x1 - x0;
    ofsY = y1 - y0;
    textMsg->moveCursor(QTextCursor::Start);
    textMsg_cursorPositionChanged();

    if (lastMsgBeforeError.isEmpty()) {
        // setup textMsg with old commit message to be amended
        QString status;
        if (amend)
            status = git->getLastCommitMsg();

        // setup textMsg with default value if user opted to do so (default)
        if (qgit::flags().test(USE_CMT_MSG_F))
            status += git->getNewCommitMsg();

        msg = status.trimmed();
        if (!amend)
            msg.prepend("\n\n"); // two first lines is empty
    } else
        msg = lastMsgBeforeError;

    if (qgit::flags().test(SPELL_CHECK_F))
    {
        bool init = true;
        if (spellCheck().initialized() != 1)
            init = spellCheck().init();

        if (init)
            spellHighlight = new SpellHighlighter(textMsg);
    }

    textMsg->setPlainText(msg);
    textMsg->setFocus();

    // if message is not changed we avoid calling refresh
    // to change patch name in stgCommit()
    origMsg = msg;

    // setup button functions
    if (amend) {
        if (git->isStGITStack()) {
            btnCommit->setText("&Add to top");
            btnCommit->setShortcut(QKeySequence("Alt+A"));
            btnCommit->setToolTip("Refresh top stack patch");
        } else {
            btnCommit->setText("&Amend");
            btnCommit->setShortcut(QKeySequence("Alt+A"));
            btnCommit->setToolTip("Amend latest commit");
        }
        chk_connect_a(btnCommit, SIGNAL(clicked()),
                      this, SLOT(btnAmend_clicked()));
    } else {
        if (git->isStGITStack()) {
            btnCommit->setText("&New patch");
            btnCommit->setShortcut(QKeySequence("Alt+N"));
            btnCommit->setToolTip("Create a new patch");
        }
        chk_connect_a(btnCommit, SIGNAL(clicked()),
                      this, SLOT(btnCommit_clicked()));
    }
    chk_connect_a(treeFiles, SIGNAL(customContextMenuRequested(const QPoint&)),
                  this, SLOT(contextMenuPopup(const QPoint&)));
    chk_connect_a(textMsg, SIGNAL(cursorPositionChanged()),
                  this, SLOT(textMsg_cursorPositionChanged()));

    textMsg->installEventFilter(this);
    loadGeometry();
}

void CommitImpl::loadGeometry()
{
    QVector<int> v;
    config::base().getValue("geometry.commit.window", v);

    if (v.count() == 4) {
        move(v[0], v[1]);
        resize(v[2], v[3]);
    }

    QString sval;
    if (config::base().getValue("geometry.commit.splitter", sval))
    {
        QByteArray ba = QByteArray::fromBase64(sval.toLatin1());
        splitter->restoreState(ba);
    }
}

void CommitImpl::saveGeometry()
{
    QPoint p = pos();
    QVector<int> v {p.x(), p.y(), width(), height()};
    config::base().setValue("geometry.commit.window", v);

    QByteArray ba = splitter->saveState().toBase64();
    config::base().setValue("geometry.commit.splitter", QString::fromLatin1(ba));
}

void CommitImpl::closeEvent(QCloseEvent*) {

    saveGeometry();
}

void CommitImpl::contextMenuPopup(const QPoint& pos)  {

    QMenu* contextMenu = new QMenu(this);
    QAction* a = contextMenu->addAction("Select All");
    chk_connect_a(a, SIGNAL(triggered()), this, SLOT(checkAll()));
    a = contextMenu->addAction("Unselect All");
    chk_connect_a(a, SIGNAL(triggered()), this, SLOT(unCheckAll()));
    contextMenu->popup(mapToGlobal(pos));
}

void CommitImpl::checkAll() { checkUncheck(true); }
void CommitImpl::unCheckAll() { checkUncheck(false); }

void CommitImpl::checkUncheck(bool checkAll) {

    QTreeWidgetItemIterator it {treeFiles};
    while (*it) {
        (*it)->setCheckState(0, checkAll ? Qt::Checked : Qt::Unchecked);
        ++it;
    }
}

bool CommitImpl::getFiles(QStringList& selFiles) {

    // check for files to commit
    selFiles.clear();
    QTreeWidgetItemIterator it {treeFiles};
    while (*it) {
        if ((*it)->checkState(0) == Qt::Checked)
            selFiles.append((*it)->text(0));
        ++it;
    }

    return !selFiles.isEmpty();
}

void CommitImpl::warnNoFiles() {

    QMessageBox::warning(this, "Commit changes - QGit",
                 "Sorry, no files are selected for updating.",
                 QMessageBox::Ok, QMessageBox::NoButton);
}

bool CommitImpl::checkFiles(QStringList& selFiles) {

    if (getFiles(selFiles))
        return true;

    warnNoFiles();
    return false;
}

bool CommitImpl::checkMsg(QString& msg) {

    static const QRegExp re1 {R"((^|\n)\s*#[^\n]*)"};
    static const QRegExp re2 {R"([ \t\r\f\v]+\n)"};

    msg = textMsg->toPlainText();
    msg.remove(re1);         // strip comments
    msg.replace(re2, "\n");  // strip line trailing cruft
    msg = msg.trimmed();
    if (msg.isEmpty()) {
        QMessageBox::warning(this, "Commit changes - QGit",
                             "Sorry, I don't want an empty message.",
                             QMessageBox::Ok, QMessageBox::NoButton);
        return false;
    }
    // split subject from message body
    QString subj = msg.section('\n', 0, 0, QString::SectionIncludeTrailingSep);
    QString body = msg.section('\n', 1).trimmed();
    msg = subj + '\n' + body + '\n';
    return true;
}

bool CommitImpl::checkPatchName(QString& patchName) {

    bool ok;
    patchName = patchName.simplified();
    patchName.replace(' ', "_");
    patchName = QInputDialog::getText(this, "Create new patch - QGit", "Enter patch name:",
                                      QLineEdit::Normal, patchName, &ok);
    if (!ok || patchName.isEmpty())
        return false;

    QString tmp(patchName.trimmed());
    if (patchName != tmp.remove(' '))
        QMessageBox::warning(this, "Create new patch - QGit", "Sorry, control "
                             "characters or spaces\n are not allowed in patch name.");

    else if (git->isPatchName(patchName))
        QMessageBox::warning(this, "Create new patch - QGit", "Sorry, patch name "
                             "already exists.\nPlease choose a different name.");
    else
        return true;

    return false;
}

bool CommitImpl::checkConfirm(const QString& msg, const QString& patchName,
                              const QStringList& selFiles, bool amend) {

//    QTextCodec* tc = QTextCodec::codecForCStrings();
//    QTextCodec::setCodecForCStrings(0); // set temporary Latin-1

    // NOTEME: i18n-ugly
    QString whatToDo = amend ?
        (git->isStGITStack() ? "refresh top patch with" :
                        "amend last commit with") :
        (git->isStGITStack() ? "create a new patch with" : "commit");

        QString text("Do you want to " + whatToDo);

        bool const fullList = selFiles.size() < 20;
        if (fullList)
            text.append(" the following file(s)?\n\n" + selFiles.join("\n") +
                        "\n\nwith the message:\n\n");
        else
            text.append(" those " + QString::number(selFiles.size()) +
                        " files the with the message:\n\n");

    text.append(msg);
    if (git->isStGITStack())
        text.append("\n\nAnd patch name: " + patchName);

//    QTextCodec::setCodecForCStrings(tc);

        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Commit changes - QGit");
        msgBox.setText(text);
        if (!fullList)
            msgBox.setDetailedText(selFiles.join("\n"));

        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);

        return msgBox.exec() != QMessageBox::No;
}

void CommitImpl::on_btnSettings_clicked() {

    SettingsImpl setView(this, git, 3);
    setView.exec();
}

void CommitImpl::on_btnCancel_clicked() {

    close();
}

void CommitImpl::btnCommit_clicked() {

    QStringList selFiles; // retrieve selected files
    if (!checkFiles(selFiles))
        return;

    QString msg; // check for commit message and strip comments
    if (!checkMsg(msg))
        return;

    QString patchName = msg.section('\n', 0, 0); // the subject
    if (git->isStGITStack() && !checkPatchName(patchName))
        return;

    // ask for confirmation
    if (qgit::flags().test(COMMIT_CONFIRM_F)) {
        if (!checkConfirm(msg, patchName, selFiles, !Git::optAmend))
            return;
    }

    // ok, let's go
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    EM_PROCESS_EVENTS; // to close message box
    bool ok;
    if (git->isStGITStack())
        ok = git->stgCommit(selFiles, msg, patchName, !Git::optFold);
    else
        ok = git->commitFiles(selFiles, msg, !Git::optAmend);

    lastMsgBeforeError = (ok ? "" : msg);
    QApplication::restoreOverrideCursor();
    hide();
    emit changesCommitted(ok);
    close();
}

void CommitImpl::btnAmend_clicked() {

    QStringList selFiles; // retrieve selected files
    getFiles(selFiles);
    // FIXME: If there are no files AND no changes to message, we should not
    // commit. Disabling the commit button in such case might be preferable.

    QString msg = textMsg->toPlainText();
    if (msg == origMsg && selFiles.isEmpty()) {
        warnNoFiles();
        return;
    }

    if (msg == origMsg && git->isStGITStack()) {
        msg = "";
    }
    else if (!checkMsg(msg)) {
        // We are going to replace the message, so it better isn't empty
        return;
    }

    // ask for confirmation
    if (qgit::flags().test(COMMIT_CONFIRM_F)) {
        // FIXME: We don't need patch name for refresh, do we?
        if (!checkConfirm(msg, "", selFiles, Git::optAmend))
            return;
    }

    // ok, let's go
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    EM_PROCESS_EVENTS; // to close message box
    bool ok;
    if (git->isStGITStack())
        ok = git->stgCommit(selFiles, msg, "", Git::optFold);
    else
        ok = git->commitFiles(selFiles, msg, Git::optAmend);

    QApplication::restoreOverrideCursor();
    hide();
    emit changesCommitted(ok);
    close();
}

void CommitImpl::on_btnUpdateCache_clicked() {

    QStringList selFiles;
    if (!checkFiles(selFiles))
        return;

    bool ok = git->updateIndex(selFiles);

    QApplication::restoreOverrideCursor();
    emit changesCommitted(ok);
    close();
}

void CommitImpl::textMsg_cursorPositionChanged() {

    int column, line;
    computePosition(column, line);
    QString lineNumber = QString("Line: %1 Col: %2")
                                 .arg(line + 1).arg(column + 1);
    labelLineCol->setText(lineNumber);
}

void CommitImpl::computePosition(int& columns, int& line) {

    QRect r = textMsg->cursorRect();
    int vs = textMsg->verticalScrollBar()->value();
    int hs = textMsg->horizontalScrollBar()->value();

    // when in start position r.x() = -r.width() / 2
    columns = (ofsX) ? ((r.x() + hs + r.width() / 2) / ofsX) : 0;
    line = (ofsY) ? ((r.y() + vs) / ofsY) : 0;
}

bool CommitImpl::eventFilter(QObject* obj, QEvent* event) {

    if (obj == textMsg) {
        if (event->type() == QEvent::KeyPress) {
             QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
             if (( keyEvent->key() == Qt::Key_Return
                   || keyEvent->key() == Qt::Key_Enter
                 )
                 && keyEvent->modifiers() & Qt::ControlModifier) {

                QMetaObject::invokeMethod(btnCommit, "clicked", Qt::QueuedConnection);
                return true;
             }
         }
         return false;
    }
    return QObject::eventFilter(obj, event);
}
