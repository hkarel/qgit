/*
    Description: custom action handling

    Author: Marco Costalba (C) 2006-2007

    Copyright: See COPYING file that comes with this distribution

*/

#include "common.h"
#include "customactionimpl.h"

#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

#include <QMessageBox>
#include <QInputDialog>

using namespace qgit;

CustomActionImpl::CustomActionImpl(QWidget* parent) : QDialog(parent)
{
    setupUi(this);
    //setAttribute(Qt::WA_DeleteOnClose);

    loadGeometry();
    QTimer::singleShot(10, this, &CustomActionImpl::loadActions);
}

void CustomActionImpl::loadGeometry()
{
    QVector<int> v;
    config::base().getValue("geometry.custact.window", v);

    if (v.count() == 4) {
        move(v[0], v[1]);
        resize(v[2], v[3]);
    }
}

void CustomActionImpl::saveGeometry()
{
    QPoint p = pos();
    QVector<int> v {p.x(), p.y(), width(), height()};
    config::base().setValue("geometry.custact.window", v);
}

void CustomActionImpl::closeEvent(QCloseEvent* ce)
{
    QWidget::closeEvent(ce);
}

void CustomActionImpl::done(int r)
{
    QDialog::done(r);

    if (r == QDialog::Accepted) {
        saveActions();
        config::base().saveFile();
    }
    else
        config::base().rereadFile();

    saveGeometry();
}

void CustomActionImpl::loadActions()
{
    YamlConfig::Func loadFunc =
        [this](YamlConfig* conf, YAML::Node& actions, bool /*logWarn*/)
    {
        for (size_t i = 0; i < actions.size(); ++i)
        {
            CustomActionData::Ptr cad {new CustomActionData};
            conf->getValue(actions[i], "name",    cad->name);
            conf->getValue(actions[i], "command", cad->command);
            conf->getValue(actions[i], "refresh", cad->refresh);

            QListWidgetItem* item = new QListWidgetItem(cad->name);
            item->setData(Qt::UserRole, QVariant::fromValue(cad));
            lstActionNames->addItem(item);
        }
        return true;
    };
    config::base().getValue("custom_actions", loadFunc);
}

void CustomActionImpl::saveActions()
{
    YamlConfig::Func saveFunc =
        [this](YamlConfig* conf, YAML::Node& actions, bool /*logWarn*/)
    {
        actions = YAML::Node();
        for (int i = 0; i < lstActionNames->count(); ++i)
        {
            QListWidgetItem* item = lstActionNames->item(i);
            QVariant var = item->data(Qt::UserRole);
            CustomActionData::Ptr cad = var.value<CustomActionData::Ptr>();

            YAML::Node node;
            conf->setValue(node, "name",    cad->name);
            conf->setValue(node, "command", cad->command);
            conf->setValue(node, "refresh", cad->refresh);
            actions.push_back(node);
        }
        return true;
    };
    config::base().setValue("custom_actions", saveFunc);
}

void CustomActionImpl::on_lstActionNames_currentItemChanged(QListWidgetItem* item,
                                                            QListWidgetItem*)
{
    if (item) {
        QVariant var = item->data(Qt::UserRole);
        CustomActionData::Ptr cad = var.value<CustomActionData::Ptr>();

        chkRefreshAfterAction->setChecked(cad->refresh);

        QSignalBlocker blocker {txtAction}; (void) blocker;
        txtAction->setText(cad->command);
    }
    else {
        chkRefreshAfterAction->setChecked(false);

        QSignalBlocker blocker {txtAction}; (void) blocker;
        txtAction->clear();
    }

//    bool empty = (item == nullptr);
//    txtAction->setEnabled(!empty);
//    chkRefreshAfterAction->setEnabled(!empty);
//    _ui->btnRename->setEnabled(!empty);
//    _ui->btnRemove->setEnabled(!empty);
//    _ui->btnMoveUp->setEnabled(!empty && (item != lstActionNames->item(0)));
//    int lastRow = lstActionNames->count() - 1;
//    _ui->btnMoveDown->setEnabled(!empty && (item != lstActionNames->item(lastRow)));
}

bool CustomActionImpl::newNameAction(QString& name, const QString& caption)
{
    bool ok;
    QString oldName = name;
    name = QInputDialog::getText(this, caption + " - QGit", "Enter action name:",
                                 QLineEdit::Normal, name, &ok);

    if (!ok || name.isEmpty() || name == oldName)
        return false;

    for (int i = 0; i < lstActionNames->count(); ++i) {
        QListWidgetItem* item = lstActionNames->item(i);
        if (item->text() == name) {
            QMessageBox::warning(this, caption + " - QGit",
                                 "Sorry, action name already exists.\n"
                                 "Please choose a different name.");
            return false;
        }
    }
    return true;
}

void CustomActionImpl::on_btnNew_clicked()
{
    QString name;
    if (!newNameAction(name, "Create new action"))
        return;

    CustomActionData::Ptr cad {new CustomActionData};
    cad->name = name;

    QListWidgetItem* item = new QListWidgetItem(cad->name);
    item->setData(Qt::UserRole, QVariant::fromValue(cad));
    lstActionNames->addItem(item);
    lstActionNames->setCurrentItem(item);

    QSignalBlocker blocker {txtAction}; (void) blocker;
    txtAction->setPlainText("<write here your action's commands sequence>");
    txtAction->selectAll();
    txtAction->setFocus();
}

void CustomActionImpl::on_btnRename_clicked()
{
    QListWidgetItem* item = lstActionNames->currentItem();
    if (!item)
        return;

    QString newName = item->text();
    if (!newNameAction(newName, "Rename action"))
        return;

    QVariant var = item->data(Qt::UserRole);
    CustomActionData::Ptr cad = var.value<CustomActionData::Ptr>();
    cad->name = newName;
    item->setText(cad->name);
}

void CustomActionImpl::on_btnRemove_clicked()
{
    QListWidgetItem* item = lstActionNames->currentItem();
    if (!item)
        return;

    delete item;
    if (!lstActionNames->count())
        on_lstActionNames_currentItemChanged(NULL, NULL);
}

void CustomActionImpl::on_btnMoveUp_clicked()
{
    int row = lstActionNames->currentRow();
    if (row <= 0)
        return;

    QListWidgetItem* item = lstActionNames->takeItem(row);
    lstActionNames->insertItem(row - 1, item);
    lstActionNames->setCurrentRow(row - 1);
}

void CustomActionImpl::on_btnMoveDown_clicked()
{
    int row = lstActionNames->currentRow();
    if ((row == -1) || (row == (lstActionNames->count() - 1)))
        return;

    QListWidgetItem* item = lstActionNames->takeItem(row);
    lstActionNames->insertItem(row + 1, item);
    lstActionNames->setCurrentRow(row + 1);
}

void CustomActionImpl::on_txtAction_textChanged()
{
    if (QListWidgetItem* item = lstActionNames->currentItem()) {
        QVariant var = item->data(Qt::UserRole);
        CustomActionData::Ptr cad = var.value<CustomActionData::Ptr>();
        cad->command = txtAction->toPlainText();
    }
}

void CustomActionImpl::on_chkRefreshAfterAction_toggled(bool b)
{
    if (QListWidgetItem* item = lstActionNames->currentItem()) {
        QVariant var = item->data(Qt::UserRole);
        CustomActionData::Ptr cad = var.value<CustomActionData::Ptr>();
        cad->refresh = chkRefreshAfterAction->isChecked();
    }
}
