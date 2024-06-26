/*
        Description: interface to git programs

        Author: Marco Costalba (C) 2005-2007

        Copyright: See COPYING file that comes with this distribution

*/

#include "common.h"

#include "shared/break_point.h"
#include "shared/safe_singleton.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

#include <QApplication>
#include <QTextDocument>
#include <QScreen>

static inline uint hexVal(const QChar* ch) {

    return ch->unicode();
}

uint qHash(const ShaString& s) {

    // for debug
    //return qHash((QString) s);

    const QChar* ch = s.unicode();
    return (hexVal(ch     ) << 24)
         + (hexVal(ch +  2) << 20)
         + (hexVal(ch +  4) << 16)
         + (hexVal(ch +  6) << 12)
         + (hexVal(ch +  8) <<  8)
         + (hexVal(ch + 10) <<  4)
         +  hexVal(ch + 12);
}

namespace qgit {

#ifdef Q_OS_WIN32 // *********  platform dependent code ******

const QString SCRIPT_EXT = ".bat";

static void adjustPath(QStringList& args, bool* winShell) {
/*
   To run an application/script under Windows you need
   to wrap the command line in the shell interpreter.
   You need this also to start native commands as 'dir'.
   An exception is if application is 'git' in that case we
   call with absolute path to be sure to find it.
*/
    if (args.first() == "git" || args.first().startsWith("git-")) {

        if (!GIT_DIR.isEmpty()) // application built from sources
            args.first().prepend(GIT_DIR + '/');

        if (winShell)
            *winShell = false;

    } else if (winShell) {
        args.prepend("/c");
        args.prepend("cmd.exe");
        *winShell = true;
    }
}

#elif defined(Q_OS_MACX) // MacOS X specific code

#include <sys/types.h> // used by chmod()
#include <sys/stat.h>  // used by chmod()

const QString SCRIPT_EXT = ".sh";

static void adjustPath(QStringList& args, bool*) {
/*
    Under MacOS X, git typically doesn't live in the PATH
    So use GIT_DIR from the settings if available

    Note: I (OC) think that this should be the default behaviour,
          but I don't want to break other platforms, so I introduced
          the MacOS X special case. Feel free to make this the default if
          you do feel the same.
*/
    if (args.first() == "git" || args.first().startsWith("git-")) {

        if (!GIT_DIR.isEmpty()) // application built from sources
            args.first().prepend(GIT_DIR + '/');

    }
}

#else

#include <sys/types.h> // used by chmod()
#include <sys/stat.h>  // used by chmod()

const QString SCRIPT_EXT = ".sh";

static void adjustPath(QStringList&, bool*) {}

#endif // *********  end of platform dependent code ******

// minimum git version required
const QString GIT_VERSION = "1.5.5";

// colors
const QColor BROWN        = QColor(150, 75, 0);
const QColor ORANGE       = QColor(255, 160, 50);
const QColor DARK_ORANGE  = QColor(216, 144, 0);
const QColor LIGHT_ORANGE = QColor(255, 221, 170);
const QColor LIGHT_BLUE   = QColor(85, 255, 255);
const QColor PURPLE       = QColor(221, 221, 255);
const QColor DARK_GREEN   = QColor(0, 205, 0);

// initialized at startup according to system wide settings
//QString GIT_DIR;

/*
   Default QFont c'tor calls static method QApplication::font() that could
   be still NOT initialized at this time, so set a dummy font family instead,
   it will be properly changed later, at startup
*/
QFont STD_FONT("Helvetica");
QFont TYPE_WRITER_FONT("Helvetica");

// patches drag and drop
const QString PATCHES_DIR  = "/.qgit_patches_copy";
const QString PATCHES_NAME = "qgit_import";

// git index parameters
const QString ZERO_SHA        = "0000000000000000000000000000000000000000";
const QString CUSTOM_SHA      = "*** CUSTOM * CUSTOM * CUSTOM * CUSTOM **";
const QString ALL_MERGE_FILES = "ALL_MERGE_FILES";

//const QByteArray QGit::ZERO_SHA_BA(QGit::ZERO_SHA.toLatin1());
//const ShaString  QGit::ZERO_SHA_RAW(QGit::ZERO_SHA_BA.constData());

//// settings keys
//const QString ORG_KEY         = "qgit";
//const QString APP_KEY         = "qgit4";
//const QString GIT_DIR_KEY     = "msysgit_exec_dir";
//const QString EXT_DIFF_KEY    = "external_diff_viewer";
//const QString EXT_EDITOR_KEY  = "external_editor";
//const QString ICON_SIZE_INDEX = "icon_size_index";
//const QString REC_REP_KEY     = "recent_open_repos";
//const QString STD_FNT_KEY     = "standard_font";
//const QString TYPWRT_FNT_KEY  = "typewriter_font";
//const QString FLAGS_KEY       = "flags";
//const QString PATCH_DIR_KEY   = "Patch/last_dir";
//const QString FMT_P_OPT_KEY   = "Patch/args";
//const QString AM_P_OPT_KEY    = "Patch/args_2";
//const QString EX_KEY          = "Working_dir/exclude_file_path";
//const QString EX_PER_DIR_KEY  = "Working_dir/exclude_per_directory_file_name";
//const QString CON_GEOM_KEY    = "Console/geometry";
//const QString CMT_GEOM_KEY    = "Commit/geometry";
//const QString MAIN_GEOM_KEY   = "Top_window/geometry";
//const QString REV_GEOM_KEY    = "Rev_List_view/geometry";
//const QString REV_COLS_KEY    = "Rev_List_view/columns";
//const QString FILE_COLS_KEY   = "File_List_view/columns";
//const QString CMT_TEMPL_KEY   = "Commit/template_file_path";
//const QString CMT_ARGS_KEY    = "Commit/args";
//const QString RANGE_FROM_KEY  = "RangeSelect/from";
//const QString RANGE_TO_KEY    = "RangeSelect/to";
//const QString RANGE_OPT_KEY   = "RangeSelect/options";
//const QString ACT_GEOM_KEY    = "Custom_actions/geometry";
//const QString ACT_LIST_KEY    = "Custom_actions/list";
//const QString ACT_GROUP_KEY   = "Custom_action_list/";
//const QString ACT_TEXT_KEY    = "/commands";
//const QString ACT_FLAGS_KEY   = "/flags";

//// settings default values
//const QString CMT_TEMPL_DEF   = ".git/commit-template";
//const QString EX_DEF          = ".git/info/exclude";
//const QString EX_PER_DIR_DEF  = ".gitignore";
//const QString EXT_DIFF_DEF    = "kompare";
//const QString EXT_EDITOR_DEF  = "emacs";

const QString SPELL_CHECK_DIR    = "/usr/share/hunspell/";
const QString SPELL_CHECK_DIR_EX = "~/.config/enchant/";

// cache file
const QString BAK_EXT          = ".bak";
const QString C_DAT_FILE       = "/qgit_cache.dat";

// misc
const QString QUOTE_CHAR = "$";

// settings helpers
void Flags::load()
{
    uint f = FLAGS_DEF;
    config::base().getValue("general.flags", f);
    value = f;
}

void Flags::save() const
{
    config::base().setValue("general.flags", uint(value));
    config::base().saveFile();
}

bool Flags::test(uint flag) const
{
    uint flags = value;
    return (flags & flag);
}

void Flags::set(uint flag, bool b)
{
    uint flags = value;
    flags = b ? (flags | flag) : (flags & ~flag);
    value = flags;
}

Flags::operator FlagType() const
{
    return static_cast<qgit::FlagType>(value.load());
}

Flags& flags()
{
    return ::safe_singleton<Flags>();
}

// tree view icons helpers
static QHash<QString, const QPixmap*> mimePixMap;

void initMimePix() {

    if (!mimePixMap.empty()) // only once
        return;

    QPixmap* pm;
    bool ultraHD = false;
    QList<QScreen*> screens = QGuiApplication::screens();
    if (!screens.isEmpty())
        ultraHD = (screens[0]->geometry().width() >= 2560);

    auto svgToPixmap = [ultraHD](const char* imageRes) -> QPixmap*
    {
        if (ultraHD)
            return new QPixmap(QIcon(imageRes).pixmap({32, 32}));
        else
            return new QPixmap(QIcon(imageRes).pixmap({16, 16}));
    };

    //pm = new QPixmap(u8":/icons/resources/misc.png");
    pm = svgToPixmap(u8":/icons/resources/misc.svg");
    mimePixMap.insert("#default", pm);

    pm = svgToPixmap(u8":/icons/resources/folder.svg");
    mimePixMap.insert("#folder_closed", pm);

    pm = svgToPixmap(u8":/icons/resources/folder-open.svg");
    mimePixMap.insert("#folder_open", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-cmake.svg");
    mimePixMap.insert("CMakeLists.txt", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-dockerfile.svg");
    mimePixMap.insert("Dockerfile", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-csv.svg");
    mimePixMap.insert("csv", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-csrc.svg");
    mimePixMap.insert("c", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-c++src.svg");
    mimePixMap.insert("cpp", pm);
    mimePixMap.insert("cc", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-chdr.svg");
    mimePixMap.insert("h", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-c++hdr.svg");
    mimePixMap.insert("hpp", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-generic.svg");
    mimePixMap.insert("txt", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-rtf.svg");
    mimePixMap.insert("rtf", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-script.svg");
    mimePixMap.insert("sh", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-perl.svg");
    mimePixMap.insert("perl", pm);
    mimePixMap.insert("pl",   pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-python-bytecode.svg");
    mimePixMap.insert("py", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-java.svg");
    mimePixMap.insert("java", pm);
    mimePixMap.insert("jar",  pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-tar.svg");
    mimePixMap.insert("tar", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-ace.svg");
    mimePixMap.insert("gz",  pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-compressed-tar.svg");
    mimePixMap.insert("tgz", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-zip.svg");
    mimePixMap.insert("zip", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-bzip.svg");
    mimePixMap.insert("bz",  pm);
    mimePixMap.insert("bz2", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-html.svg");
    mimePixMap.insert("html", pm);

    pm = svgToPixmap(u8":/icons/resources/file/dialog-xml-editor.svg");
    mimePixMap.insert("xml", pm);

    pm = svgToPixmap(u8":/icons/resources/file/image-bmp.svg");
    mimePixMap.insert("bmp", pm);

    pm = svgToPixmap(u8":/icons/resources/file/image-gif.svg");
    mimePixMap.insert("gif", pm);

    pm = svgToPixmap(u8":/icons/resources/file/image-jpeg.svg");
    mimePixMap.insert("jpg", pm);
    mimePixMap.insert("jpeg", pm);

    pm = svgToPixmap(u8":/icons/resources/file/image-png.svg");
    mimePixMap.insert("png", pm);

    pm = svgToPixmap(u8":/icons/resources/file/image-svg+xml-compressed.svg");
    mimePixMap.insert("svg", pm);

    pm = svgToPixmap(u8":/icons/resources/file/image-tiff.svg");
    mimePixMap.insert("tiff", pm);

    pm = svgToPixmap(u8":/icons/resources/file/image-x-ico.svg");
    mimePixMap.insert("ico", pm);

    pm = svgToPixmap(u8":/icons/resources/file/image-x-xcf.svg");
    mimePixMap.insert("xcf", pm);

    pm = svgToPixmap(u8":/icons/resources/file/image-x-generic.svg");
    mimePixMap.insert("pbm", pm);
    mimePixMap.insert("pgm", pm);
    mimePixMap.insert("ppm", pm);
    mimePixMap.insert("xbm", pm);
    mimePixMap.insert("xpm", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-json.svg");
    mimePixMap.insert("json", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-pdf.svg");
    mimePixMap.insert("pdf", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-javascript.svg");
    mimePixMap.insert("js", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-go.svg");
    mimePixMap.insert("go", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-markdown.svg");
    mimePixMap.insert("md", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-patch.svg");
    mimePixMap.insert("patch", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-qml.svg");
    mimePixMap.insert("qml", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-tex.svg");
    mimePixMap.insert("tex", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-ruby.svg");
    mimePixMap.insert("rb", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-rust.svg");
    mimePixMap.insert("rs", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-css.svg");
    mimePixMap.insert("css", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-csharp.svg");
    mimePixMap.insert("cs", pm);

    pm = svgToPixmap(u8":/icons/resources/file/text-x-r.svg");
    mimePixMap.insert("r", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-designer.svg");
    mimePixMap.insert("ui", pm);

    pm = svgToPixmap(u8":/icons/resources/file/project-development.svg");
    mimePixMap.insert("pro", pm);
    mimePixMap.insert("sln", pm);
    mimePixMap.insert("vcproj", pm);
    mimePixMap.insert("vcxproj", pm);

    pm = svgToPixmap(u8":/icons/resources/file/application-x-sharedlib.svg");
    mimePixMap.insert("Makefile", pm);
}

void freeMimePix() {

    // Do not release the map, the operating system will free elements
    // when application destroyed.
    // qDeleteAll(mimePixMap);
}

const QPixmap* mimePix(const QString& fileName) {

    // Try to match full filename (e.g. CMakeLists.txt)
    if (mimePixMap.contains(fileName))
        return mimePixMap.value(fileName);

    // Try to match extension
    const QString& ext = fileName.section('.', -1, -1).toLower();
    auto it = mimePixMap.find(ext);
    return (it != mimePixMap.end()) ? it.value() : mimePixMap["#default"];
}

// misc helpers
bool stripPartialParaghraps(const QByteArray& ba, QString* dst, QString* prev) {

    QTextCodec* tc = QTextCodec::codecForLocale();

    if (ba.endsWith('\n')) { // optimize common case
        *dst = tc->toUnicode(ba);

        // handle rare case of a '\0' inside content
        while (dst->size() < ba.size() && ba.at(dst->size()) == '\0') {
            QString s = tc->toUnicode(ba.mid(dst->size() + 1)); // sizes should match
            dst->append(" ").append(s);
        }

        dst->truncate(dst->size() - 1); // strip trailing '\n'
        if (!prev->isEmpty()) {
            dst->prepend(*prev);
            prev->clear();
        }
        return true;
    }
    QString src = tc->toUnicode(ba);
    // handle rare case of a '\0' inside content
    while (src.size() < ba.size() && ba.at(src.size()) == '\0') {
        QString s = tc->toUnicode(ba.mid(src.size() + 1));
        src.append(" ").append(s);
    }

    int idx = src.lastIndexOf('\n');
    if (idx == -1) {
        prev->append(src);
        dst->clear();
        return false;
    }
    *dst = src.left(idx).prepend(*prev); // strip trailing '\n'
    *prev = src.mid(idx + 1); // src[idx] is '\n', skip it
    return true;
}

bool writeToFile(const QString& fileName, const QString& data, bool setExecutable) {

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        log_error << log_format("Unable to write file %?", fileName);
        return false;
    }
    QString data2(data);
    QTextStream stream(&file);

#ifdef Q_OS_WIN32
    data2.replace("\r\n", "\n"); // change windows CRLF to linux
    data2.replace("\n", "\r\n"); // then change all linux CRLF to windows
#endif
    stream << data2;
    file.close();

#ifndef Q_OS_WIN32
    if (setExecutable)
        chmod(fileName.toLatin1().constData(), 0755);
#endif
    return true;
}

bool writeToFile(const QString& fileName, const QByteArray& data, bool setExecutable) {

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        log_error << log_format("Unable to write file %?", fileName);
        return false;
    }
    QDataStream stream(&file);
    stream.writeRawData(data.constData(), data.size());
    file.close();

#ifndef Q_OS_WIN32
    if (setExecutable)
        chmod(fileName.toLatin1().constData(), 0755);
#endif
    return true;
}

bool readFromFile(const QString& fileName, QString& data) {

    data = "";
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        log_error << log_format("Unable to read file %?", fileName);
        return false;
    }
    QTextStream stream(&file);
    data = stream.readAll();
    file.close();
    return true;
}

bool startProcess(QProcess* proc, const QStringList& args, const QString& buf, bool* winShell) {

    if (!proc || args.isEmpty())
        return false;

    QStringList arguments(args);
    adjustPath(arguments, winShell);

    QString prog(arguments.first());
    arguments.removeFirst();
    if (!buf.isEmpty()) {
    /*
       On Windows buffer size of QProcess's standard input
       pipe is quite limited and a crash can occur in case
       a big chunk of data is written to process stdin.
       As a workaround we use a temporary file to store data.
       Process stdin will be redirected to this file
    */
        QTemporaryFile* bufFile = new QTemporaryFile(proc);
        bufFile->open();
        QTextStream stream(bufFile);
        stream << buf;
        proc->setStandardInputFile(bufFile->fileName());
        bufFile->close();
    }
    QStringList env = QProcess::systemEnvironment();
    env << "GIT_TRACE=0"; // avoid choking on debug traces
    env << "GIT_FLUSH=0"; // skip the fflush() in 'git log'
    proc->setEnvironment(env);

    proc->start(prog, arguments); // TODO test QIODevice::Unbuffered
    return proc->waitForStarted();
}

} // namespace qgit


//---------------------------------- Rev ------------------------------------

Rev::Rev(const QString& sha, const QStringList& parents, const QString& committer,
         const QString& author, const QString& authorDate,
         const QString& shortLog, const QString& longLog,
         const QString& diff, int orderIdx) {

    _sha = sha;
    for (const QString& p : parents)
        _parents.append(p);
    _committer = committer;
    _author = author;
    _authorDate = authorDate;
    _shortLog = shortLog;
    _longLog = longLog;
    _diff = diff;
    this->orderIdx = orderIdx;
}

QString Rev::mid(const QString& s, int start, int len) const {

    const QChar* data = s.constData();
    return QString(data + start, len);
}

QString Rev::midSha(int start, int len) const {

    return mid(_sha, start, len);
}

const QStringList Rev::parents() const {

    QStringList p;
    for (int i = 0; i < _parents.count(); i++)
        p.append(_parents.at(i));
    return p;
}

int Rev::parse(const QString& str, int start, int orderIdx, bool withDiff) {

    this->orderIdx = orderIdx;
    _isBoundary = false;
    isDiffCache = isApplied = isUnApplied = false;
    descRefsMaster = ancRefsMaster = descBrnMaster = -1;

/*
  This is what 'git log' produces:

        - a possible one line with "Final output:\n" in case of --early-output option
        - one line with "log size" + len of this record
        - one line with boundary info + sha + an arbitrary amount of parent's sha
        - one line with committer name + e-mail
        - one line with author name + e-mail
        - one line with author date as unix timestamp
        - zero or more non blank lines with other info, as the encoding FIXME
        - one blank line
        - zero or one line with log title
        - zero or more lines with log message
        - zero or more lines with diff content (only for file history)
        - a terminating '\0'
*/
    static int error = -1;
    static int shaXEndlLength = qgit::SHA_LENGTH + 2; // an sha key + X marker + \n
    static QChar finalOutputMarker('F'); // marks the beginning of "Final output" string
    static QChar logSizeMarker('l'); // marks the beginning of "log size" string
    static int logSizeStrLength = 9; // "log size"
    static int asciiPosOfZeroChar = 48; // char "0" has value 48 in ascii table

    const int last = str.size() - 1;
    int logSize = 0, idx = start;
    int logEnd, revEnd;

    int shaStart, comStart, autStart, autDateStart;
    int sLogStart, sLogLen, lLogStart, lLogLen, diffStart, diffLen;

    //if (str.contains(">e2f8a9883b84db38bdcd2dfca27ebe2bc2934ecb"))
    //    break_point

    // direct access is faster then QByteArray.at()
    const QChar* data = str.constData();

    if (start + shaXEndlLength > last) // at least sha header must be present
        return -1;

    if (data[start] == finalOutputMarker) // "Final output", let caller handle this
        return (str.indexOf(QChar('\n'), start) != -1) ? -2 : -1;

    // parse   'log size xxx\n'   if present -- from git ref. spec.
    if (data[idx] == logSizeMarker) {
        idx += logSizeStrLength; // move idx to beginning of log size value

        // parse log size value
        int digit;
        while ((digit = data[idx++].toLatin1()) != '\n')
            logSize = logSize * 10 + digit - asciiPosOfZeroChar;
    }
    // idx points to the boundary information, which has the same length as an sha header.
    if (++idx + shaXEndlLength > last)
        return error;

    shaStart = idx;

    _isBoundary = (str.at(shaStart - 1) == '-');
    _sha = mid(str, shaStart, qgit::SHA_LENGTH);

    // ok, now shaStart is valid but msgSize could be still 0 if not available
    logEnd = shaStart - 1 + logSize;
    if (logEnd > last)
        logEnd = last;

    idx += qgit::SHA_LENGTH; // now points to 'X' place holder

    //parentsCnt = 0;

    if (data[idx + 2] == '\n') // initial revision
        ++idx;
    else do {
        _parents.append(mid(str, idx + 1, qgit::SHA_LENGTH));
        idx += qgit::SHA_END_LENGTH;

        if (idx + 1 >= last)
            break;

    } while (data[idx + 1] != '\n');

    ++idx; // now points to the trailing '\n' of sha line

    // check for !msgSize
    if (withDiff || !logSize) {

        revEnd = (logEnd > idx) ? logEnd - 1: idx;
        revEnd = str.indexOf(QChar('\0'), revEnd + 1);
        if (revEnd == -1)
            return error;

    } else
        revEnd = logEnd;

    if (revEnd > last) // after this point we know to have the whole record
        return error;

    // ok, now revEnd is valid but logEnd could be not if !logSize
    // in case of diff we are sure content will be consumed so
    // we go all the way
    //if (/*quick && */!withDiff) {
    //    //break_point
    //    return ++revEnd;
    //}

    // commiter
    comStart = ++idx;
    idx = str.indexOf(QChar('\n'), idx); // committer line end
    if (idx == -1) {
        log_error << "Unexpected end of data";
        return error;
    }

    // author
    autStart = ++idx;
    idx = str.indexOf(QChar('\n'), idx); // author line end
    if (idx == -1) {
        log_error << "Unexpected end of data";
        return error;
    }
    _committer = mid(str, comStart, autStart - comStart - 1);

    // author date in Unix format (seconds since epoch)
    autDateStart = ++idx;
    idx = str.indexOf(QChar('\n'), idx); // author date end without '\n'
    if (idx == -1) {
        log_error << "Unexpected end of data";
        return error;
    }
    _author = mid(str, autStart, autDateStart - autStart - 1);
    _authorDate = mid(str, autDateStart, 10);

    // if no error, point to trailing \n
    ++idx;

    diffStart = diffLen = 0;
    if (withDiff) {
        diffStart = logSize ? logEnd : str.indexOf(QLatin1String("\ndiff "), idx);

        if (diffStart != -1 && diffStart < revEnd) {
            diffLen = revEnd - ++diffStart;
            _diff = mid(str, diffStart, diffLen);
        }
        else
            diffStart = 0;
    }
    if (!logSize)
        logEnd = diffStart ? diffStart : revEnd;

    // ok, now logEnd is valid and we can handle the log
    sLogStart = idx;

    if (logEnd < sLogStart) { // no shortlog no longLog

        sLogStart = sLogLen = 0;
        lLogStart = lLogLen = 0;
    } else {
        lLogStart = str.indexOf(QChar('\n'), sLogStart);
        if (lLogStart != -1 && lLogStart < logEnd - 1) {

            sLogLen = lLogStart - sLogStart; // skip sLog trailing '\n'
            lLogLen = logEnd - lLogStart; // include heading '\n' in long log

        } else { // no longLog
            sLogLen = logEnd - sLogStart;
            if (data[sLogStart + sLogLen - 1] == '\n')
                sLogLen--; // skip trailing '\n' if any

            lLogStart = lLogLen = 0;
        }
    }
    if (sLogLen)
        _shortLog = mid(str, sLogStart, sLogLen);

    if (lLogLen)
        _longLog = mid(str, lLogStart, lLogLen);

    return ++revEnd;
}


/**
 * RevFile streaming out
 */
QDataStream& operator<<(QDataStream& stream, const RevFile& rf) {

    stream << rf.pathsIdx;

    // skip common case of only modified files
    bool isEmpty = rf.onlyModified;
    stream << isEmpty;
    if (!isEmpty)
        stream << rf.status;

    // skip common case of just one parent
    isEmpty = (rf.mergeParent.isEmpty() || rf.mergeParent.last() == 1);
    stream << isEmpty;
    if (!isEmpty)
        stream << rf.mergeParent;

    // skip common case of no rename/copies
    isEmpty = rf.extStatus.isEmpty();
    stream << isEmpty;
    if (!isEmpty)
        stream << rf.extStatus;

    return stream;
}

/**
 * RevFile streaming in
 */
QDataStream& operator>>(QDataStream& stream, RevFile& rf) {

    stream >> rf.pathsIdx;

    bool isEmpty;

    stream >> isEmpty;
    rf.onlyModified = isEmpty;
    if (!rf.onlyModified)
        stream >> rf.status;

    stream >> isEmpty;
    if (!isEmpty)
        stream >> rf.mergeParent;

    stream >> isEmpty;
    if (!isEmpty)
        stream >> rf.extStatus;

    return stream;
}

QString qt4and5escaping(QString toescape) {
    return toescape.toHtmlEscaped();
}
