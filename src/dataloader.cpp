/*
    Author: Pavel Karelin 2021 (hkarel), <hkarel@yandex.ru>

    Copyright: See COPYING file that comes with this distribution

*/
#include "dataloader.h"
#include "FileHistory.h"
//#include "git.h"

#include "shared/defmac.h"
#include "shared/break_point.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/qt/logger_operators.h"

#include <QDir>
#include <QProcess>
#include <QTemporaryFile>

#define log_error_m   alog::logger().error   (alog_line_location, "DataLoader")
#define log_warn_m    alog::logger().warn    (alog_line_location, "DataLoader")
#define log_info_m    alog::logger().info    (alog_line_location, "DataLoader")
#define log_verbose_m alog::logger().verbose (alog_line_location, "DataLoader")
#define log_debug_m   alog::logger().debug   (alog_line_location, "DataLoader")
#define log_debug2_m  alog::logger().debug2  (alog_line_location, "DataLoader")

#define GUI_UPDATE_INTERVAL 250
#define READ_BLOCK_SIZE     65535

class UnbufferedTemporaryFile : public QTemporaryFile
{
public:
    explicit UnbufferedTemporaryFile(const QString &templateName)
        : QTemporaryFile(templateName)
    {}
    bool unbufOpen() {
        return open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }
};

DataLoader::DataLoader()
{}

DataLoader::~DataLoader()
{
    stop();
}

bool DataLoader::init(FileHistory* fileHist, const QStringList& args,
                       const QString& workDir, const QString& buff)
{
    _runInit = -1;
    _fileHist = fileHist;
    _args = args;
    _workDir = workDir;
    _buff = buff;

    return true;
}

void DataLoader::cancel(const FileHistory* fh)
{
    if (fh == _fileHist)
        cancel();
}

void DataLoader::cancel()
{
    stop();
}

void DataLoader::runSetError()
{
    _runInit = 0;
    _fileHist = nullptr;
}

void DataLoader::run()
{
    log_info_m << "Started";

    if (!_fileHist)
    {
        runSetError();
        log_error_m << "Not initialized 'fileHist' field";
        log_info_m  << "Stopped";
        return;
    }

    QString tmpDirPath = QDir::tempPath() + "/qgit";
    UnbufferedTemporaryFile dataFile {tmpDirPath};

    if (!dataFile.open())
    {
        runSetError();
        log_error_m << "Failed open temporary file";
        log_info_m  << "Stopped";
        return;
    }

    QString tmpFilePath = dataFile.fileName();
    dataFile.close();

    QProcess proc;
    proc.setWorkingDirectory(_workDir);
    proc.setStandardOutputFile(tmpFilePath);

    bool procFinished = false;
    bool procFinishLog = false;

    auto procFinishFunc = [&procFinished](int /*exitCode*/) {
        procFinished = true;
    };
    connect(&proc, QOverload<int>::of(&QProcess::finished), procFinishFunc);

    if (!qgit::startProcess(&proc, _args, _buff))
    {
        runSetError();
        log_info_m  << "Stopped";
        return;
    }
    while (proc.state() == QProcess::Starting)
        usleep(10);

    _runInit = 1;
    msleep(GUI_UPDATE_INTERVAL);

    if (!dataFile.unbufOpen())
    {
        log_error_m << "Failed open temporary file: " << tmpFilePath;
        log_info_m  << "Stopped";
        return;
    }

    int loopCounter = 0;
    qint64 loadedBytes = 0;

    QByteArray rawBuff;
    QTextCodec* tc = QTextCodec::codecForLocale();

    QTime loadTime;
    loadTime.start();

    while (true)
    {
        CHECK_QTHREADEX_STOP

        if (!procFinishLog && procFinished)
        {
            procFinishLog = true;
            log_debug_m << "Process finished: " << proc.program();
        }

        QByteArray ba;
        ba.resize(READ_BLOCK_SIZE);
        qint64 size = dataFile.read((char*) ba.constData(), READ_BLOCK_SIZE);

        log_debug_m << "Read git data. Size: " << size;

        if (size != 0)
        {
            if (size < ba.size())
                ba.resize(int(size));

            rawBuff.append(ba);

            int pos = -1;
            while ((pos = rawBuff.indexOf('\0')) != -1)
            {
                QByteArray b = QByteArray::fromRawData(rawBuff.constData(), pos + 1);
                QString s = tc->toUnicode(b);
                emit addChunk(_fileHist, s);
                rawBuff.remove(0, pos + 1);
            }
        }
        else
        {
            bool atEnd = dataFile.atEnd();
            if (procFinished && atEnd)
            {
                if (!rawBuff.isEmpty())
                {
                    rawBuff.append('\0');
                    QString s = tc->toUnicode(rawBuff);
                    emit addChunk(_fileHist, s);
                }
                size = -1;
            }
        }

        if (size == -1)
        {
            emit allDataLoaded(_fileHist, loadedBytes, loadTime.elapsed(), true, "", "");
            log_debug_m << "All git data readed";
            break;
        }
        loadedBytes += size;

        if ((loopCounter <= 3) || (loopCounter % 10 == 0))
            emit newDataReady(_fileHist);
        ++loopCounter;

        if (!procFinished)
            proc.waitForFinished(GUI_UPDATE_INTERVAL);
    }

    if (!procFinished)
    {
        proc.terminate();
        log_debug_m << "Wait a completion process: " << proc.program();
        if (!proc.waitForFinished(5*1000))
        {
            proc.kill();
            log_debug_m << "Process killed: " << proc.program();
        }
    }

    _runInit = -1;
    _fileHist = nullptr;

    log_info_m << "Stopped";
}

DataLoader& dataLoader()
{
    return ::safe_singleton<DataLoader>();
}
