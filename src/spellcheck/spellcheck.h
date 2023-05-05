/*
    Author: Pavel Karelin 2021 (hkarel), <hkarel@yandex.ru>

    Copyright: See COPYING file that comes with this distribution
*/

#pragma once

#include "shared/list.h"
#include "shared/defmac.h"
#include "shared/container_ptr.h"
#include "shared/safe_singleton.h"

#include <QtCore>
#include "hunspell/hunspell.hxx"

class SpellCheck : public QObject
{
public:
    bool init();
    void deinit();

    int initialized() const;

    QStringList dictNames() const;
    QString langDetect(const QString& word) const;

    bool spell(const QString& word, const QString& dictName) const;
    QStringList suggest(const QString& word, const QString& dictName) const;

    void addWord(const QString& word, const QString& dictName);
    void addIgnore(const QString& word, const QString& dictName);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(SpellCheck)
    SpellCheck() = default;

    //            language       trigram  weight
    typedef QHash<QString, QHash<QString, double>> TrigramsMap;

    struct HunsItem
    {
        ~HunsItem() {delete hunspell;}

        QString dictName;
        QString encoding;
        QTextCodec* codec = {0};
        Hunspell* hunspell = {0};

        struct Compare
        {
          // Sort
          int operator() (const HunsItem* item1, const HunsItem* item2) const
            {return QString::compare(item1->dictName, item2->dictName);}

          // Find
          int operator() (const QString* dictName, const HunsItem* item2) const
            {return QString::compare(*dictName, item2->dictName);}
        };

    };
    typedef lst::List<HunsItem, HunsItem::Compare> HunsList;

    struct DetectItem
    {
        QString lang;
        int     trigramCount = {0};
        double  trigramWeight = {0};
    };
    typedef lst::List<DetectItem> DetectList;

private:
    int _initialized = {-1};

    HunsList _huns;
    TrigramsMap _trigramsMap;

    //    language  words
    QHash<QString,  QSet<QString>> _externWords;
    QHash<QString,  QSet<QString>> _ignoreWords;

    template<typename T, int> friend T& ::safe_singleton();
};

SpellCheck& spellCheck();

