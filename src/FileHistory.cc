/*
        Description: interface to git programs

        Author: Marco Costalba (C) 2005-2007

        Copyright: See COPYING file that comes with this distribution

*/

#include "FileHistory.h"
#include "lanes.h"
#include "git.h"

#include "shared/defmac.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/qt/logger_operators.h"

#include <QApplication>
#include <QDateTime>
#include <QFontMetrics>

using namespace qgit;

FileHistory::FileHistory(QObject* p, Git* g) : QAbstractItemModel(p), git(g) {

  headerInfo << "Graph" << "Id" << "Short Log" << "Commit" << "Author" << "Author Date";
  lns = new Lanes();
  revs.reserve(qgit::MAX_DICT_SIZE);
  clear(); // after _headerInfo is set

  chk_connect_a(git, SIGNAL(newRevsAdded(const FileHistory*, const QVector<ShaString>&)),
                this, SLOT(on_newRevsAdded(const FileHistory*, const QVector<ShaString>&)))

  chk_connect_a(git, SIGNAL(loadCompleted(const FileHistory*, const QString&)),
                this, SLOT(on_loadCompleted(const FileHistory*, const QString&)));

  chk_connect_a(git, SIGNAL(changeFont(const QFont&)), this, SLOT(on_changeFont(const QFont&)));
}

FileHistory::~FileHistory() {

  clear();
  delete lns;
}

void FileHistory::resetFileNames(const QString& fn) {

  fNames.clear();
  fNames.append(fn);
  curFNames = fNames;
}

int FileHistory::rowCount(const QModelIndex& parent) const {

  return (!parent.isValid() ? rowCnt : 0);
}

bool FileHistory::hasChildren(const QModelIndex& parent) const {

  return !parent.isValid();
}

int FileHistory::row(const QString& sha) const {

  const Rev* r = git->revLookup(sha, this);
  return (r ? r->orderIdx : -1);
}

const QString FileHistory::sha(int row) const {

  return (row < 0 || row >= rowCnt ? "" : QString(revOrder.at(row)));
}

void FileHistory::flushTail() {

  if (earlyOutputCnt < 0 || earlyOutputCnt >= revOrder.count()) {
    log_warn << log_format("earlyOutputCnt is %?", earlyOutputCnt);
    return;
  }
  int cnt = revOrder.count() - earlyOutputCnt + 1;
  beginResetModel();
  while (cnt > 0) {
    const ShaString& sha = revOrder.last();
    const Rev* c = revs[sha];
    delete c;
    revs.remove(sha);
    revOrder.pop_back();
    cnt--;
  }
  // reset all lanes, will be redrawn
  for (int i = earlyOutputCntBase; i < revOrder.count(); i++) {
    Rev* c = const_cast<Rev*>(revs[revOrder[i]]);
    c->lanes.clear();
  }
  firstFreeLane = earlyOutputCntBase;
  lns->clear();
  rowCnt = revOrder.count();
  endResetModel();
}

void FileHistory::clear(bool complete) {

  if (!complete) {
    if (revOrder.count() > 0)
      flushTail();
    return;
  }
  git->cancelDataLoading(this);

  beginResetModel();
  qDeleteAll(revs);
  revs.clear();
  revOrder.clear();
  firstFreeLane = loadTime = earlyOutputCntBase = 0;
  setEarlyOutputState(false);
  lns->clear();
  fNames.clear();
  curFNames.clear();
  //qDeleteAll(rowData);
  //rowData.clear();

  if (qgit::flags().test(REL_DATE_F)) {
    secs = QDateTime::currentDateTime().toTime_t();
    headerInfo[ColumnType::TIME_COL] = "Last Change";
  } else {
    secs = 0;
    headerInfo[ColumnType::TIME_COL] = "Author Date";
  }
  rowCnt = revOrder.count();
  annIdValid = false;
  endResetModel();
  emit headerDataChanged(Qt::Horizontal, 0, ColumnType::TIME_COL);
}

void FileHistory::on_newRevsAdded(const FileHistory* fh, const QVector<ShaString>& shaVec) {

  if (fh != this) // signal newRevsAdded() is broadcast
    return;

  // do not process revisions if there are possible renamed points
  // or pending renamed patch to apply
  if (!renamedRevs.isEmpty() || !renamedPatches.isEmpty())
    return;

  // do not attempt to insert 0 rows since the inclusive range would be invalid
  if (rowCnt == shaVec.count())
    return;

  beginInsertRows(QModelIndex(), rowCnt, shaVec.count()-1);
  rowCnt = shaVec.count();
  endInsertRows();
}

