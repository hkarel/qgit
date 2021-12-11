/*
    Author: Marco Costalba (C) 2005-2007

    Copyright: See COPYING file that comes with this distribution

*/

#pragma once

#include "common_types.h"
#include "shared/defmac.h"
#include "shared/container_ptr.h"

#include <QHeaderView>
#include <QSplitter>
#include <atomic>
#include <memory>

// some syntactic sugar
#define FOREACH(i, c) \
    for (auto i = (c).constBegin(), _e##i##_ = (c).constEnd(); i != _e##i##_; ++i)

uint qHash(const ShaString&); // optimized custom hash for sha strings

namespace qgit {

// minimum git version required
extern const QString GIT_VERSION;

// tab pages
enum TabType
{
    TAB_REV,
    TAB_PATCH,
    TAB_FILE
};

// graph elements
enum LaneType
{
    EMPTY,
    ACTIVE,
    NOT_ACTIVE,
    MERGE_FORK,
    MERGE_FORK_R,
    MERGE_FORK_L,
    JOIN,
    JOIN_R,
    JOIN_L,
    HEAD,
    HEAD_R,
    HEAD_L,
    TAIL,
    TAIL_R,
    TAIL_L,
    CROSS,
    CROSS_EMPTY,
    INITIAL,
    BRANCH,
    UNAPPLIED,
    APPLIED,
    BOUNDARY,
    BOUNDARY_C, // corresponds to MERGE_FORK
    BOUNDARY_R, // corresponds to MERGE_FORK_R
    BOUNDARY_L, // corresponds to MERGE_FORK_L

    LANE_TYPES_NUM
};
const int COLORS_NUM = 8;

// graph helpers
inline bool isHead(int x) {return (x == HEAD || x == HEAD_R || x == HEAD_L);}
inline bool isTail(int x) {return (x == TAIL || x == TAIL_R || x == TAIL_L);}
inline bool isJoin(int x) {return (x == JOIN || x == JOIN_R || x == JOIN_L);}

inline bool isFreeLane(int x) {return (x == NOT_ACTIVE || x == CROSS
                                       || isJoin(x));}

inline bool isBoundary(int x) {return (x == BOUNDARY || x == BOUNDARY_C
                                       || x == BOUNDARY_R || x == BOUNDARY_L);}

inline bool isMerge(int x)    {return (x == MERGE_FORK || x == MERGE_FORK_R
                                       || x == MERGE_FORK_L || isBoundary(x));}

inline bool isActive(int x)   {return (x == ACTIVE || x == INITIAL || x == BRANCH
                                       || isMerge(x));}
// custom events
enum EventType
{
    ERROR_EV      = 65432,
    POPUP_LIST_EV = 65433,
    POPUP_FILE_EV = 65434,
    POPUP_TREE_EV = 65435,
    MSG_EV        = 65436,
    ANN_PRG_EV    = 65437,
    UPD_DM_EV     = 65438,
    UPD_DM_MST_EV = 65439
};

// list views columns
enum ColumnType
{
    GRAPH_COL   = 0,
    ANN_ID_COL  = 1,
    LOG_COL     = 2,
    AUTH_COL    = 3,
    TIME_COL    = 4,
    COMMIT_COL  = 97, // dummy col used for sha searching
    LOG_MSG_COL = 98, // dummy col used for log messages searching
    SHA_MAP_COL = 99  // dummy col used when filter output is a set of matching sha
};

inline bool isInfoCol(int x) {return (x == TIME_COL || x == LOG_COL
                                      || x == AUTH_COL);}

// default list view widths
const int DEF_GRAPH_COL_WIDTH = 80;
const int DEF_LOG_COL_WIDTH   = 500;
const int DEF_AUTH_COL_WIDTH  = 230;
const int DEF_TIME_COL_WIDTH  = 160;

// colors
extern const QColor BROWN;
extern const QColor ORANGE;
extern const QColor DARK_ORANGE;
extern const QColor LIGHT_ORANGE;
extern const QColor LIGHT_BLUE;
extern const QColor PURPLE;
extern const QColor DARK_GREEN;

// initialized at startup according to system wide settings
extern QColor  ODD_LINE_COL;
extern QColor  EVEN_LINE_COL;
extern QFont   STD_FONT;
extern QFont   TYPE_WRITER_FONT;
//extern QString GIT_DIR;

// patches drag and drop
extern const QString PATCHES_DIR;
extern const QString PATCHES_NAME;

extern const QString ZERO_SHA;
extern const QString CUSTOM_SHA;
extern const QString ALL_MERGE_FILES;

//// settings keys
//extern const QString ORG_KEY;
//extern const QString APP_KEY;
//extern const QString GIT_DIR_KEY;
//extern const QString PATCH_DIR_KEY;
//extern const QString FMT_P_OPT_KEY;
//extern const QString AM_P_OPT_KEY;
//extern const QString STD_FNT_KEY;
//extern const QString TYPWRT_FNT_KEY;
//extern const QString FLAGS_KEY;
//extern const QString CON_GEOM_KEY;
//extern const QString CMT_GEOM_KEY;
//extern const QString MAIN_GEOM_KEY;
//extern const QString REV_GEOM_KEY;
//extern const QString REV_COLS_KEY;
//extern const QString FILE_COLS_KEY;
//extern const QString CMT_TEMPL_KEY;
//extern const QString CMT_ARGS_KEY;
//extern const QString RANGE_FROM_KEY;
//extern const QString RANGE_TO_KEY;
//extern const QString RANGE_OPT_KEY;
//extern const QString EX_KEY;
//extern const QString EX_PER_DIR_KEY;
//extern const QString EXT_DIFF_KEY;
//extern const QString EXT_EDITOR_KEY;
//extern const QString ICON_SIZE_INDEX;
//extern const QString REC_REP_KEY;
//extern const QString ACT_LIST_KEY;
//extern const QString ACT_GEOM_KEY;
//extern const QString ACT_GROUP_KEY;
//extern const QString ACT_TEXT_KEY;
//extern const QString ACT_FLAGS_KEY;

//// settings default values
//extern const QString CMT_TEMPL_DEF;
//extern const QString EX_DEF;
//extern const QString EX_PER_DIR_DEF;
//extern const QString EXT_DIFF_DEF;
//extern const QString EXT_EDITOR_DEF;

extern const QString SPELL_CHECK_DIR;
extern const QString SPELL_CHECK_DIR_EX;

// settings booleans
enum FlagType
{
    MSG_ON_NEW_F       = 1 << 0,
    ACT_REFRESH_F      = 1 << 1,
    NUMBERS_F          = 1 << 2,
    LOG_DIFF_TAB_F     = 1 << 3,
    ACT_CMD_LINE_F     = 1 << 4,
    DIFF_INDEX_F       = 1 << 5,
    SIGN_PATCH_F       = 1 << 6,
    SIGN_CMT_F         = 1 << 7,
    VERIFY_CMT_F       = 1 << 8,
    SMART_LBL_F        = 1 << 9,
    REL_DATE_F         = 1 << 10,
    ALL_BRANCHES_F     = 1 << 11,
    WHOLE_HISTORY_F    = 1 << 12,
    RANGE_SELECT_F     = 1 << 13,
    REOPEN_REPO_F      = 1 << 14,
    USE_CMT_MSG_F      = 1 << 15,
    OPEN_IN_EDITOR_F   = 1 << 16,
    ENABLE_DRAGNDROP_F = 1 << 17,
    COMMIT_CONFIRM_F   = 1 << 18,
    SPELL_CHECK_F      = 1 << 19,
    SHOW_CLOSE_BTN_F   = 1 << 20,
    ENABLE_SHORTREF_F  = 1 << 21,
};
const int FLAGS_DEF = USE_CMT_MSG_F | RANGE_SELECT_F | SMART_LBL_F
                      | VERIFY_CMT_F | SIGN_PATCH_F | LOG_DIFF_TAB_F
                      | MSG_ON_NEW_F | ENABLE_DRAGNDROP_F | COMMIT_CONFIRM_F;

struct Flags
{
    void load();
    void save() const;

    bool test(uint flag) const;
    void set(uint flag, bool b);

    operator FlagType() const;

    std::atomic_uint value = {0};
};
Flags& flags();

struct CustomActionData
{
    typedef container_ptr<CustomActionData> Ptr;

    QString name;
    QString command;
    bool refresh = {false};
};

// tree view icons helpers
void initMimePix();
void freeMimePix();
const QPixmap* mimePix(const QString& fileName);

// misc helpers
bool stripPartialParaghraps(const QByteArray& src, QString* dst, QString* prev);
bool writeToFile(const QString& fileName, const QString& data, bool setExecutable = false);
bool writeToFile(const QString& fileName, const QByteArray& data, bool setExecutable = false);
bool readFromFile(const QString& fileName, QString& data);
bool startProcess(QProcess* proc, const QStringList& args, const QString& buf = "", bool* winShell = NULL);

// cache file
const uint C_MAGIC  = 0xA0B0C0D0;
const int C_VERSION = 17;

extern const QString BAK_EXT;
extern const QString C_DAT_FILE;

// misc
const int MAX_DICT_SIZE    = 100003; // must be a prime number see QDict docs
const int MAX_MENU_ENTRIES = 20;
const int MAX_RECENT_REPOS = 9;
extern const QString QUOTE_CHAR;
extern const QString SCRIPT_EXT;

const int SHA_LENGTH = 40; // from git ref. spec.
const int SHA_END_LENGTH = SHA_LENGTH + 1; // an sha key + \n

} // namespace qgit

Q_DECLARE_METATYPE(qgit::CustomActionData::Ptr)

class Rev
{
public:
    Rev() = default;
    Rev(const QString& sha, const QStringList& parents, const QString& committer,
        const QString& author, const QString& authorDate,
        const QString& shortLog, const QString& longLog,
        const QString& diff, int orderIdx);

    int parse(const QString& str, int start, int orderIdx, bool withDiff);

    const QStringList parents() const;
    bool  isBoundary() const {return _isBoundary;}
    uint  parentsCount() const {return _parents.count();}
    const ShaString& sha() const {return _sha;}
    const ShaString& parent(int idx) const {return _parents.at(idx);}
    const QString&   committer() const {return _committer;}
    const QString&   author() const {return _author;}
    const QString&   authorDate() const {return _authorDate;}
    const QString&   diff() const {return _diff;}
    const QString&   shortLog() const {return _shortLog;}
    const QString&   longLog() const {return _longLog;}

    QVector<int> lanes;
    QVector<int> children;
    QVector<int> descRefs;     // list of descendant refs index, normally tags
    QVector<int> ancRefs;      // list of ancestor refs index, normally tags
    QVector<int> descBranches; // list of descendant branches index
    int descRefsMaster = {-1}; // in case of many Rev have the same descRefs, ancRefs or
    int ancRefsMaster  = {-1}; // descBranches these are stored only once in a Rev pointed
    int descBrnMaster  = {-1}; // by corresponding index xxxMaster
    int orderIdx = {-1};

    bool isDiffCache = {false}; // put here to optimize padding
    bool isApplied   = {false}; //
    bool isUnApplied = {false}; //

private:
    Rev(const Rev&) = delete;
    Rev& operator== (const Rev&) = delete;

    QString mid(const QString& s, int start, int len) const;

    bool       _isBoundary = {false};
    ShaString  _sha;
    ShaVect    _parents;
    QString    _committer;
    QString    _author;
    QString    _authorDate;
    QString    _shortLog;
    QString    _longLog;
    QString    _diff;

};
typedef QHash<ShaString, const Rev*> RevMap;  // faster then a map


class RevFile
{
    friend class Cache; // to directly load status
    friend class Git;

    // Status information is splitted in a flags vector and in a string
    // vector in 'status' are stored flags according to the info returned
    // by 'git diff-tree' without -C option.
        // In case of a working directory file an IN_INDEX flag is or-ed togheter in
    // case file is present in git index.
    // If file is renamed or copied an entry in 'extStatus' stores the
    // value returned by 'git diff-tree -C' plus source and destination
    // files info.
    // When status of all the files is 'modified' then onlyModified is
    // set, this let us to do some optimization in this common case
    bool onlyModified;
    QVector<int> status;
    QVector<QString> extStatus;

    // prevent implicit C++ compiler defaults
    RevFile(const RevFile&);
    RevFile& operator=(const RevFile&);

public:
    enum StatusFlag
    {
        MODIFIED = 1,
        DELETED  = 2,
        NEW      = 4,
        RENAMED  = 8,
        COPIED   = 16,
        UNKNOWN  = 32,
        IN_INDEX = 64,
        ANY      = 127
    };

    RevFile() : onlyModified(true) {}

    /* This QByteArray keeps indices in some dir and names vectors,
     * defined outside RevFile. Paths are splitted in dir and file
     * name, first all the dirs are listed then the file names to
     * achieve a better compression when saved to disk.
     * A single QByteArray is used instead of two vectors because it's
     * much faster to load from disk when using a QDataStream
     */
    QByteArray pathsIdx;

    int dirAt(uint idx) const {return ((const int*)pathsIdx.constData())[idx];}
    int nameAt(uint idx) const {return ((const int*)pathsIdx.constData())[count() + idx];}

    QVector<int> mergeParent;

    // helper functions
    int count() const {
        return pathsIdx.size() / ((int)sizeof(int) * 2);
    }

    bool statusCmp(int idx, StatusFlag sf) const {
        return ((onlyModified ? MODIFIED : status.at(idx)) & sf);
    }

    const QString extendedStatus(int idx) const {
        /*
           rf.extStatus has size equal to position of latest copied/renamed file,
           that could be lower then count(), so we have to explicitly check for
           an out of bound condition.
        */
        return (!extStatus.isEmpty() && idx < extStatus.count() ? extStatus.at(idx) : "");
    }

    friend QDataStream& operator>>(QDataStream&, RevFile&);
    friend QDataStream& operator<<(QDataStream&, const RevFile&);
};
typedef QHash<ShaString, const RevFile*> RevFileMap;

class FileAnnotation
{
public:
    explicit FileAnnotation(int id) : isValid(false), annId(id) {}
    FileAnnotation() : isValid(false) {}
    QStringList lines;
    bool isValid;
    int annId;
    QString fileSha;
};
typedef QHash<ShaString, FileAnnotation> AnnotateHistory;


class BaseEvent: public QEvent
{
public:
    BaseEvent(const QString& d, int id)
        : QEvent((QEvent::Type)id), payLoad(d)
    {}
    const QString myData() const {return payLoad;}

private:
    const QString payLoad; // passed by copy
};

#define DEF_EVENT(X, T) class X : public BaseEvent { public:        \
                        explicit X (const QString& d) : BaseEvent(d, T) {} }

DEF_EVENT(MessageEvent, qgit::MSG_EV);
DEF_EVENT(AnnotateProgressEvent, qgit::ANN_PRG_EV);

class DeferredPopupEvent : public BaseEvent
{
public:
    DeferredPopupEvent(const QString& msg, int type) : BaseEvent(msg, type) {}
};

class MainExecErrorEvent : public BaseEvent
{
public:
    MainExecErrorEvent(const QString& c, const QString& e)
        : BaseEvent("", qgit::ERROR_EV), cmd(c), err(e)
    {}
    const QString command() const {return cmd;}
    const QString report() const {return err;}

private:
    const QString cmd, err;
};

QString qt4and5escaping(QString toescape);
