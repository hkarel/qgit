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

//double langWeight(const QStringList& textTrigtams,
//                  const QHash<QString, double>& langTrigrams)
//{
//    double weight = 0;
//    for (const QString& trigram : textTrigtams)
//    {
//        if (langTrigrams.contains(trigram))
//        {
//            double val = langTrigrams.value(trigram);
//            weight += val;
//        }
//    }
//    return weight;
//}

//struct TrigramItem
//{
//    QString trigtam;
//    double  weight;
//    uint    hash = {0};

//    struct Compare
//    {
////        int operator() (const TrigramItem* ti1, const TrigramItem* ti2, void*) const
////            {return ti1->trigtam.compare(ti2->trigtam);}

////        int operator() (const QString* trigtam, const TrigramItem* ti2, void*) const
////            {return trigtam->compare(ti2->trigtam);}

//        int operator() (const TrigramItem* ti1, const TrigramItem* ti2, void*) const {
//            LIST_COMPARE_MULTI_ITEM(ti1->trigtam[0], ti2->trigtam[0])
//            LIST_COMPARE_MULTI_ITEM(ti1->trigtam[1], ti2->trigtam[1])
//            LIST_COMPARE_MULTI_ITEM(ti1->trigtam[2], ti2->trigtam[2])
//            return 0;
//        }
//        int operator() (const QString* trigtam, const TrigramItem* ti2, void*) const {

////            if (trigtam->compare(ti2->trigtam) == 0)
////                break_point

//            LIST_COMPARE_MULTI_ITEM((*trigtam)[0], ti2->trigtam[0])
//            LIST_COMPARE_MULTI_ITEM((*trigtam)[1], ti2->trigtam[1])
//            LIST_COMPARE_MULTI_ITEM((*trigtam)[2], ti2->trigtam[2])
//            return 0;
//        }
//    };

//    struct Compare2
//    {
//        int operator() (const TrigramItem* ti1, const TrigramItem* ti2, void*) const
//            {return LIST_COMPARE_ITEM(ti1->hash, ti2->hash);}

//        int operator() (const uint* hash, const TrigramItem* ti2, void*) const
//            {return LIST_COMPARE_ITEM(*hash, ti2->hash);}
//    };
//};
//typedef lst::List<TrigramItem, TrigramItem::Compare> TrigramList;
//typedef lst::List<TrigramItem, TrigramItem::Compare2> TrigramList2;

//struct TrigramItem3
//{
//    std::string trigtam;
//    double  weight;
//    uint    hash = {0};

//    struct Compare
//    {
//        int operator() (const TrigramItem3* ti1, const TrigramItem3* ti2, void*) const
//            {return strcmp(ti1->trigtam.c_str(), ti2->trigtam.c_str());}

//        int operator() (const std::string* trigtam, const TrigramItem3* ti2, void*) const
//            {return strcmp(trigtam->c_str(), ti2->trigtam.c_str());}
//    };

////    struct Compare2
////    {
////        int operator() (const TrigramItem* ti1, const TrigramItem* ti2, void*) const
////            {return LIST_COMPARE_ITEM(ti1->hash, ti2->hash);}

////        int operator() (const uint* hash, const TrigramItem* ti2, void*) const
////            {return LIST_COMPARE_ITEM(*hash, ti2->hash);}
////    };
//};
//typedef lst::List<TrigramItem3, TrigramItem3::Compare> TrigramList3;


//double langWeight2(const QStringList& textTrigtams,
//                   const TrigramList& langTrigrams)
//{
//    double weight = 0;
//    for (const QString& trigram : textTrigtams)
//    {
//        if (lst::FindResult fr = langTrigrams.findRef(trigram))
//        {
//            double val = langTrigrams.item(fr.index())->weight;
//            weight += val;
//        }
//    }
//    return weight;
//}

//double langWeight3(const QStringList& textTrigtams,
//                   const QMap<QString, double>& langTrigrams)
//{
//    double weight = 0;
////    for (const QString& trigram : textTrigtams)
////    {
////        if (langTrigrams.contains(trigram))
////        {
////            double val = langTrigrams.value(trigram);
////            weight += val;
////        }
////    }
//    for (const QString& trigram : textTrigtams)
//    {
//        auto it = langTrigrams.constFind(trigram);
//        if (it != langTrigrams.constEnd())
//        {
//            double val = it.value();
//            weight += val;
//        }
//    }
//    return weight;
//}

