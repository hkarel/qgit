/**
 * Modified: Pavel Karelin 2021 (hkarel), <hkarel@yandex.ru>

 * parsetrigrams.cpp
 *
 * Parse a set of trigram files into a QHash, and serialize to stdout.
 *
 * SPDX-FileCopyrightText: 2006 Jacob Rideout <kde@jacobrideout.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <QString>
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QDataStream>

int main(int argc, char** argv)
{
    if (argc < 3)
        return 1;

    QFile sout {argv[2]};
    if (!sout.open(QIODevice::WriteOnly))
        return 1;

    QFile fin {QLatin1String(argv[1])};
    if (!fin.open(QFile::ReadOnly | QFile::Text))
        return 1;

    //    trigram  weight
    QHash<QString, double> trigrams;
    const QRegularExpression rx {R"((?:.{3})\s+(.*))"};

    QTextStream stream {&fin};
    stream.setCodec("UTF-8");
    int totalWeight = 0;
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        const QRegularExpressionMatch match = rx.match(line);
        if (match.hasMatch())
        {
            int weight = match.capturedRef(1).toInt();
            totalWeight += weight;
            trigrams[line.left(3)] = weight;
        }
    }
    for (double& weight : trigrams)
        weight /= totalWeight;

    QDataStream out {&sout};
    out << trigrams;
    sout.close();

    return 0;
}
