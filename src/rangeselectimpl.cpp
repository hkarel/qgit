/*
    Description: start-up dialog

    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#include <QRegularExpression>
#include "common.h"
#include "git.h"
#include "rangeselectimpl.h"
#include "shared/config/appl_conf.h"

using namespace qgit;

RangeSelectImpl::RangeSelectImpl(QWidget* parent, QString* r, bool repoChanged, Git* g)
    : QDialog(parent), git(g), range(r)
{
    setupUi(this);

    QStringList orl, tmp;
    orderRefs(git->getAllRefNames(Git::BRANCH, !Git::optOnlyLoaded), tmp);
    if (!tmp.isEmpty())
        orl << tmp << "";

    orderRefs(git->getAllRefNames(Git::RMT_BRANCH, !Git::optOnlyLoaded), tmp);
    if (!tmp.isEmpty())
        orl << tmp << "";

    orderRefs(git->getAllRefNames(Git::TAG, !Git::optOnlyLoaded), tmp);
    if (!tmp.isEmpty())
        orl << tmp;

    // as default select first tag that is not also the current HEAD
    int defIdx = orl.count() - tmp.count();
    if (!tmp.empty()) {
        const QString& tagSha(git->getRefSha(tmp.first(), Git::TAG, false));
        if (!tagSha.isEmpty() && git->checkRef(tagSha, Git::CUR_BRANCH))
            // in this case set as default tag the next one if any
            defIdx += (tmp.count() > 1 ? 1 : -1);
    }

    if (!orl.isEmpty() && orl.last().isEmpty())
        orl.pop_back();

    QString from, to, options;

    if (!repoChanged) {
        // range values are sensible only when reloading the same repo
        config::base().getValue("range_select.from",    from);
        config::base().getValue("range_select.to",      to);
        config::base().getValue("range_select.options", options);
    }
    cboxTo->insertItem(0, "HEAD");
    cboxTo->insertItems(1, orl);
    int idx = repoChanged ? 0 : cboxTo->findText(to);
    if (idx != -1)
        cboxTo->setCurrentIndex(idx);
    else
        cboxTo->setEditText(to);

    cboxFrom->insertItems(0, orl);
    idx = repoChanged ? defIdx : cboxFrom->findText(from);
    if (idx != -1)
        cboxFrom->setCurrentIndex(idx);
    else
        cboxFrom->setEditText(from);

    cboxFrom->setFocus();

    lineOptions->setText(options);

    qgit::FlagType f = qgit::flags();
    chkDiffCache->setChecked(f & DIFF_INDEX_F);
    chkShowAll->setChecked(f & ALL_BRANCHES_F);
    chkShowWholeHistory->setChecked(f & WHOLE_HISTORY_F);
    chkShowDialog->setChecked(f & RANGE_SELECT_F);
}

void RangeSelectImpl::orderRefs(const QStringList& src, QStringList& dst) {
// we use an heuristic to list release candidates before corresponding
// releases as example v.2.6.18-rc4 before v.2.6.18

    // match a (dotted) number + something else + a number + EOL
    static const QRegularExpression re {R"([\d\.]+([^\d\.]+\d+$))"};

    // in ASCII the space ' ' (32) comes before '!' (33) and both
    // before the rest, we need this to correctly order a sequence like
    //
    //    [v1.5, v1.5-rc1, v1.5.1] --> [v1.5.1, v1.5, v1.5-rc1]

    static const QString rcMark   {" $$%%"}; // an impossible to find string starting with a space
    static const QString noRcMark {"!$$%%"}; // an impossible to find string starting with a '!'

    typedef QMap<QString, QString> OrderedMap;
    OrderedMap map;

    for (const QString& s : src) {

        QString tmpStr = s;

        // if (re.indexIn(tmpStr) != -1)
        //     tmpStr.insert(re.pos(1), rcMark);
        // else
        //     tmpStr += noRcMark;

        QRegularExpressionMatch match;
        if (tmpStr.indexOf(re, 0, &match) != -1)
            tmpStr.insert(match.capturedStart(1), rcMark);
        else
            tmpStr += noRcMark;

        // Normalize all numbers to 3 digits with leading zeros, so one-digit
        // version numbers are always smaller than two-digit version numbers
        //     [v1.10.3, v1.5.1, v1.7.2] --> [v.1.10.3, v1.7.2, v1.5.1]
        // QMap automatically sorts by keys, so we only have to iterate over it
        // and return the original strings (stored as the data() in the map)
        static const QRegularExpression verRE {R"(([^\d])(\d{1,2})(?=[^\d]))"};
        while (tmpStr.contains(verRE))
            tmpStr.replace(verRE, R"(\10\2)");

        map[tmpStr] = s;
    }
    dst.clear();
    FOREACH(it, map)
        dst.prepend(it.value());
}

void RangeSelectImpl::on_chkDiffCache_toggled(bool b) {

    qgit::flags().set(DIFF_INDEX_F, b);
}

void RangeSelectImpl::on_chkShowDialog_toggled(bool b) {

    qgit::flags().set(RANGE_SELECT_F, b);
}

void RangeSelectImpl::on_chkShowAll_toggled(bool b) {

    QString opt(lineOptions->text());
    opt.remove("--all");
    if (b)
        opt.append(" --all");

    lineOptions->setText(opt.trimmed());
    qgit::flags().set(ALL_BRANCHES_F, b);
}

void RangeSelectImpl::on_chkShowWholeHistory_toggled(bool b) {

    cboxFrom->setEnabled(!b);
    cboxTo->setEnabled(!b);
    qgit::flags().set(WHOLE_HISTORY_F, b);
}

void RangeSelectImpl::done(int r) {

    QDialog::done(r);

    if (r != QDialog::Accepted) {
        config::base().rereadFile();
        qgit::flags().load();
        return;
    }

    if (qgit::flags().test(WHOLE_HISTORY_F)) {
        *range = "HEAD";
    }
    else {
        *range = cboxFrom->currentText();
        if (!range->isEmpty())
            range->append("..");

        range->append(cboxTo->currentText());
    }
    // all stuff after "--" should go after range
    if (lineOptions->text().contains("--")) {
        QString tmp {lineOptions->text()};
        tmp.insert(tmp.indexOf("--"), *range + " ");
        *range = tmp;
    }
    else
        range->prepend(lineOptions->text() + " ");

    *range = range->trimmed();

    config::base().setValue("range_select.from",    cboxFrom->currentText());
    config::base().setValue("range_select.to",      cboxTo->currentText());
    config::base().setValue("range_select.options", lineOptions->text());

    qgit::flags().save(); // Call config::base().save() inside
}

QString RangeSelectImpl::getDefaultArgs() {
    QString options;
    config::base().getValue("range_select.options", options);
    return options;
}
