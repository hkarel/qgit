/*
    Author: Pavel Karelin 2021 (hkarel), <hkarel@yandex.ru>

    Copyright: See COPYING file that comes with this distribution
*/

#include "spellcheck.h"
#include "common.h"

#include "shared/list.h"
#include "shared/crc32.h"
#include "shared/steady_timer.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

#include <QRegExp>
#include <map>
#include <unordered_map>

#define log_error_m   alog::logger().error   (alog_line_location, "SpellCheck")
#define log_warn_m    alog::logger().warn    (alog_line_location, "SpellCheck")
#define log_info_m    alog::logger().info    (alog_line_location, "SpellCheck")
#define log_verbose_m alog::logger().verbose (alog_line_location, "SpellCheck")
#define log_debug_m   alog::logger().debug   (alog_line_location, "SpellCheck")
#define log_debug2_m  alog::logger().debug2  (alog_line_location, "SpellCheck")

using namespace std;

namespace detail {

const int MIN_LENGTH = 3;
const int MAX_GRAMS  = 2000;

QList<QChar::Script> relevantScripts(const QString& word)
{
    int totalCount = 0;
    QHash<QChar::Script, int> scriptCounts;
    //QChar::Script script = QChar::Script_Unknown;
    for (const QChar c : word)
    {
        QChar::Script script = c.script();
        if (script == QChar::Script_Common
            || script == QChar::Script_Inherited)
            continue;

        if (!c.isLetter())
            continue;

        ++scriptCounts[script];
        ++totalCount;
    }
    if (totalCount == 0)
        return {};

    QList<QChar::Script> relevScripts;
    for (QChar::Script script : scriptCounts.keys())
    {
        int percent = scriptCounts[script] * 100 / totalCount;

        // return run types that used for 40% or more of the string
        if (percent >= 40)
        {
            relevScripts << script;
        }
        // always return basic latin if found more than 15%.
        else if ((script == QChar::Script_Latin) && (percent >= 15))
        {
            relevScripts << script;
        }
    }
    return relevScripts;
}

QStringList createWordTrigrams(const QString& word)
{
    QStringList res;
    for (int i = 0; i < (word.length() - 2); ++i)
    {
        QString tri = word.mid(i, 3);
        res.append(tri.toLower());
    }
    return res;
}

double langWeight(const QStringList& wordTrigtams,
                  const QHash<QString, double>& langTrigrams)
{
    double weight = 0;
    for (const QString& trigram : wordTrigtams)
    {
        auto it = langTrigrams.constFind(trigram);
        if (it != langTrigrams.constEnd())
            weight += it.value();
    }
    return weight;
}

} // namespace detail