//double langWeight4(const QStringList& textTrigtams,
//                   const TrigramList2& langTrigrams)
//{
//    double weight = 0;
//    for (const QString& trigram : textTrigtams)
//    {
//        if (lst::FindResult fr = langTrigrams.findRef(const_crc32(trigram.toStdString())))
//        {
//            double val = langTrigrams.item(fr.index())->weight;
//            weight += val;
//        }
//    }
//    return weight;
//}

//double langWeight5(const QStringList& textTrigtams,
//                   const std::map<QString, double>& langTrigrams)
//{
//    double weight = 0;
//    for (const QString& trigram : textTrigtams)
//    {
//        auto it = langTrigrams.find(trigram);
//        if (it != langTrigrams.end())
//        {
//            double val = it->second;
//            weight += val;
//        }
//    }
//    return weight;
//}

//double langWeight6(const std::list<std::string>& textTrigtams,
//                   const std::unordered_map<std::string, double>& langTrigrams)
//{
//    double weight = 0;
//    for (const auto& trigram : textTrigtams)
//    {
//        auto it = langTrigrams.find(trigram);
//        if (it != langTrigrams.end())
//        {
//            double val = it->second;
//            weight += val;
//        }
//    }
//    return weight;
//}

//double langWeight7(const std::list<std::string>& textTrigtams,
//                   const TrigramList3& langTrigrams)
//{
//    double weight = 0;
//    for (const std::string& trigram : textTrigtams)
//    {
//        if (lst::FindResult fr = langTrigrams.findRef(trigram))
//        {
//            double val = langTrigrams.item(fr.index())->weight;
//            weight += val;
//        }
//    }
//    return weight;
//}


//QStringList guessFromTrigrams(const QString& word,
//                              const QHash<QString, QHash<QString, double>>& trigramsMap)
//{
//    const QStringList& wordTrigrams = createWordTrigrams(word);

//    QMultiMap<double, QString> scores;
//    for (const QString& lang : trigramsMap.keys())
//    {
//        log_info_m << "lang: " << lang;

////        if (lang == "en_US")
////            continue;

//        const QHash<QString, double>& langTrigrams = trigramsMap[lang];
//        double weight = langWeight(wordTrigrams, langTrigrams);

//        double weight;
//        TrigramList trigramList;
//        TrigramList2 trigramList2;
//        TrigramList3 trigramList3;

//        for (const QString& tr : langTrigrams.keys())
//            trigramList.addCopy({tr, langTrigrams.value(tr), 0});
//        trigramList.sort();

//        for (const QString& tr : langTrigrams.keys())
//            trigramList2.addCopy({tr, langTrigrams.value(tr), const_crc32(tr.toStdString())});
//        trigramList2.sort();

//        std::map<QString, double> trigramMap;
//        for (const QString& tr : langTrigrams.keys())
//            trigramMap[tr] = langTrigrams.value(tr);

//        std::unordered_map<std::string, double> trigramUMap;
//        for (const QString& tr : langTrigrams.keys())
//        {
//            trigramUMap[tr.toStdString()] = langTrigrams.value(tr);
//            trigramList3.addCopy({tr.toStdString(), langTrigrams.value(tr), 0});
//        }
//        trigramList3.sort();

////        for (TrigramItem* ti : trigramList)
////            log_info_m << ti->trigtam;

//        for (int i = 0; i < (trigramList2.count() - 1); ++i)
//        {
//            uint hash1 = trigramList2[i].hash;
//            uint hash2 = trigramList2[i + 1].hash;

//            if (hash1 == hash2)
//                log_info_m << "double hash: " << hash2 << ", i: " << i;
//        }

//        steady_timer timer;
//        for (int i = 0; i < 10000; ++i)
//        {
//            weight = langWeight(wordTrigrams, langTrigrams);
//        }
//        log_info_m << "timer QHash: " << timer.elapsed() << " weight: " << weight;

//        timer.reset();
//        for (int i = 0; i < 10000; ++i)
//        {
//            weight = langWeight2(wordTrigrams, trigramList);
//        }
//        log_info_m << "timer List: " << timer.elapsed() << " weight: " << weight;

