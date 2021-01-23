/*
 * highlighter.cpp
 *
 * Modified: Pavel Karelin 2021 (hkarel), <hkarel@yandex.ru>
 *
 * SPDX-FileCopyrightText: 2004 Zack Rusin <zack@kde.org>
 * SPDX-FileCopyrightText: 2006 Laurent Montel <montel@kde.org>
 * SPDX-FileCopyrightText: 2013 Martin Sandsmark <martin.sandsmark@org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "highlighter.h"
#include "spellcheck.h"

#include <QAction>
#include <QMenu>
//#include <QMetaMethod>

#include "shared/break_point.h"
#include "shared/simple_ptr.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

#define log_error_m   alog::logger().error   (alog_line_location, "SpellCheck")
#define log_warn_m    alog::logger().warn    (alog_line_location, "SpellCheck")
#define log_info_m    alog::logger().info    (alog_line_location, "SpellCheck")
#define log_verbose_m alog::logger().verbose (alog_line_location, "SpellCheck")
#define log_debug_m   alog::logger().debug   (alog_line_location, "SpellCheck")
#define log_debug2_m  alog::logger().debug2  (alog_line_location, "SpellCheck")


SpellHighlighter::SpellHighlighter(QTextEdit *edit, const QColor& col) :
    QSyntaxHighlighter(edit),
    _textEdit(edit),
    _spellColor(col.isValid() ? col : Qt::red)
{
    setDocument(_textEdit->document());
//    connect(document(), &QTextDocument::contentsChange,
//            this, &SpellHighlighter::contentsChange);

    _textEdit->installEventFilter(this);
    _textEdit->viewport()->installEventFilter(this);
}

SpellHighlighter::SpellHighlighter(QPlainTextEdit *edit, const QColor &col) :
    QSyntaxHighlighter(edit),
    _plainTextEdit(edit),
    _spellColor(col.isValid() ? col : Qt::red)
{
    setDocument(_plainTextEdit->document());
//    connect(document(), &QTextDocument::contentsChange,
//            this, &SpellHighlighter::contentsChange);

    _plainTextEdit->installEventFilter(this);
    _plainTextEdit->viewport()->installEventFilter(this);
}

SpellHighlighter::~SpellHighlighter()
{}

//void SpellHighlighter::contentsChange(int pos, int add, int rem)
//{
//}

void SpellHighlighter::_on_actWordAdd_triggered(bool)
{
    if (!_currentWord.isEmpty() && !_currentLang.isEmpty())
    {
        spellCheck().addWord(_currentWord, _currentLang);
        log_debug_m << log_format("Spellcheck add word: %?", _currentWord);

        QTimer::singleShot(0, this, &QSyntaxHighlighter::rehighlight);
    }
}

void SpellHighlighter::_on_actWordIgnor_triggered(bool)
{
    if (!_currentWord.isEmpty() && !_currentLang.isEmpty())
    {
        spellCheck().addIgnore(_currentWord, _currentLang);
        log_debug_m << log_format("Spellcheck ignore word: %?", _currentWord);

        QTimer::singleShot(0, this, &QSyntaxHighlighter::rehighlight);
    }
}

void SpellHighlighter::_on_actWordReplace_triggered(bool)
{
    if (QAction* act = qobject_cast<QAction*>(sender()))
    {
        QTextCursor cursor;
        if (_textEdit)
            cursor = _textEdit->textCursor();
        else
            cursor = _plainTextEdit->textCursor();

        cursor.clearSelection();
        cursor.select(QTextCursor::WordUnderCursor);

        QString word = cursor.selectedText();
        log_debug_m << log_format("Spellcheck replace word: %? -> %?", word, act->text());

        cursor.insertText(act->text());
    }
}

SpellHighlighter::WordPosList SpellHighlighter::wordsBreak(const QString& text) const
{
    if (text.isEmpty())
        return {};

    WordPosList list;
    QTextBoundaryFinder finder {QTextBoundaryFinder::Word, text};
    while (finder.position() < text.length())
    {
        if (finder.boundaryReasons().testFlag(QTextBoundaryFinder::StartOfItem))
        {
            WordPos pos;
            pos.start = finder.position();
            int end = finder.toNextBoundary();
            if (end == -1)
                break;

            pos.length = end - pos.start;
            if (pos.length < 1)
                continue;

            list.append(pos);
        }
        if (finder.toNextBoundary() == -1)
            break;
    }
    return list;
}

bool SpellHighlighter::getCurrentWord(QString& word, QString& lang) const
{
    QTextCursor cursor = (_textEdit) ? _textEdit->textCursor()
                                     : _plainTextEdit->textCursor();

    const QString& text = cursor.block().text();
    const int cursorPos = cursor.positionInBlock();
    WordPosList wordPosList = wordsBreak(text);

    for (const WordPos& wordPos : wordPosList)
    {
        if (cursorPos >= wordPos.start
            && cursorPos < (wordPos.start + wordPos.length))
        {
            word = text.mid(wordPos.start, wordPos.length);
            lang = spellCheck().langDetect(word);

            log_debug_m << log_format("Word: %?, lang: %?", word, lang);
            return true;
        }
    }
    return false;
}

void SpellHighlighter::highlightBlock(const QString& text)
{
    //log_debug_m << "Call highlightBlock()";

    auto textIsComment = [](const QString& text) -> bool
    {
        for (const QChar ch : text)
            if (!ch.isSpace())
                return (ch == QChar('#'));
        return true;
    };

    if (textIsComment(text))
        return;

    //log_debug_m << "Text: " << text;

    WordPosList wordPosList = wordsBreak(text);
    for (const WordPos& wordPos : wordPosList)
    {
        bool res = true;
        QString word = text.mid(wordPos.start, wordPos.length);
        QString lang = spellCheck().langDetect(word);
        if (lang.isEmpty())
            for (const QString& dict : spellCheck().dictNames())
            {
                res = spellCheck().spell(word, dict);
                if (res)
                    break;
            }
        else
            res = spellCheck().spell(word, lang);

        if (res)
            unsetMisspelled(wordPos.start, wordPos.length);
        else
            setMisspelled(wordPos.start, wordPos.length);
    }

    setCurrentBlockState(0);
}

void SpellHighlighter::setMisspelled(int start, int count)
{
    QTextCharFormat format;
    format.setFontUnderline(true);
    format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    format.setUnderlineColor(_spellColor);
    setFormat(start, count, format);
}

void SpellHighlighter::unsetMisspelled(int start, int count)
{
    setFormat(start, count, QTextCharFormat());
}

bool SpellHighlighter::eventFilter(QObject* o, QEvent* event)
{
    if (event->type() == QEvent::ContextMenu)
        return onContextMenuEvent(static_cast<QContextMenuEvent*>(event));

    return false;
}

bool SpellHighlighter::onContextMenuEvent(QContextMenuEvent* event)
{
    //log_debug_m << "Call onContextMenuEvent()";

    simple_ptr<QMenu> popup {(_textEdit)
                             ? _textEdit->createStandardContextMenu()
                             : _plainTextEdit->createStandardContextMenu()};
    _currentWord.clear();
    _currentLang.clear();
    bool spellRes = true;

    if (getCurrentWord(_currentWord, _currentLang))
        spellRes = spellCheck().spell(_currentWord, _currentLang);

    if (spellRes)
    {
        popup->exec(event->globalPos());
        return true;
    }

    QMenu* subMenu = new QMenu(popup);
    subMenu->setTitle(u8"Spelling");

    QAction* act = new QAction(u8"Add to dictionary", popup);
    connect(act, &QAction::triggered,
            this, &SpellHighlighter::_on_actWordAdd_triggered);
    subMenu->addAction(act);

    act = new QAction(u8"Ignore word ", popup);
    connect(act, &QAction::triggered,
         this, &SpellHighlighter::_on_actWordIgnor_triggered);
    subMenu->addAction(act);

    QStringList suggestWords = spellCheck().suggest(_currentWord, _currentLang);
    if (!suggestWords.isEmpty())
        subMenu->addSeparator();

    for (const QString& suggestWord : suggestWords)
    {
        act = new QAction(suggestWord, popup);
        connect(act, &QAction::triggered,
                this, &SpellHighlighter::_on_actWordReplace_triggered);
        subMenu->addAction(act);
    }

    popup->addSeparator();
    popup->addMenu(subMenu);
    popup->exec(event->globalPos());

    _currentWord.clear();
    _currentLang.clear();
    return true;
}