bool SpellCheck::init()
{
    if (_initialized == 1)
        return true;

    if (_initialized == 0)
        return false;

    _huns.clear();
    _trigramsMap.clear();
    _externWords.clear();
    _ignoreWords.clear();

    QDir spellDir {qgit::SPELL_CHECK_DIR};
    QStringList dictFiles = spellDir.entryList({"*.dic"}, QDir::Files);
    for (QString dictName : dictFiles)
    {
        //QString dictName = dict;
        dictName.chop(4);

        QString tmapFile = QString(u8":/trigrams/%1.tmap").arg(dictName);
        if (!QFile::exists(tmapFile))
        {
            log_warn_m << "Trigrams for dictionary " << dictName << " not found"
                       << ". Dictionary will be skipped";
            continue;
        }

        QString affixFilePath = qgit::SPELL_CHECK_DIR + dictName + ".aff";
        if (!QFile::exists(affixFilePath))
        {
            log_error_m << "File not found " << affixFilePath
                        << ". Dictionary " << dictName << " will be skipped";
            continue;
        }

        QString encoding;
        QFile affixFile {affixFilePath};
        if (affixFile.open(QIODevice::ReadOnly))
        {
            QTextStream stream {&affixFile};
            while (!stream.atEnd())
            {
                QString line = stream.readLine();
                if (line.isEmpty())
                    continue;

                static QRegExp encDetector {R"(^\s*SET\s+([A-Z0-9\-]+)\s*)", Qt::CaseInsensitive};

                if (encDetector.indexIn(line) > -1)
                {
                    encoding = encDetector.cap(1);
                    break;
                }
            }
            affixFile.close();
        }

        QString dictFilePath = qgit::SPELL_CHECK_DIR + dictName + ".dic";

        HunsItem* hunsItem = _huns.add();
        hunsItem->dictName = dictName;
        hunsItem->encoding = encoding;
        hunsItem->codec = QTextCodec::codecForName(encoding.toUtf8());
        hunsItem->hunspell = new Hunspell(affixFilePath.toUtf8(), dictFilePath.toUtf8());

        QString dictFilePathEx = qgit::SPELL_CHECK_DIR_EX + dictName + ".dic";
        config::dirExpansion(dictFilePathEx);
        if (QFile::exists(dictFilePathEx))
        {
            QFile dictFileEx {dictFilePathEx};
            if (dictFileEx.open(QIODevice::ReadOnly))
            {
                QTextStream stream {&dictFileEx};
                stream.setCodec("UTF-8");
                while (!stream.atEnd())
                {
                    QString line = stream.readLine();
                    if (line.isEmpty())
                        continue;

                    _externWords[dictName].insert(line);

                    QByteArray ba = hunsItem->codec->fromUnicode(line);
                    hunsItem->hunspell->add(string(ba.constData()));
                }
                dictFileEx.close();
            }
            else
                log_error_m << "Unable open user dictionary " << dictFilePathEx;
        }
        else
            log_warn_m << "User dictionary not found " << dictFilePathEx;

//        QString ignoreFilePath = qgit::SPELL_CHECK_DIR_EX + dictName + ".ign";
//        config::dirExpansion(ignoreFilePath);
//        if (QFile::exists(ignoreFilePath))
//        {
//            QFile ignoreFile {ignoreFilePath};
//            if (ignoreFile.open(QIODevice::ReadOnly))
//            {
//                QTextStream stream {&ignoreFile};
//                stream.setCodec("UTF-8");
//                while (!stream.atEnd())
//                {
//                    QString line = stream.readLine();
//                    if (line.isEmpty())
//                        continue;

//                    _ignoreWords[dictName].insert(line);
//                }
//                ignoreFile.close();
//            }
//            else
//            {
//                log_error_m << "Unable open user ignore-word dictionary: " << ignoreFilePath;
//                _initialized = 0;
//                return false;
//            }
//        }
    }
    _huns.sort();

    QList<QString> langs;
    config::base().getValue("spell_check.langs", langs);

    for (HunsItem* hun : _huns)
    {
        if (!langs.contains(hun->dictName))
            continue;

        QString tmapFile = QString(u8":/trigrams/%1.tmap").arg(hun->dictName);
        QFile file {tmapFile};
        if (!file.open(QIODevice::ReadOnly))
        {
            log_error_m << "Unable load trigram models from " << tmapFile;
            continue;
        }

        //    trigram  weight
        QHash<QString, double> trigrams;

        QDataStream stream {&file};
        stream >> trigrams;
        file.close();

        if (trigrams.count() < detail::MAX_GRAMS)
        {
            log_error_m << log_format("%? is has only %? trigrams, expected %?",
                                      hun->dictName, trigrams.count(), detail::MAX_GRAMS);
            continue;
        }
        _trigramsMap[hun->dictName] = trigrams;
    }

//    QStringList li;
//    //QString word = u8"интенрационализация";
//    QString word = u8"internationalization";
//    li = langDetect(QStringRef(&word));
//    //identify(u8"болото");
//    //langIdentify(u8"много");

    _initialized = 1;
    return true;
}

void SpellCheck::deinit()
{
    _initialized = -1;
    _huns.clear();
    _trigramsMap.clear();
    _externWords.clear();
    _ignoreWords.clear();
}

int SpellCheck::initialized() const
{
    return _initialized;
}

bool SpellCheck::spell(const QString& word, const QString& dictName) const
{
    if (_ignoreWords[dictName].contains(word))
        return true;

    bool res = true;
    if (lst::FindResult fr = _huns.findRef(dictName))
    {
        HunsItem* hi = _huns.item(fr.index());
        QByteArray ba = hi->codec->fromUnicode(word);
        res = hi->hunspell->spell(string(ba.constData()));
    }
    return res;
}

