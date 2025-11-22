/*
    Description: stdout viewer

    Author: Marco Costalba (C) 2006-2007

    Copyright: See COPYING file that comes with this distribution

*/
#include <QStatusBar>
#include <QMessageBox>
#include "myprocess.h"
#include "git.h"
#include "consoleimpl.h"

#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

ConsoleImpl::ConsoleImpl(qgit::CustomActionData::Ptr actionData, Git* g) :
    git(g),
    actionData(actionData)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(this);
    textEditOutput->setFont(qgit::TYPE_WRITER_FONT);
    QFont f = textLabelCmd->font();
    f.setBold(true);
    textLabelCmd->setFont(f);

    if (actionData) {
        QString msg = "'%1' output window - QGit";
        setWindowTitle(msg.arg(actionData->name));
    }
    loadGeometry();
}

void ConsoleImpl::loadGeometry()
{
    QVector<int> v;
    config::base().getValue("geometry.console.window", v);

    if (v.count() == 4) {
        move(v[0], v[1]);
        resize(v[2], v[3]);
    }
}

void ConsoleImpl::saveGeometry()
{
    QPoint p = pos();
    QVector<int> v {p.x(), p.y(), width(), height()};
    config::base().setValue("geometry.console.window", v);
}

void ConsoleImpl::typeWriterFontChanged() {

    QTextEdit* te = textEditOutput;
    te->setFont(qgit::TYPE_WRITER_FONT);
    te->setPlainText(te->toPlainText());
    te->moveCursor(QTextCursor::End);
}

void ConsoleImpl::on_pushButtonOk_clicked(bool) {

    close();
}

void ConsoleImpl::on_pushButtonTerminate_clicked(bool) {

    git->cancelProcess(proc);
    procFinished();
}

void ConsoleImpl::closeEvent(QCloseEvent* ce) {

	if (proc && proc->state() == QProcess::Running) {
		QMessageBox q(QMessageBox::Question,
			"Action output window - QGit",
			"Action is still running.\nAre you sure you want to close "
			"the window and leave the action running in background?",
			QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
			this
		);
		if (q.exec() == QMessageBox::StandardButton::No) {
            ce->ignore();
            return;
        }
	}
    if (QApplication::overrideCursor())
        QApplication::restoreOverrideCursor();

    saveGeometry();
    QMainWindow::closeEvent(ce);
}

bool ConsoleImpl::start(const QString& cmd) {

    if (actionData) {
        QString msg = "Executing '%1' action...";
        textLabelEnd->setText(msg.arg(actionData->name));
    }
    textLabelCmd->setText(cmd);
    if (cmd.indexOf('\n') < 0)
        proc = git->runAsync(cmd, this);
    else
        proc = git->runAsScript(cmd, this); // wrap multiline cmd in a script

    if (proc.isNull())
        deleteLater();
    else
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    return !proc.isNull();
}

void ConsoleImpl::procReadyRead(const QByteArray& data) {

    QString newParagraph;
    if (qgit::stripPartialParaghraps(data, &newParagraph, &inpBuf))
        // QTextEdit::append() adds a new paragraph,
        // i.e. inserts a LF if not already present.
        textEditOutput->append(newParagraph);
}

void ConsoleImpl::procFinished() {

    textEditOutput->append(inpBuf);
    inpBuf = "";
    QApplication::restoreOverrideCursor();

    if (actionData) {
           QString msg = "End of '%1' execution";
           textLabelEnd->setText(msg.arg(actionData->name));
    }
    pushButtonTerminate->setEnabled(false);
    emit customAction_exited(actionData);
}
