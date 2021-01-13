/*
    Description: file names persistent cache

    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#include <QFile>
#include <QDataStream>
#include <QDir>
#include "cache.h"

#include "shared/break_point.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/qt/logger_operators.h"

using namespace QGit;

bool Cache::save(const QString& gitDir, const RevFileMap& rfm,
                 const StrVect& dirs, const StrVect& files) {

    //break_point

    if (gitDir.isEmpty() || rfm.isEmpty()) {
        log_error << "Unable save cache-file";
        return false;
    }

    QString path(gitDir + C_DAT_FILE);
    QString tmpPath(path + BAK_EXT);

    QDir dir;
    if (!dir.exists(gitDir)) {
        log_error << "Git directory not found, unable to save cache";
        return false;
    }
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
        log_error << "Failed open file: " << tmpPath;
        return false;
    }

    log_info << "Saving cache. Please wait...";

    // compress in memory before write to file
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_8);

    // Write a header with a "magic number" and a version
    stream << (quint32)C_MAGIC;
    stream << (qint32)C_VERSION;

    stream << dirs;
    stream << files;

    StrVect shav;
    QVector<const RevFile*> rfv;

    FOREACH(it, rfm) {

        const ShaString& sha = it.key();
        if (   sha.isEmpty()
            || sha == ZERO_SHA
            || sha == CUSTOM_SHA
            || sha.at(0) == QChar('A')) // ALL_MERGE_FILES + rev sha
            continue;

        shav.append(sha);
        rfv.append(it.value());
    }
    stream << shav;

    int rfvCount = shav.count();
    for (int i = 0; i < rfvCount; ++i)
        stream << *(rfv.at(i));

    log_info << "Compressing data...";

    f.write(qCompress(data, 1)); // no need to encode with compressed data
    f.close();

    // rename C_DAT_FILE + BAK_EXT -> C_DAT_FILE
    if (dir.exists(path)) {
        if (!dir.remove(path)) {
            log_error << log_format("Access denied to %?", path);
            dir.remove(tmpPath);
            return false;
        }
    }
    dir.rename(tmpPath, path);

    log_info << "Cache saved";
    return true;
}

bool Cache::load(const QString& gitDir, RevFileMap& rfm, StrVect& dirs, StrVect& files) {

    // check for cache file
    QString path(gitDir + C_DAT_FILE);
    QFile f(path);
    if (!f.exists()) {
        log_error << "Unable to load file-cache";
        return true; // no cache file is not an error
    }

    if (!f.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        log_error << "Failed open file: " << path;
        return false;
    }

    QDataStream stream(qUncompress(f.readAll()));
    stream.setVersion(QDataStream::Qt_4_8);

    quint32 magic;
    qint32 version;
    stream >> magic;
    stream >> version;
    if (magic != C_MAGIC || version != C_VERSION) {
        log_error << "Unable to load file-cache. File version failed";
        f.close();
        return false;
    }

    //break_point

    stream >> dirs;
    stream >> files;

    StrVect shav;
    stream >> shav;

    int rfvCount = shav.count();
    for (int i = 0; i < rfvCount; ++i)
    {
        RevFile* rf = new RevFile();
        stream >> *rf;
        rfm.insert(shav[i], rf);
    }
    f.close();

    return true;
}