//        QMap<QString, double> trMap;
//        for (const QString& key : langTrigrams.keys())
//            trMap[key] = langTrigrams.value(key);

//        timer.reset();
//        for (int i = 0; i < 10000; ++i)
//        {
//            weight = langWeight3(wordTrigrams, trMap);
//        }
//        log_info_m << "timer QMap: " << timer.elapsed() << " weight: " << weight;

//        timer.reset();
//        for (int i = 0; i < 10000; ++i)
//        {
//            weight = langWeight4(wordTrigrams, trigramList2);
//        }
//        log_info_m << "timer List hash: " << timer.elapsed() << " weight: " << weight;

//        timer.reset();
//        for (int i = 0; i < 10000; ++i)
//        {
//            weight = langWeight5(wordTrigrams, trigramMap);
//        }
//        log_info_m << "timer std::map: " << timer.elapsed() << " weight: " << weight;

//        std::list<std::string> wordTrig;
//        for (const QString& trigram : wordTrigrams)
//            wordTrig.push_back(trigram.toStdString());

//        timer.reset();
//        for (int i = 0; i < 10000; ++i)
//        {
//            weight = langWeight6(wordTrig, trigramUMap);
//        }
//        log_info_m << "timer std::unorder map: " << timer.elapsed() << " weight: " << weight;

//        timer.reset();
//        for (int i = 0; i < 10000; ++i)
//        {
//            weight = langWeight7(wordTrig, trigramList3);
//        }
//        log_info_m << "timer List std::string: " << timer.elapsed() << " weight: " << weight;


//        // Sort by weight
////        scores.insert(-1 * weight, lang);
//    }

//    QStringList res;
//    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it)
//    {
//        if (it.key() < -0.001)
//            res.append(it.value());
//    }
//    return res;
//}

//QStringList guessFromTrigrams(const QStringRef& word,
//                              const SpellCheck::TrigramsMap& trigramsMap)
//{
//    const QStringList& wordTrigrams = createWordTrigrams(word);

//    QMultiMap<double, QString> scores;
//    for (const QString& lang : trigramsMap.keys())
//    {
//        //const QHash<QString, double>& langTrigrams = trigramsMap[lang];
//        double weight = langWeight(wordTrigrams, trigramsMap[lang]);

//        // Sort by weight
//        scores.insert(-1 * weight, lang);
//    }

//    QStringList res;
//    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it)
//    {
//        if (it.key() < -0.001)
//            res.append(it.value());
//    }
//    return res;
//}

} // namespace detail

bool SpellCheck::init()
{
    if (_initialized == 1)
        return true;

    if (_initialized == 0)
        return false;

    _huns.clear();
    _externWords.clear();
    _ignoreWords.clear();

    QDir spellDir {qgit::SPELL_CHECK_DIR};
    QStringList dictFiles = spellDir.entryList({"*.dic"}, QDir::Files);
    for (QString dictName : dictFiles)
    {
        //QString dictName = dict;
        dictName.chop(4);

        QString affixFilePath = qgit::SPELL_CHECK_DIR + dictName + ".aff";
        if (!QFile::exists(affixFilePath))
        {
            log_error_m << "File not found: " << affixFilePath
                        << ". Dictionary " << dictName << " will be ignored";
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
            {
                log_error_m << "Unable open user dictionary: " << dictFilePathEx;
                _initialized = 0;
                return false;
            }
        }
        else
            log_warn_m << "User dictionary not found: " << dictFilePathEx;

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

    for (HunsItem* hun : _huns)
    {
        QString tmapFile = QString(u8":/trigrams/%1.tmap").arg(hun->dictName);
        if (!QFile::exists(tmapFile))
        {
            log_error_m << "Failed load trigrams from " << tmapFile;
            _initialized = 0;
            return false;
        }

        QFile file {tmapFile};
        if (!file.open(QIODevice::ReadOnly))
        {
            log_error_m << "Unable to load trigram models from " << tmapFile;
            _initialized = 0;
            return false;
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
    _huns.clear();
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

    auto detectSort = [](const DetectItem* item1, const DetectItem* item2, void*) -> int
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
