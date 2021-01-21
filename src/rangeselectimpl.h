/*
    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#ifndef RANGESELECTIMPL_H
#define RANGESELECTIMPL_H

#include "ui_rangeselect.h"

class Git;

class RangeSelectImpl: public QDialog, public Ui_RangeSelect {
Q_OBJECT
public:
    RangeSelectImpl(QWidget* parent, QString* range, bool rc, Git* g);
    static QString getDefaultArgs();

public slots:
    void on_chkDiffCache_toggled(bool b);
    void on_chkShowAll_toggled(bool b);
    void on_chkShowDialog_toggled(bool b);
    void on_chkShowWholeHistory_toggled(bool b);

private:
    void done(int r) override;
    void orderRefs(const QStringList& src, QStringList& dst);

    Git* git;
    QString* range;
};

#endif
