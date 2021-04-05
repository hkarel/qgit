/*
    Author: Pavel Karelin 2021 (hkarel), <hkarel@yandex.ru>

    Copyright: See COPYING file that comes with this distribution

*/
#pragma once

#include "shared/defmac.h"
#include "shared/safe_singleton.h"
#include "shared/qt/qthreadex.h"

#include <QtCore>

class FileHistory;

class DataLoader : public QThreadEx
{
public:
    ~DataLoader();

    bool init(FileHistory*, const QStringList& args, const QString& workDir,
              const QString& buff);

     int runInit() const {return _runInit;}

signals:
    void addChunk(FileHistory*, const QString&);
    void newDataReady(FileHistory*);
    void allDataLoaded(FileHistory*, ulong byteSize, int loadTime, bool normalExit,
                       const QString& cmd, const QString& errorDesc);

private slots:
    void cancel();
    void cancel(const FileHistory*);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(DataLoader)
    DataLoader();

    void runSetError();
    void run() override;

private:
    volatile int _runInit = {-1};
    FileHistory* _fileHist = {nullptr};

    QStringList _args;
    QString _workDir;
    QString _buff;

    template<typename T, int> friend T& ::safe_singleton();
};

DataLoader& dataLoader();