QStringList SpellCheck::suggest(const QString& word, const QString& dictName) const
{
    QStringList list;
    if (lst::FindResult fr = _huns.findRef(dictName))
    {
        HunsItem* hi = _huns.item(fr.index());
        QByteArray ba = hi->codec->fromUnicode(word);
        vector<string> ls = hi->hunspell->suggest(string(ba.constData()));
        for (const string& w : ls)
        {
            QString w2 = hi->codec->toUnicode(w.c_str(), int(w.size()));
            list.append(w2);
        }
    }
    return list;
}

void SpellCheck::addWord(const QString& word, const QString& dictName)
{
    lst::FindResult fr = _huns.findRef(dictName);
    if (fr.failed())
        return;

    HunsItem* hi = _huns.item(fr.index());
    QByteArray ba = hi->codec->fromUnicode(word);
    hi->hunspell->add(string(ba.constData()));

    if (!_externWords[dictName].contains(word))
    {
        _externWords[dictName].insert(word);

        QString dictFilePathEx = qgit::SPELL_CHECK_DIR_EX + dictName + ".dic";
        config::dirExpansion(dictFilePathEx);
        if (QFile::exists(dictFilePathEx))
        {
            QFile::remove(dictFilePathEx + ".bak");
            QFile::rename(dictFilePathEx, dictFilePathEx + ".bak");
        }

        QFile dictFileEx {dictFilePathEx};
        if (dictFileEx.open(QIODevice::WriteOnly))
        {
            QTextStream stream {&dictFileEx};
            stream.setCodec("UTF-8");
            for (const QString& w : _externWords[dictName])
                stream << w << endl;

            dictFileEx.close();
        }
        else
            log_error_m << "Unable open user dictionary: " << dictFilePathEx;
    }
}

void SpellCheck::addIgnore(const QString& word, const QString& dictName)
{
    if (_ignoreWords[dictName].contains(word))
        return;

    _ignoreWords[dictName].insert(word);

//    QString ignoreFilePath = qgit::SPELL_CHECK_DIR_EX + dictName + ".ign";
//    config::dirExpansion(ignoreFilePath);
//    if (QFile::exists(ignoreFilePath))
//    {
//        QFile::remove(ignoreFilePath + ".bak");
//        QFile::rename(ignoreFilePath, ignoreFilePath + ".bak");
//    }

//    QFile ignoreFile {ignoreFilePath};
//    if (ignoreFile.open(QIODevice::WriteOnly))
//    {
//        QTextStream stream {&ignoreFile};
//        stream.setCodec("UTF-8");
//        for (const QString& w : _ignoreWords[dictName])
//            stream << w << endl;

//        ignoreFile.close();
//    }
//    else
//        log_error_m << "Unable open user ignore-word dictionary: " << ignoreFilePath;
}

QStringList SpellCheck::dictNames() const
{
    QStringList dicts;
    for (HunsItem* hun : _huns)
        dicts.append(hun->dictName);

    return dicts;
}

QString SpellCheck::langDetect(const QString& word) const
{
    if (word.length() < detail::MIN_LENGTH)
        return {};

    DetectList detections;
    const QStringList& wordTrigrams = detail::createWordTrigrams(word);

    for (const QString& lang : _trigramsMap.keys())
    {
        DetectItem* detect = detections.add();
        detect->lang = lang;

        const QHash<QString, double>& langTrigrams = _trigramsMap[lang];
        for (const QString& trigram : wordTrigrams)
        {
            auto it = langTrigrams.constFind(trigram);
            if (it != langTrigrams.constEnd())
            {
                detect->trigramCount  += 1;
                detect->trigramWeight += it.value();
            }
        }
    }

    auto detectSort = [](const DetectItem* item1, const DetectItem* item2) -> int
    {
        LIST_COMPARE_MULTI_ITEM(item1->trigramCount,  item2->trigramCount)
        LIST_COMPARE_MULTI_ITEM(item1->trigramWeight, item2->trigramWeight)
        return 0;
    };
    detections.sort(detectSort, lst::SortMode::Down);

    return (!detections.empty()) ? detections[0].lang : QString();
}

SpellCheck& spellCheck()
{
    return ::safe_singleton<SpellCheck>();
}
