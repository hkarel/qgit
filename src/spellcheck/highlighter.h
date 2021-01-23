/*
 * highlighter.h
 *
 * Modified: Pavel Karelin 2021 (hkarel), <hkarel@yandex.ru>
 *
 * SPDX-FileCopyrightText: 2004 Zack Rusin <zack@kde.org>
 * SPDX-FileCopyrightText: 2013 Martin Sandsmark <martin.sandsmark@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "shared/defmac.h"

#include <QtCore>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>

class SpellHighlighter : public QSyntaxHighlighter
{
public:
    /**
     * @brief SpellHighlighter
     * @param textEdit
     * @param col define spellchecking color.
     * @since 5.12
     */
    explicit SpellHighlighter(QTextEdit* textEdit, const QColor& col = QColor());
    explicit SpellHighlighter(QPlainTextEdit* textEdit, const QColor& col = QColor());

    ~SpellHighlighter() override;

private slots:
    void _on_actWordAdd_triggered(bool);
    void _on_actWordIgnor_triggered(bool);
    void _on_actWordReplace_triggered(bool);

private:
    struct WordPos
    {
        int start;
        int length;
    };
    typedef QList<WordPos> WordPosList;

    bool eventFilter(QObject*, QEvent*) override;
    void highlightBlock(const QString& text) override;

    void setMisspelled(int start, int count);
    void unsetMisspelled(int start, int count);

    WordPosList wordsBreak(const QString& text) const;
    bool getCurrentWord(QString& word, QString& lang) const;

    bool onContextMenuEvent(QContextMenuEvent*);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(SpellHighlighter)

    QTextEdit* _textEdit = {nullptr};
    QPlainTextEdit* _plainTextEdit = {nullptr};

    QColor  _spellColor;
    QString _currentWord;
    QString _currentLang;
};
