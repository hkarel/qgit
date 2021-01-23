/*
    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#ifndef COMMITIMPL_H
#define COMMITIMPL_H

#include "common.h"
#include "ui_commit.h"

#include "spellcheck/highlighter.h"

class Git;


class CommitImpl : public QWidget, public Ui_CommitBase {
Q_OBJECT
public:
    CommitImpl(Git* g, bool amend);

    void loadGeometry();
    void saveGeometry();

signals:
    void changesCommitted(bool);

public slots:
    virtual void closeEvent(QCloseEvent*);
    void btnCommit_clicked();
    void btnAmend_clicked();

    void on_btnCancel_clicked();
    void on_btnUpdateCache_clicked();
    void on_btnSettings_clicked();

private slots:
    void textMsg_cursorPositionChanged();
    void contextMenuPopup(const QPoint&);
    void checkAll();
    void unCheckAll();

private:
    void checkUncheck(bool checkAll);
    bool getFiles(QStringList& selFiles);
    void warnNoFiles();
    bool checkFiles(QStringList& selFiles);
    bool checkMsg(QString& msg);
    bool checkPatchName(QString& patchName);
    bool checkConfirm(const QString& msg, const QString& patchName,
                      const QStringList& selFiles, bool amend);
    void computePosition(int& columns, int& line);
    bool eventFilter(QObject* obj, QEvent* event);

    Git* git;
    QString origMsg;
    int ofsX, ofsY;

    SpellHighlighter* spellHighlight = {nullptr};

    static QString lastMsgBeforeError;
};

#endif