void FileHistory::on_loadCompleted(const FileHistory* fh, const QString&) {

  if (fh != this || rowCnt >= revOrder.count())
    return;

  // now we can process last revision
  rowCnt = revOrder.count();
  beginResetModel(); // force a reset to avoid artifacts in file history graph under Windows
  endResetModel();

  // adjust Id column width according to the numbers of revisions we have
  if (!git->isMainHistory(this))
    on_changeFont(qgit::STD_FONT);
}

void FileHistory::on_changeFont(const QFont& f) {

  QString maxStr(QString::number(rowCnt).length() + 1, '8');
  QFontMetrics fmRows(f);
  int neededWidth = fmRows.boundingRect(maxStr).width();

  QString id("Id");
  QFontMetrics fmId(qApp->font());

  while (fmId.boundingRect(id).width() < neededWidth)
    id += ' ';

  headerInfo[1] = id;
  emit headerDataChanged(Qt::Horizontal, 1, 1);
}

Qt::ItemFlags FileHistory::flags(const QModelIndex&) const {

  return Qt::ItemIsEnabled | Qt::ItemIsSelectable; // read only
}

QVariant FileHistory::headerData(int section, Qt::Orientation orientation, int role) const {

  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    return headerInfo.at(section);

  return QVariant();
}

QModelIndex FileHistory::index(int row, int column, const QModelIndex&) const {
  /*
        index() is called much more then data(), also by a 100X factor on
        big archives, so we use just the row number as QModelIndex payload
        and defer the revision lookup later, inside data().
        Because row and column info are stored anyway in QModelIndex we
        don't need to add any additional data.
*/
  if (row < 0 || row >= rowCnt)
    return QModelIndex();

  return createIndex(row, column, (void*)0);
}

QModelIndex FileHistory::parent(const QModelIndex&) const {

  static const QModelIndex no_parent;
  return no_parent;
}

const QString FileHistory::timeDiff(unsigned long secs) const {

  ulong days  =  secs / (3600 * 24);
  ulong hours = (secs - days * 3600 * 24) / 3600;
  ulong min   = (secs - days * 3600 * 24 - hours * 3600) / 60;
  ulong sec   =  secs - days * 3600 * 24 - hours * 3600 - min * 60;
  QString tmp;
  if (days > 0)
    tmp.append(QString::number(days) + "d ");

  if (hours > 0 || !tmp.isEmpty())
    tmp.append(QString::number(hours) + "h ");

  if (min > 0 || !tmp.isEmpty())
    tmp.append(QString::number(min) + "m ");

  tmp.append(QString::number(sec) + "s");
  return tmp;
}

QVariant FileHistory::data(const QModelIndex& index, int role) const {

  static const QVariant no_value;

  if (!index.isValid() || role != Qt::DisplayRole) {
    //if (role == Qt::FontRole && index.column() == qgit::HASH_COL)
    //    return qgit::TYPE_WRITER_FONT;
    return no_value; // fast path, 90% of calls ends here!
  }

  const Rev* r = git->revLookup(revOrder.at(index.row()), this);
  if (!r)
    return no_value;

  int col = index.column();

  // calculate lanes
  if (r->lanes.count() == 0)
    git->setLane(r->sha(), const_cast<FileHistory*>(this));

  if (col == qgit::ANN_ID_COL)
    return (annIdValid ? rowCnt - index.row() : QVariant());

  if (col == qgit::LOG_COL)
    return r->shortLog();

  if (col == qgit::HASH_COL) {
    if (r->sha() == qgit::ZERO_SHA)
      return QString();
    else
      return r->shortHash(git->shortHashLength());
  }

  if (col == qgit::AUTH_COL)
    return r->author();

  if (col == qgit::TIME_COL && r->sha() != qgit::ZERO_SHA) {

    if (secs != 0) // secs is 0 for absolute date
      return timeDiff(secs - r->authorDate().toULong());
    else
      return git->getLocalDate(r->authorDate());
  }
  return no_value;
}
