#pragma once

class TabWidget : public QTabWidget
{
public:
    explicit TabWidget(QWidget *parent = 0) : QTabWidget(parent) {}
    QTabBar* tabBar() const {return QTabWidget::tabBar();}
};
