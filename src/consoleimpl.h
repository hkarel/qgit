/*
    Description: stdout viewer

    Author: Marco Costalba (C) 2006-2007

    Copyright: See COPYING file that comes with this distribution

*/
#ifndef CONSOLEIMPL_H
#define CONSOLEIMPL_H

#include <QCloseEvent>
#include <QPointer>

#include "common.h"
#include "ui_console.h"

class MyProcess;
class Git;

class ConsoleImpl : public QMainWindow, Ui_Console { // we need a statusbar
Q_OBJECT
public:
    ConsoleImpl(qgit::CustomActionData::Ptr actionData, Git* g);
    bool start(const QString &cmd);

    void loadGeometry();
    void saveGeometry();

signals:
    void customAction_exited(qgit::CustomActionData::Ptr);

public slots:
    void typeWriterFontChanged();
    void procReadyRead(const QByteArray& data);
    void procFinished();

protected slots:
    virtual void closeEvent(QCloseEvent* ce);
    void on_pushButtonTerminate_clicked(bool);
    void on_pushButtonOk_clicked(bool);

private:
    Git* git;
    qgit::CustomActionData::Ptr actionData;
    QPointer<MyProcess> proc;
    QString inpBuf;
};

#endif
