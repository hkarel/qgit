/**
 * parsetrigrams.cpp
 *
 * Parse a corpus of data and generate trigrams
 *
 * SPDX-FileCopyrightText: 2013 Martin Sandsmark <martin.sandsmark@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "shared/list.h"

#include <QFile>
#include <QHash>
#include <QString>
#include <QDebug>

struct Item
{
    QString trigram;
    int frequency;
};

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        qWarning() << argv[0] << "corpus.txt outfile.trigram [latin, greek, cyrillic ...]";
        return 1;
    }

    QChar::Script script = QChar::Script_Latin;
    QString scriptStr = QString::fromLocal8Bit(argv[3]);
    if (scriptStr == "latin")
        script = QChar::Script_Latin;
    else if (scriptStr == "greek")
        script = QChar::Script_Greek;
    else if (scriptStr == "cyrillic")
        script = QChar::Script_Cyrillic;
    else
    {
        qWarning() << "Unknown script code: " << argv[3];
        return 1;
    }

    QFile file {QString::fromLocal8Bit(argv[1])};
    if (!file.open(QIODevice::ReadOnly | QFile::Text))
    {
        qWarning() << "Unable to open corpus: " << argv[1];
        return 1;
    }
    QTextStream stream {&file};
    stream.setCodec("UTF-8");

    QFile outFile {QString::fromLocal8Bit(argv[2])};
    if (!outFile.open(QIODevice::WriteOnly))
    {
        qWarning() << "Unable to open output file" << argv[2];
        return 1;
    }

    QHash<QString, int> model;
    qDebug() << "Reading in" << file.size() << "bytes";
    QString trigram = stream.read(3).toLower();
    QString contents = stream.readAll();
    qDebug() << "finished reading!";
    qDebug() << "Building model...";
    for (int i = 0; i < contents.length(); ++i)
    {
        bool skip = false;
        for (int j = 0; j < 3; ++j)
        {
            QChar ch = trigram[j];
            if (!ch.isPrint()
                || ch.isPunct()
                || ch.isSpace()
                || ch.isNumber())
            {
                skip = true;
                break;
            }

            if (ch.script() == QChar::Script_Inherited)
            {
                qWarning() << "Skipped Script_Inherited: " << ch;
                skip = true;
                break;
            }
            if (ch.script() == QChar::Script_Common)
            {
                qWarning() << "Skipped Script_Common: " << ch;
                skip = true;
                break;
            }

            if (ch.script() != script)
            {
                skip = true;
                break;
            }
        }
        if (script == QChar::Script_Latin
            && trigram == "the")
        {
            skip = true;
        }

        if (!skip)
            ++model[trigram];

        trigram[0] = trigram[1];
        trigram[1] = trigram[2];
        trigram[2] = contents[i].toLower();

    }
    qDebug() << "Model built!";

    lst::List<Item> list;
    for (auto it = model.cbegin(); it != model.cend(); ++it)
        list.addCopy({it.key(), it.value()});

    qDebug() << "Sorting...";
    auto srtFunc = [](const Item* i1, const Item* i2, void*)
    {
        LIST_COMPARE_MULTI_ITEM(i1->frequency, i2->frequency)
        LIST_COMPARE_MULTI_ITEM(i2->trigram,   i1->trigram)
        return 0;
    };
    list.sort(srtFunc, lst::SortMode::Down);
    qDebug() << "Sorted";

    for (int i = 3000; i < list.count(); ++i)
        list.remove(i, lst::CompressList::No);
    list.compressList();
    qDebug() << "Weeded";

    QTextStream outStream {&outFile};
    outStream.setCodec("UTF-8");
    for (Item* item : list)
        outStream << item->trigram  << "\t\t\t" << item->frequency << '\n';
    outFile.close();

    return 0;
}
