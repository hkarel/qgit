/*
    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/
#ifndef DATALOADER_H
#define DATALOADER_H

#include <QProcess>
#include <QTime>
#include <QTimer>

class Git;
class FileHistory;
class QString;
class UnbufferedTemporaryFile;

// data exchange facility with 'git log' could be based on QProcess or on
// a temporary file (default). Uncomment following line to use QProcess
// #define USE_QPROCESS

class DataLoader : public QProcess {
Q_OBJECT
public:
    DataLoader(Git* g, FileHistory* f);
    ~DataLoader();
    bool start(const QStringList& args, const QString& wd, const QString& buf);

signals:
    void newDataReady(const FileHistory*);
    void loaded(FileHistory*, ulong, int, bool, const QString&, const QString&);

private slots:
    void on_finished(int, QProcess::ExitStatus);
    void on_cancel();
    void on_cancel(const FileHistory*);
    void on_timeout();

private:
    //void parseSingleBuffer(const QByteArray& ba);
    //void baAppend(QByteArray** src, const char* ascii, int len);
    //void addSplittedChunks(const QByteArray* halfChunk);
    bool createTemporaryFile();
    qint64 readNewData();

    Git* const git = {nullptr};
    FileHistory* const fileHist = {nullptr};
    UnbufferedTemporaryFile* dataFile = {nullptr};

    QTime loadTime;
    QTimer guiUpdateTimer;

    bool parsing = {false};
    bool canceling = {false};
    bool procFinished = {true};
    qint64 loadedBytes = {0};
    int timerCallCounter = {0};

    QByteArray rawBuff;
};

#endif
