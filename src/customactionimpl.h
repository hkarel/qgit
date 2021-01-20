/*
    Description: custom action handling

    Author: Marco Costalba (C) 2006-2007

    Copyright: See COPYING file that comes with this distribution

*/
#ifndef CUSTOMACTIONIMPL_H
#define CUSTOMACTIONIMPL_H

#include "ui_customaction.h"

class CustomActionImpl : public QDialog, public Ui_CustomAction {
Q_OBJECT
public:
    CustomActionImpl(QWidget* parent);

    void loadGeometry();
    void saveGeometry();

signals:
    void listChanged(const QStringList&);

protected slots:
    virtual void closeEvent(QCloseEvent*);
    void on_lstActionNames_currentItemChanged(QListWidgetItem*, QListWidgetItem*);
    void on_btnNew_clicked();
    void on_btnRename_clicked();
    void on_btnRemove_clicked();
    void on_btnMoveUp_clicked();
    void on_btnMoveDown_clicked();
    void on_txtAction_textChanged();
    void on_chkRefreshAfterAction_toggled(bool);

private:
    void done(int r) override;

    void loadActions();
    void saveActions();
    bool newNameAction(QString& name, const QString& caption);

//    const QStringList actions();
//    void updateActions();
//    bool getNewName(QString& name, const QString& caption);
//    void loadAction(const QString& name);
//    void removeAction(const QString& name);
};

#endif
